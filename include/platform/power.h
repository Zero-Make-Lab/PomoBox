// Power. Legacy RP2040 and ESP32 DevKitC carriers use a hardware latch.
// FireBeetle C6 switches peripherals off and enters deep sleep, waking on GPIO2.
// Its onboard charger and regulator remain connected; off is not battery isolation.
// While USB is plugged in, USB keeps the board powered: off is then the original soft off, until USB is unplugged.
// The plain ESP32 devkit build: always powered, soft off.
#pragma once

void powerHold();             // enable board/peripheral power before initializing interfaces
void powerRelease();          // release legacy latch, or deep sleep on C6 (does not return)
bool powerStartedByButton();  // this boot came from the encoder press, not from USB being plugged in or an upload
bool powerBatteryCritical();  // filtered low battery; C6 also stops on USB with a depleted cell
