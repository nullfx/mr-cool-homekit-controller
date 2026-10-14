<!--
Thanks for contributing! Fill in what applies and delete what doesn't.
Never paste your Wi-Fi password, HomeKit setup code or OTA password here,
including inside serial logs.
-->

## What this changes

<!-- One or two sentences: what's different after this PR, and why. -->

Fixes #

## Type of change

- [ ] Bug fix
- [ ] New feature or Home app control
- [ ] Support for another chip or AC model
- [ ] Docs only
- [ ] Build / tooling (Makefile, `tools/`)
- [ ] Vendored library update (`lib/MideaUART`)

## How it was tested

**Builds**: `make build` passes for:

- [ ] `CHIP=esp32c3` (default)
- [ ] `CHIP=esp32`
- [ ] Other: <!-- esp32c5 / esp32c6 / esp32s3 -->

**On hardware**:

- Board: <!-- e.g. ESP32-C3 SuperMini, NodeMCU-32S -->
- AC link: <!-- USB socket on the C3 / GPIO through a level shifter -->
- Indoor unit: <!-- e.g. MRCOOL O-ES-09-HP-230E -->
- Flashed via: <!-- make flash / make ota -->
- [ ] Not tested on hardware (say why)

**Behavior checked** (tick what you checked):

- [ ] Power, mode and set point from the Home app reach the unit
- [ ] Changes made with the IR remote show up in the Home app
- [ ] Extra services this PR touches (fan, Dry, Eco/Turbo/Sleep, outdoor temp) still work
- [ ] Device reboots and reconnects to Wi-Fi without re-pairing
- [ ] OTA update still works (`make ota`)

<!-- Relevant serial output (`make monitor`), with credentials removed: -->

```
```

## Checklist

- [ ] `secrets.h` and any real credentials are not committed
- [ ] Unmodified upstream files in `lib/MideaUART` are left alone. If you re-vendored, you used `tools/vendor-mideauart.sh` and updated `VENDORED.md`
- [ ] New options have a sensible default in `config.h`, and existing setups behave the same unless they opt in
- [ ] Docs are updated: `README.md` for controls, commands and troubleshooting, `docs/wiring.md` for pins and boards
- [ ] If this bumps the ESP32 core or HomeSpan version in the `Makefile`, the README matches

## Notes for reviewers

<!-- Anything risky, open questions, or protocol details worth a second look. -->
