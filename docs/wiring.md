# Wiring

The "USB" port on the indoor unit is the socket for MRCOOL's SmartHVAC Wi-Fi dongle. It isn't USB. It's a serial (UART) link at 9600 baud that uses the Midea protocol, with power and data on the four USB pins. The controller replaces the dongle: it takes power from the port and talks to the unit over the two data pins.

| USB pin | Cable wire | Function at the AC |
|---|---|---|
| 1 | red | +5 V (VBUS) |
| 2 | white | D− = data |
| 3 | green | D+ = data |
| 4 | black | GND |

There are two ways to connect:

- **Option A: ESP32-C3, single cable.** No soldering. The C3's own USB socket becomes the AC link. It only works if the AC's data lines are 3.3 V, so measure them first. The C5, C6 and S3 can do the same; see [Other chips](#other-chips-c5-c6-s3).
- **Option B: wired, with a level shifter.** A USB plug and a level shifter wired to GPIO pins. It works with any ESP32, whatever the line voltage. The original ESP32 (NodeMCU-32S, DevKit) has to use this option.

Sources disagree on which data line carries which direction. The firmware handles that in both options: if the unit stays silent for 8 seconds, it swaps RX and TX in software, and it remembers the orientation that works.

## Option A: ESP32-C3 through its USB socket

### 1. Measure the AC's data lines

ESPHome documents these lines as 5 V logic. At least one user reports 3.3 V. The C3's pins are rated for at most 3.6 V, and in this option nothing sits between them and the AC. **Check your unit before plugging the C3 in.**

1. Get access to the port's pins: a USB-A male breakout board, or an old USB cable with its far end cut and the wires stripped.
2. Remove the original dongle, plug in the breakout, and turn the mini-split on at the breaker. It can stay off on the remote.
3. Set a multimeter to DC volts, and put the black probe on **GND** (pin 4, black wire). Measure **D+** (pin 3, green), then **D−** (pin 2, white).

| Reading on both lines | Meaning |
|---|---|
| About 3.3 V or less | Option A is within the chip's ratings. Go ahead. |
| About 5 V on either line | **Don't connect the C3 directly.** Wire it with a level shifter as in Option B (`AC_LINK_ON_USB_PORT 0`, GPIO4/5), or use the NodeMCU-32S. |

A data line that idles high shows its logic level on a meter, so a 5 V reading means 5 V signals.

### 2. Parts

| Part | Notes |
|---|---|
| ESP32-C3 board whose USB-C socket is native USB | For example the ESP32-C3 SuperMini, Seeed XIAO ESP32C3, or LOLIN C3 Mini. The socket must connect straight to GPIO18/19. Boards that route USB through a USB-to-serial chip, such as Espressif's ESP32-C3-DevKitM-1 (CP2102), can only use Option B. |
| USB-A male to USB-C male **data** cable | Charge-only cables have no data wires and won't work. Short is better. |

### 3. Connect

Plug the cable into the AC's port and the C3. That's all. The AC powers the board through VBUS, and the C3's D−/D+ pins (GPIO18/19) carry the UART.

When a computer is attached at power-up, the firmware leaves the socket as a USB serial console. Anything else, the AC included, gets the UART. One socket means the board can't be powered from the computer and the AC at the same time.

## Option B: wired, with a level shifter

### Parts

| Part | Notes |
|---|---|
| ESP32 dev board | NodeMCU-32S or any ESP32-WROOM-32 DevKit. On WROVER boards GPIO16/17 drive PSRAM, so use 25/26. Newer chips work too; see the pin table under [Other chips](#other-chips-c5-c6-s3). |
| USB-A male plug | Cut the A end off an old USB cable, or use a USB-A breakout plug. |
| Bidirectional logic level converter | 4-channel BSS138 module with HV/LV sides (5 V ↔ 3.3 V). |
| 470 µF capacitor, 10 V or higher (optional) | Across 5V–GND at the ESP32. It smooths the current spikes when Wi-Fi transmits. |
| Small enclosure, heat-shrink | The board sits next to the indoor unit, so relieve strain on the cable. |

### Connections

```
  AC "USB" plug          Level shifter (BSS138)          ESP32 / NodeMCU-32S
  ─────────────          ──────────────────────          ───────────────────
  1 VBUS (red)  ───┬──── HV
                   └─────────────────────────────────── 5V / VIN
                         LV ─────────────────────────── 3V3
  4 GND (black) ──────── GND ────────────────────────── GND
  3 D+  (green) ──────── HV1 ⇄ LV1 ──────────────────── GPIO17  (ESP TX → AC)
  2 D−  (white) ──────── HV2 ⇄ LV2 ──────────────────── GPIO16  (ESP RX ← AC)
```

Most modules have a GND pin on each side that's connected internally. Wire at least one of them to the common ground.

The starting orientation follows the [MRCOOL write-up from Digital Forge Systems](https://www.digitalforgesystems.com/2024/09/removing-the-chinese-wifi-from-midea-mr-cool-minisplit-with-esphome/) (ESP TX → D+, ESP RX → D−). If your unit is the other way round, the auto-swap finds it. The pins are set in [config.h](../firmware/mrcool_homekit/config.h) (`AC_UART_RX_PIN`, `AC_UART_TX_PIN`).

### Power: computer vs. air conditioner

The ESP32 can draw power from the AC port (VBUS → 5V pin) or from its own USB socket when it's plugged into your computer. **Don't use both at once.** That ties the AC's 5 V supply to your computer's.

- Do the first flash, Wi-Fi setup and pairing on your desk, with the board powered from your computer only.
- Once it's installed, update over Wi-Fi with `make ota HOST=...`.
- If you need USB on an installed unit, unplug it from the AC first, or disconnect the red VBUS wire.

## Other chips: C5, C6, S3

The C5, C6 and S3 have native USB like the C3, so Option A works on them the same way. **Measure the data lines first**, exactly as for the C3: all of these chips are 3.3 V parts. The firmware compiles for each of them, but only the C3 is in use in this project. On the others, nothing has been run on real hardware.

| | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-S3 |
|---|---|---|---|---|
| Build | `make flash` | `make flash CHIP=esp32c5` | `make flash CHIP=esp32c6` | `make flash CHIP=esp32s3` |
| Option A: USB D− / D+ | GPIO18 / 19 | GPIO13 / 14 | GPIO12 / 13 | GPIO19 / 20 |
| Option B: default RX / TX | GPIO4 / 5 | GPIO4 / 5 | GPIO2 / 3 | GPIO18 / 17 |
| Status LED default | GPIO8, plain LED that lights when driven low (C3 SuperMini) | GPIO27, RGB pixel | GPIO8, RGB pixel | GPIO48, RGB pixel (GPIO38 on DevKitC-1 v1.1) |
| BOOT button | GPIO9 | GPIO28 | GPIO9 | GPIO0 |

The LED and button defaults match Espressif's DevKit boards and the C3 SuperMini. Other boards may differ, so check the board's pinout and set `STATUS_LED_PIN`, `STATUS_LED_PIXEL`, `STATUS_LED_ACTIVE_LOW` and `CONTROL_BUTTON_PIN` in [config.h](../firmware/mrcool_homekit/config.h) to match. A wrong LED setting only affects the status blinking. The Option B defaults avoid the USB pins, the chip's strapping pins, and the RX/TX pins that DevKits wire to their USB-to-serial chip.

### Boards with two USB sockets

Espressif's ESP32-C5-DevKitC-1, ESP32-C6-DevKitC-1 and ESP32-S3-DevKitC-1 have two USB-C sockets. One goes through a USB-to-serial chip, and the other is the chip's native USB.

- **Use the native socket for everything:** the AC cable, flashing and the serial console. With this firmware the console runs on native USB, so the other socket shows no output.
- **Only plug one socket in at a time.** Depending on the board, a computer on the other socket while the AC is on the native one can tie the two 5 V supplies together.

### ESP32-C5

- The BOOT pin is GPIO28. Espressif's esptool documentation says the C5 enters the bootloader when GPIO28 is held low at reset. The DevKitC-1 user guide doesn't name the pin, so check your board if command mode doesn't respond.
- GPIO27 (the RGB pixel) is also a strapping pin, as on the DevKitC-1. That only matters at power-up, and the firmware drives it only after boot.

### ESP32-S3

- The single-cable mode needs the S3's "Hardware CDC and JTAG" USB mode. `make flash CHIP=esp32s3` selects it. A build in the S3's other USB mode (USB-OTG / TinyUSB, the IDE default) quietly falls back to Option B pins.
- On boards with octal PSRAM (for example N8R8 / N16R8 modules), GPIO35–37 are taken. None of the default pins use them.

## Installing

1. Switch the mini-split off with its remote. For extra safety, turn the breaker off.
2. Lift the indoor unit's front panel and find the SmartHVAC dongle's USB socket. Unplug the original dongle: the port supports only one device.
3. Plug in the controller and route the cable so the panel closes without pinching it.
4. Restore power. Within about 20 seconds the web log at `http://<host>.local/log` should show `UART lines confirmed` and `Indoor unit is answering on the UART`. (A wired board also prints these on its serial console.)
