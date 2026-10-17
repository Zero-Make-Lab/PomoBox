#pragma once
#include "common.h"
#include "qt_time.h"

// =============================================================================
// Quick Timer App - Countdown Timer, set as MM:SS (up to 60 min) or HH:MM (up to 12 h)
// The mode is Settings > Timer Mode; the screen always sets the left field, then the right.
// =============================================================================

enum class QTState { SETTING_LEFT, SETTING_RIGHT, RUNNING, PAUSED, DONE, MENU };

// --- State ---
static QTState  qtState     = QTState::SETTING_LEFT;
static int32_t  qtSetLeft   = 0;      // minutes (MM:SS) or hours (HH:MM)
static int32_t  qtSetRight  = 0;      // seconds (MM:SS) or minutes (HH:MM)
static bool     qtFieldsSet = false;  // fields filled for the saved mode
static int32_t  qtRemaining = 0;
static int32_t  qtTotalSec  = 0;
static uint32_t qtLastTick  = 0;

// --- Display cache ---
static QTState lastQTState = QTState::SETTING_LEFT;
static char    qtShown[12] = "";  // the digits on screen; "" redraws them all
static uint8_t qtShownSize = 0;

// The saved mode is only known after loadSettings(), so the fields start at 5 minutes on first use.
static void qtEnsureFields() {
  if (qtFieldsSet) return;
  qtSplitTotal(qtUnits, 5 * 60, qtSetLeft, qtSetRight);
  qtFieldsSet = true;
}

// --- Timer Tick ---

static void qtTick() {
  if (qtState != QTState::RUNNING) return;
  uint32_t now = millis();
  if (now - qtLastTick >= 1000) {
    qtLastTick += 1000;
    if (qtRemaining > 0) qtRemaining--;
    if (qtRemaining == 0) {
      qtState = QTState::DONE;
      needsFullRedraw = true;
      playAlarmMelody();
    }
  }
}

// --- Draw Timer Screen ---

// Draws only the characters that changed. "HH:MM" and "MM:SS" use the big size; "H:MM:SS" needs the next one down.
static void qtDrawDigits(const char *text, uint16_t accent) {
  const uint8_t size = strlen(text) > 5 ? 5 : 7;
  const int16_t cw = 6 * size;
  const int16_t bx = (320 - cw * (int16_t)strlen(text)) / 2;
  const int16_t by = size == 7 ? 115 : 123;

  bool relayout = size != qtShownSize || strlen(text) != strlen(qtShown);
  if (relayout) {
    tft.fillRect(40, 112, 240, 62, COLOR_BG);
    qtShown[0] = '\0';
  }

  tft.setTextSize(size);
  for (size_t i = 0; text[i]; i++) {
    if (!relayout && qtShown[0] && text[i] == qtShown[i]) continue;

    // While setting, the field being set is bright and the rest dim: left (0-1) : right (3-4)
    uint16_t color = COLOR_TEXT;
    if (qtState == QTState::SETTING_LEFT) color = i < 2 ? COLOR_TEXT : COLOR_TEXT_DIM;
    else if (qtState == QTState::SETTING_RIGHT) color = i > 2 ? COLOR_TEXT : COLOR_TEXT_DIM;
    else if (qtState == QTState::DONE) color = accent;

    tft.setTextColor(color, COLOR_BG);
    tft.setCursor(bx + cw * (int16_t)i, by);
    tft.print(text[i]);
  }

  strncpy(qtShown, text, sizeof qtShown - 1);
  qtShown[sizeof qtShown - 1] = '\0';
  qtShownSize = size;
}

static void drawQTTimerScreen() {
  uint16_t accent = COLOR_ACCENT_QTIMER;
  if (qtState == QTState::PAUSED) accent = COLOR_ACCENT_PAUSE;
  if (qtState == QTState::DONE) accent = COLOR_ACCENT_BREAK;

  bool isSetting = (qtState == QTState::SETTING_LEFT || qtState == QTState::SETTING_RIGHT);

  if (needsFullRedraw || lastQTState != qtState || dispApp != ActiveApp::QUICK_TIMER) {
    tft.fillScreen(COLOR_BG);

    // Title
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(12, 12);
    tft.print("QUICK TIMER");

    // Mode card
    tft.fillRoundRect(10, 35, 300, 50, 8, COLOR_SURFACE);

    if (qtState == QTState::DONE) {
      drawCheckIcon(24, 45, accent);
    } else {
      drawHourglassIcon(24, 45, accent);
    }

    tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
    tft.setTextSize(2);
    tft.setCursor(70, 43);
    const bool hm = qtUnits == QTUnits::HOUR_MIN;
    switch (qtState) {
      case QTState::SETTING_LEFT:  tft.print(hm ? "Set Hours" : "Set Minutes");   break;
      case QTState::SETTING_RIGHT: tft.print(hm ? "Set Minutes" : "Set Seconds"); break;
      case QTState::RUNNING:       tft.print("Counting Down"); break;
      case QTState::PAUSED:        tft.print("Paused");        break;
      case QTState::DONE:          tft.print("Time's Up!");    break;
      default: break;
    }

    tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
    tft.setTextSize(1);
    tft.setCursor(70, 65);
    if (qtState == QTState::SETTING_LEFT) {
      tft.print(hm ? "Rotate to set hours (up to 12)" : "Rotate to set minutes (up to 60)");
    } else if (qtState == QTState::SETTING_RIGHT) {
      tft.print(hm ? "Rotate to set minutes" : "Rotate to set seconds");
    } else {
      char total[16];
      qtFormatTotal(total, sizeof total, qtTotalSec);
      tft.print(total);
      tft.print(" timer");
    }

    // Status icon (right side)
    if (qtState == QTState::RUNNING)    drawPauseIcon(270, 50, accent);
    else if (qtState == QTState::PAUSED) drawPlayIcon(270, 50, accent);
    else if (qtState == QTState::DONE)   drawCheckIcon(270, 45, accent);

    // Encoder arrows and field names in setting modes
    if (isSetting) {
      tft.fillTriangle(30, 140, 38, 130, 38, 150, COLOR_TEXT_DIM);
      tft.fillTriangle(290, 140, 282, 130, 282, 150, COLOR_TEXT_DIM);
      tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
      tft.setTextSize(1);
      tft.setCursor(hm ? 82 : 88, 177);
      tft.print(hm ? "hours" : "min");
      tft.setCursor(214, 177);
      tft.print(hm ? "min" : "sec");
    }

    // Hints
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(65, 215);
    switch (qtState) {
      case QTState::SETTING_LEFT:  tft.print(hm ? "rotate: set hr  click: set min" : "rotate: set min  click: set sec"); break;
      case QTState::SETTING_RIGHT: tft.print(hm ? "rotate: set min  click: start" : "rotate: set sec  click: start");  break;
      case QTState::RUNNING:       tft.print("click: pause");                    break;
      case QTState::PAUSED:        tft.print("click: resume  rotate: +/- 1m");  break;
      case QTState::DONE:          tft.print("click: reset");                    break;
      default: break;
    }
    tft.setCursor(65, 227);
    tft.print("hold: settings");

    lastQTState = qtState;
    dispApp = ActiveApp::QUICK_TIMER;
    needsFullRedraw = false;
    resetDispCache();
  }

  // Time display
  int32_t displaySec = isSetting ? qtSetLeft * 60 + qtSetRight : qtRemaining;  // a change key, not a duration

  if (displaySec != dispTime) {
    if (dispTime == -1) qtShownSize = 0;  // the screen was cleared: draw every digit

    char text[12];
    if (isSetting) qtFormatSetting(text, sizeof text, qtSetLeft, qtSetRight);
    else           qtFormatRemaining(text, sizeof text, qtRemaining);
    qtDrawDigits(text, accent);

    // Progress bar (only when running/paused/done)
    if (!isSetting && qtTotalSec > 0) {
      int32_t elapsed = qtTotalSec - qtRemaining;
      int16_t pw = (int16_t)((elapsed * 280L) / qtTotalSec);
      tft.fillRoundRect(20, 190, 280, 4, 2, COLOR_SURFACE);
      if (pw > 0) tft.fillRoundRect(20, 190, pw, 4, 2, accent);
    }

    dispTime = displaySec;
  }
}

// --- Input ---

static void qtEnterMenu() {
  qtState = QTState::MENU;
  simpleMenuSel = SimpleMenuItem::VOLUME;
  needsFullRedraw = true;
  playMenuSound();
}

static void qtExitMenu() {
  if (qtRemaining > 0) qtState = QTState::PAUSED;
  else qtState = QTState::SETTING_LEFT;
  needsFullRedraw = true;
}

static void qtHandleEncoder(int32_t delta) {
  qtEnsureFields();
  if (qtState == QTState::SETTING_LEFT) {
    qtStepLeft(qtUnits, qtSetLeft, qtSetRight, -delta);
  } else if (qtState == QTState::SETTING_RIGHT) {
    qtStepRight(qtUnits, qtSetLeft, qtSetRight, -delta);
  } else if (qtState == QTState::PAUSED) {
    qtRemaining = qtNudgeRemaining(qtUnits, qtRemaining, -delta);
    // Adjust total for progress bar
    if (qtTotalSec < qtRemaining) qtTotalSec = qtRemaining;
    dispTime = -1;
    needsFullRedraw = true;
  } else if (qtState == QTState::MENU) {
    handleSimpleMenuEncoder(delta);
  }
}

static void qtHandleClick() {
  qtEnsureFields();
  if (qtState == QTState::MENU) {
    if (!handleSimpleMenuClick() && simpleMenuSel == SimpleMenuItem::TIMER_MODE) {
      // Switch MM:SS <-> HH:MM, keeping the set time where it fits
      int32_t total = qtSetTotal(qtUnits, qtSetLeft, qtSetRight);
      qtUnits = qtUnits == QTUnits::HOUR_MIN ? QTUnits::MIN_SEC : QTUnits::HOUR_MIN;
      qtSplitTotal(qtUnits, total, qtSetLeft, qtSetRight);
      saveSettings();
      playClickSound();
      needsFullRedraw = true;
    }
    return;
  }

  switch (qtState) {
    case QTState::SETTING_LEFT:
      qtState = QTState::SETTING_RIGHT;
      playClickSound();
      needsFullRedraw = true;
      break;
    case QTState::SETTING_RIGHT:
      qtRemaining = qtSetTotal(qtUnits, qtSetLeft, qtSetRight);
      qtTotalSec = qtRemaining;
      qtState = QTState::RUNNING;
      qtLastTick = millis();
      playClickSound();
      needsFullRedraw = true;
      break;
    case QTState::RUNNING:
      qtState = QTState::PAUSED;
      playClickSound();
      needsFullRedraw = true;
      break;
    case QTState::PAUSED:
      qtState = QTState::RUNNING;
      qtLastTick = millis();
      playClickSound();
      needsFullRedraw = true;
      break;
    case QTState::DONE:
      qtState = QTState::SETTING_LEFT;
      qtRemaining = 0;
      playClickSound();
      needsFullRedraw = true;
      break;
    default: break;
  }
}

static void qtHandleLongPress() {
  if (qtState == QTState::MENU) qtExitMenu();
  else qtEnterMenu();
}

static void qtDraw() {
  qtEnsureFields();
  if (qtState == QTState::MENU) drawSimpleMenu("Settings");
  else drawQTTimerScreen();
}
