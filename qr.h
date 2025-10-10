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

#ifndef __QR_H__
#define __QR_H__

#include <psp2/kernel/sysmem.h>

int qr_scan_thread(SceSize args, void *argp);

int initQR();
int finishQR();
int startQR();
int stopQR();
int enabledQR();
int scannedQR();
int renderCameraQR(int x, int y);
char *getLastQR();
char *getLastDownloadQR();
void setScannedQR(int scanned);

// Enhanced QR scanning functions
int getQRScanStats(int *attempts, int *success);
int setQRScanDelay(int delay_ms);
int resetQRScanStats();
int validateQRData(const char *data, int length);
void resetQRScanner();

// QR quality and UI enhancement functions
typedef enum {
  QR_QUALITY_POOR,
  QR_QUALITY_FAIR,
  QR_QUALITY_GOOD,
  QR_QUALITY_EXCELLENT
} QRQuality;

QRQuality getQRQuality(const char *data, int length);
void drawQRTargetingFrame(int x, int y, int size);
void drawQRQualityIndicator(int x, int y, QRQuality quality);
void playQRBeep(QRQuality quality);
void updateQRAnimation(float *animation_progress);

#endif
