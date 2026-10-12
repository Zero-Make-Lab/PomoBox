#if (defined(POMOBOX_C6) || defined(POMOBOX_C3_USB))
#include "platform/compat.h"

namespace {
struct Channel {
  int pin = -1;
  uint32_t frequency = 2000;
  uint8_t bits = 8;
};
Channel channels[6];
}

uint32_t pomoLedcSetup(uint8_t c, uint32_t frequency, uint8_t bits) {
  if (c >= 6 || !frequency || !bits || bits > 14) return 0;
  auto &state = channels[c];
  const uint32_t actual = state.pin < 0 ? frequency : ledcChangeFrequency(state.pin, frequency, bits);
  if (!actual) return 0;
  state.frequency = actual;
  state.bits = bits;
  return actual;
}

void pomoLedcAttachPin(uint8_t pin, uint8_t c) {
  if (c >= 6) return;
  auto &state = channels[c];
  if (ledcAttachChannel(pin, state.frequency, state.bits, c)) {
    state.pin = pin;
    ledcWrite(pin, 0);
  }
}

void pomoLedcWrite(uint8_t c, uint32_t duty) {
  if (c < 6 && channels[c].pin >= 0) ledcWrite(channels[c].pin, duty);
}
uint32_t pomoLedcRead(uint8_t c) {
  return c < 6 && channels[c].pin >= 0 ? ledcRead(channels[c].pin) : 0;
}
uint32_t pomoLedcReadFreq(uint8_t c) {
  // A silent channel must restore silence after a click, not a 50% tone.
  return pomoLedcRead(c) ? ledcReadFreq(channels[c].pin) : 0;
}
double pomoLedcWriteTone(uint8_t c, double frequency) {
  if (c >= 6 || channels[c].pin < 0) return 0;
  if (frequency <= 0) { pomoLedcWrite(c, 0); return 0; }
  const uint32_t actual = pomoLedcSetup(c, static_cast<uint32_t>(frequency + 0.5), 10);
  if (actual) pomoLedcWrite(c, 0x1FF);
  return actual;
}
#endif
