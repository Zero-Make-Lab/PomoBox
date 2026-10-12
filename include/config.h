#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>

// Board layer: on the RP2040 it supplies the ESP32 APIs the apps use (encoder, settings, LEDC, esp_random)
#include "platform/compat.h"
#include "platform/display.h"
#include "platform/encoder.h"
#include "platform/power.h"
#include "platform/prefs.h"

// =============================================================================
// Hardware Pin Definitions
// =============================================================================

#if defined(ARDUINO_ARCH_RP2040)
// Zer0 rev C board: RP2040, ER-TFT032A3-3 (ST7789V) on SPI0, EN11 encoder, buzzer driver, power latch.
// README.md has the full pin map.

static constexpr int TFT_CS   = 1;
static constexpr int TFT_DC   = 3;
static constexpr int TFT_RST  = 8;
static constexpr int TFT_MOSI = 7;   // SPI0 TX. Write-only SPI: the panel's SDO isn't wired
static constexpr int TFT_SCK  = 2;   // SPI0 SCK
static constexpr int TFT_LED  = 11;  // backlight switch (AO3400A), high = on

static constexpr int ENC_CLK  = 29;  // EN11 A
static constexpr int ENC_DT   = 28;  // EN11 B
static constexpr int ENC_SW   = 27;

static constexpr int BUZZER_PIN     = 5;   // through the SS8050 driver
static constexpr int BUZZER_CHANNEL = 0;

static constexpr int PWR_HOLD   = 21;  // high: keep the regulator on after the encoder press switched it on
static constexpr int VBUS_SENSE = 18;  // high while USB is plugged in
static constexpr int VBAT_SENSE = 26;  // ADC0: battery through a 330k / 100k divider
static constexpr uint32_t VBAT_DIVIDER_TOP = 330, VBAT_DIVIDER_BOTTOM = 100;  // kOhm

// The RP2040 runs SPI at the nearest step at or below this: 33 MHz at the 133 MHz system clock
static constexpr uint32_t TFT_SPI_HZ = 40000000;
// The panel sits with its flex on the left (knob on the right). If the picture is upside down, use 1.
static constexpr uint8_t DISPLAY_ROTATION = 3;
// TN panel: colors are right without inversion (an IPS ST7789 panel would need it)
static constexpr bool DISPLAY_INVERT = false;

#elif defined(POMOBOX_C3_USB)
// ESP32-C3 SuperMini (USB kit). Straps 8/9 (LED, BOOT) and GPIO10 are untouched; USB 18/19 are not on its pins.
// GPIO2 CS has an external 10k pull-up. No panel readback or battery circuitry.
static constexpr int TFT_CS = 2;
static constexpr int TFT_DC = 6;
static constexpr int TFT_RST = 21;  // UART ROM chatter ends before the explicit display reset
static constexpr int TFT_MOSI = 5;
static constexpr int TFT_SCK = 4;
static constexpr int TFT_MISO = -1;
static constexpr int TFT_LED = 20;
static constexpr int ENC_CLK = 0;
static constexpr int ENC_DT = 1;
static constexpr int ENC_SW = 3;
static constexpr int BUZZER_PIN = 7;
static constexpr int BUZZER_CHANNEL = 0;
static constexpr uint32_t TFT_SPI_HZ = 8000000;
static constexpr uint8_t DISPLAY_ROTATION = 3;
static constexpr bool DISPLAY_INVERT = false;

#elif defined(POMOBOX_C6)
// DIY rev C. GPIO numbers, not DFRobot's D/A aliases. Avoid boot straps 8/9/15
// and native USB 12/13. The onboard battery divider already occupies GPIO0.
static constexpr int TFT_CS = 1;
static constexpr int TFT_DC = 16;
static constexpr int TFT_RST = 14;
static constexpr int TFT_MOSI = 22;
static constexpr int TFT_SCK = 23;
static constexpr int TFT_MISO = 21;
static constexpr int TFT_LED = 17;
static constexpr int ENC_CLK = 6;
static constexpr int ENC_DT = 7;
static constexpr int ENC_SW = 2;  // LP GPIO: external 47k pull-up, low wakes from deep sleep
static constexpr int BUZZER_PIN = 3;
static constexpr int BUZZER_CHANNEL = 0;
static constexpr int PERIPHERAL_OFF = 18;  // Q6 PNP load switch: high or floating = off, low = on
static constexpr int VBAT_SENSE = 0;
static constexpr uint32_t VBAT_DIVIDER_TOP = 1000, VBAT_DIVIDER_BOTTOM = 1000;
static constexpr uint32_t TFT_SPI_HZ = 8000000;
static constexpr uint8_t DISPLAY_ROTATION = 3;
static constexpr bool DISPLAY_INVERT = false;

#else
// The original PomoBox: ESP32-WROOM-32E devkit, 3.2" ILI9341 module, KY-040 encoder, passive buzzer.
// The Zer0 DIY board (env zer0_diy) has the same pins, plus the battery and power latch below.

static constexpr int TFT_CS   = 5;
static constexpr int TFT_DC   = 16;
static constexpr int TFT_RST  = 27;
static constexpr int TFT_MOSI = 23;
static constexpr int TFT_SCK  = 18;
static constexpr int TFT_MISO = 19;
static constexpr int TFT_LED  = 4;

static constexpr int ENC_CLK  = 32;
static constexpr int ENC_DT   = 33;
static constexpr int ENC_SW   = 25;

static constexpr int BUZZER_PIN     = 26;
static constexpr int BUZZER_CHANNEL = 0;

static constexpr uint32_t TFT_SPI_HZ = 8000000;
static constexpr uint8_t DISPLAY_ROTATION = 3;
static constexpr bool DISPLAY_INVERT = false;

#if defined(ZER0_DIY)
// Zer0 DIY board: an Adafruit Micro-Lipo module charges the battery from the DevKitC's USB-C, and a press of the knob
// switches it on.
static constexpr int PWR_HOLD   = 22;  // high: keep the battery switched on after the knob press switched it on
static constexpr int VBUS_SENSE = 35;  // ADC: the DevKitC's 5 V pin through a 100k / 100k divider
static constexpr int VBAT_SENSE = 34;  // ADC: the battery through a 1M / 330k divider

static constexpr uint32_t VBUS_DIVIDER_TOP = 100, VBUS_DIVIDER_BOTTOM = 100;   // kOhm
static constexpr uint32_t VBAT_DIVIDER_TOP = 1000, VBAT_DIVIDER_BOTTOM = 330;  // kOhm
#endif

#endif

#if defined(ARDUINO_ARCH_RP2040) || defined(ZER0_DIY)
// A battery and the press-the-knob power latch: Zer0 rev C and the Zer0 DIY board, the same way on both
#define ZER0_POWER_LATCH 1
#endif

#if defined(ZER0_POWER_LATCH) || defined(POMOBOX_C6)
// Save and switch off below this, with a few percent left in the cell. On rev C it also sits above the point where
// the 3.3 V regulator drops out: below that the RP2040's ADC reference sinks with the battery and the reading stops
// falling. The ESP32's ADC has its own reference.
static constexpr uint32_t VBAT_CRITICAL_MV       = 3400;
static constexpr uint8_t  VBAT_CRITICAL_READINGS = 3;     // in a row,
static constexpr uint32_t VBAT_CHECK_MS          = 2000;  // this far apart
#endif

// =============================================================================
// Colors (Apple-inspired Dark Mode)
// =============================================================================

static constexpr uint16_t COLOR_BG              = 0x0841;  // Near black
static constexpr uint16_t COLOR_SURFACE         = 0x18E3;  // Dark gray card
static constexpr uint16_t COLOR_TEXT            = 0xFFFF;  // White
static constexpr uint16_t COLOR_TEXT_DIM        = 0x7BEF;  // Gray
static constexpr uint16_t COLOR_ACCENT_FOCUS    = 0x2D7F;  // Soft blue
static constexpr uint16_t COLOR_ACCENT_BREAK    = 0x2E8B;  // Soft green
static constexpr uint16_t COLOR_ACCENT_PAUSE    = 0xFB80;  // Soft orange
static constexpr uint16_t COLOR_ACCENT_QTIMER   = 0xFD20;  // Warm amber
static constexpr uint16_t COLOR_ACCENT_STOPWATCH = 0x07FF; // Cyan
static constexpr uint16_t COLOR_RED              = 0xF800;  // Red
static constexpr uint16_t COLOR_ACCENT_COUNTER  = 0x07C0;  // Green
static constexpr uint16_t COLOR_ACCENT_DICE     = 0xB81F;  // Purple
static constexpr uint16_t COLOR_ACCENT_METRO    = 0xFC10;  // Salmon pink

// =============================================================================
// Timing Constants
// =============================================================================

static constexpr uint32_t LONG_PRESS_MS    = 800;
static constexpr int32_t  COUNTS_PER_DETENT = 2;
