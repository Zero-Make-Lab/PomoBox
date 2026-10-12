// The ESP32-only Arduino calls PomoBox makes, provided on the RP2040 so the app code is the same on both chips.
// On the ESP32 this header adds nothing.
#pragma once
#include <Arduino.h>

#if defined(ARDUINO_ARCH_RP2040)

// Hardware random number (the dice)
inline uint32_t esp_random() { return rp2040.hwrand32(); }

// ESP32 LEDC PWM (Arduino-ESP32 2.x API) on the RP2040 PWM slices, one pin per channel. Same behavior as the ESP32
// core, including ledcWriteTone() switching the channel to 10-bit duty at 50% and ledcReadFreq() reading 0 while the
// duty is 0: playTone() and the knob clicks depend on both.
uint32_t ledcSetup(uint8_t channel, uint32_t freq, uint8_t resolutionBits);
void ledcAttachPin(uint8_t pin, uint8_t channel);
void ledcWrite(uint8_t channel, uint32_t duty);
uint32_t ledcRead(uint8_t channel);
uint32_t ledcReadFreq(uint8_t channel);
double ledcWriteTone(uint8_t channel, double freq);

#endif

// The application uses channels on every board. Arduino-ESP32 3.x uses pins.
#if (defined(POMOBOX_C6) || defined(POMOBOX_C3_USB))
#include <esp_random.h>
uint32_t pomoLedcSetup(uint8_t channel, uint32_t freq, uint8_t bits);
void pomoLedcAttachPin(uint8_t pin, uint8_t channel);
void pomoLedcWrite(uint8_t channel, uint32_t duty);
uint32_t pomoLedcRead(uint8_t channel);
uint32_t pomoLedcReadFreq(uint8_t channel);
double pomoLedcWriteTone(uint8_t channel, double freq);
#else
inline uint32_t pomoLedcSetup(uint8_t c, uint32_t f, uint8_t b) { return ledcSetup(c, f, b); }
inline void pomoLedcAttachPin(uint8_t p, uint8_t c) { ledcAttachPin(p, c); }
inline void pomoLedcWrite(uint8_t c, uint32_t d) { ledcWrite(c, d); }
inline uint32_t pomoLedcRead(uint8_t c) { return ledcRead(c); }
inline uint32_t pomoLedcReadFreq(uint8_t c) { return ledcReadFreq(c); }
inline double pomoLedcWriteTone(uint8_t c, double f) { return ledcWriteTone(c, f); }
#endif
