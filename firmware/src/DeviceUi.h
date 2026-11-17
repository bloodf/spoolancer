#pragma once
// Local user interfaces: status LEDs/OLED, the ST7789 display with buttons, and the touchscreen.
#include "AppState.h"
#include "StatusUi.h"

namespace spoolancer {
void renderStatusUi(bool force = false);
void nfcIdle();
void startStatusOperation(StatusModel::Operation operation);
void finishStatusOperation(bool success);
#ifdef SPOOLANCER_TOUCH
void renderTouch();
void pollTouch();
#endif
#ifndef SPOOLANCER_HEADLESS
void displayStatus();
#endif
void buttons();
}  // namespace spoolancer
