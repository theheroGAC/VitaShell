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
#include "config.h"
#include "init.h"
#include "theme.h"
#include "language.h"
#include "settings.h"
#include "message_dialog.h"
#include "ime_dialog.h"
#include "utils.h"

static void restartShell();
static void rebootDevice();
static void shutdownDevice();
static void suspendDevice();

static int changed = 0;
static int theme = 0;
int qr_used = 0; // Track if QR has been used once

static char spoofed_version[6];

static SettingsMenuEntry *settings_menu_entries = NULL;
static int n_settings_entries = 0;

static char *usbdevice_options[4];
static char *select_button_options[3];
static char *focus_color_options_texts[6];

static char **theme_options = NULL;
static int theme_count = 0;
static char *theme_name = NULL;

static int language_changed = 0; // Flag to track language changes
static char *language_options[19]; // Complete language options array

static ConfigEntry settings_entries[] = {
  { "LANGUAGE",            CONFIG_TYPE_DECIMAL, (int *)&language },
  { "USBDEVICE",          CONFIG_TYPE_DECIMAL, (int *)&vitashell_config.usbdevice },
  { "SELECT_BUTTON",      CONFIG_TYPE_DECIMAL, (int *)&vitashell_config.select_button },
  { "DISABLE_AUTOUPDATE", CONFIG_TYPE_BOOLEAN, (int *)&vitashell_config.disable_autoupdate },
  { "DISABLE_WARNING",    CONFIG_TYPE_BOOLEAN, (int *)&vitashell_config.disable_warning },
  { "FOCUS_COLOR",        CONFIG_TYPE_DECIMAL, (int *)&vitashell_config.focus_color },
};

static ConfigEntry theme_entries[] = {
  { "THEME_NAME", CONFIG_TYPE_STRING, (void *)&theme_name },
};

SettingsMenuOption main_settings[] = {
  // { VITASHELL_SETTINGS_LANGUAGE,     SETTINGS_OPTION_TYPE_OPTIONS, NULL, NULL, 0,
  //   language_options, sizeof(language_options) / sizeof(char *), &language },
  { VITASHELL_SETTINGS_THEME,           SETTINGS_OPTION_TYPE_OPTIONS, NULL, NULL, 0, NULL, 0, NULL },
  
  { VITASHELL_SETTINGS_USBDEVICE,       SETTINGS_OPTION_TYPE_OPTIONS, NULL, NULL, 0,
    usbdevice_options, sizeof(usbdevice_options) / sizeof(char **), &vitashell_config.usbdevice },
  { VITASHELL_SETTINGS_SELECT_BUTTON,   SETTINGS_OPTION_TYPE_OPTIONS, NULL, NULL, 0,
    select_button_options, sizeof(select_button_options) / sizeof(char **), &vitashell_config.select_button },
  { VITASHELL_SETTINGS_FOCUS_COLOR,     SETTINGS_OPTION_TYPE_OPTIONS, NULL, NULL, 0,
    focus_color_options_texts, 6, &vitashell_config.focus_color }, // 6 colors
  { VITASHELL_SETTINGS_NO_AUTO_UPDATE,  SETTINGS_OPTION_TYPE_BOOLEAN, NULL, NULL, 0, NULL, 0, &vitashell_config.disable_autoupdate },
  { VITASHELL_SETTINGS_WARNING_MESSAGE, SETTINGS_OPTION_TYPE_BOOLEAN, NULL, NULL, 0, NULL, 0, &vitashell_config.disable_warning },

  { VITASHELL_SETTINGS_RESTART_SHELL,   SETTINGS_OPTION_TYPE_CALLBACK, (void *)restartShell, NULL, 0, NULL, 0, NULL },
};

SettingsMenuOption power_settings[] = {
  { VITASHELL_SETTINGS_REBOOT,    SETTINGS_OPTION_TYPE_CALLBACK, (void *)rebootDevice, NULL, 0, NULL, 0, NULL },
  { VITASHELL_SETTINGS_POWEROFF,  SETTINGS_OPTION_TYPE_CALLBACK, (void *)shutdownDevice, NULL, 0, NULL, 0, NULL },
  { VITASHELL_SETTINGS_STANDBY,   SETTINGS_OPTION_TYPE_CALLBACK, (void *)suspendDevice, NULL, 0, NULL, 0, NULL },
};

SettingsMenuOption language_settings[] = {
  { VITASHELL_SETTINGS_LANGUAGE,  SETTINGS_OPTION_TYPE_OPTIONS, NULL, NULL, 0,
    language_options, 12, &language }, // Only 12 languages actually set
};

SettingsMenuEntry vitashell_settings_menu_entries[] = {
  { VITASHELL_SETTINGS_MAIN,  main_settings,  sizeof(main_settings) / sizeof(SettingsMenuOption) },
  { VITASHELL_SETTINGS_LANGUAGE, language_settings, sizeof(language_settings) / sizeof(SettingsMenuOption) },
  { VITASHELL_SETTINGS_POWER, power_settings, sizeof(power_settings) / sizeof(SettingsMenuOption) },
};

static SettingsMenu settings_menu;

void loadSettingsConfig() {
  // Load settings config file
  memset(&vitashell_config, 0, sizeof(VitaShellConfig));
  readConfig("ux0:VitaShell/settings.txt", settings_entries, sizeof(settings_entries) / sizeof(ConfigEntry));
}

void saveSettingsConfig() {
  // Save settings config file
  writeConfig("ux0:VitaShell/settings.txt", settings_entries, sizeof(settings_entries) / sizeof(ConfigEntry));

  if (sceKernelGetModel() == SCE_KERNEL_MODEL_VITATV) {
    vitashell_config.select_button = SELECT_BUTTON_MODE_FTP;
  }
}

static void restartShell() {
  closeSettingsMenu();
  sceAppMgrLoadExec("app0:eboot.bin", NULL, NULL);
}

static void rebootDevice() {
  closeSettingsMenu();
  scePowerRequestColdReset();
}

static void shutdownDevice() {
  closeSettingsMenu();
  scePowerRequestStandby();
}

static void suspendDevice() {
  closeSettingsMenu();
  scePowerRequestSuspend();
}

void initSettingsMenu() {
  int i;

  memset(&settings_menu, 0, sizeof(SettingsMenu));
  settings_menu.status = SETTINGS_MENU_CLOSED;

  n_settings_entries = sizeof(vitashell_settings_menu_entries) / sizeof(SettingsMenuEntry);
  settings_menu_entries = vitashell_settings_menu_entries;

  for (i = 0; i < n_settings_entries; i++)
    settings_menu.n_options += settings_menu_entries[i].n_options;

  usbdevice_options[0] = language_container[VITASHELL_SETTINGS_USB_MEMORY_CARD];
  usbdevice_options[1] = language_container[VITASHELL_SETTINGS_USB_GAME_CARD];
  usbdevice_options[2] = language_container[VITASHELL_SETTINGS_USB_SD2VITA];
  usbdevice_options[3] = language_container[VITASHELL_SETTINGS_USB_PSVSD];

  select_button_options[0] = language_container[VITASHELL_SETTINGS_SELECT_BUTTON_USB];
  select_button_options[1] = language_container[VITASHELL_SETTINGS_SELECT_BUTTON_FTP];
  select_button_options[2] = language_container[VITASHELL_SETTINGS_SELECT_BUTTON_QR];

  focus_color_options_texts[0] = language_container[VITASHELL_SETTINGS_FOCUS_COLOR_DEFAULT];
  focus_color_options_texts[1] = language_container[VITASHELL_SETTINGS_FOCUS_COLOR_LIGHT_GREY];
  focus_color_options_texts[2] = language_container[VITASHELL_SETTINGS_FOCUS_COLOR_BLUE];
  focus_color_options_texts[3] = language_container[VITASHELL_SETTINGS_FOCUS_COLOR_RED];
  focus_color_options_texts[4] = language_container[VITASHELL_SETTINGS_FOCUS_COLOR_PINK];
  focus_color_options_texts[5] = language_container[VITASHELL_SETTINGS_FOCUS_COLOR_YELLOW];

  // Language options - must match lang[] array in language.c order!
  language_options[0] = "Japanese";       // matches lang[0]
  language_options[1] = "English/US";     // matches lang[1]
  language_options[2] = "French";         // matches lang[2]
  language_options[3] = "Spanish";        // matches lang[3]
  language_options[4] = "German";         // matches lang[4]
  language_options[5] = "Italian";        // matches lang[5]
  language_options[6] = "Dutch";          // matches lang[6]
  language_options[7] = "Portuguese";     // matches lang[7]
  language_options[8] = "Russian";        // matches lang[8]
  language_options[9] = "Korean";         // matches lang[9]
  language_options[10] = "Chinese (Trad)";// matches lang[10]
  language_options[11] = "Chinese (Simp)";// matches lang[11]
  // Set only first 12 options, rest unused for now

  theme_options = malloc(MAX_THEMES * sizeof(char *));

  for (i = 0; i < MAX_THEMES; i++)
    theme_options[i] = malloc(MAX_THEME_LENGTH);
}

void openSettingsMenu() {
  settings_menu.status = SETTINGS_MENU_OPENING;
  settings_menu.entry_sel = 0;
  settings_menu.option_sel = 0;

  // Get current theme
  if (theme_name)
    free(theme_name);

  readConfig("ux0:VitaShell/theme/theme.txt", theme_entries, sizeof(theme_entries) / sizeof(ConfigEntry));

  // Get theme index in main tab
  int theme_index = -1;

  int i;
  for (i = 0; i < (sizeof(main_settings) / sizeof(SettingsMenuOption)); i++) {
    if (main_settings[i].name == VITASHELL_SETTINGS_THEME) {
      theme_index = i;
      break;
    }
  }

  // Find all themes
  if (theme_index >= 0) {
    SceUID dfd = sceIoDopen("ux0:VitaShell/theme");
    if (dfd >= 0) {
      theme_count = 0;
      theme = 0;

      int res = 0;

      do {
        SceIoDirent dir;
        memset(&dir, 0, sizeof(SceIoDirent));

        res = sceIoDread(dfd, &dir);
        if (res > 0) {
          if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
            if (theme_name && strcasecmp(dir.d_name, theme_name) == 0)
              theme = theme_count;
            
            snprintf(theme_options[theme_count], MAX_THEME_LENGTH, "%s", dir.d_name);
            theme_count++;
          }
        }
      } while (res > 0 && theme_count < MAX_THEMES);
      
      sceIoDclose(dfd);
      
      main_settings[theme_index].options = theme_options;
      main_settings[theme_index].n_options = theme_count;
      main_settings[theme_index].value = &theme;
    }
  }

  changed = 0;
}

void closeSettingsMenu() {
  settings_menu.status = SETTINGS_MENU_CLOSING;

  // Save settings
  if (changed) {
    saveSettingsConfig();

    // Save theme config file
    theme_entries[0].value = &theme_options[theme];
    writeConfig("ux0:VitaShell/theme/theme.txt", theme_entries, sizeof(theme_entries) / sizeof(ConfigEntry));
    theme_entries[0].value = (void *)&theme_name;
  }

  // If language changed, ask for restart
  if (language_changed) {
    // Reset flag
    language_changed = 0;

    // Show language restart message (translated)
    initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_YESNO, language_container[LANGUAGE_CHANGED_RESTART_MSG]);
    setDialogStep(DIALOG_STEP_SETTINGS_AGREEMENT);
  }
}

int getSettingsMenuStatus() {
  return settings_menu.status;
}

void drawSettingsMenu() {
  if (settings_menu.status == SETTINGS_MENU_CLOSED)
    return;

  // Closing settings menu
  if (settings_menu.status == SETTINGS_MENU_CLOSING) {
    if (settings_menu.cur_pos > 0.0f) {
      settings_menu.cur_pos -= easeOut(0.0f, settings_menu.cur_pos, 0.25f, 0.01f);
    } else {
      settings_menu.status = SETTINGS_MENU_CLOSED;
    }
  }

  // Opening settings menu
  if (settings_menu.status == SETTINGS_MENU_OPENING) {
    if (settings_menu.cur_pos < SCREEN_HEIGHT) {
      settings_menu.cur_pos += easeOut(settings_menu.cur_pos, SCREEN_HEIGHT, 0.25f, 0.01f);
    } else {
      settings_menu.status = SETTINGS_MENU_OPENED;
    }
  }

  // Draw settings menu
  vita2d_draw_texture(settings_image, 0.0f, SCREEN_HEIGHT - settings_menu.cur_pos);

  float y = SCREEN_HEIGHT - settings_menu.cur_pos + START_Y;

  int i;
  for (i = 0; i < n_settings_entries; i++) {
    // Title
    float x = pgf_text_width(language_container[settings_menu_entries[i].name]);
    pgf_draw_text(ALIGN_CENTER(SCREEN_WIDTH, x), y, SETTINGS_MENU_TITLE_COLOR, language_container[settings_menu_entries[i].name]);

    y += FONT_Y_SPACE;

    SettingsMenuOption *options = settings_menu_entries[i].options;

    int j;
    int visible_count = 0;
    for (j = 0; j < settings_menu_entries[i].n_options; j++) {
      // Skip QR option completely if used
      if (options[j].name == VITASHELL_SETTINGS_SELECT_BUTTON && qr_used == 1 &&
          options[j].value && *(options[j].value) == SELECT_BUTTON_MODE_QR) {
        continue;
      }

      // Count visible options for proper selection index
      visible_count++;
      int current_visible_index = visible_count - 1;

      int is_selected = (settings_menu.entry_sel == i && settings_menu.option_sel == current_visible_index);

      // Focus for selected visible option
      if (is_selected)
        vita2d_draw_rectangle(SHELL_MARGIN_X, y + 3.0f, MARK_WIDTH, FONT_Y_SPACE, SETTINGS_MENU_FOCUS_COLOR);

      if (options[j].type == SETTINGS_OPTION_TYPE_CALLBACK) {
        float x = pgf_text_width(language_container[options[j].name]);
        pgf_draw_text(ALIGN_CENTER(SCREEN_WIDTH, x), y, SETTINGS_MENU_ITEM_COLOR, language_container[options[j].name]);
      } else {
        float x = pgf_text_width(language_container[options[j].name]);
        pgf_draw_text(ALIGN_RIGHT(SCREEN_HALF_WIDTH - 10.0f, x), y, SETTINGS_MENU_ITEM_COLOR, language_container[options[j].name]);

        // Option
        switch (options[j].type) {
          case SETTINGS_OPTION_TYPE_BOOLEAN:
            pgf_draw_text(SCREEN_HALF_WIDTH + 10.0f, y, SETTINGS_MENU_OPTION_COLOR,
                          (options[j].value && *(options[j].value)) ? language_container[ON] : language_container[OFF]);
            break;

          case SETTINGS_OPTION_TYPE_STRING:
            pgf_draw_text(SCREEN_HALF_WIDTH + 10.0f, y, SETTINGS_MENU_OPTION_COLOR, options[j].string);
            break;

          case SETTINGS_OPTION_TYPE_OPTIONS:
          {
            int value = 0;
            if (options[j].value)
              value = *(options[j].value);
            pgf_draw_text(SCREEN_HALF_WIDTH + 10.0f, y, SETTINGS_MENU_OPTION_COLOR, options[j].options ? options[j].options[value] : "");
            break;
          }
        }
      }

      y += FONT_Y_SPACE;
    }

    y += FONT_Y_SPACE;
  }
}

static int agreement = SETTINGS_AGREEMENT_NONE;

void settingsAgree() {
  agreement = SETTINGS_AGREEMENT_AGREE;
}

void settingsDisagree() {
  agreement = SETTINGS_AGREEMENT_DISAGREE;
}

void settingsMenuCtrl() {
  // Calculate visible options count for current entry (skipping QR if used)
  int visible_option_count = 0;
  int total_options = settings_menu_entries[settings_menu.entry_sel].n_options;
  for (int j = 0; j < total_options; j++) {
    SettingsMenuOption *opt = &settings_menu_entries[settings_menu.entry_sel].options[j];
    if (!(opt->name == VITASHELL_SETTINGS_SELECT_BUTTON && qr_used == 1 &&
          opt->value && *(opt->value) == SELECT_BUTTON_MODE_QR)) {
      visible_option_count++;
    }
  }

  // Find the actual option index from visible selection
  int actual_option_index = -1;
  int visible_index = 0;
  for (int j = 0; j < total_options; j++) {
    SettingsMenuOption *opt = &settings_menu_entries[settings_menu.entry_sel].options[j];
    if (!(opt->name == VITASHELL_SETTINGS_SELECT_BUTTON && qr_used == 1 &&
          opt->value && *(opt->value) == SELECT_BUTTON_MODE_QR)) {
      if (visible_index == settings_menu.option_sel) {
        actual_option_index = j;
        break;
      }
      visible_index++;
    }
  }

  if (actual_option_index == -1) {
    // Safety: reset selection if something went wrong
    settings_menu.option_sel = 0;
    actual_option_index = 0;
  }

  SettingsMenuOption *option = &settings_menu_entries[settings_menu.entry_sel].options[actual_option_index];

  // Agreement
  if (agreement != SETTINGS_AGREEMENT_NONE) {
    agreement = SETTINGS_AGREEMENT_NONE;
  }

  // Change options - all visible options are interactable
  if (pressed_pad[PAD_ENTER] || pressed_pad[PAD_LEFT] || pressed_pad[PAD_RIGHT]) {
    changed = 1;

    switch (option->type) {
      case SETTINGS_OPTION_TYPE_BOOLEAN:
        if (option->value)
          *(option->value) = !*(option->value);
        break;

      case SETTINGS_OPTION_TYPE_STRING:
        initImeDialog(language_container[option->name], option->string, option->size_string, SCE_IME_TYPE_EXTENDED_NUMBER, 0, 0);
        setDialogStep(DIALOG_STEP_SETTINGS_STRING);
        break;

      case SETTINGS_OPTION_TYPE_CALLBACK:
        if (option->callback)
          option->callback(&option);
        break;

      case SETTINGS_OPTION_TYPE_OPTIONS:
      {
        if (option->value) {
          int old_value = *(option->value);

          if (option->name == VITASHELL_SETTINGS_SELECT_BUTTON) {
            // Special cycling for SELECT_BUTTON: skip QR if used
            int max_visible = qr_used ? 1 : 2; // 2 options if QR not used (0,1), 1 option if used (0,1 but skip 2)

            if (pressed_pad[PAD_LEFT]) {
              do {
                if (*(option->value) > 0)
                  (*(option->value))--;
                else
                  *(option->value) = max_visible;
              } while (qr_used && *(option->value) == 2); // Skip QR if hidden
            } else if (pressed_pad[PAD_ENTER] || pressed_pad[PAD_RIGHT]) {
              do {
                if (*(option->value) < max_visible)
                  (*(option->value))++;
                else
                  *(option->value) = 0;
              } while (qr_used && *(option->value) == 2); // Skip QR if hidden
            }
          } else {
            // Normal cycling for other options
            if (pressed_pad[PAD_LEFT]) {
              if (*(option->value) > 0)
                (*(option->value))--;
              else
                *(option->value) = option->n_options - 1;
            } else if (pressed_pad[PAD_ENTER] || pressed_pad[PAD_RIGHT]) {
              if (*(option->value) < option->n_options - 1)
                (*(option->value))++;
              else
                *(option->value) = 0;
            }
          }

          // Track if language changed
          if (option->name == VITASHELL_SETTINGS_LANGUAGE && old_value != *(option->value)) {
            language_changed = 1;
          }
        }

        break;
      }
    }
  }

  // Navigation based on visible options count
  if (hold_pad[PAD_UP] || hold2_pad[PAD_LEFT_ANALOG_UP]) {
    if (settings_menu.option_sel > 0) {
      settings_menu.option_sel--;
    } else if (settings_menu.entry_sel > 0) {
      settings_menu.entry_sel--;
      // Calculate visible options for previous entry
      int prev_visible_count = 0;
      for (int j = 0; j < settings_menu_entries[settings_menu.entry_sel].n_options; j++) {
        SettingsMenuOption *opt = &settings_menu_entries[settings_menu.entry_sel].options[j];
        if (!(opt->name == VITASHELL_SETTINGS_SELECT_BUTTON && qr_used == 1 &&
              opt->value && *(opt->value) == SELECT_BUTTON_MODE_QR)) {
          prev_visible_count++;
        }
      }
      settings_menu.option_sel = prev_visible_count - 1;
    }
  } else if (hold_pad[PAD_DOWN] || hold2_pad[PAD_LEFT_ANALOG_DOWN]) {
    if (settings_menu.option_sel < visible_option_count - 1) {
      settings_menu.option_sel++;
    } else if (settings_menu.entry_sel < n_settings_entries - 1) {
      settings_menu.entry_sel++;
      settings_menu.option_sel = 0;
    }
  }

  // Close
  if (pressed_pad[PAD_START] || pressed_pad[PAD_CANCEL]) {
    closeSettingsMenu();
  }
}
