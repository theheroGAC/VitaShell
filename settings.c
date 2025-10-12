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
#include "network_update.h"

static void restartShell();
static void rebootDevice();
static void shutdownDevice();
static void suspendDevice();
static void showBatteryInfo();

static int changed = 0;
static int theme = 0;
int qr_used = 0; // QR functionality always available - no usage restrictions

static volatile int auto_close_timer = 0;

static void checkUpdates() {
  // Close menu immediately
  closeSettingsMenu();

  // Start network update thread (it handles all UI internally)
  // This reuses the existing auto-update logic which works reliably
  SceUID thid = sceKernelCreateThread("network_update", (SceKernelThreadEntry)network_update_thread, 0x10000100, 0x10000, 0, 0, NULL);
  if (thid >= 0)
    sceKernelStartThread(thid, 0, NULL);

  // Let the thread handle everything - no custom dialogs to avoid conflicts
}

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
static char *repeat_options[3]; // audio repeat options

static ConfigEntry settings_entries[] = {
  { "LANGUAGE",            CONFIG_TYPE_DECIMAL, (int *)&language },
  { "USBDEVICE",          CONFIG_TYPE_DECIMAL, (int *)&vitashell_config.usbdevice },
  { "SELECT_BUTTON",      CONFIG_TYPE_DECIMAL, (int *)&vitashell_config.select_button },
  { "DISABLE_AUTOUPDATE", CONFIG_TYPE_BOOLEAN, (int *)&vitashell_config.disable_autoupdate },
  { "DISABLE_WARNING",    CONFIG_TYPE_BOOLEAN, (int *)&vitashell_config.disable_warning },
  { "DISABLE_LOW_BATTERY_WARNING", CONFIG_TYPE_BOOLEAN, (int *)&vitashell_config.disable_low_battery_warning },
  { "AUDIO_REPEAT",       CONFIG_TYPE_DECIMAL, (int *)&vitashell_config.audio_repeat },
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
  { VITASHELL_SETTINGS_LOW_BATTERY_WARNING, SETTINGS_OPTION_TYPE_BOOLEAN, NULL, NULL, 0, NULL, 0, &vitashell_config.disable_low_battery_warning },
  { VITASHELL_SETTINGS_AUDIO_REPEAT,    SETTINGS_OPTION_TYPE_OPTIONS, NULL, NULL, 0,
    repeat_options, 3, &vitashell_config.audio_repeat },
  { VITASHELL_SETTINGS_BATTERY_INFO, SETTINGS_OPTION_TYPE_CALLBACK, (void *)showBatteryInfo, NULL, 0, NULL, 0, NULL },

  { VITASHELL_SETTINGS_CHECK_UPDATES,  SETTINGS_OPTION_TYPE_CALLBACK, (void *)checkUpdates, NULL, 0, NULL, 0, NULL },

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

// Scrolling support for settings menu
#define MAX_VISIBLE_OPTIONS 14  // Maximum options visible on screen
static int settings_scroll_pos = 0;  // Current scroll position

void loadSettingsConfig() {
  // Load settings config file
  memset(&vitashell_config, 0, sizeof(VitaShellConfig));

  // Disable automatic updates by default (disable_autoupdate = 1)
  vitashell_config.disable_autoupdate = 1;

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

static void showBatteryInfo() {
  int percent = -1;
  int exists = 0;
  int charging = 0;
  percent = scePowerGetBatteryLifePercent();
  exists = 1; // Assume battery exists
  charging = scePowerIsBatteryCharging();

  char message[256];
  snprintf(message, sizeof(message), "Battery Information:\nExists: %s\nLevel: %d%%\nCharging: %s",
           exists ? "Yes" : "No", percent, charging ? "Yes" : "No");

  initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_OK, message);
  setDialogStep(DIALOG_STEP_INFO);  // Set step to handle dialog properly
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

static uint64_t last_battery_check = 0;
static int battery_warning_shown = 0;

void checkLowBatteryWarning() {
  // Check every 30 seconds
  uint64_t now = sceKernelGetSystemTimeWide();
  if (now - last_battery_check < 30LL * 1000 * 1000) {
    return;
  }
  last_battery_check = now;

  // Skip if warning disabled
  if (vitashell_config.disable_low_battery_warning) {
    return;
  }

  int percent = scePowerGetBatteryLifePercent();
  int charging = scePowerIsBatteryCharging();

  if (!charging && percent >= 0 && percent <= 20 && !battery_warning_shown) {
    battery_warning_shown = 1;
    initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_OK, "Low Battery Warning:\nBattery level is below 20%.\nPlease connect charger.");
  } else if (charging || percent > 20) {
    battery_warning_shown = 0;
  }
}

void initSettingsMenu() {
  int i;

  memset(&settings_menu, 0, sizeof(SettingsMenu));
  settings_menu.status = SETTINGS_MENU_CLOSED;

  n_settings_entries = sizeof(vitashell_settings_menu_entries) / sizeof(SettingsMenuEntry);
  settings_menu_entries = vitashell_settings_menu_entries;

  for (i = 0; i < n_settings_entries; i++)
    settings_menu.n_options += settings_menu_entries[i].n_options;

  // Additional safety: verify language container integrity for critical strings
  if (!language_container[VITASHELL_SETTINGS_MAIN] ||
      !language_container[VITASHELL_SETTINGS_USBDEVICE] ||
      !language_container[VITASHELL_SETTINGS_SELECT_BUTTON] ||
      !language_container[VITASHELL_SETTINGS_FOCUS_COLOR] ||
      !language_container[ON] || !language_container[OFF]) {
    // If critical language strings are missing, force reload of English language
    loadLanguage(1); // Force load English language (index 1)
  }

  // Verify essential language strings for settings menu are available
  int essential_strings_missing = 0;
  if (!language_container[VITASHELL_SETTINGS_USB_MEMORY_CARD] ||
      !language_container[VITASHELL_SETTINGS_USB_GAME_CARD] ||
      !language_container[VITASHELL_SETTINGS_USB_SD2VITA] ||
      !language_container[VITASHELL_SETTINGS_USB_PSVSD] ||
      !language_container[VITASHELL_SETTINGS_SELECT_BUTTON_USB] ||
      !language_container[VITASHELL_SETTINGS_SELECT_BUTTON_FTP] ||
      !language_container[VITASHELL_SETTINGS_SELECT_BUTTON_QR] ||
      !language_container[VITASHELL_SETTINGS_FOCUS_COLOR_DEFAULT] ||
      !language_container[VITASHELL_SETTINGS_FOCUS_COLOR_LIGHT_GREY] ||
      !language_container[VITASHELL_SETTINGS_FOCUS_COLOR_BLUE] ||
      !language_container[VITASHELL_SETTINGS_FOCUS_COLOR_RED] ||
      !language_container[VITASHELL_SETTINGS_FOCUS_COLOR_PINK] ||
      !language_container[VITASHELL_SETTINGS_FOCUS_COLOR_YELLOW]) {
    essential_strings_missing = 1;
  }

  if (essential_strings_missing) {
    // Force reload English language again to ensure all strings are available
    loadLanguage(1);
  }

  // Initialize options with safety checks for language container
  usbdevice_options[0] = language_container[VITASHELL_SETTINGS_USB_MEMORY_CARD] ?
    language_container[VITASHELL_SETTINGS_USB_MEMORY_CARD] : "Memory Card";
  usbdevice_options[1] = language_container[VITASHELL_SETTINGS_USB_GAME_CARD] ?
    language_container[VITASHELL_SETTINGS_USB_GAME_CARD] : "Game Card";
  usbdevice_options[2] = language_container[VITASHELL_SETTINGS_USB_SD2VITA] ?
    language_container[VITASHELL_SETTINGS_USB_SD2VITA] : "SD2Vita";
  usbdevice_options[3] = language_container[VITASHELL_SETTINGS_USB_PSVSD] ?
    language_container[VITASHELL_SETTINGS_USB_PSVSD] : "PSVSD";

  select_button_options[0] = language_container[VITASHELL_SETTINGS_SELECT_BUTTON_USB] ?
    language_container[VITASHELL_SETTINGS_SELECT_BUTTON_USB] : "USB";
  select_button_options[1] = language_container[VITASHELL_SETTINGS_SELECT_BUTTON_FTP] ?
    language_container[VITASHELL_SETTINGS_SELECT_BUTTON_FTP] : "FTP";
  select_button_options[2] = language_container[VITASHELL_SETTINGS_SELECT_BUTTON_QR] ?
    language_container[VITASHELL_SETTINGS_SELECT_BUTTON_QR] : "QR";

  focus_color_options_texts[0] = language_container[VITASHELL_SETTINGS_FOCUS_COLOR_DEFAULT] ?
    language_container[VITASHELL_SETTINGS_FOCUS_COLOR_DEFAULT] : "Default";
  focus_color_options_texts[1] = language_container[VITASHELL_SETTINGS_FOCUS_COLOR_LIGHT_GREY] ?
    language_container[VITASHELL_SETTINGS_FOCUS_COLOR_LIGHT_GREY] : "Light Grey";
  focus_color_options_texts[2] = language_container[VITASHELL_SETTINGS_FOCUS_COLOR_BLUE] ?
    language_container[VITASHELL_SETTINGS_FOCUS_COLOR_BLUE] : "Blue";
  focus_color_options_texts[3] = language_container[VITASHELL_SETTINGS_FOCUS_COLOR_RED] ?
    language_container[VITASHELL_SETTINGS_FOCUS_COLOR_RED] : "Red";
  focus_color_options_texts[4] = language_container[VITASHELL_SETTINGS_FOCUS_COLOR_PINK] ?
    language_container[VITASHELL_SETTINGS_FOCUS_COLOR_PINK] : "Pink";
  focus_color_options_texts[5] = language_container[VITASHELL_SETTINGS_FOCUS_COLOR_YELLOW] ?
    language_container[VITASHELL_SETTINGS_FOCUS_COLOR_YELLOW] : "Yellow";

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

  // Audio repeat options
  repeat_options[0] = "None";
  repeat_options[1] = "Repeat One";
  repeat_options[2] = "Repeat All";

  theme_options = malloc(MAX_THEMES * sizeof(char *));

  for (i = 0; i < MAX_THEMES; i++)
    theme_options[i] = malloc(MAX_THEME_LENGTH);
}

void openSettingsMenu() {
  settings_menu.status = SETTINGS_MENU_OPENING;
  settings_menu.entry_sel = 0;
  settings_menu.option_sel = 0;
  settings_scroll_pos = 0; // Reset scroll position

  // Get current theme - safer memory management
  if (theme_name) {
    free(theme_name);
    theme_name = NULL;
  }

  // Safely read theme config with error handling
  if (readConfig("ux0:VitaShell/theme/theme.txt", theme_entries, sizeof(theme_entries) / sizeof(ConfigEntry)) < 0 ||
      !theme_name) {
    // If theme config fails to load or theme_name is NULL, initialize with safe defaults
    if (theme_name) {
      free(theme_name);
    }
    theme_name = strdup("default");
  }

  // Get theme index in main tab
  int theme_index = -1;

  int i;
  for (i = 0; i < (sizeof(main_settings) / sizeof(SettingsMenuOption)); i++) {
    if (main_settings[i].name == VITASHELL_SETTINGS_THEME) {
      theme_index = i;
      break;
    }
  }

  // Find all themes - with better error handling
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
          if (SCE_S_ISDIR(dir.d_stat.st_mode) && dir.d_name[0] != '.') {
            // Safety check for theme_name and array bounds
            if (theme_name && strcasecmp(dir.d_name, theme_name) == 0 && theme_count < MAX_THEMES)
              theme = theme_count;

            // Ensure we don't exceed array bounds
            if (theme_count < MAX_THEMES && theme_options[theme_count]) {
              snprintf(theme_options[theme_count], MAX_THEME_LENGTH, "%s", dir.d_name);
              theme_count++;
            }
          }
        }
      } while (res > 0 && theme_count < MAX_THEMES);

      sceIoDclose(dfd);

      // Ensure we have at least one theme option
      if (theme_count == 0) {
        if (theme_options[0]) {
          snprintf(theme_options[0], MAX_THEME_LENGTH, "default");
          theme_count = 1;
          theme = 0;
        }
      }

      main_settings[theme_index].options = theme_options;
      main_settings[theme_index].n_options = theme_count;
      main_settings[theme_index].value = &theme;
    } else {
      // If theme directory doesn't exist or can't be opened, use safe defaults
      if (theme_options[0]) {
        snprintf(theme_options[0], MAX_THEME_LENGTH, "default");
        theme_count = 1;
        theme = 0;

        main_settings[theme_index].options = theme_options;
        main_settings[theme_index].n_options = theme_count;
        main_settings[theme_index].value = &theme;
      }
    }
  }

  changed = 0;
}

void closeSettingsMenu() {
  settings_menu.status = SETTINGS_MENU_CLOSING;

  // Save settings
  if (changed) {
    saveSettingsConfig();

    // Save theme config file - with safety checks
    if (theme < theme_count && theme >= 0 && theme_options[theme]) {
      theme_entries[0].value = &theme_options[theme];
      writeConfig("ux0:VitaShell/theme/theme.txt", theme_entries, sizeof(theme_entries) / sizeof(ConfigEntry));
    }
    theme_entries[0].value = (void *)&theme_name;
  }

  // If language changed, ask for restart
  if (language_changed) {
    // Reset flag
    language_changed = 0;

    // Show language restart message (translated) with fallback
    char *restart_msg = language_container[LANGUAGE_CHANGED_RESTART_MSG] ?
      language_container[LANGUAGE_CHANGED_RESTART_MSG] : "Language changed. Restart VitaShell to apply?";
    initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_YESNO, restart_msg);
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

  // Safety check for texture
  if (!settings_image)
    return;

  // Draw settings menu
  vita2d_draw_texture(settings_image, 0.0f, SCREEN_HEIGHT - settings_menu.cur_pos);

  float y = SCREEN_HEIGHT - settings_menu.cur_pos + START_Y;

  // Calculate total menu height for scrolling
  float total_menu_height = 0;
  for (int i = 0; i < n_settings_entries; i++) {
    total_menu_height += FONT_Y_SPACE * 1.2f; // Title + bar space

    SettingsMenuOption *options = settings_menu_entries[i].options;
    int j;
    for (j = 0; j < settings_menu_entries[i].n_options; j++) {
      total_menu_height += FONT_Y_SPACE; // Each option
    }
    total_menu_height += FONT_Y_SPACE; // Space after section
  }

  // Enable scrolling if menu is too tall
  int needs_scrolling = (total_menu_height > (SCREEN_HEIGHT - START_Y - 50.0f));

  if (needs_scrolling) {
    // Calculate scrollable area
    float scrollable_height = total_menu_height - (SCREEN_HEIGHT - START_Y - 50.0f);
    int max_scroll = (int)ceil(scrollable_height / FONT_Y_SPACE);

    // Clamp scroll position
    if (settings_scroll_pos > max_scroll) {
      settings_scroll_pos = max_scroll;
    }
    if (settings_scroll_pos < 0) {
      settings_scroll_pos = 0;
    }

    // Adjust Y position based on scroll
    y -= settings_scroll_pos * FONT_Y_SPACE;
  }

  int i;
  for (i = 0; i < n_settings_entries; i++) {
    // Title - safety check for language container
    char *title_text = language_container[settings_menu_entries[i].name] ?
      language_container[settings_menu_entries[i].name] : "Settings";
    float x = pgf_text_width(title_text);
    pgf_draw_text(ALIGN_CENTER(SCREEN_WIDTH, x), y, SETTINGS_MENU_TITLE_COLOR, title_text);

    // Add colored decorative bar under each section title (positioned lower)
    float bar_width = pgf_text_width(title_text) + 20.0f; // Slightly wider than text
    float bar_x = ALIGN_CENTER(SCREEN_WIDTH, bar_width);

    // Different colors for each section
    uint32_t bar_color;
    if (settings_menu_entries[i].name == VITASHELL_SETTINGS_MAIN) {
      bar_color = RGBA8(0xFF, 0xA5, 0x00, 0xFF); // Orange for Main Settings
    } else if (settings_menu_entries[i].name == VITASHELL_SETTINGS_LANGUAGE) {
      bar_color = RGBA8(0x4E, 0xC4, 0xE8, 0xFF); // Light blue for Language Settings
    } else if (settings_menu_entries[i].name == VITASHELL_SETTINGS_POWER) {
      bar_color = RGBA8(0xFF, 0x6B, 0x6B, 0xFF); // Light red for Power Settings
    } else {
      bar_color = SETTINGS_MENU_TITLE_COLOR; // Default color
    }

    // Draw decorative bar positioned lower under the title
    vita2d_draw_rectangle(bar_x, y + FONT_Y_SPACE * 1.1f, bar_width, 2.0f, bar_color);

    y += FONT_Y_SPACE * 1.2f; // Optimized space for the bar

    SettingsMenuOption *options = settings_menu_entries[i].options;

    int j;
    int visible_count = 0;
    for (j = 0; j < settings_menu_entries[i].n_options; j++) {
      // QR option always available - no longer skip after first use

      // Count visible options for proper selection index
      visible_count++;
      int current_visible_index = visible_count - 1;

      int is_selected = (settings_menu.entry_sel == i && settings_menu.option_sel == current_visible_index);

      // Focus for selected visible option
      if (is_selected)
        vita2d_draw_rectangle(SHELL_MARGIN_X, y + 3.0f, MARK_WIDTH, FONT_Y_SPACE, SETTINGS_MENU_FOCUS_COLOR);

      if (options[j].type == SETTINGS_OPTION_TYPE_CALLBACK) {
        char *option_name = language_container[options[j].name] ?
          language_container[options[j].name] : "Option";
        float x = pgf_text_width(option_name);
        pgf_draw_text(ALIGN_CENTER(SCREEN_WIDTH, x), y, SETTINGS_MENU_ITEM_COLOR, option_name);
      } else {
        char *option_name = language_container[options[j].name] ?
          language_container[options[j].name] : "Option";
        float x = pgf_text_width(option_name);
        pgf_draw_text(ALIGN_RIGHT(SCREEN_HALF_WIDTH - 10.0f, x), y, SETTINGS_MENU_ITEM_COLOR, option_name);

        // Option
        switch (options[j].type) {
          case SETTINGS_OPTION_TYPE_BOOLEAN:
          {
            char *bool_text = (options[j].value && *(options[j].value)) ?
              (language_container[ON] ? language_container[ON] : "On") :
              (language_container[OFF] ? language_container[OFF] : "Off");
            pgf_draw_text(SCREEN_HALF_WIDTH + 10.0f, y, SETTINGS_MENU_OPTION_COLOR, bool_text);
            break;
          }

          case SETTINGS_OPTION_TYPE_STRING:
            pgf_draw_text(SCREEN_HALF_WIDTH + 10.0f, y, SETTINGS_MENU_OPTION_COLOR, options[j].string);
            break;

          case SETTINGS_OPTION_TYPE_OPTIONS:
          {
            int value = 0;
            if (options[j].value)
              value = *(options[j].value);
            char *option_text = (options[j].options && options[j].options[value]) ?
              options[j].options[value] : "";
            pgf_draw_text(SCREEN_HALF_WIDTH + 10.0f, y, SETTINGS_MENU_OPTION_COLOR, option_text);
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
  // Actually restart VitaShell when user agrees to language change
  sceAppMgrLoadExec("app0:eboot.bin", NULL, NULL);
}

void settingsDisagree() {
  agreement = SETTINGS_AGREEMENT_DISAGREE;
}

void settingsMenuCtrl() {
  // Safety checks for menu bounds
  if (settings_menu.entry_sel < 0 || settings_menu.entry_sel >= n_settings_entries) {
    settings_menu.entry_sel = 0;
  }

  if (settings_menu.option_sel < 0) {
    settings_menu.option_sel = 0;
  }

  // Calculate visible options count for current entry (QR always available)
  int visible_option_count = settings_menu_entries[settings_menu.entry_sel].n_options;

  // Find the actual option index from visible selection
  int actual_option_index = settings_menu.option_sel;

  if (actual_option_index == -1) {
    // Safety: reset selection if something went wrong
    settings_menu.option_sel = 0;
    actual_option_index = 0;
  }

  // Additional safety check for option bounds
  if (actual_option_index >= visible_option_count) {
    settings_menu.option_sel = visible_option_count - 1;
    actual_option_index = settings_menu.option_sel;
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
            // Normal cycling for SELECT_BUTTON: QR always available
            if (pressed_pad[PAD_LEFT]) {
              if (*(option->value) > 0)
                (*(option->value))--;
              else
                *(option->value) = 2; // 3 options: 0=USB, 1=FTP, 2=QR
            } else if (pressed_pad[PAD_ENTER] || pressed_pad[PAD_RIGHT]) {
              if (*(option->value) < 2)
                (*(option->value))++;
              else
                *(option->value) = 0;
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

  // Enhanced navigation with automatic scrolling
  if (hold_pad[PAD_UP] || hold2_pad[PAD_LEFT_ANALOG_UP]) {
    if (settings_menu.option_sel > 0) {
      settings_menu.option_sel--;
      // Auto-scroll up if selection goes above visible area
      if (settings_menu.option_sel < 2) {
        settings_scroll_pos = MAX(0, settings_scroll_pos - 1);
      }
    } else if (settings_menu.entry_sel > 0) {
      settings_menu.entry_sel--;
      // Set to last option of previous entry - with bounds check
      int prev_entry_options = settings_menu_entries[settings_menu.entry_sel].n_options;
      settings_menu.option_sel = (prev_entry_options > 0) ? (prev_entry_options - 1) : 0;
      // Auto-scroll to show the new section
      settings_scroll_pos = MAX(0, settings_scroll_pos - 2);
    }
  } else if (hold_pad[PAD_DOWN] || hold2_pad[PAD_LEFT_ANALOG_DOWN]) {
    if (settings_menu.option_sel < visible_option_count - 1) {
      settings_menu.option_sel++;
      // Auto-scroll down if selection goes below visible area
      if (settings_menu.option_sel > MAX_VISIBLE_OPTIONS - 3) {
        settings_scroll_pos++;
      }
    } else if (settings_menu.entry_sel < n_settings_entries - 1) {
      settings_menu.entry_sel++;
      settings_menu.option_sel = 0;
      // Auto-scroll to show the new section
      settings_scroll_pos++;
    }
  }

  // Optional manual scrolling with L1/R1 buttons (still available)
  if (pressed_pad[PAD_LTRIGGER]) {
    // Manual scroll up
    settings_scroll_pos = MAX(0, settings_scroll_pos - 3);
  } else if (pressed_pad[PAD_RTRIGGER]) {
    // Manual scroll down
    settings_scroll_pos += 3;
  }

  // Close
  if (pressed_pad[PAD_START] || pressed_pad[PAD_CANCEL]) {
    closeSettingsMenu();
  }
}
