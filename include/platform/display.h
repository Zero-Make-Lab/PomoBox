// The display. Zer0 rev C (RP2040): ER-TFT032A3-3, ST7789V 240x320. ESP32 devkit: the original 3.2" ILI9341 module.
// Both are Adafruit GFX drivers, so the apps draw the same way on both.
#pragma once
#include <Arduino.h>
#include <SPI.h>

#include "mirror_proto.h"

#if defined(ARDUINO_ARCH_RP2040)
#include <Adafruit_ST7789.h>
using PanelDriver = Adafruit_ST7789;
#else
#include <Adafruit_ILI9341.h>
using PanelDriver = Adafruit_ILI9341;
#endif

// The panel driver, plus the USB mirror (docs/specs/usb-mirror.md). Adafruit GFX ends all drawing in the eight
// primitives below; while a PC is listening (mirrorOut set) each one is also sent as a rect frame. The depth
// guard keeps a primitive the driver builds from another one from being sent twice.
class Display : public PanelDriver {
 public:
  using PanelDriver::PanelDriver;

  void (*mirrorOut)(const uint8_t *frame, size_t len) = nullptr;

  void mirrorPower(bool screenOn) {
    if (!mirrorOut) return;
    uint8_t f[MIRROR_FRAME_MAX];
    mirrorOut(f, mirrorFramePower(f, screenOn));
  }

  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    mirrorRect(x, y, 1, 1, c); depth++; PanelDriver::drawPixel(x, y, c); depth--;
  }
  void writePixel(int16_t x, int16_t y, uint16_t c) override {
    mirrorRect(x, y, 1, 1, c); depth++; PanelDriver::writePixel(x, y, c); depth--;
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override {
    mirrorRect(x, y, w, h, c); depth++; PanelDriver::fillRect(x, y, w, h, c); depth--;
  }
  void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override {
    mirrorRect(x, y, w, h, c); depth++; PanelDriver::writeFillRect(x, y, w, h, c); depth--;
  }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override {
    mirrorRect(x, y, w, 1, c); depth++; PanelDriver::drawFastHLine(x, y, w, c); depth--;
  }
  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override {
    mirrorRect(x, y, w, 1, c); depth++; PanelDriver::writeFastHLine(x, y, w, c); depth--;
  }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override {
    mirrorRect(x, y, 1, h, c); depth++; PanelDriver::drawFastVLine(x, y, h, c); depth--;
  }
  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override {
    mirrorRect(x, y, 1, h, c); depth++; PanelDriver::writeFastVLine(x, y, h, c); depth--;
  }

 private:
  uint8_t depth = 0;

  void mirrorRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
    if (!mirrorOut || depth) return;
    uint8_t f[MIRROR_FRAME_MAX];
    mirrorOut(f, mirrorFrameRect(f, x, y, w, h, c));
  }
};

extern Display tft;

// Starts SPI and the panel in landscape (320x240), with the rotation and color mode from config.h
void displayBegin();
