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

#include "qr.h"

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h> 
#include <psp2/camera.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <quirc.h>
#include <vita2d.h>

#include "main.h"
#include "io_process.h"
#include "network_download.h"
#include "package_installer.h"
#include "archive.h"
#include "file.h"
#include "message_dialog.h"
#include "language.h"
#include "utils.h"

static int qr_enabled;

static struct quirc *qr;
static uint32_t* qr_data;
static char *data = NULL;
static int qr_next;

static vita2d_texture *camera_tex;

static SceCameraInfo cam_info;
static SceCameraRead cam_info_read;

static char last_qr[MAX_QR_LENGTH];
static char last_download[MAX_QR_LENGTH];
static int last_qr_len;
static int qr_scanned = 0;

static SceUID thid;

// QR scanning improvements - Performance optimized
static int qr_scan_attempts = 0;
static int qr_scan_success = 0;
static int qr_continuous_mode = 0;
static uint64_t last_scan_time = 0;
static int scan_delay_ms = 50; // Optimized delay for faster scanning
static uint64_t last_frame_time = 0;
static int frame_skip_counter = 0;
static int adaptive_skip_frames = 0;

// UI enhancement variables
static QRQuality last_qr_quality = QR_QUALITY_POOR;
static float animation_progress = 0.0f;
static int animation_direction = 1;
static uint64_t last_animation_update = 0;

int qr_thread() {
  qr = quirc_new();
  if (!qr) return -1;

  quirc_resize(qr, CAM_WIDTH, CAM_HEIGHT);
  qr_next = 1;
  qr_scanned = 0;
  qr_scan_attempts = 0;
  qr_scan_success = 0;
  memset(last_qr, 0, MAX_QR_LENGTH);

  // Performance optimized QR processing with intelligent frame skipping
  while (1) {
    uint64_t current_time = sceKernelGetSystemTimeWide();

    // Intelligent frame skipping for performance
    frame_skip_counter++;
    if (frame_skip_counter < adaptive_skip_frames + 1) {
      sceKernelDelayThread(16 * 1000); // ~60fps skip
      continue;
    }
    frame_skip_counter = 0;

    // Dynamic delay adjustment based on performance
    int actual_delay = scan_delay_ms;
    if (current_time - last_frame_time < 20000) { // If processing too fast
      actual_delay = scan_delay_ms + 10;
    }
    sceKernelDelayThread(actual_delay * 1000);

    if (qr_next == 0 && qr_scanned == 0) {
      uint8_t *image;
      int w, h;
      image = quirc_begin(qr, &w, &h);
      last_frame_time = current_time;

      if (!image) {
        qr_next = 1;
        continue;
      }

      qr_scan_attempts++;

      // Optimized grayscale conversion - faster algorithm
      uint32_t *data_ptr = qr_data;
      uint8_t *image_ptr = image;
      int total_pixels = w * h;

      // Process pixels in batches for better cache performance
      int i = 0;
      for (; i < total_pixels - 4; i += 4) {
        // Load 4 pixels at once
        uint32_t px1 = *data_ptr++;
        uint32_t px2 = *data_ptr++;
        uint32_t px3 = *data_ptr++;
        uint32_t px4 = *data_ptr++;

        // Optimized grayscale conversion (faster integer math)
        uint8_t r1 = px1 & 0xFF, g1 = (px1 >> 8) & 0xFF, b1 = (px1 >> 16) & 0xFF;
        uint8_t r2 = px2 & 0xFF, g2 = (px2 >> 8) & 0xFF, b2 = (px2 >> 16) & 0xFF;
        uint8_t r3 = px3 & 0xFF, g3 = (px3 >> 8) & 0xFF, b3 = (px3 >> 16) & 0xFF;
        uint8_t r4 = px4 & 0xFF, g4 = (px4 >> 8) & 0xFF, b4 = (px4 >> 16) & 0xFF;

        // Faster grayscale: Y = (R*76 + G*150 + B*30) >> 8
        *image_ptr++ = (r1 * 76 + g1 * 150 + b1 * 30) >> 8;
        *image_ptr++ = (r2 * 76 + g2 * 150 + b2 * 30) >> 8;
        *image_ptr++ = (r3 * 76 + g3 * 150 + b3 * 30) >> 8;
        *image_ptr++ = (r4 * 76 + g4 * 150 + b4 * 30) >> 8;
      }

      // Handle remaining pixels
      for (; i < total_pixels; i++) {
        uint32_t colourRGBA = *data_ptr++;
        uint8_t r = colourRGBA & 0xFF;
        uint8_t g = (colourRGBA >> 8) & 0xFF;
        uint8_t b = (colourRGBA >> 16) & 0xFF;
        *image_ptr++ = (r * 76 + g * 150 + b * 30) >> 8;
      }

      quirc_end(qr);

      // Quick check for QR codes with early exit optimization
      int num_codes = quirc_count(qr);
      if (num_codes > 0) {
        struct quirc_code code;
        struct quirc_data data;
        quirc_decode_error_t err;

        // Optimized: try primary code first, then others if needed
        quirc_extract(qr, 0, &code);
        err = quirc_decode(&code, &data);

        if (!err && data.payload_len > 0 && data.payload_len < MAX_QR_LENGTH) {
          if (validateQRData((const char*)data.payload, data.payload_len)) {
            if (last_qr_len == 0 || memcmp(last_qr, data.payload, data.payload_len) != 0) {
              memcpy(last_qr, data.payload, data.payload_len);
              last_qr_len = data.payload_len;
              qr_scan_success++;
              qr_scanned = 1;
            }
          }
        }

        // Check additional codes only if no success and we have time
        if (!qr_scanned && num_codes > 1) {
          for (int code_index = 1; code_index < num_codes && code_index < 4; code_index++) {
            quirc_extract(qr, code_index, &code);
            err = quirc_decode(&code, &data);

            if (!err && data.payload_len > 0 && data.payload_len < MAX_QR_LENGTH) {
              if (validateQRData((const char*)data.payload, data.payload_len)) {
                if (last_qr_len == 0 || memcmp(last_qr, data.payload, data.payload_len) != 0) {
                  memcpy(last_qr, data.payload, data.payload_len);
                  last_qr_len = data.payload_len;
                  qr_scan_success++;
                  qr_scanned = 1;
                  break; // Found one, exit early
                }
              }
            }
          }
        }
      }

      qr_next = 1;

      // Adaptive performance tuning
      if (qr_scan_attempts > 100) {
        if (qr_scan_success == 0) {
          adaptive_skip_frames = 2; // Skip more frames if no success
          scan_delay_ms = 150;
        } else if (qr_scan_success < 5) {
          adaptive_skip_frames = 1; // Skip some frames if poor success
          scan_delay_ms = 75;
        } else {
          adaptive_skip_frames = 0; // Process every frame if good success
          scan_delay_ms = 33; // ~30fps processing
        }
        qr_scan_attempts = 0;
        qr_scan_success = 0;
      }
    }

    // Reset scanned flag after a short delay to allow for new scans
    if (qr_scanned == 1) {
      sceKernelDelayThread(50000); // Reduced delay after successful scan
      qr_scanned = 0;
    }
  }
  return 0;
}

int qr_scan_thread(SceSize args, void *argp) {
  data = last_qr;
  if (last_qr_len > 4) {
    if (!(data[0] == 'h' && data[1] == 't' && data[2] == 't' && data[3] == 'p')) {
      initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_OK, language_container[QR_SHOW_CONTENTS], data);
      setDialogStep(DIALOG_STEP_QR_SHOW_CONTENTS);
      return sceKernelExitDeleteThread(0);
    }
  } else {
    initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_OK, language_container[QR_SHOW_CONTENTS], data);
    setDialogStep(DIALOG_STEP_QR_SHOW_CONTENTS);
    return sceKernelExitDeleteThread(0);
  }
  
  initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_NONE, language_container[PLEASE_WAIT]);

  // check for attached file
  const char *headerData;
  unsigned int headerLen;
  unsigned int fileNameLength = 0;
  int vpk = 0;
  char *fileName = NULL;
  int64_t fileSize;
  long code = 0;
  char sizeString[16];
  int ret;


  ret = getDownloadFileInfo(data, &fileSize, &fileName, &code);

  if (ret < 0)
    goto NETWORK_FAILURE;

  sceMsgDialogClose();

  // Wait for it to stop loading
  while (isMessageDialogRunning()) {
    sceKernelDelayThread(10 * 1000);
  }

  if (code != 200 || fileSize < 0)
  {
    free(fileName);
    initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_YESNO, language_container[QR_OPEN_WEBSITE], data);
    setDialogStep(DIALOG_STEP_QR_OPEN_WEBSITE);
    return sceKernelExitDeleteThread(0);
  }

  getSizeString(sizeString, fileSize);

  char *ext = strrchr(fileName, '.');
  if (ext) {
    vpk = getFileType(fileName) == FILE_TYPE_VPK;
  } else {
    free(fileName);
    initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_YESNO, language_container[QR_OPEN_WEBSITE], data);
    setDialogStep(DIALOG_STEP_QR_OPEN_WEBSITE);
    return sceKernelExitDeleteThread(0);
  }

  if (vpk)
    initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_YESNO, language_container[QR_CONFIRM_INSTALL], data, fileName, sizeString);
  else
    initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_YESNO, language_container[QR_CONFIRM_DOWNLOAD], data, fileName, sizeString);
  setDialogStep(DIALOG_STEP_QR_CONFIRM);
  
  // Wait for response
  while (getDialogStep() == DIALOG_STEP_QR_CONFIRM) {
    sceKernelDelayThread(10 * 1000);
  }
  
  // No
  if (getDialogStep() == DIALOG_STEP_NONE) {
    free(fileName);
    goto EXIT;
  }
  
  // Yes
  char download_path[MAX_URL_LENGTH];
  char short_name[MAX_URL_LENGTH];
  int count = 0;

  if (ext) {
    int len = ext-fileName;
    if (len > sizeof(short_name) - 1)
      len = sizeof(short_name) - 1;
    strncpy(short_name, fileName, len);
    short_name[len] = '\0';
  } else {
    strncpy(short_name, fileName, sizeof(short_name) - 1);
    ext = "";
  }

  while (1) {
    if (count == 0)
      snprintf(download_path, sizeof(download_path) - 1, "ux0:download/%s", fileName);
    else
      snprintf(download_path, sizeof(download_path) - 1, "ux0:download/%s (%d)%s", short_name, count, ext);

    SceIoStat stat;
    memset(&stat, 0, sizeof(SceIoStat));
    if (sceIoGetstat(download_path, &stat) < 0)
      break;
    count++;
  }

  free(fileName);
  
  sceIoMkdir("ux0:download", 0006);
  
  strcpy(last_download, download_path);
  if (vpk)
  {
    return downloadFileProcess(data, download_path, DIALOG_STEP_QR_DOWNLOADED_VPK);
  }
  else
  {
    return downloadFileProcess(data, download_path, DIALOG_STEP_QR_DOWNLOADED);
  }

EXIT:
  return sceKernelExitDeleteThread(0);

NETWORK_FAILURE:
  sceMsgDialogClose();
  while (isMessageDialogRunning()) {
    sceKernelDelayThread(10 * 1000);
  }

  initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_YESNO, language_container[QR_OPEN_WEBSITE], data);
  setDialogStep(DIALOG_STEP_QR_OPEN_WEBSITE);
  return sceKernelExitDeleteThread(0);
}

int initQR() {
  // Initialize camera texture first
  SceKernelMemBlockType orig = vita2d_texture_get_alloc_memblock_type();
  vita2d_texture_set_alloc_memblock_type(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW);
  camera_tex = vita2d_create_empty_texture(CAM_WIDTH, CAM_HEIGHT);
  vita2d_texture_set_alloc_memblock_type(orig);

  if (!camera_tex) {
    qr_enabled = 0;
    return -1;
  }

  // Configure camera for optimal QR scanning
  cam_info.size = sizeof(SceCameraInfo);
  cam_info.format = SCE_CAMERA_FORMAT_ABGR;
  cam_info.resolution = SCE_CAMERA_RESOLUTION_640_360;
  cam_info.pitch = vita2d_texture_get_stride(camera_tex) - (CAM_WIDTH << 2);
  cam_info.sizeIBase = (CAM_WIDTH * CAM_HEIGHT) << 2;
  cam_info.pIBase = vita2d_texture_get_datap(camera_tex);
  cam_info.framerate = 30; // Increased framerate for better scanning

  cam_info_read.size = sizeof(SceCameraRead);
  cam_info_read.mode = 0; // Use default read mode

  // Try to open camera with better error handling
  int cam_result = sceCameraOpen(1, &cam_info);
  if (cam_result < 0) {
    qr_enabled = 0;
    vita2d_free_texture(camera_tex);
    return -1;
  }

  // Create QR processing thread with maximum priority for optimal performance
  thid = sceKernelCreateThread("qr_decode_thread", qr_thread, 0x10000100, 0x100000, 0, 0, NULL);
  if (thid < 0) {
    sceCameraClose(1);
    vita2d_free_texture(camera_tex);
    qr_enabled = 0;
    return -1;
  }

  int thread_result = sceKernelStartThread(thid, 0, NULL);
  if (thread_result < 0) {
    sceKernelDeleteThread(thid);
    sceCameraClose(1);
    vita2d_free_texture(camera_tex);
    qr_enabled = 0;
    return -1;
  }

  qr_enabled = 1;
  return 0;
}

int finishQR() {
  sceKernelDeleteThread(thid);
  vita2d_free_texture(camera_tex);
  sceCameraClose(1);
  quirc_destroy(qr);
  return 0;
}

int startQR() {
  return sceCameraStart(1);
}

int stopQR() {
  int res = sceCameraStop(1);

  int y;
  for (y = 0; y < CAM_HEIGHT; y++) {
    int x;
    for (x = 0; x < CAM_WIDTH; x++) {
      ((uint32_t *)qr_data)[x + CAM_WIDTH * y] = 0;
    }
  }

  return res;
}

int renderCameraQR(int x, int y) {  
  sceCameraRead(1, &cam_info_read);
  vita2d_draw_texture(camera_tex, x, y);
  if (qr_next) {
    qr_data = (uint32_t *)vita2d_texture_get_datap(camera_tex);
    qr_next = 0;
  }
  return 0;
} 

char *getLastQR() {
  return last_qr;
}

char *getLastDownloadQR() {
  return last_download;
}

int scannedQR() {
  return qr_scanned;
}

void setScannedQR(int s) {
  qr_scanned = s;
}

int enabledQR() {
  return qr_enabled;
}

// Additional QR scanning improvements
int getQRScanStats(int *attempts, int *success) {
  if (attempts) *attempts = qr_scan_attempts;
  if (success) *success = qr_scan_success;
  return qr_scan_success > 0 ? 0 : -1;
}

int setQRScanDelay(int delay_ms) {
  if (delay_ms < 10) delay_ms = 10; // Minimum delay
  if (delay_ms > 1000) delay_ms = 1000; // Maximum delay
  scan_delay_ms = delay_ms;
  return 0;
}

int resetQRScanStats() {
  qr_scan_attempts = 0;
  qr_scan_success = 0;
  return 0;
}

// Enhanced QR processing with better data validation
int validateQRData(const char *data, int length) {
  if (!data || length <= 0 || length >= MAX_QR_LENGTH) {
    return 0;
  }

  // Check for valid URL patterns
  if (length > 4) {
    if ((data[0] == 'h' && data[1] == 't' && data[2] == 't' && data[3] == 'p') ||
        (data[0] == 'H' && data[1] == 'T' && data[2] == 'T' && data[3] == 'P') ||
        (data[0] == 'f' && data[1] == 't' && data[2] == 'p' && data[3] == ':') ||
        (data[0] == 'F' && data[1] == 'T' && data[2] == 'P' && data[3] == ':')) {
      return 1; // Valid URL
    }
  }

  // Check for other common QR data patterns
  if (length > 0 && length < 256) {
    return 1; // Assume valid for reasonable length data
  }

  return 0;
}

// Reset QR scanner completely for fresh start
void resetQRScanner() {
  // Reset all scanning variables
  qr_scanned = 0;
  qr_scan_attempts = 0;
  qr_scan_success = 0;
  last_qr_len = 0;
  scan_delay_ms = 50; // Reset to optimized default

  // Reset performance optimization variables
  last_frame_time = 0;
  frame_skip_counter = 0;
  adaptive_skip_frames = 0;

  // Clear QR data buffers
  memset(last_qr, 0, MAX_QR_LENGTH);
  memset(last_download, 0, MAX_QR_LENGTH);

  // Reset quirc library state if initialized
  if (qr) {
    quirc_end(qr);
  }

  // Reset UI enhancement variables
  last_qr_quality = QR_QUALITY_POOR;
  animation_progress = 0.0f;
  animation_direction = 1;
  last_animation_update = 0;
}

// Get QR code quality based on data length and content
QRQuality getQRQuality(const char *data, int length) {
  if (!data || length <= 0) {
    return QR_QUALITY_POOR;
  }

  // Evaluate based on length (typical QR codes are 10-200 characters)
  if (length < 10) {
    return QR_QUALITY_POOR;
  } else if (length < 50) {
    return QR_QUALITY_FAIR;
  } else if (length < 150) {
    return QR_QUALITY_GOOD;
  } else {
    return QR_QUALITY_EXCELLENT;
  }
}

// Draw targeting frame to help user center QR code
void drawQRTargetingFrame(int x, int y, int size) {
  int frame_size = size;
  int thickness = 3;
  int bracket_size = frame_size / 3; // Size of each corner bracket

  // Use direct color values for targeting frame (bright green)
  uint32_t target_color = 0xFF00FF00; // Bright green

  // Draw corner brackets for targeting (3 lines per corner for visibility)

  // Top-left corner
  vita2d_draw_rectangle(x, y, bracket_size, thickness, target_color);                          // Top horizontal
  vita2d_draw_rectangle(x, y, thickness, bracket_size, target_color);                          // Left vertical

  // Top-right corner
  vita2d_draw_rectangle(x + frame_size - bracket_size, y, bracket_size, thickness, target_color); // Top horizontal
  vita2d_draw_rectangle(x + frame_size - thickness, y, thickness, bracket_size, target_color);     // Right vertical

  // Bottom-left corner - MATEMATICA PRECISA
  vita2d_draw_rectangle(x, y + frame_size - thickness, bracket_size, thickness, target_color);          // Bottom horizontal (CORRECTED)
  vita2d_draw_rectangle(x, y + frame_size - bracket_size, thickness, bracket_size, target_color);       // Left vertical (FIXED - moved above)

  // Bottom-right corner
  vita2d_draw_rectangle(x + frame_size - bracket_size, y + frame_size - thickness, bracket_size, thickness, target_color); // Bottom horizontal
  vita2d_draw_rectangle(x + frame_size - thickness, y + frame_size - bracket_size, thickness, bracket_size, target_color);  // Right vertical
}

// Draw quality indicator with color-coded bars
void drawQRQualityIndicator(int x, int y, QRQuality quality) {
  int bar_width = 20;
  int bar_height = 4;
  int spacing = 2;

  // Use direct color values for quality indicator
  uint32_t poor_color = 0xFFFF0000;     // Red for poor
  uint32_t fair_color = 0xFFFF7F00;     // Orange for fair
  uint32_t good_color = 0xFFFFFF00;     // Yellow for good
  uint32_t excellent_color = 0xFF00FF00; // Green for excellent
  uint32_t bg_color = 0xFF3F3F3F;       // Dark gray background
  uint32_t text_color = 0xFFFFFFFF;     // White text

  uint32_t colors[] = {
    poor_color,
    fair_color,
    good_color,
    excellent_color
  };

  // Draw quality bars
  for (int i = 0; i < 4; i++) {
    uint32_t color = (i <= quality) ? colors[i] : bg_color;
    vita2d_draw_rectangle(x + i * (bar_width + spacing), y, bar_width, bar_height, color);
  }

  // Draw quality text
  const char* quality_texts[] = { "POOR", "FAIR", "GOOD", "EXCELLENT" };
  pgf_draw_text(x, y + bar_height + 8, text_color, quality_texts[quality]);
}



// Update scanning animation progress
void updateQRAnimation(float *animation_progress) {
  uint64_t current_time = sceKernelGetSystemTimeWide();

  // Update animation every 50ms for smooth motion
  if (current_time - last_animation_update > 50000) {
    *animation_progress += animation_direction * 0.1f;

    // Reverse direction at boundaries
    if (*animation_progress >= 1.0f) {
      *animation_progress = 1.0f;
      animation_direction = -1;
    } else if (*animation_progress <= 0.0f) {
      *animation_progress = 0.0f;
      animation_direction = 1;
    }

    last_animation_update = current_time;
  }
}
