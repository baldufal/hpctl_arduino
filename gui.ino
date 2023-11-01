#include "Free_Fonts.h"  // Include the header file attached to this sketch
#include "SPI.h"
#include "TFT_eSPI.h"

#define BYTE_TO_BINARY_PATTERN "%c %c %c %c %c %c %c %c"
#define BYTE_TO_BINARY(byte) \
  ((byte)&0x80 ? '1' : '0'), \
    ((byte)&0x40 ? '1' : '0'), \
    ((byte)&0x20 ? '1' : '0'), \
    ((byte)&0x10 ? '1' : '0'), \
    ((byte)&0x08 ? '1' : '0'), \
    ((byte)&0x04 ? '1' : '0'), \
    ((byte)&0x02 ? '1' : '0'), \
    ((byte)&0x01 ? '1' : '0')

#define COLOR_TEXT TFT_WHITE
#define COLOR_TEXT_CURSOR TFT_BLACK
#define COLOR_TEXT_DIRTY TFT_YELLOW
#define COLOR_BG TFT_BLACK
#define COLOR_BG_CURSOR TFT_CYAN

#define SET_CURSOR_STYLE \
  { \
    tft.setTextColor(COLOR_TEXT_CURSOR, COLOR_BG_CURSOR); \
    tft.setFreeFont(FF22); \
  }
#define SET_NORMAL_STYLE \
  { \
    tft.setTextColor(COLOR_TEXT, COLOR_BG); \
    tft.setFreeFont(FF18); \
  }
#define SET_CURSOR_STYLE_DIRTY \
  { \
    tft.setTextColor(COLOR_TEXT_DIRTY, COLOR_BG_CURSOR); \
    tft.setFreeFont(FF22); \
  }
#define SET_NORMAL_STYLE_DIRTY \
  { \
    tft.setTextColor(COLOR_TEXT_DIRTY, COLOR_BG); \
    tft.setFreeFont(FF18); \
  }

TFT_eSPI tft = TFT_eSPI();
static int lastGuiUpdate = 0;
static int16_t lineHeight = 0;
static char buffer[40];

static uint scrollPos = 0;
// cursorPos defined in button.ino bc of include order
static uint lastScrollPos = 0;  // Used for timely display update
static uint lastCursorPos = 0;  // Used for timely display update

void setupGui() {
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);
  SET_NORMAL_STYLE
  lineHeight = tft.fontHeight();
}

void handleGuiNoWifi() {
  tft.fillScreen(COLOR_BG);
  tft.drawString("Attempting to connect to", 0, 0, GFXFF);
  tft.drawString(ssid, 0, lineHeight, GFXFF);
}

void handleGui() {
  // Only update every second.
  int time = millis();

  // Skip update if nothing changed
  if (time - lastGuiUpdate < 1000 && cursorPos == lastCursorPos && scrollPos == lastScrollPos && guiDirty == false)
    return;
  lastGuiUpdate = time;
  lastCursorPos = cursorPos;
  lastScrollPos = scrollPos;
  guiDirty = false;

  unsigned int sec = millis64() / (uint64_t)1000;
  unsigned int min = sec / 60;
  unsigned int hr = min / 60;
  unsigned int day = hr / 24;

  tft.fillScreen(COLOR_BG);

  scrollPos = (cursorPos < 3) ? 0 : 1;
  int lineStatus = 0 - scrollPos;
  if (0 <= lineStatus && lineStatus <= 5) {
    if (WiFi.status() != WL_CONNECTED) {
      tft.setTextColor(TFT_RED);
    } else {
      tft.setTextColor(TFT_GREEN);
    }
    tft.drawString("WiFi", 0, lineStatus * lineHeight, GFXFF);

    if (twiError) {
      tft.setTextColor(TFT_RED);
    } else {
      if (i2cDirty) {
        tft.setTextColor(TFT_YELLOW);
      } else {
        tft.setTextColor(TFT_GREEN);
      }
    }
    tft.drawString("I2C", 60, lineStatus * lineHeight, GFXFF);
    SET_NORMAL_STYLE

    sprintf(buffer, "%4ud %2uh %2um %2us", day % 9999, hr % 24, min % 60, sec % 60);
    tft.drawRightString(buffer, 320, lineStatus * lineHeight, GFXFF);

    tft.drawFastHLine(0, lineHeight - 5, 320, TFT_LIGHTGREY);
  }

  int lineHeater = 1 - scrollPos;
  if (0 <= lineHeater && lineHeater <= 5) {
    tft.drawString("Heater Level:", 0, lineHeater * lineHeight, GFXFF);
    sprintf(buffer, "%d / %d", currentState.heating, HEATING_LEVELS - 1);
    if (currentState.heating != nextState.heating) {
      if (cursorPos == 0) {
        SET_CURSOR_STYLE_DIRTY
      } else {
        SET_NORMAL_STYLE_DIRTY
      }
    } else {
      if (cursorPos == 0)
        SET_CURSOR_STYLE
    }
    tft.drawRightString(buffer, 320, lineHeater * lineHeight, GFXFF);
    SET_NORMAL_STYLE
  }

  int lineVent = 2 - scrollPos;
  if (0 <= lineVent && lineVent) {
    tft.drawString("Venti IN | OUT:", 0, lineVent * lineHeight, GFXFF);
    sprintf(buffer, "%d / %d", currentState.vent_out, VENT_OUT_LEVELS - 1);
    if (currentState.vent_out != nextState.vent_out) {
      if (cursorPos == 2) {
        SET_CURSOR_STYLE_DIRTY
      } else {
        SET_NORMAL_STYLE_DIRTY
      }
    } else {
      if (cursorPos == 2)
        SET_CURSOR_STYLE
    }
    tft.drawRightString(buffer, 320, lineVent * lineHeight, GFXFF);

    SET_NORMAL_STYLE
    // Idk why +3 is necessary to stop the next text from being cropped
    int16_t posOffset = tft.textWidth(buffer) + 3;
    tft.drawRightString(" |  ", 320 - posOffset, lineVent * lineHeight, GFXFF);

    sprintf(buffer, "%d / %d", currentState.vent_in, VENT_IN_LEVELS - 1);
    posOffset += tft.textWidth(" |  ");
    if (currentState.vent_in != nextState.vent_in) {
      if (cursorPos == 1) {
        SET_CURSOR_STYLE_DIRTY
      } else {
        SET_NORMAL_STYLE_DIRTY
      }
    } else {
      if (cursorPos == 1)
        SET_CURSOR_STYLE
    }
    tft.drawRightString(buffer, 320 - posOffset, lineVent * lineHeight, GFXFF);
    SET_NORMAL_STYLE
  }

  int lineSSRs = 3 - scrollPos;
  if (0 <= lineSSRs && lineSSRs <= 5) {
    tft.drawString("SSRs:", 0, lineSSRs * lineHeight, GFXFF);
    for (uint8_t i = 0; i < 8; i++) {
      sprintf(buffer, "%c", (currentState.ssrs & (0x01 << i)) ? '1' : '0');
      if ((currentState.ssrs & (0x01 << i)) != (nextState.ssrs & (0x01 << i))) {
        if (cursorPos - 3 == i) {
          SET_CURSOR_STYLE_DIRTY
        } else {
          SET_NORMAL_STYLE_DIRTY
        }
      } else {
        if (cursorPos - 3 == i)
          SET_CURSOR_STYLE
      }
      tft.drawRightString(buffer, 320 - 20 * i, lineSSRs * lineHeight, GFXFF);
      SET_NORMAL_STYLE
    }
  }

  int lineInput = 4 - scrollPos;
  if (0 <= lineInput && lineInput <= 5) {
    tft.drawString("Inputs:", 0, lineInput * lineHeight, GFXFF);
    for (uint8_t i = 0; i < 4; i++) {
      sprintf(buffer, "%c", (currentState.inputs & (0x01 << i)) ? '1' : '0');
      tft.drawRightString(buffer, 320 - 20 * i, lineInput * lineHeight, GFXFF);
    }

    tft.drawFastHLine(0, (lineInput + 1) * lineHeight - 5, 320, TFT_LIGHTGREY);
  }

  int lineIp = 5 - scrollPos;
  if (0 <= lineInput && lineInput <= 5) {
    tft.drawString("IP:", 0, lineIp * lineHeight, GFXFF);
    tft.drawRightString(WiFi.localIP().toString().c_str(), 320, lineIp * lineHeight, GFXFF);
  }

  int lineRssi = 6 - scrollPos;
  if (0 <= lineInput && lineInput <= 5) {
    tft.drawString("WiFi RSSI:", 0, lineRssi * lineHeight, GFXFF);
    sprintf(buffer, "%d dBm", WiFi.RSSI());
    tft.drawRightString(buffer, 320, lineRssi * lineHeight, GFXFF);
  }
}