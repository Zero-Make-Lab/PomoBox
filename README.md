# PomoBox

A minimalist Pomodoro-style focus timer built with ESP32 and ILI9341 TFT display.

![PomoBox](https://img.shields.io/badge/Platform-ESP32-blue) ![License](https://img.shields.io/badge/License-MIT-green)

## Features

- **Focus/Break Modes** - 50 min focus, 25 min break (customizable)
- **Apple-style Dark UI** - Clean minimalist design with icons
- **Rotary Encoder Control** - CW for Break, CCW for Focus, Click to pause
- **Auto-Continue** - Automatically cycles between Focus and Break
- **Sound Feedback** - Shared demo click samples on turn, press and release, plus melodies for state changes
- **Volume Control** - Mute, Low, Medium, High
- **Stats Tracking** - Track total focus time in hours/minutes

## Hardware

- ESP32-WROOM-32E (NodeMCU devkit)
- 3.2" ILI9341 SPI TFT Display (320x240)
- KY-040 Rotary Encoder
- Passive Buzzer

## Wiring

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
# Build
pio run

# Upload
pio run --target upload
```

## Controls

The knob clicks use the same generated PCM samples as the browser demo: a
27 ms turn click and a 45 ms press/release click. The existing passive buzzer
pin plays these through a 62.5 kHz PWM carrier; no wiring changes are needed.
Volume and mute apply to the clicks too. `node scripts/generate-knob-audio.mjs`
rebuilds the sample header from the demo's original noise/filter/decay profile.
Physical timbre and loudness still depend on the buzzer; these changes have
been built and simulated but have not been auditioned on a device.

- **Rotate CW** → Switch to Break mode
- **Rotate CCW** → Switch to Focus mode  
- **Click** → Pause/Resume timer
- **Long Press** → Open Settings menu

## Settings Menu

- Focus Duration (25-60 min)
- Break Duration (5-25 min)
- Auto Continue (On/Off)
- Volume (High/Med/Low/Mute)
- Focus Time (stats, click to reset)

## License

MIT License
