// Rotary encoder. Original ESP32: hardware pulse counter. RP2040 and ESP32-C6:
// the same application API backed by a pin-change quadrature decoder.
#pragma once

#if defined(ARDUINO_ARCH_RP2040) || (defined(POMOBOX_C6) || defined(POMOBOX_C3_USB))
#include <Arduino.h>

enum class puType { up, down, none };

class ESP32Encoder {
 public:
  static puType useInternalWeakPullResistors;

  // Half quadrature: counts both edges of A, as the ESP32 pulse counter does (see platform/quadrature.h)
  void attachHalfQuad(int aPin, int bPin);
  // ESP32 glitch filter. Not needed here: the decoder tracks the A/B state, so a bounce that comes back to the
  // same state counts nothing and a bounce that is seen counts +1 then -1.
  void setFilter(uint16_t value) { (void)value; }
  void clearCount();
  int64_t getCount();

 private:
  static void onEdge();
  static ESP32Encoder *active_;
  int a_ = -1, b_ = -1;
  volatile uint8_t state_ = 0;
  volatile int64_t count_ = 0;
};

#else
#include <ESP32Encoder.h>
#endif
