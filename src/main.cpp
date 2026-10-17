// The PomoBox kit: ESP32-C3 SuperMini + PEC12R encoder + 3.2" SPI TFT 240x320 (ILI9341), USB powered
// (env pomobox_c3_usb, the default). Also the original ESP32-WROOM-32E + KY-040 breadboard build (esp32dev) and
// earlier prototype boards (Zer0 rev C on the RP2040, the Zer0 DIY board, the FireBeetle 2 ESP32-C6 battery kit).
// PomoBox - Multi-App Timer Platform
// Apps: Pomodoro Timer, Quick Timer, Stopwatch
// Input: Rotate -> Navigate/Adjust, Click -> Action, Hold -> Menu/Back

#include <Arduino.h>
#include <SPI.h>
#include "common.h"

// =============================================================================
// Global Hardware Objects
// =============================================================================

// tft is in src/platform/display.cpp, with the board's display driver
ESP32Encoder encoder;
Preferences prefs;

// =============================================================================
// Global State (declared extern in common.h)
// =============================================================================

ActiveApp   activeApp   = ActiveApp::LAUNCHER;
VolumeLevel volumeLevel = VolumeLevel::VOL_HIGH;
QTUnits     qtUnits     = QTUnits::MIN_SEC;
bool        deviceOn    = false;
bool        needsFullRedraw = true;

// Display cache
int32_t  dispTime       = -1;
int32_t  dispMins       = -1;
int32_t  dispSecs       = -1;
int32_t  dispTenths     = -1;
int      dispSel        = -1;
int      dispMenuSel    = -1;
int      dispMenuScroll = -1;
ActiveApp dispApp       = ActiveApp::LAUNCHER;

// Shared simple menu selection
SimpleMenuItem simpleMenuSel = SimpleMenuItem::VOLUME;

// =============================================================================
// Forward Declarations (needed by common.h / app headers)
// =============================================================================

void saveSettings();
void loadSettings();
void switchToLauncher();
void turnOff();
void turnOn();

// =============================================================================
// Include App Modules
// =============================================================================

#include "launcher.h"
#include "pomodoro.h"
#include "quicktimer.h"
#include "stopwatch.h"
#include "counter.h"
#include "dice.h"
#include "metronome.h"
#include "device_settings.h"
#include "serial_update.h"

// =============================================================================
// Storage (NVS)
// =============================================================================
// Keep the persisted namespace so existing devices retain settings and focus stats.

void saveSettings() {
  prefs.begin("zerotimer", false);
  prefs.putUInt("focusTime", pomTotalFocus);
  prefs.putUChar("volume", (uint8_t)volumeLevel);
  prefs.putInt("focusDur", pomFocusSec);
  prefs.putInt("breakDur", pomBreakSec);
  prefs.putBool("autoCont", pomAutoContinue);
  prefs.putUChar("qtUnits", (uint8_t)qtUnits);
  prefs.end();
}

void loadSettings() {
  prefs.begin("zerotimer", true);
  pomTotalFocus   = prefs.getUInt("focusTime", 0);
  volumeLevel     = (VolumeLevel)prefs.getUChar("volume", (uint8_t)VolumeLevel::VOL_HIGH);
  pomFocusSec     = prefs.getInt("focusDur", 50 * 60);
  pomBreakSec     = prefs.getInt("breakDur", 25 * 60);
  pomAutoContinue = prefs.getBool("autoCont", true);
  qtUnits         = qtUnitsFromStored(prefs.getUChar("qtUnits", (uint8_t)QTUnits::MIN_SEC));
  prefs.end();
  pomRemaining = pomFocusSec;
}

// =============================================================================
// Power Management & App Switching
// =============================================================================

void turnOff() {
  saveSettings();
  tft.fillScreen(0x0000);
  displaySleep(true);
  digitalWrite(TFT_LED, LOW);
  deviceOn = false;
  Serial.println("Device OFF");
  // On battery (rev C, DIY board) the board switches off here. On USB it stays in the soft off until a press or the
  // unplug.
  powerRelease();
}

void turnOn() {
  powerHold();
  mirrorInfoOpen = false;
  settingsOpen   = false;
  digitalWrite(TFT_LED, HIGH);
  displaySleep(false);
  deviceOn = true;
  activeApp = ActiveApp::LAUNCHER;
  needsFullRedraw = true;
  Serial.println("Device ON");
  playBootSplash();
  if (volumeLevel != VolumeLevel::VOL_MUTE) playStartupJingle();
}

void switchToLauncher() {
  // Pause running apps before switching
  if (activeApp == ActiveApp::POMODORO && pomState == PomState::RUNNING) {
    pomState = PomState::PAUSED;
  }
  if (activeApp == ActiveApp::QUICK_TIMER && qtState == QTState::RUNNING) {
    qtState = QTState::PAUSED;
  }
  if (activeApp == ActiveApp::STOPWATCH && swState == SWState::RUNNING) {
    swAccumMs += millis() - swStartMs;
    swState = SWState::STOPPED;
  }
  if (activeApp == ActiveApp::METRONOME && metroState == MetroState::PLAYING) {
    metroState = MetroState::IDLE;
    pomoLedcWriteTone(BUZZER_CHANNEL, 0);
    metroBuzzing = false;
  }

  activeApp = ActiveApp::LAUNCHER;
  needsFullRedraw = true;
  playMenuSound();
}

// =============================================================================
// Encoder Reading
// =============================================================================

static int64_t lastEncPos = 0;

static int32_t readEncoderDelta() {
  int64_t rawPos = encoder.getCount();
  int32_t det    = (int32_t)(rawPos / COUNTS_PER_DETENT);
  int32_t lastDet = (int32_t)(lastEncPos / COUNTS_PER_DETENT);
  int32_t delta  = det - lastDet;
  if (delta != 0) lastEncPos = rawPos;
  return delta;
}

// =============================================================================
// Button Reading
// =============================================================================

static bool     btnDown          = false;
static uint32_t btnDownMs        = 0;
static bool     longPressHandled = false;
static bool     pressSoundPlayed = false;
static constexpr uint32_t DEBOUNCE_MS = 30;  // Minimum press duration to count

// Button events: 0=none, 1=click, 2=long press
static int readButton() {
  bool pressed = (digitalRead(ENC_SW) == LOW);
  uint32_t now = millis();
  int event = 0;

  // Button just pressed
  if (pressed && !btnDown) {
    btnDown = true;
    btnDownMs = now;
    longPressHandled = false;
    pressSoundPlayed = false;
  }

  // Only consider presses valid after debounce period
  bool validPress = btnDown && (now - btnDownMs >= DEBOUNCE_MS);

  if (validPress && !pressSoundPlayed) {
    playKnobSound(KnobSound::PRESS);
    pressSoundPlayed = true;
  }

  // Long press detection (only after debounce)
  if (validPress && pressed && !longPressHandled && (now - btnDownMs >= LONG_PRESS_MS)) {
    longPressHandled = true;
    event = 2;  // long press
  }

  // Button released
  if (!pressed && btnDown) {
    if (validPress) playKnobSound(KnobSound::RELEASE);
    if (!longPressHandled && validPress) event = 1;  // click (only if held > debounce)
    btnDown = false;
  }

  return event;
}

// =============================================================================
// Input Dispatch
// =============================================================================

static void dispatchEncoder(int32_t delta) {
  if (delta != 0) playKnobSound(KnobSound::TURN);
  if (mirrorInfoOpen) return;
  switch (activeApp) {
    case ActiveApp::LAUNCHER:
      if (settingsOpen) settingsHandleEncoder(delta);
      else              launcherHandleEncoder(delta);
      break;
    case ActiveApp::POMODORO:    pomHandleEncoder(delta);      break;
    case ActiveApp::QUICK_TIMER: qtHandleEncoder(delta);       break;
    case ActiveApp::STOPWATCH:   swHandleEncoder(delta);       break;
    case ActiveApp::COUNTER:     ctHandleEncoder(delta);       break;
    case ActiveApp::DICE:        diceHandleEncoder(delta);     break;
    case ActiveApp::METRONOME:   metroHandleEncoder(delta);    break;
  }
}

static void dispatchClick() {
  if (mirrorInfoOpen) { mirrorInfoClick(); return; }  // ask the PC again, or back once connected
  switch (activeApp) {
    case ActiveApp::LAUNCHER:
      if (settingsOpen) settingsClick();
      else              launcherClick();
      break;
    case ActiveApp::POMODORO:    pomHandleClick();   break;
    case ActiveApp::QUICK_TIMER: qtHandleClick();    break;
    case ActiveApp::STOPWATCH:   swHandleClick();    break;
    case ActiveApp::COUNTER:     ctHandleClick();    break;
    case ActiveApp::DICE:        diceHandleClick();  break;
    case ActiveApp::METRONOME:   metroHandleClick(); break;
  }
}

static void dispatchLongPress() {
  if (mirrorInfoOpen) { closeMirrorInfo(); return; }
  switch (activeApp) {
    case ActiveApp::LAUNCHER:
      if (settingsOpen) closeSettings();
      else              openSettings();
      break;
    case ActiveApp::POMODORO:    pomHandleLongPress();  break;
    case ActiveApp::QUICK_TIMER: qtHandleLongPress();   break;
    case ActiveApp::STOPWATCH:   swHandleLongPress();   break;
    case ActiveApp::COUNTER:     ctHandleLongPress();   break;
    case ActiveApp::DICE:        diceHandleLongPress(); break;
    case ActiveApp::METRONOME:   metroHandleLongPress(); break;
  }
}

static void dispatchTick() {
  switch (activeApp) {
    case ActiveApp::POMODORO:    pomTick(); break;
    case ActiveApp::QUICK_TIMER: qtTick();     break;
    case ActiveApp::METRONOME:   metroTick(); break;
    default: break;
  }
}

static void dispatchDraw() {
  if (mirrorInfoOpen) { drawMirrorInfo(); return; }
  switch (activeApp) {
    case ActiveApp::LAUNCHER:
      if (settingsOpen) drawSettings();
      else              drawLauncherScreen();
      break;
    case ActiveApp::POMODORO:    pomDraw();             break;
    case ActiveApp::QUICK_TIMER: qtDraw();              break;
    case ActiveApp::STOPWATCH:   swDraw();              break;
    case ActiveApp::COUNTER:     ctDraw();              break;
    case ActiveApp::DICE:        diceDraw();            break;
    case ActiveApp::METRONOME:   metroDraw();           break;
  }
}

// =============================================================================
// USB Screen Mirror (docs/specs/usb-mirror.md)
// =============================================================================

static MirrorSession mirrorSession{};
static MirrorLine    mirrorLine{};

bool mirrorConnected() { return mirrorSession.active; }

static void mirrorWrite(const uint8_t *frame, size_t len) { Serial.write(frame, len); }

// A page said hello: tell it the screen size and state, then draw the whole screen into it
// One saved flag, written only when it changes (NVS wear)
static bool mirrorResumeSaved = false;

void mirrorResumeMark(bool asked) {
  if (asked == mirrorResumeSaved) return;
  mirrorResumeSaved = asked;
  prefs.begin("zerotimer", false);
  prefs.putBool("mirrorAsk", asked);
  prefs.end();
}

static void mirrorStart() {
  mirrorResumeMark(false);  // the page is here: a later restart is not the hand-off
  uint8_t f[MIRROR_FRAME_MAX];
  Serial.write(f, mirrorFrameInfo(f, tft.width(), tft.height()));
  Serial.write(f, mirrorFramePower(f, deviceOn));
  tft.mirrorOut = mirrorWrite;
  needsFullRedraw = true;
  mirrorInfoShown = -1;
}

static void mirrorStop() {
  tft.mirrorOut = nullptr;
  mirrorInfoShown = -1;  // the PC Mirror screen shows the change
}

// Knob input from the page, gathered since the last loop
struct MirrorInput {
  int32_t turn;
  bool    click;
  bool    hold;
};

// =============================================================================
// Update over USB, no BOOT button (serial_update.h, docs/specs/usb-mirror.md)
// =============================================================================

#if defined(ARDUINO_ARCH_ESP32)
#include <Update.h>
#endif

static struct {
  bool     active;
  uint32_t size, received, lastMs;
  uint8_t  shownPercent;
} upd;

static void updateSay(const char *what) {
  Serial.print("@pomobox update ");
  Serial.println(what);
}

static void drawUpdateScreen(uint8_t percent, bool full) {
  if (full) {
    digitalWrite(TFT_LED, HIGH);  // also from the soft off
    displaySleep(false);
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    printCentered("Updating", 70, 3);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    printCentered("Keep the USB cable plugged in", 112, 1);
    tft.fillRoundRect(30, 140, 260, 12, 6, COLOR_SURFACE);
  }
  int16_t w = (int16_t)(260L * percent / 100);
  if (w > 0) tft.fillRoundRect(30, 140, w, 12, 6, COLOR_ACCENT_FOCUS);
  upd.shownPercent = percent;
}

static void updateFail(const char *reason) {
#if defined(ARDUINO_ARCH_ESP32)
  Update.abort();
#endif
  upd.active = false;
  char msg[64];
  snprintf(msg, sizeof msg, "error %s", reason);
  updateSay(msg);
  needsFullRedraw = true;  // back to what was on screen; the running firmware stays
  if (!deviceOn) { tft.fillScreen(0x0000); displaySleep(true); digitalWrite(TFT_LED, LOW); }
}

static void updateBegin(const char *line) {
  UpdateRequest req;
  if (!parseUpdateCommand(line, req)) { updateSay("error bad-command"); return; }
#if defined(ARDUINO_ARCH_ESP32)
  mirrorStop();  // no frames between the acks; the page says hello again after the restart
  mirrorSession.active = false;
  if (!Update.begin(req.size)) { updateSay("error no-space"); return; }
  Update.setMD5(req.md5);
  upd = {true, req.size, 0, millis(), 0};
  drawUpdateScreen(0, true);
  updateSay("ready");
#else
  updateSay("error unsupported");
#endif
}

// While an update runs, the loop does only this
static void updatePump() {
#if defined(ARDUINO_ARCH_ESP32)
  static uint8_t buf[1024];
  uint32_t n;
  while ((n = updateTake(upd.size, upd.received, (uint32_t)Serial.available(), sizeof buf)) > 0) {
    n = (uint32_t)Serial.readBytes(buf, n);
    if (Update.write(buf, n) != n) { updateFail("write"); return; }
    upd.received += n;
    upd.lastMs = millis();
    if (updateShouldAck(upd.received, upd.size)) {
      char msg[40];
      snprintf(msg, sizeof msg, "ack %lu", (unsigned long)upd.received);
      updateSay(msg);
    }
  }
  uint8_t pct = updatePercent(upd.received, upd.size);
  if (pct != upd.shownPercent) drawUpdateScreen(pct, false);
  if (upd.received == upd.size) {
    if (!Update.end()) { updateFail(Update.md5String().length() ? "checksum" : "finish"); return; }
    updateSay("done");
    Serial.flush();
    delay(300);
    ESP.restart();
  }
  if (millis() - upd.lastMs > UPDATE_IDLE_MS) updateFail("timeout");
#endif
}

static MirrorInput pollMirror() {
  MirrorInput in{};
  while (Serial.available() > 0) {
    int32_t turn = 0;
    MirrorCmd cmd = mirrorFeed(mirrorLine, (char)Serial.read(), turn);
    mirrorHeard(mirrorSession, cmd, millis());
    switch (cmd) {
      case MirrorCmd::HELLO: mirrorStart();    break;
      case MirrorCmd::BYE:   mirrorStop();     break;
      case MirrorCmd::TURN:  in.turn += turn;  break;
      case MirrorCmd::CLICK: in.click = true;  break;
      case MirrorCmd::HOLD:  in.hold  = true;  break;
      case MirrorCmd::UPDATE:
        updateBegin(mirrorLine.buf);
        if (upd.active) return in;  // what follows on the link is the image, not lines
        break;
      default: break;
    }
  }
  if (mirrorExpired(mirrorSession, millis())) {
    mirrorSession.active = false;
    mirrorStop();
  }
  return in;
}

// =============================================================================
// Arduino Setup & Loop
// =============================================================================

void setup() {
  powerHold();  // C6: power peripherals before driving their inputs or starting SPI
#if defined(ARDUINO_ARCH_ESP32)
  Serial.setTxBufferSize(8192);  // a whole-screen redraw while mirroring fits without stalling the panel
  Serial.setRxBufferSize(8192);  // a whole update block arrives before the loop reads it
#endif
  Serial.begin(MIRROR_BAUD);
  delay(200);
  Serial.println("\nPomoBox - Multi-App Platform");

  pinMode(ENC_SW, INPUT_PULLUP);

  // Backlight (start off)
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, LOW);

  // Buzzer
  pomoLedcSetup(BUZZER_CHANNEL, 2000, 8);
  pomoLedcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);

  // Encoder
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachHalfQuad(ENC_CLK, ENC_DT);
  encoder.setFilter(1023);
  encoder.clearCount();

  // TFT
  pinMode(TFT_CS, OUTPUT);  digitalWrite(TFT_CS, HIGH);
  pinMode(TFT_DC, OUTPUT);  digitalWrite(TFT_DC, HIGH);
  pinMode(TFT_RST, OUTPUT); digitalWrite(TFT_RST, HIGH);

  displayBegin();

  // Load saved settings
  loadSettings();
  Serial.print("Loaded focus time: ");
  Serial.print(pomTotalFocus / 3600);
  Serial.print("h ");
  Serial.print((pomTotalFocus % 3600) / 60);
  Serial.println("m");

  // Start with display off, wait for button press
  tft.fillScreen(0x0000);
  displaySleep(true);
  deviceOn = false;

  if (powerStartedByButton()) {
    // Rev C, DIY board: the press that switched the board on is the press that turns the timer on
    playKnobSound(KnobSound::PRESS);
    while (digitalRead(ENC_SW) == LOW) { delay(10); }
    playKnobSound(KnobSound::RELEASE);
    turnOn();
    return;
  }

  // Powered by USB (or the bare ESP32 devkit): soft off. On rev C and the DIY board, unplugging USB now switches the
  // board off.
  powerRelease();
  Serial.println("Press button to start...");
  // loop() waits in the soft off for a press of the knob, or a click on the PC mirror page

  // Restarted by the PC taking the port right after PC Mirror asked for it: come back on, at that screen
  prefs.begin("zerotimer", true);
  mirrorResumeSaved = prefs.getBool("mirrorAsk", false);
  prefs.end();
  if (mirrorResumeSaved) {
    Serial.println("Resuming PC Mirror");
    turnOn();
    mirrorInfoOpen  = true;
    mirrorAsked     = true;  // shows "Opening on your PC" until the page says hello
    mirrorAskedMs   = millis();
    mirrorInfoShown = -1;
  }
}

// On battery (rev C, DIY board): the cell is nearly empty. Say so, then save and switch off while the 3.3 V supply
// still holds.
static void lowBatteryOff() {
  Serial.println("Battery low");
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_RED, COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor((tft.width() - 11 * 12) / 2, 112);
  tft.print("Battery low");
  delay(1500);
  turnOff();
}

void loop() {
  if (upd.active) { updatePump(); return; }
  MirrorInput pc = pollMirror();
  if (upd.active) return;

  // Handle OFF state
  if (!deviceOn) {
    if (pc.click || pc.hold) {  // the mirror page switches it on like the knob
      playKnobSound(KnobSound::PRESS);
      playKnobSound(KnobSound::RELEASE);
      turnOn();
      return;
    }
    if (digitalRead(ENC_SW) == LOW) {
      delay(50);
      if (digitalRead(ENC_SW) != LOW) return;
      playKnobSound(KnobSound::PRESS);
      while (digitalRead(ENC_SW) == LOW) { delay(10); }
      playKnobSound(KnobSound::RELEASE);
      turnOn();
    }
    return;
  }

  if (powerBatteryCritical()) {
    lowBatteryOff();
    return;
  }

  // Timer ticks
  dispatchTick();

  // Encoder input, the knob's and the page's
  int32_t delta = readEncoderDelta() + pc.turn;
  if (delta != 0) {
    Serial.print("ENC delta="); Serial.println(delta);
    dispatchEncoder(delta);
  }

  // Button input; the page's clicks and holds come with the knob's sounds
  int btnEvent = readButton();
  if (pc.click || pc.hold) {
    playKnobSound(KnobSound::PRESS);
    playKnobSound(KnobSound::RELEASE);
    if (pc.click) dispatchClick();
    if (pc.hold)  dispatchLongPress();
  }
  if (btnEvent == 1) {
    Serial.println("BTN click");
    dispatchClick();
  }
  if (btnEvent == 2) {
    Serial.println("BTN long");
    dispatchLongPress();
  }

  // Display
  dispatchDraw();
}
