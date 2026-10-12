#if defined(ARDUINO_ARCH_RP2040)
#include "platform/compat.h"

#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>

#include "platform/pwm_timing.h"

namespace {
constexpr uint8_t CHANNELS = 16;

struct Channel {
  int pin = -1;
  uint8_t bits = 8;   // duty resolution the caller works in
  uint32_t duty = 0;  // last duty written, at that resolution
  PwmTiming timing{16, 0};
};
Channel channels[CHANNELS];

void apply(const Channel &c) {
  if (c.pin < 0) return;
  if (c.timing.top == 0) {
    pwm_set_gpio_level(c.pin, 0);
    return;
  }
  const uint slice = pwm_gpio_to_slice_num(c.pin);
  pwm_set_clkdiv_int_frac(slice, c.timing.div16 >> 4, c.timing.div16 & 15);
  pwm_set_wrap(slice, c.timing.top - 1);
  pwm_set_gpio_level(c.pin, pwmLevel(c.duty, c.bits, c.timing.top));
  pwm_set_enabled(slice, true);
}
}  // namespace

uint32_t ledcSetup(uint8_t channel, uint32_t freq, uint8_t resolutionBits) {
  if (channel >= CHANNELS || resolutionBits == 0 || resolutionBits > 16) return 0;
  const PwmTiming t = pwmTimingFor(clock_get_hz(clk_sys), freq);
  if (t.top == 0) return 0;
  Channel &c = channels[channel];
  c.timing = t;
  c.bits = resolutionBits;
  apply(c);
  return pwmTimingFreq(clock_get_hz(clk_sys), t);
}

void ledcAttachPin(uint8_t pin, uint8_t channel) {
  if (channel >= CHANNELS) return;
  Channel &c = channels[channel];
  c.pin = pin;
  gpio_set_function(pin, GPIO_FUNC_PWM);
  apply(c);
}

void ledcWrite(uint8_t channel, uint32_t duty) {
  if (channel >= CHANNELS) return;
  Channel &c = channels[channel];
  c.duty = duty;
  apply(c);
}

uint32_t ledcRead(uint8_t channel) { return channel < CHANNELS ? channels[channel].duty : 0; }

uint32_t ledcReadFreq(uint8_t channel) {
  if (channel >= CHANNELS || channels[channel].duty == 0) return 0;
  return pwmTimingFreq(clock_get_hz(clk_sys), channels[channel].timing);
}

double ledcWriteTone(uint8_t channel, double freq) {
  if (freq <= 0) {
    ledcWrite(channel, 0);
    return 0;
  }
  const uint32_t actual = ledcSetup(channel, (uint32_t)(freq + 0.5), 10);
  if (actual == 0) return 0;
  ledcWrite(channel, 0x1FF);
  return actual;
}
#endif
