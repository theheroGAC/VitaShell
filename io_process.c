/*
  VitaShell
  Copyright (C) 2015-2018, TheFloW

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "main.h"
#include "io_process.h"
#include "archive.h"
#include "file.h"
#include "message_dialog.h"
#include "uncommon_dialog.h"
#include "language.h"
#include "utils.h"

static uint64_t current_value = 0;
static char current_file_name[256] = {0};
static uint64_t start_time = 0;
static uint64_t last_update_time = 0;

int cancelHandler() {
  return (updateMessageDialog() != MESSAGE_DIALOG_RESULT_RUNNING);
}

void SetProgress(uint64_t value, uint64_t max) {
  current_value = value;
}

void SetCurrentFile(const char *filename) {
  if (!filename || filename[0] == '\0') {
    current_file_name[0] = '\0';
    return;
  }
  
  // Extract just the filename from path
  const char *basename = strrchr(filename, '/');
  if (basename) {
    basename++; // Skip the '/'
  } else {
    basename = filename;
  }
  
  // Quick copy with length limit
  int i = 0;
  const int max_len = 50; // Reduced for better performance
  
  while (i < max_len - 1 && basename[i] != '\0') {
    current_file_name[i] = basename[i];
    i++;
  }
  
  // Add truncation indicator if needed
  if (basename[i] != '\0') {
    if (i > 3) {
      current_file_name[i-3] = '.';
      current_file_name[i-2] = '.';
      current_file_name[i-1] = '.';
    }
  }
  
  current_file_name[i] = '\0';
}

static int update_thread(SceSize args_size, UpdateArguments *args) {
  uint64_t previous_value = current_value;
  SceUInt64 cur_micros = 0, delta_micros = 0, last_micros = 0;
  double kbs = 0.0;
  int update_count = 0;
  
  // Initialize start time
  if (start_time == 0) {
    start_time = sceKernelGetProcessTimeWide();
    last_update_time = start_time;
    last_micros = start_time;
  }

  while (current_value < args->max && isMessageDialogRunning()) {
    // Update current time
    cur_micros = sceKernelGetProcessTimeWide();

    // Always update progress bar - this is critical!
    if (args->max > 0) {
      uint32_t progress = (uint32_t)((current_value * 100) / args->max);
      if (progress <= 100) {
        sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, progress);
      }
    }

    // Show speed info every 2 seconds to avoid lag
    if (args->show_kbs && cur_micros >= (last_micros + 2000 * 1000)) {
      delta_micros = cur_micros - last_micros;
      last_micros = cur_micros;
      
      if (delta_micros > 0 && current_value > previous_value) {
        kbs = (double)(current_value - previous_value) / 1024.0;
        previous_value = current_value;
        
        char msg[64];
        if (kbs >= 1024.0) {
          snprintf(msg, sizeof(msg), "%.1f MB/s", kbs / 1024.0);
        } else if (kbs > 0.1) {
          snprintf(msg, sizeof(msg), "%.0f KB/s", kbs);
        } else {
          strcpy(msg, "Processing...");
        }
        
        sceMsgDialogProgressBarSetInfo(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, (SceChar8 *)msg);
      }
    }
    update_count++;
    
    // Temporarily disabled KB/s for debugging progress

    sceKernelDelayThread(COUNTUP_WAIT);
  }

  return sceKernelExitDeleteThread(0);
}

SceUID createStartUpdateThread(uint64_t max, int show_kbs) {
  return createStartUpdateThreadEx(max, show_kbs, NULL, 0);
}

SceUID createStartUpdateThreadEx(uint64_t max, int show_kbs, char *current_file, int show_eta) {
  current_value = 0;
  start_time = 0;
  last_update_time = 0;
  memset(current_file_name, 0, sizeof(current_file_name));

  UpdateArguments args;
  args.max = max;
  args.show_kbs = show_kbs;
  args.current_file = current_file;
  args.show_eta = show_eta;

  // Create update thread with normal priority
  SceUID thid = sceKernelCreateThread("update_thread", (SceKernelThreadEntry)update_thread, 0xBF, 0x4000, 0, 0, NULL);
  if (thid >= 0) {
    sceKernelStartThread(thid, sizeof(UpdateArguments), &args);
  }

  return thid;
}

int delete_thread(SceSize args_size, DeleteArguments *args) {
  SceUID thid = -1;

  // Lock power timers
  powerLock();

  // Set progress to 0%
  sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, 0);
  sceKernelDelayThread(DIALOG_WAIT); // Needed to see the percentage

  FileListEntry *file_entry = fileListGetNthEntry(args->file_list, args->index);

  int count = 0;
  FileListEntry *head = NULL;
  FileListEntry *mark_entry_one = NULL;

  if (fileListFindEntry(args->mark_list, file_entry->name)) { // On marked entry
    count = args->mark_list->length;
    head = args->mark_list->head;
  } else {
    count = 1;
    mark_entry_one = fileListCopyEntry(file_entry);
    head = mark_entry_one;
  }

  char path[MAX_PATH_LENGTH];
  FileListEntry *mark_entry = NULL;

  // Get paths info
  uint64_t total = 0;
  uint32_t folders = 0, files = 0;

  mark_entry = head;

  int i;
  for (i = 0; i < count; i++) {
    snprintf(path, MAX_PATH_LENGTH, "%s%s", args->file_list->path, mark_entry->name);
    getPathInfo(path, NULL, &folders, &files, NULL);
    mark_entry = mark_entry->next;
  }

  total = folders + files;

  // Update thread
  thid = createStartUpdateThread(total, 0);

  // Remove process
  uint64_t value = 0;

  mark_entry = head;

  for (i = 0; i < count; i++) {
    snprintf(path, MAX_PATH_LENGTH, "%s%s", args->file_list->path, mark_entry->name);

    FileProcessParam param;
    param.value = &value;
    param.max = total;
    param.SetProgress = SetProgress;
    param.cancelHandler = cancelHandler;
    int res = removePath(path, &param);
    if (res <= 0) {
      closeWaitDialog();
      setDialogStep(DIALOG_STEP_CANCELED);
      errorDialog(res);
      goto EXIT;
    }

    mark_entry = mark_entry->next;
  }

  // Set progress to 100%
  sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, 100);
  sceKernelDelayThread(COUNTUP_WAIT);

  // Close
  sceMsgDialogClose();

  setDialogStep(DIALOG_STEP_DELETED);

EXIT:
  if (mark_entry_one)
    free(mark_entry_one);

  if (thid >= 0)
    sceKernelWaitThreadEnd(thid, NULL, NULL);

  // Unlock power timers
  powerUnlock();

  return sceKernelExitDeleteThread(0);
}

int copy_thread(SceSize args_size, CopyArguments *args) {
  SceUID thid = -1;

  // Lock power timers
  powerLock();

  // Set progress to 0%
  sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, 0);
  sceKernelDelayThread(DIALOG_WAIT); // Needed to see the percentage

  char path[MAX_PATH_LENGTH], src_path[MAX_PATH_LENGTH], dst_path[MAX_PATH_LENGTH];
  FileListEntry *copy_entry = NULL;

  // Check if src and dst are in the same partition when moving
  int diff_partition = 0;

  if (args->copy_mode == COPY_MODE_MOVE) {
    char *p = strchr(args->file_list->path, ':');
    char *q = strchr(args->copy_list->path, ':');
    if (p && q) {
      *p = '\0';
      *q = '\0';

      if (strcasecmp(args->file_list->path, args->copy_list->path) != 0) {
        diff_partition = 1;
      }

      *q = ':';
      *p = ':';
    }
  }

  if (args->copy_mode == COPY_MODE_MOVE && !diff_partition) { // Move
    // Update thread
    thid = createStartUpdateThread(args->copy_list->length, 0);

    copy_entry = args->copy_list->head;

    int i;
    for (i = 0; i < args->copy_list->length; i++) {
      snprintf(src_path, MAX_PATH_LENGTH, "%s%s", args->copy_list->path, copy_entry->name);
      snprintf(dst_path, MAX_PATH_LENGTH, "%s%s", args->file_list->path, copy_entry->name);

      int res = movePath(src_path, dst_path, MOVE_INTEGRATE | MOVE_REPLACE, NULL);
      if (res < 0) {
        closeWaitDialog();
        errorDialog(res);
        goto EXIT;
      }

      SetProgress(i + 1, args->copy_list->length);

      if (cancelHandler()) {
        closeWaitDialog();
        setDialogStep(DIALOG_STEP_CANCELED);
        goto EXIT;
      }

      copy_entry = copy_entry->next;
    }

    // Set progress to 100%
    sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, 100);
    sceKernelDelayThread(COUNTUP_WAIT);

    // Close
    sceMsgDialogClose();

    setDialogStep(DIALOG_STEP_MOVED);
  } else { // Copy
    // Open archive, because when you copy from an archive, you leave the archive to paste
    if (args->copy_mode == COPY_MODE_EXTRACT) {
      int res = archiveOpen(args->archive_path);
      if (res < 0) {
        closeWaitDialog();
        errorDialog(res);
        goto EXIT_ARCHIVE_OPEN;
      }
    }

    // Get src paths info
    uint64_t total = 0, size = 0;
    uint32_t folders = 0, files = 0;

    copy_entry = args->copy_list->head;

    int i;
    for (i = 0; i < args->copy_list->length; i++) {
      snprintf(src_path, MAX_PATH_LENGTH, "%s%s", args->copy_list->path, copy_entry->name);

      if (args->copy_mode == COPY_MODE_EXTRACT) {
        getArchivePathInfo(src_path, &size, &folders, &files, NULL);
      } else {
        getPathInfo(src_path, &size, &folders, &files, NULL);
      }

      copy_entry = copy_entry->next;
    }

    total = size + folders * DIRECTORY_SIZE;
    if (args->copy_mode == COPY_MODE_MOVE)
      total += files + folders;

    // Check memory card free space
	// If the copy destination is host0:, the check will fail, so host0: will not be checked.
    if (strncmp(args->file_list->path, "host0:", 6) != 0 && checkMemoryCardFreeSpace(args->file_list->path, size))
      goto EXIT;

    // Update thread with enhanced progress (show speed and ETA for copy operations)
    thid = createStartUpdateThreadEx(total, 1, NULL, 1);

    // Copy process
    uint64_t value = 0;

    copy_entry = args->copy_list->head;

    for (i = 0; i < args->copy_list->length; i++) {
      snprintf(src_path, MAX_PATH_LENGTH, "%s%s", args->copy_list->path, copy_entry->name);
      snprintf(dst_path, MAX_PATH_LENGTH, "%s%s", args->file_list->path, copy_entry->name);

      FileProcessParam param;
      param.value = &value;
      param.max = total;
      param.SetProgress = SetProgress;
      param.cancelHandler = cancelHandler;

      if (args->copy_mode == COPY_MODE_EXTRACT) {
        int res = extractArchivePath(src_path, dst_path, &param);
        if (res <= 0) {
          closeWaitDialog();
          setDialogStep(DIALOG_STEP_CANCELED);
          errorDialog(res);
          goto EXIT;
        }
      } else {
        int res = copyPath(src_path, dst_path, &param);
        if (res <= 0) {
          closeWaitDialog();
          setDialogStep(DIALOG_STEP_CANCELED);
          errorDialog(res);
          goto EXIT;
        }
      }

      copy_entry = copy_entry->next;
    }

    // Remove src when moving between partitions
    if (args->copy_mode == COPY_MODE_MOVE) {
      copy_entry = args->copy_list->head;

      for (i = 0; i < args->copy_list->length; i++) {
        snprintf(path, MAX_PATH_LENGTH, "%s%s", args->copy_list->path, copy_entry->name);

        FileProcessParam param;
        param.value = &value;
        param.max = total;
        param.SetProgress = SetProgress;
        param.cancelHandler = cancelHandler;
        int res = removePath(path, &param);
        if (res <= 0) {
          closeWaitDialog();
          setDialogStep(DIALOG_STEP_CANCELED);
          errorDialog(res);
          goto EXIT;
        }

        copy_entry = copy_entry->next;
      }
    }

    // Set progress to 100%
    sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, 100);
    sceKernelDelayThread(COUNTUP_WAIT);

    // Close
    sceMsgDialogClose();

    if (args->copy_mode == COPY_MODE_NORMAL)
      setDialogStep(DIALOG_STEP_COPIED);
    else
      setDialogStep(DIALOG_STEP_MOVED);
  }

EXIT:
  // Close archive
  if (args->copy_mode == COPY_MODE_EXTRACT)
    archiveClose();
  
EXIT_ARCHIVE_OPEN:
  if (thid >= 0)
    sceKernelWaitThreadEnd(thid, NULL, NULL);

  // Unlock power timers
  powerUnlock();

  return sceKernelExitDeleteThread(0);
}

static int mediaPathHandler(const char *path) {
  // Avoid export-ception
  if (strncasecmp(path, "ux0:music/", 10) == 0 ||
      strncasecmp(path, "ux0:video/", 10) == 0 ||
      strncasecmp(path, "ux0:picture/", 12) == 0) {
    return 1;
  }

  // The files allowed
  int type = getFileType(path);
  switch (type) {
    case FILE_TYPE_BMP:
    case FILE_TYPE_JPEG:
    case FILE_TYPE_PNG:
    case FILE_TYPE_MP3:
    case FILE_TYPE_MP4:
      return 0;
  }

  // Folders are allowed too
  SceIoStat stat;
  memset(&stat, 0, sizeof(SceIoStat));
  if (sceIoGetstat(path, &stat) >= 0 && SCE_S_ISDIR(stat.st_mode)) {
    return 0;
  }

  // Ignore the rest
  return 1;
}

static void musicExportProgress(void *data, int progress) {
  uint32_t *args = (uint32_t *)data;

  uint32_t *value = (uint32_t *)args[1];
  uint32_t old_value = *value;
  *value = (uint32_t)(((float)progress / 100.0f) * (float)args[0]);

  FileProcessParam *param = (FileProcessParam *)args[2];
  if (param) {
    if (param->value)
      (*param->value) += (*value - old_value);

    if (param->SetProgress)
      param->SetProgress(param->value ? *param->value : 0, param->max);
  }
}

static int exportMedia(char *path, uint32_t *songs, uint32_t *videos, uint32_t *pictures, FileProcessParam *process_param) {
  static char buf[64 * 1024];
  char out[MAX_PATH_LENGTH];

  SceIoStat stat;
  memset(&stat, 0, sizeof(SceIoStat));

  int res = sceIoGetstat(path, &stat);
  if (res < 0)
    return res;

  int type = getFileType(path);
  if (type == FILE_TYPE_BMP || type == FILE_TYPE_JPEG || type == FILE_TYPE_PNG) {
    PhotoExportParam param;
    memset(&param, 0, sizeof(PhotoExportParam));
    param.version = 0x03150021;
    res = scePhotoExportFromFile(path, &param, buf, process_param ? process_param->cancelHandler : NULL,
                                 NULL, out, MAX_PATH_LENGTH);
    if (res < 0)
      return (res == 0x80101A0B) ? 0 : res;

    if (process_param) {
      if (process_param->value)
        (*process_param->value) += stat.st_size;

      if (process_param->SetProgress)
        process_param->SetProgress(process_param->value ? *process_param->value : 0, process_param->max);
    }

    (*pictures)++;
  } else if (type == FILE_TYPE_MP3) {
    uint32_t value = 0;

    uint32_t args[3];
    args[0] = (uint32_t)stat.st_size;
    args[1] = (uint32_t)&value;
    args[2] = (uint32_t)process_param;

    MusicExportParam param;
    memset(&param, 0, sizeof(MusicExportParam));
    res = sceMusicExportFromFile(path, &param, buf, process_param ? process_param->cancelHandler : NULL,
                                 musicExportProgress, &args, out, MAX_PATH_LENGTH);
    if (res < 0)
      return (res == 0x8010530A) ? 0 : res;

    (*songs)++;
  } else if (type == FILE_TYPE_MP4) {
    VideoExportInputParam in_param;
    memset(&in_param, 0, sizeof(VideoExportInputParam));
    strncpy(in_param.path, path, MAX_PATH_LENGTH-1);
    in_param.path[MAX_PATH_LENGTH-1] = '\0';

    VideoExportOutputParam out_param;
    memset(&out_param, 0, sizeof(VideoExportOutputParam));

    res = sceVideoExportFromFile(&in_param, 1, buf, process_param ? process_param->cancelHandler : NULL,
                                 NULL, NULL, 0, &out_param);
    if (res < 0)
      return (res == 0x8010540A) ? 0 : res;

    if (process_param) {
      if (process_param->value)
        (*process_param->value) += stat.st_size;

      if (process_param->SetProgress)
        process_param->SetProgress(process_param->value ? *process_param->value : 0, process_param->max);
    }

    (*videos)++;
  }

  return 1;
}

int exportPath(char *path, uint32_t *songs, uint32_t *videos, uint32_t *pictures, FileProcessParam *param) {
  SceUID dfd = sceIoDopen(path);
  if (dfd >= 0) {
    int res = 0;

    do {
      SceIoDirent dir;
      memset(&dir, 0, sizeof(SceIoDirent));

      res = sceIoDread(dfd, &dir);
      if (res > 0) {
        char *new_path = malloc(strlen(path) + strlen(dir.d_name) + 2);
        snprintf(new_path, MAX_PATH_LENGTH, "%s%s%s", path, hasEndSlash(path) ? "" : "/", dir.d_name);

        if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
          int ret = exportPath(new_path, songs, videos, pictures, param);
          if (ret <= 0) {
            free(new_path);
            sceIoDclose(dfd);
            return ret;
          }
        } else {
          if (mediaPathHandler(new_path)) {
            free(new_path);
            continue;
          }

          int ret = exportMedia(new_path, songs, videos, pictures, param);
          if (ret <= 0) {
            free(new_path);
            sceIoDclose(dfd);
            return ret;
          }
        }

        free(new_path);
      }
    } while (res > 0);

    sceIoDclose(dfd);
  } else {
    if (mediaPathHandler(path))
      return 1;

    int ret = exportMedia(path, songs, videos, pictures, param);
    if (ret <= 0)
      return ret;
  }

  return 1;
}

int export_thread(SceSize args_size, ExportArguments *args) {
  SceUID thid = -1;

  // Lock power timers
  powerLock();

  // Set progress to 0%
  sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, 0);
  sceKernelDelayThread(DIALOG_WAIT); // Needed to see the percentage

  FileListEntry *file_entry = fileListGetNthEntry(args->file_list, args->index);

  int count = 0;
  FileListEntry *head = NULL;
  FileListEntry *mark_entry_one = NULL;

  if (fileListFindEntry(args->mark_list, file_entry->name)) { // On marked entry
    count = args->mark_list->length;
    head = args->mark_list->head;
  } else {
    count = 1;
    mark_entry_one = fileListCopyEntry(file_entry);
    head = mark_entry_one;
  }

  char path[MAX_PATH_LENGTH];
  FileListEntry *mark_entry = NULL;

  // Get paths info
  uint64_t size = 0;
  uint32_t files = 0;

  mark_entry = head;

  int i;
  for (i = 0; i < count; i++) {
    snprintf(path, MAX_PATH_LENGTH, "%s%s", args->file_list->path, mark_entry->name);
    getPathInfo(path, &size, NULL, &files, mediaPathHandler);
    mark_entry = mark_entry->next;
  }

  // No media files
  if (size == 0) {
    closeWaitDialog();
    infoDialog(language_container[EXPORT_NO_MEDIA]);
    goto EXIT;
  }

  // Check memory card free space
  if (checkMemoryCardFreeSpace("ux0:", size))
    goto EXIT;

  // Update thread
  thid = createStartUpdateThread(size, 0);

  // Export process
  uint64_t value = 0;
  uint32_t songs = 0, videos = 0, pictures = 0;

  mark_entry = head;

  for (i = 0; i < count; i++) {
    snprintf(path, MAX_PATH_LENGTH, "%s%s", args->file_list->path, mark_entry->name);

    FileProcessParam param;
    param.value = &value;
    param.max = size;
    param.SetProgress = SetProgress;
    param.cancelHandler = cancelHandler;

    int res = exportPath(path, &songs, &videos, &pictures, &param);
    if (res <= 0) {
      closeWaitDialog();
      setDialogStep(DIALOG_STEP_CANCELED);
      errorDialog(res);
      goto EXIT;
    }

    mark_entry = mark_entry->next;
  }

  // Set progress to 100%
  sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, 100);
  sceKernelDelayThread(COUNTUP_WAIT);

  // Close
  closeWaitDialog();

  // Info
  if (songs > 0 && videos > 0 && pictures > 0) {
    infoDialog(language_container[EXPORT_SONGS_VIDEOS_PICTURES_INFO], songs, videos, pictures);
  } else if (songs > 0 && videos > 0) {
    infoDialog(language_container[EXPORT_SONGS_VIDEOS_INFO], songs, videos);
  } else if (songs > 0 && pictures > 0) {
    infoDialog(language_container[EXPORT_SONGS_PICTURES_INFO], songs, pictures);
  } else if (videos > 0 && pictures > 0) {
    infoDialog(language_container[EXPORT_VIDEOS_PICTURES_INFO], videos, pictures);
  } else if (songs > 0) {
    infoDialog(language_container[EXPORT_SONGS_INFO], songs);
  } else if (videos > 0) {
    infoDialog(language_container[EXPORT_VIDEOS_INFO], videos);
  } else if (pictures > 0) {
    infoDialog(language_container[EXPORT_PICTURES_INFO], pictures);
  }

EXIT:
  if (mark_entry_one)
    free(mark_entry_one);

  if (thid >= 0)
    sceKernelWaitThreadEnd(thid, NULL, NULL);

  // Unlock power timers
  powerUnlock();

  return sceKernelExitDeleteThread(0);
}

int hash_thread(SceSize args_size, HashArguments *args) {
  SceUID thid = -1;

  // Lock power timers
  powerLock();

  // Set progress to 0%
  sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, 0);
  sceKernelDelayThread(DIALOG_WAIT); // Needed to see the percentage

  uint64_t max = (uint64_t)(getFileSize(args->file_path) / TRANSFER_SIZE);

  // Hash process
  uint64_t value = 0;

  // Spin off a thread to update the progress dialog 
  thid = createStartUpdateThread(max, 0);

  FileProcessParam param;
  param.value = &value;
  param.max = max;
  param.SetProgress = SetProgress;
  param.cancelHandler = cancelHandler;

  int res = 0;
  char hashmsg[128];
  memset(hashmsg, 0, sizeof(hashmsg));

  // Perform hash based on type
  switch (args->hash_type) {
    case HASH_TYPE_SHA1:
    {
      uint8_t sha1out[20];
      res = getFileSha1(args->file_path, sha1out, &param);
      if (res > 0) {
        int i;
        for (i = 0; i < 20; i++) {
          char string[4];
          sprintf(string, "%02X", sha1out[i]);
          strcat(hashmsg, string);
          if (i == 9)
            strcat(hashmsg, "\n");
        }
      }
      break;
    }
    case HASH_TYPE_MD5:
    {
      uint8_t md5out[16];
      res = getFileMd5(args->file_path, md5out, &param);
      if (res > 0) {
        int i;
        for (i = 0; i < 16; i++) {
          char string[4];
          sprintf(string, "%02X", md5out[i]);
          strcat(hashmsg, string);
          if (i == 7)
            strcat(hashmsg, "\n");
        }
      }
      break;
    }
    case HASH_TYPE_SHA256:
    {
      uint8_t sha256out[32];
      res = getFileSha256(args->file_path, sha256out, &param);
      if (res > 0) {
        int i;
        for (i = 0; i < 32; i++) {
          char string[4];
          sprintf(string, "%02X", sha256out[i]);
          strcat(hashmsg, string);
          if (i == 15)
            strcat(hashmsg, "\n");
        }
      }
      break;
    }
  }

  if (res <= 0) {
    // Hash didn't complete successfully, or was canceled
    closeWaitDialog();
    setDialogStep(DIALOG_STEP_CANCELED);
    errorDialog(res);
    goto EXIT;
  }

  // Since we hit here, we're done. Set progress to 100%
  sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, 100);
  sceKernelDelayThread(COUNTUP_WAIT);

  // Close
  closeWaitDialog();

  infoDialog(hashmsg);

EXIT:

  // Ensure the update thread ends gracefully
  if (thid >= 0)
    sceKernelWaitThreadEnd(thid, NULL, NULL);

  powerUnlock();

  // Kill current thread
  return sceKernelExitDeleteThread(0);
}
