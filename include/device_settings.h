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

static bool mirrorInfoOpen  = false;
static int  mirrorInfoShown = -1;  // the link state on screen (0/1); -1 draws it again

static void openMirrorInfo() {
  mirrorInfoOpen  = true;
  mirrorInfoShown = -1;
  playClickSound();
}

static void closeMirrorInfo() {
  mirrorInfoOpen  = false;
  needsFullRedraw = true;  // the menu underneath
  playMenuSound();
}

static void printCentered(const char *s, int16_t y, uint8_t size) {
  tft.setTextSize(size);
  tft.setCursor((tft.width() - (int16_t)strlen(s) * 6 * size) / 2, y);
  tft.print(s);
}

static void drawMirrorInfo() {
  int linked = mirrorConnected() ? 1 : 0;
  if (!needsFullRedraw && linked == mirrorInfoShown) return;

  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  printCentered("PC Mirror", 20, 2);
  tft.fillRect(20, 50, 280, 1, COLOR_SURFACE);

  tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
  printCentered("On your PC, open in Chrome or Edge:", 66, 1);

  tft.fillRoundRect(15, 82, 290, 40, 8, COLOR_SURFACE);
  tft.setTextColor(COLOR_ACCENT_FOCUS, COLOR_SURFACE);
  printCentered(MIRROR_ADDRESS, 95, 2);

  // Link state: a dot and a word, centred together
  const char *state = linked ? "Connected" : "Not connected";
  int16_t w = 16 + (int16_t)strlen(state) * 12;
  int16_t x = (tft.width() - w) / 2;
  if (linked) tft.fillCircle(x + 5, 151, 5, COLOR_ACCENT_BREAK);
  else        tft.drawCircle(x + 5, 151, 5, COLOR_TEXT_DIM);
  tft.setTextColor(linked ? COLOR_TEXT : COLOR_TEXT_DIM, COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor(x + 16, 144);
  tft.print(state);

  tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
  printCentered(linked ? "The page shows this screen live" : "Plug in USB, then click Connect on the page",
                172, 1);
  printCentered("click or hold: back", 228, 1);

  mirrorInfoShown = linked;
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
