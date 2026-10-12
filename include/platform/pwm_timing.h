// RP2040 PWM slice timing for a frequency: the fractional clock divider and the counter period.
// No Arduino dependencies, so the host unit tests (pio test -e native) cover it.
#pragma once
#include <stdint.h>

struct PwmTiming {
  uint32_t div16;  // clock divider in 1/16ths: 16..4095 = 1.0..255.9375
  uint32_t top;    // counter period in divided clock ticks (wrap + 1): 2..65536. 0 = frequency out of range
};

// The finest timing for freq: the smallest divider that fits one period in the 16-bit counter.
inline PwmTiming pwmTimingFor(uint32_t clockHz, uint32_t freq) {
  if (freq == 0) return {16, 0};
  const uint64_t clock16 = (uint64_t)clockHz * 16;  // the divider has 4 fractional bits
  uint64_t div16 = (clock16 + (uint64_t)freq * 65536 - 1) / ((uint64_t)freq * 65536);
  if (div16 < 16) div16 = 16;
  if (div16 > 4095) return {4095, 0};  // below ~8 Hz at 133 MHz
  const uint64_t top = (clock16 + div16 * freq / 2) / (div16 * freq);
  if (top < 2) return {16, 0};  // above half the clock
  return {(uint32_t)div16, (uint32_t)(top > 65536 ? 65536 : top)};
}

// The frequency a timing actually produces, rounded to the nearest Hz.
inline uint32_t pwmTimingFreq(uint32_t clockHz, PwmTiming t) {
  if (t.top == 0) return 0;
  const uint64_t ticks16 = (uint64_t)t.div16 * t.top;
  return (uint32_t)(((uint64_t)clockHz * 16 + ticks16 / 2) / ticks16);
}

// Counter compare level for an ESP32-style duty of `bits` resolution (duty == 1 << bits is fully on).
inline uint32_t pwmLevel(uint32_t duty, uint8_t bits, uint32_t top) {
  const uint32_t full = 1u << bits;
  if (duty > full) duty = full;
  return (uint32_t)(((uint64_t)duty * top + full / 2) >> bits);
}
