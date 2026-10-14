# MrCool HomeKit Controller

ESP32 firmware that replaces the MRCOOL SmartHVAC Wi-Fi dongle and puts the mini-split (written for an **O-ES-09-HP-230E**, Olympus E Star 9k) straight into Apple Home. No cloud, no app, no bridge.

The indoor unit's "USB" port is really a 9600-baud serial link that speaks the Midea UART protocol (MRCOOL units are Midea-built). The firmware talks that protocol with [MideaUART](https://github.com/dudanov/MideaUART), the library behind ESPHome's `midea` component, and serves HomeKit over Wi-Fi with [HomeSpan](https://github.com/HomeSpan/HomeSpan).

## What shows up in the Home app

| Home app control | Unit function |
|---|---|
| **Mini-Split** tile: power, Auto / Heat / Cool | Power and operating mode |
| Temperature dial | Set point, 17–30 °C (62–86 °F), 0.5 °C steps |
| Fan speed slider (20 / 40 / 60 / 80 / 100 %) | Silent / Low / Medium / High / Turbo fan |
| Oscillate | Louver swing (vertical by default) |
| Current temperature, Heating/Cooling status | Indoor sensor; the status is inferred from room temp vs. set point |
| **Fan Only** fan tile | Fan-only mode with its own speed control |
| **Dry Mode** switch | Dehumidify mode |
| **Auto Fan** switch | Automatic fan speed |
| **Eco**, **Turbo**, **Sleep** switches | Presets. The unit only allows Eco in Cool and Turbo in Heat/Cool; an impossible request flips back off. |
| **Outdoor Temperature** sensor | Condenser's outdoor temperature reading |

Each extra service can be turned off in [config.h](firmware/mrcool_homekit/config.h). Every control reads back from the unit, so changes made with the IR remote show up in the Home app within about a second.

## Hardware

Two ways to connect. Details and diagrams: **[docs/wiring.md](docs/wiring.md)**.

| | ESP32-C3, single cable (default build) | Original ESP32 (NodeMCU-32S, DevKit), wired |
|---|---|---|
| Parts | ESP32-C3 board, USB-A to USB-C **data** cable | Board, USB-A plug, 5 V ↔ 3.3 V level shifter |
| AC link | The board's own USB socket | GPIO16/17 through the shifter |
| Build | `make flash` | `make flash CHIP=esp32` |
| Catch | The AC's data lines reach the chip directly. **Measure them first:** they must be about 3.3 V, not 5 V. | Soldering |

Sources disagree on which data line is TX. The firmware tries both orientations and remembers the one that works, so you never have to swap wires.

## Software prerequisites

- **arduino-cli**: `brew install arduino-cli`, or the copy inside Arduino IDE 2 is found automatically.
- **make**: on macOS, run `sudo xcodebuild -license accept` once, or `brew install make` and use `gmake`.
- **python3**: used by the ESP32 tools for OTA and by the optional QR helper.

`make setup` installs the ESP32 Arduino core (3.3.6) and HomeSpan (2.1.8) into `.arduino/` inside this folder. Builds don't use or change your global Arduino IDE installation. MideaUART is vendored in [lib/MideaUART](lib/MideaUART/VENDORED.md).

## Build and flash

```bash
make setup            # once: ~5.5 GB of ESP32 toolchains into .arduino/
make flash            # compile + flash the first ESP32 found on USB
make monitor          # serial console at 115200 baud (Ctrl-C to exit)
```

- **Which chip:** the default is `CHIP=esp32c3`. Use `CHIP=esp32` for the NodeMCU-32S or a DevKit. `CHIP=esp32c5`, `esp32c6` and `esp32s3` compile but haven't been run on hardware. Their pins and board notes are in [docs/wiring.md](docs/wiring.md#other-chips-c5-c6-s3).
- `make ports` lists connected boards. If auto-detection picks the wrong one, pass `PORT=/dev/cu.usbmodem1101` (C3) or `PORT=/dev/cu.usbserial-0001` (NodeMCU).
- **Flash mode:** boards enter the bootloader by themselves. If an upload hangs at `Connecting....`, hold **BOOT** while you plug in USB (C3) or while you tap **EN/RST** (NodeMCU), release it, and run `make upload`.
- `make build` only compiles. The binaries land in `build/` (`mrcool_homekit.ino.bin`, plus a `.merged.bin` for any ESP flashing tool at offset 0x0).
- **NodeMCU-32S:** use `CHIP=esp32`, not the core's `nodemcu-32s` board profile. That profile leaves only 1.25 MB for the app, and this firmware needs about 1.5 MB.

### The C3's USB socket does two jobs

At power-up the firmware spends 1.5 s checking the USB socket:

- **A computer is attached:** the socket is a normal serial console. Flashing, `make monitor`, Wi-Fi setup and the setup code all work, and the AC link is off.
- **Anything else (the AC):** the socket's data pins become the AC's UART. There's no console then, so use the web log at `http://<host>.local/log` and update with `make ota`.

So do the first flash, Wi-Fi setup and pairing at your desk, then move the board to the AC. It keeps its Wi-Fi and pairing. Set `AC_LINK_ON_USB_PORT 0` in `config.h` to wire a C3 through GPIO4/5 instead.

## First-time setup

Do this with the board plugged into your computer.

### 1. Wi-Fi (2.4 GHz)

Choose one:

- **Serial console:** in `make monitor`, type `W` and follow the prompts.
- **Setup access point:** with no Wi-Fi stored, the board opens a network called **MrCool-Setup** (password `homespan`, changeable in `config.h`). Join it from your phone, and the page asks for your Wi-Fi and a setup code. The code you type there becomes the HomeKit code.
- **Build time:** copy `firmware/mrcool_homekit/secrets.example.h` to `secrets.h` (git-ignored) and set `WIFI_SSID` / `WIFI_PASSWORD`.

### 2. The HomeKit setup code

The code is printed on the serial console at every boot, and whenever you type `@c`:

```
------------------------------------------------------------
  HomeKit setup code : 482-91-637   (saved in flash)
  QR setup payload   : X-HM://00KV294XHMRCL
                       (make qr CODE=48291637 SETUP_ID=MRCL draws it)
  Pairing            : NOT PAIRED - in the Home app tap + > Add Accessory
                       > More options..., pick this device, enter the code
------------------------------------------------------------
```

| You want | Do this |
|---|---|
| A random code (default) | Nothing. A code is generated on first boot from the hardware RNG and kept in flash. |
| A fixed code in the firmware | `make flash SETUP_CODE=31415926`, or set `HOMEKIT_SETUP_CODE` in `config.h` / `secrets.h`. It's re-applied at every boot. |
| A new random code | `@n` in the serial console |
| A specific code, without rebuilding | `@p 31415926` in the serial console |

Apple rejects trivial codes (`12345678`, `87654321`, all-same digits). The firmware checks for these before applying a code.

### 3. Pair

1. On an iPhone on the same Wi-Fi, open **Home** and tap **+** > **Add Accessory**.
2. Either scan the QR code from `make qr CODE=<code>` (needs `brew install qrencode`), or tap **More options...**, pick **Mini-Split**, and type the 8-digit code.
3. Approve the "Uncertified Accessory" prompt. HomeSpan devices aren't MFi-certified, which is normal.

## Updating over Wi-Fi

Once it's installed in the unit:

```bash
make build
make ota HOST=MrCool-AC-XXXXXXXXXXXX.local      # or its IP address
```

The default OTA password is HomeSpan's `homespan-ota`. Set your own with `OTA_PASSWORD` in `secrets.h`, then pass it to the upload as `make ota HOST=... OTA_PASSWORD=...`.

## Serial console commands

HomeSpan's built-in commands work too. Type `?` for the list. The most useful ones are `W` (Wi-Fi), `U` (unpair), `A` (setup access point) and `E` (erase everything).

| Command | Action |
|---|---|
| `@c` | Show setup code, QR payload and pairing status |
| `@n` | New random setup code |
| `@p <code>` | Set a specific setup code |
| `@s` | Unit status: mode, temperatures, fan, swing, preset, UART byte counts, capability report |
| `@v` | Toggle Midea protocol debug logging |
| `@d` | Toggle the indoor unit's display LEDs |

On a C3 the console only exists while the board is plugged into a computer.

The **BOOT** button doubles as HomeSpan's control button. Hold it about 3 seconds to enter command mode (unpair, start the setup AP, factory reset). A web log of recent events is at `http://<host>.local/log`.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `Nothing received from the indoor unit on either line orientation` (web log) | Both RX/TX orientations were tried. On a C3, make sure it's a **data** cable, not charge-only. When wired, check that 5 V and GND reach the shifter's HV side and 3.3 V its LV side. Make sure the original dongle is unplugged. |
| `rx` grows but there's never a status | When wired, usually a missing level shifter or one without HV pull-ups. Run `@v` to watch the frames. |
| C3 plugged into the AC still acts like a console | It saw USB traffic at boot. Power it from the AC port only, not a powered hub that also connects to a computer. |
| Home app can't find the accessory | The phone and ESP32 must be on the same 2.4 GHz network and VLAN, with mDNS/Bonjour allowed. Check the console says `READY` or `NOT PAIRED`. |
| Paired accessory shows "No Response" after re-flashing | Pairings survive flashing. If you ran `make erase` or `E`, remove the accessory from the Home app and pair again. |
| A control jumps back after you change it | The unit refused it, for example Eco outside Cool mode, or a fan speed in Auto mode (the unit forces auto fan). |
| Start completely fresh | `make erase` wipes Wi-Fi, pairings and the setup code, then `make flash`. |

## Project layout

```
Makefile                         build / flash / monitor / ota / qr
firmware/mrcool_homekit/
  mrcool_homekit.ino             setup(): HomeSpan config, accessory, CLI commands
  config.h                       every user setting (pins, names, features)
  AcPort.{h,cpp}                 USB socket vs. wired pins; computer detection
  secrets.example.h              template for Wi-Fi / fixed code / OTA password
  MideaLink.{h,cpp}              UART link, command merging, link health
  AcServices.h                   HomeKit services and value mapping
  Pairing.{h,cpp}                setup code: generate, persist, display, QR
lib/MideaUART/                   vendored Midea protocol library (MIT)
tools/homekit_qr.py              QR payload / code for a setup code
tools/vendor-mideauart.sh        refresh the vendored library
docs/wiring.md                   hardware and installation
```

## Credits

- [MideaUART](https://github.com/dudanov/MideaUART) by Sergey Dudanov, MIT. Protocol implementation.
- [HomeSpan](https://github.com/HomeSpan/HomeSpan) by Gregg Berman, MIT. HomeKit Accessory Protocol.

This is an unofficial project, not affiliated with MRCOOL or Midea. Swapping the Wi-Fi dongle doesn't touch refrigerant or mains wiring, but read your warranty terms.
