// The display. Zer0 rev C (RP2040): ER-TFT032A3-3, ST7789V 240x320. ESP32 devkit: the original 3.2" ILI9341 module.
// Both are Adafruit GFX drivers, so the apps draw the same way on both.
#pragma once
#include <Arduino.h>
#include <SPI.h>

#if defined(ARDUINO_ARCH_RP2040)
#include <Adafruit_ST7789.h>
using Display = Adafruit_ST7789;
#else
#include <Adafruit_ILI9341.h>
using Display = Adafruit_ILI9341;
#endif

extern Display tft;

// Starts SPI and the panel in landscape (320x240), with the rotation and color mode from config.h
void displayBegin();
