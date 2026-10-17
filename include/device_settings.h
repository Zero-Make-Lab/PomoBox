#pragma once
#include "common.h"

// =============================================================================
// Device settings (docs/specs/usb-mirror.md)
// - the PC Mirror screen: opened from every app's Settings and from the general Settings, shows the address
//   to open on the PC and whether a PC is connected
// - the general Settings menu: hold the knob on the app list
// =============================================================================

static constexpr const char *MIRROR_ADDRESS = "zeromakelab.com/mirror";

inline const char *volumeName() {
  switch (volumeLevel) {
    case VolumeLevel::VOL_MUTE: return "Mute";
    case VolumeLevel::VOL_LOW:  return "Low";
    case VolumeLevel::VOL_MED:  return "Med";
    case VolumeLevel::VOL_HIGH: return "High";
  }
  return "";
}

// --- PC Mirror screen: drawn over whichever menu opened it ---
// Opening it asks the PC to open the page (the PomoBox helper does, docs/specs/usb-mirror.md); a click asks again,
// or goes back once connected; a hold goes back.

static bool     mirrorInfoOpen  = false;
static int      mirrorInfoShown = -1;  // the MirrorLink state on screen; -1 draws it again
static bool     mirrorAsked     = false;
static uint32_t mirrorAskedMs   = 0;

static void requestMirrorOpen() {
  Serial.println(MIRROR_OPEN_REQUEST);
  mirrorAsked   = true;
  mirrorAskedMs = millis();
  mirrorInfoShown = -1;
}

static void openMirrorInfo() {
  mirrorInfoOpen  = true;
  mirrorInfoShown = -1;
  mirrorAsked     = false;
  if (!mirrorConnected()) requestMirrorOpen();
  playClickSound();
}

static void closeMirrorInfo() {
  mirrorInfoOpen  = false;
  needsFullRedraw = true;  // the menu underneath
  playMenuSound();
}

static void mirrorInfoClick() {
  if (mirrorConnected()) { closeMirrorInfo(); return; }
  requestMirrorOpen();
  playClickSound();
}

static void printCentered(const char *s, int16_t y, uint8_t size) {
  tft.setTextSize(size);
  tft.setCursor((tft.width() - (int16_t)strlen(s) * 6 * size) / 2, y);
  tft.print(s);
}

static void drawMirrorInfo() {
  MirrorLink link = mirrorLinkState(mirrorConnected(), mirrorAsked, mirrorAskedMs, millis());
  if (!needsFullRedraw && (int)link == mirrorInfoShown) return;

  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  printCentered("PC Mirror", 20, 2);
  tft.fillRect(20, 50, 280, 1, COLOR_SURFACE);

  tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
  printCentered("On your PC, open in Chrome or Edge:", 66, 1);

  tft.fillRoundRect(15, 82, 290, 40, 8, COLOR_SURFACE);
  tft.setTextColor(COLOR_ACCENT_FOCUS, COLOR_SURFACE);
  printCentered(MIRROR_ADDRESS, 95, 2);

  const char *state = "Not connected", *detail = "Click to open it on your PC", *hint = "click: open on PC  hold: back";
  uint16_t dot = COLOR_TEXT_DIM;
  bool filled = false;
  switch (link) {
    case MirrorLink::IDLE: break;
    case MirrorLink::OPENING:
      state = "Opening on your PC"; detail = "Your browser is opening the mirror"; hint = "hold: back";
      dot = COLOR_ACCENT_QTIMER; filled = true;
      break;
    case MirrorLink::NO_ANSWER:
      state = "No answer from PC"; detail = "Start the PomoBox helper, or open the address";
      hint = "click: try again  hold: back"; dot = COLOR_ACCENT_PAUSE;
      break;
    case MirrorLink::CONNECTED:
      state = "Connected"; detail = "The page shows this screen live"; hint = "click or hold: back";
      dot = COLOR_ACCENT_BREAK; filled = true;
      break;
  }

  // Link state: a dot and a word, centred together
  int16_t w = 16 + (int16_t)strlen(state) * 12;
  int16_t x = (tft.width() - w) / 2;
  if (filled) tft.fillCircle(x + 5, 151, 5, dot);
  else        tft.drawCircle(x + 5, 151, 5, dot);
  tft.setTextColor(link == MirrorLink::IDLE ? COLOR_TEXT_DIM : COLOR_TEXT, COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor(x + 16, 144);
  tft.print(state);

  tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
  printCentered(detail, 172, 1);
  printCentered(hint, 228, 1);

  mirrorInfoShown = (int)link;
  needsFullRedraw = false;
}

// A "PC Mirror" row for an app's Settings: the label, and the link state on the right. Not static: common.h
// declares it for the Quick Timer's menu, which is drawn there.
void drawMirrorMenuRow(int16_t y, int16_t textDy, bool selected, uint16_t bg) {
  tft.setTextSize(2);
  tft.setTextColor(selected ? COLOR_TEXT : COLOR_TEXT_DIM, bg);
  tft.setCursor(25, y + textDy);
  tft.print("PC Mirror");
  tft.setTextColor(selected ? COLOR_ACCENT_FOCUS : COLOR_TEXT_DIM, bg);
  tft.setCursor(220, y + textDy);
  tft.print(mirrorConnected() ? "Linked" : "Open");
}

// --- General Settings: hold the knob on the app list ---

enum class SettingsItem { VOLUME, PC_MIRROR, TURN_OFF };
static constexpr int SETTINGS_COUNT = 3;

static bool settingsOpen = false;
static int  settingsSel  = 0;

static void openSettings() {
  settingsOpen = true;
  settingsSel  = 0;
  needsFullRedraw = true;
  playMenuSound();
}

static void closeSettings() {
  settingsOpen = false;
  needsFullRedraw = true;  // the app list
  playMenuSound();
}

static void drawSettingsRow(int i, bool s) {
  const int IH = 50, SY = 60;
  int16_t y = SY + i * IH;
  uint16_t bg = s ? COLOR_SURFACE : COLOR_BG;
  tft.fillRoundRect(15, y, 290, 44, 8, bg);
  tft.setTextSize(2);
  switch ((SettingsItem)i) {
    case SettingsItem::VOLUME:
      tft.setTextColor(s ? COLOR_TEXT : COLOR_TEXT_DIM, bg);
      tft.setCursor(25, y + 14); tft.print("Volume");
      tft.setTextColor(s ? COLOR_ACCENT_FOCUS : COLOR_TEXT_DIM, bg);
      tft.setCursor(220, y + 14); tft.print(volumeName());
      break;
    case SettingsItem::PC_MIRROR:
      drawMirrorMenuRow(y, 14, s, bg);
      break;
    case SettingsItem::TURN_OFF:
      tft.setTextColor(COLOR_RED, bg);
      tft.setCursor(115, y + 14); tft.print("Turn Off");
      break;
  }
}

static void drawSettings() {
  if (needsFullRedraw) {
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    printCentered("Settings", 20, 2);
    tft.fillRect(20, 50, 280, 1, COLOR_SURFACE);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    printCentered("click: change  hold: back", 228, 1);
    for (int i = 0; i < SETTINGS_COUNT; i++) drawSettingsRow(i, i == settingsSel);
    needsFullRedraw = false;
    dispMenuSel = settingsSel;
  } else if (settingsSel != dispMenuSel) {
    if (dispMenuSel >= 0 && dispMenuSel < SETTINGS_COUNT) drawSettingsRow(dispMenuSel, false);
    drawSettingsRow(settingsSel, true);
    dispMenuSel = settingsSel;
  }
}

static void settingsHandleEncoder(int32_t delta) {
  if (delta > 0)      settingsSel = (settingsSel + 1) % SETTINGS_COUNT;
  else if (delta < 0) settingsSel = (settingsSel - 1 + SETTINGS_COUNT) % SETTINGS_COUNT;
}

static void settingsClick() {
  switch ((SettingsItem)settingsSel) {
    case SettingsItem::VOLUME:
      cycleVolume();
      needsFullRedraw = true;
      break;
    case SettingsItem::PC_MIRROR:
      openMirrorInfo();
      break;
    case SettingsItem::TURN_OFF:
      settingsOpen = false;  // switching on again starts at the app list
      turnOff();
      break;
  }
}
