# AGENTS.md

ESP32 Arduino firmware that talks the Midea UART protocol to an MRCOOL mini-split and exposes it to Apple Home through HomeSpan. User-facing docs are in [README.md](README.md), hardware in [docs/wiring.md](docs/wiring.md), contribution rules in [CONTRIBUTING.md](CONTRIBUTING.md).

## Commands

```bash
make build                 # compile for the default chip (esp32c3); ~80 s
make build CHIP=esp32      # original ESP32 (NodeMCU-32S, DevKit)
make help                  # every target and option
```

- `make build` is the only check that runs without hardware. There are no unit tests, no linter and no CI. A change is verified when it builds for `CHIP=esp32c3` **and** `CHIP=esp32` with no new warnings.
- For changes to pins, `AcPort` or `#if` on chip features, also build `CHIP=esp32c6` and `CHIP=esp32s3`.
- If the build says the core or HomeSpan is missing, `make setup` installs them (~5.5 GB download). Ask before running it.
- Don't run `make flash`, `upload`, `erase`, `ota` or `monitor` unless asked. They act on a physical board, and `erase` wipes the user's Wi-Fi and HomeKit pairing. `monitor` never exits on its own.
- Tell the user what still needs testing on real hardware. Building isn't proof that the AC responds.

## Layout

All firmware is in `firmware/mrcool_homekit/`:

- `mrcool_homekit.ino` — `setup()`: HomeSpan config, the accessory and its services, the `@` console commands
- `config.h` — every user setting and feature switch
- `AcServices.h` — HomeKit services and value mapping
- `MideaLink.{h,cpp}` — UART link to the unit, write merging, resync, line-swap
- `AcPort.{h,cpp}` — USB socket vs. wired pins; detects a computer at boot
- `Pairing.{h,cpp}` — HomeKit setup code

## Rules

- **Never edit `lib/MideaUART/`.** It is vendored upstream code kept unmodified (see `VENDORED.md`). Work around problems in `MideaLink`. Re-vendoring only happens through `tools/vendor-mideauart.sh`, as a separate change.
- **Don't bump the pinned versions** (`ESP32_CORE_VERSION`, `HOMESPAN_VERSION` in the `Makefile`) unless asked. If you do, update the README to match.
- **Secrets:** `firmware/mrcool_homekit/secrets.h` is git-ignored and holds real Wi-Fi, setup-code and OTA credentials. Don't read it, print it, or commit it. Put new secret settings in `secrets.example.h` with placeholder values.
- **Settings in `config.h`** go inside `#ifndef NAME` … `#endif` so `secrets.h` can override them, with a comment saying what each does. New features get an `ENABLE_...` switch, and existing setups must behave the same unless the user turns it on.
- **Flash is tight.** A C3 build uses about 79% of the app partition (`min_spiffs`, so OTA has room). Watch the "Sketch uses" line and say if a change adds much.

## Things that aren't obvious

- **On a C3 wired through its USB socket, there is no serial console once it's on the AC.** `AcPort` decides at boot (within `USB_HOST_DETECT_MS`): computer attached → console; anything else → the USB pins become the AC's UART. So anything a user needs to see after installation must go to the web log (`homeSpan.addWebLog`) as well as `Serial`. `note()` in `MideaLink.cpp` does both.
- **HomeKit writes are merged.** Power, mode and set point arrive as separate HomeKit writes. A service's `update()` calls `MideaLink`'s `request*()` methods (`requestMode`, `requestTargetTemp`, …), which queue the change. `MideaLink` waits `kSettleMs` after the last one and sends one frame. Don't bypass the queue to talk to the unit directly.
- **The unit can refuse requests** (Eco outside Cool, fan speed in Auto, etc.). `MideaLink` re-reads state `kResyncMs` after a command, so the Home app snaps back. A new control must read its state back from the unit rather than trusting what it sent.
- **Services follow the unit through `AcFollower`.** Read state in `loop()` only when `shouldSync()` is true. It skips work when nothing changed and avoids overwriting a value the user just set before the unit confirms it.
- **RX/TX may be swapped** on any given cable. `AC_AUTO_SWAP_LINES` makes `MideaLink` try the other orientation after `kSwapAfterMs` of silence, and stores the one that works in NVS.
- **The status shown as Heating/Cooling is inferred** from room temperature vs. set point. The unit doesn't report it.
- **The `@` console commands** are `SpanUserCommand`s registered in `mrcool_homekit.ino`. If you add one, add it to the README's console table.

## Style

Match the surrounding code: 2-space indents, braces on the same line, project code in `namespace mrcool`, `static constexpr` constants named `kNameUnit` (e.g. `kSettleMs`). Comments explain why, especially for timing values and protocol quirks.

## Docs

Update docs in the same change:

- `README.md` — Home app controls table, console commands, Makefile targets, troubleshooting
- `docs/wiring.md` — pins, boards, wiring

Write in plain, short sentences.
