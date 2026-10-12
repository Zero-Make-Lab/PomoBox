// Power decisions for the power latch on Zer0 rev C and the Zer0 DIY board. No Arduino dependencies, so the host unit
// tests (pio test -e native) cover them.
#pragma once
#include <stdint.h>

// On battery the board only runs while the latch holds, and only the encoder press switches it on, so a boot without
// USB came from the button. With USB the board is always powered: only a held button means "turn on".
inline bool startedByButton(bool buttonHeld, bool usbPresent) { return buttonHeld || !usbPresent; }

// Battery voltage from the VBAT_SENSE divider reading
inline uint32_t batteryMillivolts(uint32_t adcRaw, uint32_t adcFullScale, uint32_t refMillivolts,
                                  uint32_t dividerTop, uint32_t dividerBottom) {
  return (uint32_t)((uint64_t)adcRaw * refMillivolts * (dividerTop + dividerBottom) /
                    ((uint64_t)adcFullScale * dividerBottom));
}

// The voltage above a divider, from the voltage at its tap (the ESP32's ADC reads millivolts directly)
inline uint32_t dividerInputMillivolts(uint32_t tapMillivolts, uint32_t dividerTop, uint32_t dividerBottom) {
  return (uint32_t)((uint64_t)tapMillivolts * (dividerTop + dividerBottom) / dividerBottom);
}

// Zer0 DIY: USB is sensed on the DevKitC's 5 V pin. On battery that pin isn't at 0 V: the devkit's own regulator
// leaks the 3.3 V supply back onto it at about 2.6 V. USB gives 4.4 V or more, after the devkit's diode.
inline bool usbPresentFromVbus(uint32_t vbusMillivolts) { return vbusMillivolts >= 3600; }

inline bool c6BatteryAbsent(uint32_t batteryMv) { return batteryMv < 1000; }

// DIY rev C1: the battery reaches the FireBeetle through a 45 C thermostat, which can open while USB runs the board.
// With no cell the charger's BAT node swings as the charger restarts into its capacitor, while a nearly empty cell
// under a changing load moves by tens of millivolts. A check whose 8-sample burst spans more than 300 mV, or that
// moved more than 400 mV since the check 2 s earlier (previousMv; 0 = none yet), is not a battery level.
static constexpr uint32_t C6_BURST_SPREAD_MV = 300;
static constexpr uint32_t C6_STEP_MV = 400;
inline bool c6BatteryUnsteady(uint32_t minMv, uint32_t maxMv, uint32_t mv, uint32_t previousMv) {
  if (maxMv - minMv > C6_BURST_SPREAD_MV) return true;
  if (previousMv == 0) return false;
  const uint32_t step = mv > previousMv ? mv - previousMv : previousMv - mv;
  return step > C6_STEP_MV;
}

// Battery nearly empty: `needed` readings in a row below the threshold, while on battery.
// One reading above it, or USB power, starts the count again.
class LowBatteryFilter {
 public:
  LowBatteryFilter(uint32_t thresholdMillivolts, uint8_t needed) : threshold_(thresholdMillivolts), needed_(needed) {}

  bool update(uint32_t millivolts, bool usbPresent) {
    if (usbPresent || millivolts >= threshold_) {
      lowCount_ = 0;
      return false;
    }
    if (lowCount_ < needed_) lowCount_++;
    return lowCount_ >= needed_;
  }

 private:
  uint32_t threshold_;
  uint8_t needed_;
  uint8_t lowCount_ = 0;
};
