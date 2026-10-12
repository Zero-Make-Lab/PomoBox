#include "platform/display.h"

#include "config.h"

// The two drivers take their pins in a different order
#if defined(ARDUINO_ARCH_RP2040)
Display tft(&SPI, TFT_CS, TFT_DC, TFT_RST);

void displayBegin() {
  SPI.setSCK(TFT_SCK);
  SPI.setTX(TFT_MOSI);
  SPI.setRX(NOPIN);  // write-only: nothing is read back from the panel
  tft.init(240, 320);  // starts SPI and pulses the reset line
  tft.setSPISpeed(TFT_SPI_HZ);
  tft.setRotation(DISPLAY_ROTATION);
  tft.invertDisplay(DISPLAY_INVERT);
}

#else
Display tft(&SPI, TFT_DC, TFT_CS, TFT_RST);

void displayBegin() {
  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.begin(TFT_SPI_HZ);
  tft.setRotation(DISPLAY_ROTATION);
  tft.invertDisplay(DISPLAY_INVERT);
}
#endif
