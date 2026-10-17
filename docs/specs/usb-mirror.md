# USB screen mirror

Owner request, 2026-10-17: open an address on the PC and see the PomoBox's screen live, and control it from
there. Transport is the USB cable that is already plugged in (Bluetooth and Wi-Fi were considered and dropped).

## What the owner gets

- Every app's Settings (hold the knob) has a **PC Mirror** row. Holding the knob on the app list, which did
  nothing before, opens a **general Settings** screen: Volume, PC Mirror, Turn Off.
- PC Mirror opens a screen showing the address, **zeromakelab.com/mirror**, and the link state
  ("Connected" or "Plug in USB and open the address"). Click or hold goes back.
- On the PC, Chrome or Edge opens zeromakelab.com/mirror. The first visit needs one **Connect** click to pick
  the port (the browser requires it); later visits reconnect by themselves.
- The page shows an exact copy of the screen, every app, as it is drawn. It controls the PomoBox like the
  knob: mouse wheel or arrow keys turn, a click or Enter clicks, holding the mouse button or H holds (opens
  Settings). A click on the page also switches the PomoBox on from the soft-off state, like the knob does.

## How it works

The apps draw through Adafruit GFX. All of its drawing ends in a few virtual primitives (pixel, filled
rectangle, horizontal and vertical line). `Display` becomes a wrapper around the panel driver that, while a PC
is listening, also sends each primitive over serial as a rectangle. The page replays them onto a 320x240
canvas. Nothing per app; any app drawn today or later is mirrored.

When no PC is listening the wrapper sends nothing, so drawing costs what it did.

## Link

- UART boards (the ESP32 devkit, the DIY board): **921600 baud** (was 115200), 8 KB transmit buffer so a
  full-screen redraw does not stall the panel. USB-CDC boards (C3, C6, RP2040) ignore the baud rate.
- Boot ROM output stays at the chip's own rate; the page ignores what it cannot decode.

## Protocol v1

### PomoBox to PC: binary frames

```
0xA5  type  len  payload[len]  check
```

`check` is the XOR of `type`, `len` and every payload byte. Integers are little-endian.

| type | name  | payload                                                          |
|------|-------|------------------------------------------------------------------|
| 'I'  | info  | u8 protocol version (1), u16 width, u16 height                   |
| 'R'  | rect  | i16 x, i16 y, i16 w, i16 h, u16 color (RGB565, as the app drew it) |
| 'P'  | power | u8 screen on (1) / off (0)                                       |

Bytes outside a frame are the firmware's text log (ASCII, so never 0xA5). The page shows it in a log panel.
A frame whose check fails is dropped and the reader resynchronises on the next 0xA5.

### PC to PomoBox: text lines

ASCII, newline-terminated, at most 31 characters; anything else is ignored.

| line      | meaning                                                                  |
|-----------|--------------------------------------------------------------------------|
| `hello`   | start mirroring: reply with `I`, `P`, then redraw the whole screen        |
| `ping`    | keep the session alive                                                   |
| `turn N`  | the knob turned N detents (signed, the firmware's own delta, -20..20)    |
| `click`   | a knob click                                                             |
| `hold`    | a knob long press                                                        |
| `bye`     | stop mirroring                                                           |

The session ends 3 s after the last line from the PC, or on `bye`. The page pings every second.

PC input goes through the same dispatch as the knob, with the same knob sounds.

## Where things live

- Firmware: `include/mirror_proto.h` (pure: frame encoding, line parsing, session timing; host-tested),
  `include/platform/display.h` (the mirroring wrapper), `src/main.cpp` (serial in, input injection).
- Page: `mirror/` in this repo is the source (`index.html`, `mirror.js`, decoder tests). The website gets a
  byte-identical copy at `public/mirror/` in Zero-Make-Lab/zeromakelabweb when it deploys.

## Tests

- Host (C++): frame bytes for each type, check byte, line parsing (valid, junk, overlong, out-of-range
  turns), session timeout.
- Page (Node): decoding frames, text interleaved between frames, a corrupted frame dropped with the next one
  still decoded, frames split across reads. A shared golden byte sequence is checked by both sides.
- Live: the page connected to the real PomoBox, every app mirrored, each control exercised, screenshots.

## Risks

- Opening the port can pulse the reset line on boards with auto-reset wiring. The page opens with DTR/RTS
  released; the live test checks a running timer survives connecting.
- Mirroring adds serial time to each draw while connected. Measured live; the 8 KB buffer absorbs bursts.

## Not in scope

Bluetooth, Wi-Fi, audio on the PC, reading pixels back from the panel.
