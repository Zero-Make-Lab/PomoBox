#pragma once
#include <stdint.h>
uint32_t ledcChangeFrequency(uint8_t pin, uint32_t frequency, uint8_t bits);
bool ledcAttachChannel(uint8_t pin, uint32_t frequency, uint8_t bits, uint8_t channel);
bool ledcWrite(uint8_t pin, uint32_t duty);
uint32_t ledcRead(uint8_t pin);
uint32_t ledcReadFreq(uint8_t pin);
