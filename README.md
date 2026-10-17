# PomoBox

A minimalist focus timer: Pomodoro, Quick Timer, Stopwatch, Counter, Dice and Metronome on a 3.2" screen, driven by
one knob. This repository is the firmware for the **PomoBox kit** (ESP32-C3 SuperMini, USB-C powered) and for the
**original breadboard build** (ESP32 devkit).

![Platform](https://img.shields.io/badge/Platform-ESP32--C3%20%7C%20ESP32-blue) ![License](https://img.shields.io/badge/License-MIT-green)

## Features

- **Apps** - Pomodoro, Quick Timer, Stopwatch, Counter, Dice and Metronome, from a launcher
- **Focus/Break Modes** - 50 min focus, 25 min break (customizable)
- **Apple-style Dark UI** - Clean minimalist design with icons
- **Rotary Encoder Control** - Turn to navigate, click to act, hold for the menu
- **Auto-Continue** - Automatically cycles between Focus and Break
- **Sound Feedback** - Shared demo click samples on turn, press and release, plus melodies for state changes
- **Volume Control** - Mute, Low, Medium, High
- **Stats Tracking** - Track total focus time in hours/minutes

## Install without building

[zeroFlash](https://zeromakelab.com/zeroflash) installs PomoBox from the browser over USB (Chrome or Edge on a
computer). Choose your board: **PomoBox kit · ESP32-C3 SuperMini** or **ESP32-WROOM devkit · ILI9341**.

## The PomoBox kit

The solder-it-yourself kit: every part the builder solders is through-hole.

| Part | |
|------|---|
| Controller | ESP32-C3 SuperMini (ESP32-C3 FN4, 4 MB flash) on two 8-pin headers. Its USB-C is the only power input and the flashing port |
| Display | MSP3217: 3.2" 240x320 ILI9341, 4-wire SPI, on a 14-pin socket, run from 3.3 V (bridge the display's own J1) |
| Knob | Bourns PEC12R-2230F-S0024 side-shaft encoder (30 mm shaft) with push switch, 10k pull-ups and 10 nF filters |
| Sound | 12 mm passive buzzer through an SS8050 transistor |
| Power | The USB-C cable. No battery |

The pins are in `include/config.h`, block `POMOBOX_C3_USB` (GPIO numbers):

| Function | GPIO |
|----------|-----:|
| Display SCK / MOSI / DC / CS / reset / backlight | 4 / 5 / 6 / 2 / 21 / 20 |
| Knob A / B / switch | 0 / 1 / 3 |
| Buzzer | 7 |

- **On:** plug in the USB-C cable, then press the knob.
- **Off:** in any app, hold the knob for its menu and choose **Turn Off**. The settings are saved and the screen goes
  dark; a press turns it back on. Unplugging the cable switches it off completely.

The kit firmware builds and its board logic is unit-tested, but it has not run on a built kit yet. If the picture is
upside down set `DISPLAY_ROTATION` to 1; if the colours look inverted set `DISPLAY_INVERT` to true; if the knob turns
the wrong way swap `ENC_CLK` and `ENC_DT`.

## The original breadboard build

- ESP32-WROOM-32E (NodeMCU devkit)
- 3.2" ILI9341 SPI TFT Display (320x240)
- KY-040 Rotary Encoder
- Passive Buzzer

### Display (ILI9341)
| Signal | ESP32 GPIO |
|--------|------------|
| VCC | 5V |
| GND | GND |
| CS | GPIO 5 |
| DC | GPIO 16 |
| RST | GPIO 27 |
| MOSI | GPIO 23 |
| SCK | GPIO 18 |
| MISO | GPIO 19 |
| LED | 3.3V/5V |

### Encoder (KY-040)
| Signal | ESP32 GPIO |
|--------|------------|
| + | 3.3V |
| GND | GND |
| CLK | GPIO 32 |
| DT | GPIO 33 |
| SW | GPIO 25 |

### Buzzer
| Pin | ESP32 GPIO |
|-----|------------|
| + | GPIO 26 |
| - | GND |

## Build & Upload

This project uses [PlatformIO](https://platformio.org/).

```bash
# The kit (the default target)
pio run -e pomobox_c3_usb
pio run -e pomobox_c3_usb --target upload   # first time: hold BOOT while plugging in the USB-C

# The original breadboard build
pio run -e esp32dev --target upload

# Host unit tests for the board layer (needs gcc on PATH)
pio test -e native
```

`platformio.ini` also keeps earlier prototype boards (an RP2040 board, a battery-powered ESP32-C6 kit and an ESP32
carrier); none of them is the current kit. `include/platform/` and `src/platform/` hold the small board layer that
gives every target the same ESP32 APIs the apps use (buzzer PWM, knob decoding, settings storage, power).

## Controls

The knob clicks use the same generated PCM samples as the browser demo: a
27 ms turn click and a 45 ms press/release click. The buzzer plays these
through a 62.5 kHz PWM carrier. Volume and mute apply to the clicks too.
`node scripts/generate-knob-audio.mjs` rebuilds the sample header from the
demo's original noise/filter/decay profile. Physical timbre and loudness
still depend on the buzzer.

- **Rotate** → Browse apps or change a value
- **Click** → Open an app or act (in Pomodoro: pause/resume)
- **Long Press** → Open the app's Settings menu

## Settings Menu

- Focus Duration (25-60 min)
- Break Duration (5-25 min)
- Auto Continue (On/Off)
- Volume (High/Med/Low/Mute)
- Focus Time (stats, click to reset)
- Switch App
- Turn Off

## License

MIT License
