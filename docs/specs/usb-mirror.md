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
- The page shows an exact copy of the screen, every app, as it is drawn, inside the website simulator's 3D
  PomoBox. It controls the PomoBox like the knob, with the simulator's controls: Turn left / Press / Turn
  right / Hold buttons; the 3D knob (click presses, a long press holds, the mouse wheel over it turns); and,
  on the focused PomoBox, the arrow keys turn, Enter or Space presses and Escape holds. The wheel turns only
  over the knob, so it still scrolls the page elsewhere. A press from the page also switches the PomoBox on
  from the soft-off state, like the knob does.

## Opening it from the PomoBox (owner, 2026-10-17)

Choosing **PC Mirror** in Settings opens the page on the PC by itself: one click, no typing the address.

- Over USB serial the PomoBox can only talk to the program holding the port, so a small **PomoBox helper** runs on
  the PC (`tools/pc-helper/pomobox_helper.py`, Python + pyserial). While no page is connected it holds the port
  (DTR and RTS released, so the board is never reset) and listens.
- Opening the PC Mirror screen prints the log line `@pomobox open-mirror` (`MIRROR_OPEN_REQUEST`). The helper lets go
  of the port and opens the page in the default browser; the page connects to the port it was allowed before. When
  the page lets go, the helper listens again.
- The screen shows Opening on your PC, then Connected. With no page after 15 s it says nobody answered: start the
  helper, or open the address by hand. A click asks again.
- Starting the helper with Windows is the owner's call (a persistent change to the PC); on the owner's PC it starts
  silently with `pythonw` from the Startup folder and logs with `--log` (2026-10-17). The first visit to the page
  still needs one Connect click: browsers only remember a port after the person picks it once.
- Anything else that needs the port asks the helper to step aside: `POST http://127.0.0.1:47321/release` (this
  computer only; from a browser, only zeromakelab.com and local previews). The helper lets go and stays off the
  port for 15 s after each ask. zeroFlash and the mirror page ask before opening the port and every 5 s while
  they hold it (the website's `public/flash/helper.js`), so flashing works with the helper running. Command-line
  flashers: run `python tools/pc-helper/pomobox_helper.py --release` first (it lasts 15 s).
- Only one helper runs: a second one finds the release port taken and exits.

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
- Page: in Zero-Make-Lab/zeromakelabweb, because it reuses the simulator's 3D view and the site's styles:
  `public/mirror.html`, `mirror.js`, `mirror-protocol.js` (the decoder), `mirror.css`, and
  `tests/mirror-protocol.test.mjs`. `mirror/golden.hex` here is the contract fixture; the website keeps a
  byte-identical copy at `tests/fixtures/mirror-golden.hex`. Change both together.

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
