# Contributing

Thanks for helping out. This is a small hobby project, so bug reports, reports from other AC models, doc fixes and code are all welcome.

By taking part you agree to the [Code of Conduct](CODE_OF_CONDUCT.md).

## Before you start

- **Keep credentials out of everything you post.** Serial logs, the web log, screenshots and `secrets.h` can contain your Wi-Fi password, HomeKit setup code or OTA password. Remove them before you paste.
- **Stay low-voltage.** This project only replaces the Wi-Fi dongle. Don't open the indoor unit's electronics or touch mains wiring for a contribution, and measure the data lines before you connect a board (see [docs/wiring.md](docs/wiring.md)).
- **For anything bigger than a small fix, open an issue first** so we can agree on the approach before you spend time on it.

## Reporting a bug

Open an issue and include:

- **Hardware:** board (e.g. ESP32-C3 SuperMini, NodeMCU-32S), how it's connected to the AC (C3 USB socket, or GPIO through a level shifter), and the indoor unit's model number.
- **Firmware:** the `FW_VERSION` from `config.h` or the commit hash, and any changes you made to `config.h`.
- **What you did, what you expected, and what happened**, including what the Home app showed.
- **Logs:**
  - The output of `@s` from the serial console. It shows the unit's state, UART byte counts and what the unit says it supports.
  - When the board is on the AC there's no console, so copy the web log from `http://<host>.local/log`.
  - For protocol problems, turn on `@v` and include the frames around the failure.

Check the [Troubleshooting](README.md#troubleshooting) table first. Many link problems turn out to be a charge-only cable or a missing level shifter.

## Reporting another AC model

The firmware was written against an MRCOOL O-ES-09-HP-230E. Other Midea-built units (MRCOOL, Midea, and other brands that use the same Wi-Fi dongle) may work too. If you try one, open an issue with:

- the model number of the indoor and outdoor units
- whether the link came up, and the `@s` output
- which controls work, which don't, and any that behave differently

A report that a unit works is as useful as one that it doesn't.

## Development setup

You need `arduino-cli`, `make` and `python3`. The [README](README.md#software-prerequisites) has install notes.

```bash
make setup     # once: installs the pinned ESP32 core and HomeSpan into .arduino/
make build     # compile only
make flash     # compile and flash over USB
make monitor   # serial console at 115200
```

The toolchain lives in `.arduino/` in the repo, so it doesn't touch your global Arduino IDE. The core and HomeSpan versions are pinned in the `Makefile` on purpose. Don't bump them as part of an unrelated change.

For Wi-Fi and a fixed setup code while developing, copy `firmware/mrcool_homekit/secrets.example.h` to `secrets.h`. It's git-ignored. Don't remove it from `.gitignore`.

Remember that a C3 connected to the AC through its USB socket has no serial console. Do your debugging at your desk with the board on your computer, then use `make ota` and the web log once it's on the unit.

## Where things live

| Path | What it does |
|---|---|
| `firmware/mrcool_homekit/mrcool_homekit.ino` | `setup()`: HomeSpan config, the accessory and its services, the `@` console commands |
| `config.h` | Every user setting: pins, names, feature switches |
| `AcServices.h` | HomeKit services and how their values map to the unit |
| `MideaLink.{h,cpp}` | UART link, merging HomeKit writes into one command, link health |
| `AcPort.{h,cpp}` | USB socket vs. wired pins, detecting a computer at boot |
| `Pairing.{h,cpp}` | Setup code: generate, save, display, QR |
| `lib/MideaUART/` | Vendored Midea protocol library. See below. |
| `tools/` | QR helper and the vendoring script |
| `docs/wiring.md` | Hardware and installation |

## Code style

There's no formatter config yet, so match the code around your change:

- 2-space indents, braces on the same line.
- Project code goes in `namespace mrcool`.
- Constants are `static constexpr` with a `k` prefix and the unit in the name, e.g. `kSettleMs`.
- User settings in `config.h` are wrapped in `#ifndef` so `secrets.h` can override them, and each one has a comment saying what it does.
- Send messages that matter after installation to the web log (`homeSpan.addWebLog`) as well as `Serial`, as `note()` in `MideaLink.cpp` does. A `Serial.printf` alone can't be seen once a C3 is on the AC.
- Comment *why*, not *what*. Timing values and protocol quirks especially need a reason.
- Check that `make build` produces no new warnings.

## Adding a Home app control

1. Add an `ENABLE_...` switch in the feature section of `config.h`. Default it to `1` only if it's useful for most units.
2. Add the service to `AcServices.h`, following the existing ones (`ModeSwitch`, `PresetSwitch` and so on). Derive from `AcFollower`. Send the user's change to the unit in `update()`, and read the unit's state back in `loop()` behind `shouldSync()`. That way the control follows changes made with the IR remote, and doesn't overwrite a value the user just set before the unit confirms it.
3. Create it in `mrcool_homekit.ino` inside `#if ENABLE_...`.
4. If the unit can refuse the request, as with Eco outside Cool mode, make sure the control flips back rather than showing a state the unit isn't in.
5. Add a row to the "What shows up in the Home app" table in the README.

## The vendored MideaUART library

`lib/MideaUART` is a copy of [dudanov/MideaUART](https://github.com/dudanov/MideaUART), the same version ESPHome uses. Its source files are unmodified upstream code.

- Don't edit them. Fix bugs upstream, or work around them in `MideaLink`.
- To take a new upstream version, run `tools/vendor-mideauart.sh <tag>`, update the tag and commit in `lib/MideaUART/VENDORED.md`, and test on hardware. Put that in its own PR.

## Documentation

Update the docs in the same PR as the change:

- `README.md` for controls, console commands, Makefile targets and troubleshooting
- `docs/wiring.md` for pins, boards and wiring

Plain, short sentences. Readers are often standing next to an air conditioner with a laptop.

## Pull requests

1. Fork the repo and create a branch from `main`.
2. Keep each PR to one change. Separate refactors from behavior changes.
3. Run `make build` for `CHIP=esp32c3` and `CHIP=esp32`. For changes to `AcPort` or pins, build the other chips too.
4. Test on a real unit if you can. If you can't, say so in the PR and someone with hardware will try it.
5. Fill in the [pull request template](PULL_REQUEST_TEMPLATE.md).

Write commit messages in the imperative mood ("Add sleep preset switch"), with a short first line and details below it if needed.

## License

The project is MIT licensed (see [LICENSE](LICENSE)). By contributing, you agree that your contributions are licensed under the same terms.
