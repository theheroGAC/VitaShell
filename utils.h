/*
  VitaShell - utils.h
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

#pragma once

#include <stdint.h>
#include <psp2/ctrl.h>       // SceCtrlData
#include <psp2/rtc.h>        // SceDateTime
#include <psp2/appmgr.h>     // launchAppByUriExit
#include <psp2/vshbridge.h>  // provides vshIoMount, vshIoUmount, _vshIoMount
#include <vita2d.h>          // vita2d_texture

#include "main.h"

// Alignment helpers for UI rendering
#define ALIGN_CENTER(a, b)  (((a) - (b)) / 2)
#define ALIGN_RIGHT(x, w)   ((x) - (w))

// Analog stick constants
#define ANALOG_CENTER      128
#define ANALOG_THRESHOLD    64
#define ANALOG_SENSITIVITY  16

// Enumeration for mapped pad buttons
enum PadButtons {
  PAD_UP,
  PAD_DOWN,
  PAD_LEFT,
  PAD_RIGHT,
  PAD_LTRIGGER,
  PAD_RTRIGGER,
  PAD_TRIANGLE,
  PAD_CIRCLE,
  PAD_CROSS,
  PAD_SQUARE,
  PAD_START,
  PAD_SELECT,
  PAD_ENTER,
  PAD_CANCEL,
  PAD_LEFT_ANALOG_UP,
  PAD_LEFT_ANALOG_DOWN,
  PAD_LEFT_ANALOG_LEFT,
  PAD_LEFT_ANALOG_RIGHT,
  PAD_RIGHT_ANALOG_UP,
  PAD_RIGHT_ANALOG_DOWN,
  PAD_RIGHT_ANALOG_LEFT,
  PAD_RIGHT_ANALOG_RIGHT,
  PAD_N_BUTTONS
};

// Pad state type
typedef uint8_t Pad[PAD_N_BUTTONS];

// Global pad states
extern SceCtrlData pad;
extern Pad old_pad, current_pad, pressed_pad, released_pad, hold_pad, hold2_pad;
extern Pad hold_count, hold2_count;

// Math helpers
float easeOut(float x0, float x1, float a, float b);

// Drawing functions
void startDrawing(vita2d_texture *bg);
void endDrawing(void);

// Dialog helpers
void closeWaitDialog(void);
void errorDialog(int error);
void infoDialog(const char *msg, ...);

// Storage helpers
int checkMemoryCardFreeSpace(const char *path, uint64_t size);
int getPartitionFreeSpace(const char *device, uint64_t *free_size, uint64_t *max_size);
uint32_t getFreeSpaceColor(uint64_t free_size, uint64_t max_size);

// Power management helpers
void initPowerTickThread(void);
void powerLock(void);
void powerUnlock(void);

// Pad handling
void setEnterButton(int circle);
void readPad(void);
int holdButtons(SceCtrlData *data, uint32_t buttons, uint64_t time);

// Path helpers
int hasEndSlash(const char *path);
int removeEndSlash(char *path);
int addEndSlash(char *path);

// File size formatting
void getSizeString(char string[20], uint64_t size);

// Time conversion and formatting
void convertUtcToLocalTime(SceDateTime *time_local, SceDateTime *time_utc);
void convertLocalTimeToUtc(SceDateTime *time_utc, SceDateTime *time_local);
void getDateString(char string[24], int date_format, SceDateTime *time);
void getTimeString(char string[16], int time_format, SceDateTime *time);

// Debugging
int debugPrintf(const char *text, ...);

// App launching
int launchAppByUriExit(const char *titleid);

// String helpers (non-standard)
char *strcasestr(const char *haystack, const char *needle);

// Remount storage device
void remount(int id);
int mount(int id, const char *path, int permission);