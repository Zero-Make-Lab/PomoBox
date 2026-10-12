#include "platform/power.h"

#include "config.h"
#include "platform/power_logic.h"

#if defined(POMOBOX_C6)
#include <driver/gpio.h>
#include <esp_sleep.h>

void powerHold() {
  // FireBeetle's LED is active-low. Keep it off in normal use and in sleep.
  digitalWrite(15, HIGH);
  pinMode(15, OUTPUT);
  gpio_hold_dis(GPIO_NUM_15);
  digitalWrite(PERIPHERAL_OFF, LOW);
  pinMode(PERIPHERAL_OFF, OUTPUT);
  gpio_hold_dis(static_cast<gpio_num_t>(PERIPHERAL_OFF));
  delay(60);  // Q6's soft start (C15) takes ~1.6 ms; leave the switched 3V3 well settled before SPI
}

void powerRelease() {
  pomoLedcWrite(BUZZER_CHANNEL, 0);
  ledcDetach(BUZZER_PIN);
  detachInterrupt(ENC_CLK);
  detachInterrupt(ENC_DT);
  SPI.end();
  // No high output or internal pull-up may feed an unpowered peripheral.
  for (int pin : {TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCK, TFT_MISO,
                  TFT_LED, ENC_CLK, ENC_DT, BUZZER_PIN}) pinMode(pin, INPUT);
  digitalWrite(PERIPHERAL_OFF, HIGH);
  gpio_hold_en(static_cast<gpio_num_t>(PERIPHERAL_OFF));
  gpio_hold_en(GPIO_NUM_15);
  // ESP32-C6 holds individual pads in deep sleep; it has no global hold API.
  // Do not instantly wake on the same held button that requested shutdown.
  while (digitalRead(ENC_SW) == LOW) delay(10);
  delay(30);
  esp_sleep_enable_ext1_wakeup_io(1ULL << ENC_SW, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

bool powerStartedByButton() {
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1 || digitalRead(ENC_SW) == LOW;
}

bool powerBatteryCritical() {
  static LowBatteryFilter filter(VBAT_CRITICAL_MV, VBAT_CRITICAL_READINGS);
  static uint32_t lastCheckMs = 0;
  static bool critical = false;
  const uint32_t now = millis();
  if (now - lastCheckMs < VBAT_CHECK_MS) return critical;
  lastCheckMs = now;
  static uint32_t previousMv = 0;
  analogSetPinAttenuation(VBAT_SENSE, ADC_11db);
  uint32_t sum = 0, lo = UINT32_MAX, hi = 0;
  for (int i = 0; i < 8; ++i) {
    const uint32_t tap = analogReadMilliVolts(VBAT_SENSE);
    sum += tap;
    lo = min(lo, tap);
    hi = max(hi, tap);
  }
  const uint32_t mv = dividerInputMillivolts(sum / 8, VBAT_DIVIDER_TOP, VBAT_DIVIDER_BOTTOM);
  const uint32_t minMv = dividerInputMillivolts(lo, VBAT_DIVIDER_TOP, VBAT_DIVIDER_BOTTOM);
  const uint32_t maxMv = dividerInputMillivolts(hi, VBAT_DIVIDER_TOP, VBAT_DIVIDER_BOTTOM);
  // There is no USB-present signal on the DFR1075 headers. A depleted connected cell stops the app even on USB;
  // charging continues while asleep. Below 1 V means the battery is unplugged or its protection is open, and a
  // swinging reading means the 45 C thermostat has opened: either way USB alone can still run the board.
  const bool notABattery = c6BatteryAbsent(mv) || c6BatteryUnsteady(minMv, maxMv, mv, previousMv);
  previousMv = mv;
  critical = filter.update(mv, notABattery);
  return critical;
}

#elif defined(ZER0_POWER_LATCH)

// The earliest point to take the latch over from the button, which only keeps the board on while it is pressed.
// arduino-pico calls this before USB starts and before setup(); the ESP32 core at the end of its own start-up, also
// before setup().
void initVariant() { powerHold(); }

void powerHold() {
  pinMode(PWR_HOLD, OUTPUT);
  digitalWrite(PWR_HOLD, HIGH);
}

void powerRelease() { digitalWrite(PWR_HOLD, LOW); }

namespace {
#if defined(ARDUINO_ARCH_RP2040)
bool usbPresent() {
  pinMode(VBUS_SENSE, INPUT);
  return digitalRead(VBUS_SENSE) == HIGH;
}

uint32_t readBatteryMillivolts() {
  analogReadResolution(12);
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(VBAT_SENSE);
  return batteryMillivolts(sum / 8, 4095, 3300, VBAT_DIVIDER_TOP, VBAT_DIVIDER_BOTTOM);
}

#else
// Zer0 DIY: USB and the battery both come in through dividers, read with the ESP32's calibrated ADC
uint32_t tapMillivolts(int pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(pin);
  return sum / 8;
}

bool usbPresent() {
  return usbPresentFromVbus(
      dividerInputMillivolts(tapMillivolts(VBUS_SENSE), VBUS_DIVIDER_TOP, VBUS_DIVIDER_BOTTOM));
}

uint32_t readBatteryMillivolts() {
  return dividerInputMillivolts(tapMillivolts(VBAT_SENSE), VBAT_DIVIDER_TOP, VBAT_DIVIDER_BOTTOM);
}
#endif
}  // namespace

bool powerStartedByButton() {
  pinMode(ENC_SW, INPUT_PULLUP);
  return startedByButton(digitalRead(ENC_SW) == LOW, usbPresent());
}

bool powerBatteryCritical() {
  static LowBatteryFilter filter(VBAT_CRITICAL_MV, VBAT_CRITICAL_READINGS);
  static uint32_t lastCheckMs = 0;
  static bool critical = false;
  const uint32_t now = millis();
  if (now - lastCheckMs < VBAT_CHECK_MS) return critical;
  lastCheckMs = now;

  critical = filter.update(readBatteryMillivolts(), usbPresent());
  return critical;
}

#else

void powerHold() {}
void powerRelease() {
#if defined(POMOBOX_C3_USB)
  pomoLedcWrite(BUZZER_CHANNEL, 0);
#endif
}
bool powerStartedByButton() { return false; }
bool powerBatteryCritical() { return false; }

#endif
