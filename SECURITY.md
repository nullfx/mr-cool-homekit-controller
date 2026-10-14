# Security policy

This firmware sits on your home network and controls an air conditioner. Thanks for helping keep it safe.

## Supported versions

Only the latest code on `main` gets security fixes. Older versions don't get backports. The version is `FW_VERSION` in `config.h`. It's printed at boot and shown as "Firmware" in the accessory's settings in the Home app.

## Reporting a vulnerability

**Don't open a public issue.** Report it privately through GitHub instead:

1. Go to the [Security tab](https://github.com/nullfx/mr-cool-homekit-controller/security) of this repository.
2. Click **Report a vulnerability**.

Please include:

- what the problem is and what an attacker could do with it
- the firmware version or commit, and the board and chip build (`CHIP=...`)
- the steps to reproduce it, or a proof of concept
- any settings you changed in `config.h`

Remove your Wi-Fi password, HomeKit setup code and OTA password from logs and screenshots before you send them.

### What happens next

This is a small hobby project, so responses are best effort:

- You'll get a reply within 7 days.
- We'll agree on a fix and a disclosure date with you. The aim is a fix within 30 days.
- You'll be credited in the advisory unless you'd rather not be.

## Scope

**In scope:**

- the firmware in `firmware/mrcool_homekit/`: setup code generation and storage, the `@` console commands, how `MideaLink` handles data from the indoor unit, and the defaults in `config.h`
- the `Makefile` and `tools/`, for example if they leak secrets into build output
- docs that would lead someone into an unsafe setup

**Out of scope.** Report these upstream:

- HomeKit protocol, pairing, the web log server, OTA and the setup access point are handled by [HomeSpan](https://github.com/HomeSpan/HomeSpan).
- The Midea protocol library is [MideaUART](https://github.com/dudanov/MideaUART). Please tell us too if it affects this firmware, because fixes are made in `MideaLink`, not in the vendored copy.
- The ESP32 Arduino core and ESP-IDF belong to [Espressif](https://github.com/espressif/arduino-esp32).
- MRCOOL's own Wi-Fi dongle, app and cloud aren't part of this project.

**Not a vulnerability:**

- attacks that need physical access to the board (see below)
- the known limitations listed below
- electrical or wiring safety problems. Those are bugs, so please open a normal issue.

## Known limitations

These are design choices. Each one has a setting you can change.

| Limitation | What to do |
|---|---|
| The default OTA password is HomeSpan's public `homespan-ota`. Anyone on your network who knows it can install new firmware. | Set `OTA_PASSWORD` in `secrets.h`, or set `ENABLE_OTA 0`. |
| The setup access point opens when no Wi-Fi is stored, and its default password `homespan` is public. | Change `SETUP_AP_PASSWORD`, or set `ENABLE_SETUP_AP 0` and use the serial console or `secrets.h` for Wi-Fi. |
| The web log at `http://<host>.local/log` has no password. Anyone on your network can read it. It shows the unit's state and link messages, not credentials. | Set `ENABLE_WEB_LOG 0`. |
| Anyone with USB access to the board can use the serial console to read or change the setup code, unpair the board or erase it. Flash encryption and secure boot are off. | Keep the board where others can't reach it. Physical access means full control. |
| Values in `secrets.h` are compiled into the firmware. Binaries in `build/` contain your Wi-Fi password. | Don't share binaries built with your `secrets.h`. |

## Keeping your setup safe

- Set your own `OTA_PASSWORD` and `SETUP_AP_PASSWORD`.
- Keep `secrets.h` out of git. It's already in `.gitignore`.
- Update to the latest `main` with `make ota`.
- Remove credentials from anything you post in an issue.
