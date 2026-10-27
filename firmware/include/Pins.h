#pragma once
namespace Pins {
#ifdef SPOOLANCER_TOUCH
constexpr int hxData = 12, hxClock = 17;
constexpr int nfcRx = 13, nfcTx = 11;
constexpr int sdSelect = 10;
#else
constexpr int hxData = 15, hxClock = 16;
constexpr int nfcRx = 18, nfcTx = 17;
#endif
constexpr int wifiReset = 0;
constexpr int displayClock = 12, displayData = 11;
constexpr int displayCs = 10, displayDc = 9, displayReset = 8;
constexpr int previous = 4, next = 5, select = 6;
}
