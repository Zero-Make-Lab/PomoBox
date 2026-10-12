// Host contract test against Arduino 3.x's PIN-based LEDC API.
#include <cassert>
#include <cstdio>
#if !defined(POMOBOX_C3_USB)
#define POMOBOX_C6
#endif
#include "../../../src/platform/compat_c6.cpp"

struct Pin { bool attached = false; uint32_t freq = 0, duty = 0; uint8_t bits = 0; } pins[32];
static bool failChange = false;
uint32_t ledcChangeFrequency(uint8_t p, uint32_t f, uint8_t b) {
  assert(p < 32 && pins[p].attached);
  if (failChange) return 0;
  pins[p].freq = f; pins[p].bits = b; return f;
}
bool ledcAttachChannel(uint8_t p, uint32_t f, uint8_t b, uint8_t c) {
  assert(p < 32 && c < 6); pins[p] = {true, f, 0, b}; return true;
}
bool ledcWrite(uint8_t p, uint32_t d) { assert(pins[p].attached); pins[p].duty = d; return true; }
uint32_t ledcRead(uint8_t p) { assert(pins[p].attached); return pins[p].duty; }
uint32_t ledcReadFreq(uint8_t p) { assert(pins[p].attached); return pins[p].freq; }

int main() {
  assert(pomoLedcReadFreq(0) == 0);  // unassigned is silent
  assert(pomoLedcSetup(0, 2000, 8) == 2000);
  pomoLedcAttachPin(3, 0);           // channel zero maps to GPIO3, never GPIO0
  assert(pins[3].attached && !pins[0].attached);
  assert(pomoLedcReadFreq(0) == 0);
  assert(pomoLedcWriteTone(0, 440) == 440);
  assert(pins[3].bits == 10 && pomoLedcRead(0) == 511);
  pomoLedcWrite(0, 230);
  const auto frequency = pomoLedcReadFreq(0), duty = pomoLedcRead(0);
  assert(pomoLedcSetup(0, 62500, 8) == 62500);
  pomoLedcWrite(0, 128);
  assert(pins[3].bits == 8 && pins[3].freq == 62500);
  pomoLedcWriteTone(0, frequency); pomoLedcWrite(0, duty);
  assert(pins[3].bits == 10 && pins[3].freq == 440 && pins[3].duty == 230);
  pomoLedcWriteTone(0, 0);
  assert(pomoLedcReadFreq(0) == 0 && pins[3].duty == 0);
  failChange = true;
  assert(pomoLedcSetup(0, 1000, 8) == 0 && pins[3].freq == 440);
  assert(pomoLedcSetup(6, 2000, 8) == 0);
  assert(pomoLedcSetup(0, 0, 8) == 0);
  assert(pomoLedcSetup(0, 2000, 15) == 0);
  pomoLedcWrite(9, 123);  // out of range must not call Arduino with an invalid pin
  assert(pomoLedcRead(9) == 0 && pomoLedcReadFreq(9) == 0);
  puts("C6 PWM adapter: pin mapping, tone/PCM restoration, silence and failure cases passed");
}
