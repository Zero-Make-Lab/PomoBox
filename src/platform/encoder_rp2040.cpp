#if defined(ARDUINO_ARCH_RP2040) || (defined(POMOBOX_C6) || defined(POMOBOX_C3_USB))
#include "platform/encoder.h"

#include "platform/quadrature.h"

puType ESP32Encoder::useInternalWeakPullResistors = puType::down;  // the library's default
ESP32Encoder *ESP32Encoder::active_ = nullptr;

void ESP32Encoder::attachHalfQuad(int aPin, int bPin) {
  a_ = aPin;
  b_ = bPin;
  auto mode = INPUT;
  if (useInternalWeakPullResistors == puType::up) mode = INPUT_PULLUP;
  if (useInternalWeakPullResistors == puType::down) mode = INPUT_PULLDOWN;
  pinMode(a_, mode);
  pinMode(b_, mode);
  state_ = (digitalRead(a_) << 1) | digitalRead(b_);
  active_ = this;
  attachInterrupt(digitalPinToInterrupt(a_), onEdge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(b_), onEdge, CHANGE);
}

void ESP32Encoder::clearCount() {
  noInterrupts();
  count_ = 0;
  interrupts();
}

int64_t ESP32Encoder::getCount() {
  noInterrupts();
  const int64_t c = count_;
  interrupts();
  return c;
}

void ESP32Encoder::onEdge() {
  ESP32Encoder *e = active_;
  if (!e) return;
  const uint8_t cur = (digitalRead(e->a_) << 1) | digitalRead(e->b_);
  const uint8_t prev = e->state_;
  e->state_ = cur;
  e->count_ += halfQuadStep(prev, cur);
}
#endif
