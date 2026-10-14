---
name: Bug report
about: Something doesn't work as described in the README
title: ""
labels: bug
assignees: ""
---

<!--
Thanks for reporting a bug. Fill in what applies and delete what doesn't.
Never paste your Wi-Fi password, HomeKit setup code or OTA password here,
including inside serial logs, the web log and screenshots.

Found a security problem? Don't post it here. See SECURITY.md.

Check the Troubleshooting table in README.md first. Many link problems
turn out to be a charge-only cable or a missing level shifter.
-->

## What happened

<!-- What you did, what you expected, and what happened instead. Include what the Home app showed. -->

**Steps to reproduce:**

1.
2.
3.

**Expected:**

**Actual:**

**How often:**

- [ ] Every time
- [ ] Sometimes
- [ ] Once so far

## Where it goes wrong

- [ ] Build or flash (`make setup`, `make build`, `make flash`, `make ota`)
- [ ] Wi-Fi or the MrCool-Setup access point
- [ ] HomeKit setup code or pairing
- [ ] No link to the indoor unit (nothing received, `rx` stays at 0)
- [ ] A Home app control doesn't reach the unit, or jumps back
- [ ] Changes made with the IR remote don't show up in the Home app
- [ ] Board reboots, drops off Wi-Fi, or shows "No Response"
- [ ] Other:

## Hardware

- Board: <!-- e.g. ESP32-C3 SuperMini, NodeMCU-32S -->
- Build: <!-- CHIP=esp32c3 / esp32 / esp32c5 / esp32c6 / esp32s3 -->
- AC link: <!-- USB socket on the C3 / GPIO through a level shifter (which shifter?) -->
- Cable: <!-- for the C3 USB socket: is it a data cable? -->
- Indoor unit model: <!-- e.g. MRCOOL O-ES-09-HP-230E -->
- Outdoor unit model:
- [ ] The original Wi-Fi dongle is unplugged
- [ ] I measured the data lines at about 3.3 V (see docs/wiring.md)

## Firmware

- Version: <!-- FW_VERSION, printed at boot and shown as "Firmware" in the accessory's settings in the Home app -->
- Commit: <!-- output of `git rev-parse --short HEAD` -->
- Built with: <!-- make / Arduino IDE / other -->
- Settings you changed in `config.h` or `secrets.h` (no credentials):

```
// e.g. #define AC_LINK_ON_USB_PORT 0
```

## Apple Home

<!-- Only for pairing, "No Response" or Home app problems. -->

- Home hub: <!-- HomePod / Apple TV / none -->
- iOS version:
- Network: <!-- same 2.4 GHz network and VLAN as the phone? mesh? -->

## Logs

<!--
Remove credentials before you paste.
- Board on your computer: run `make monitor` and type `@s`.
- Board on the AC: there's no console on a C3, so copy http://<host>.local/log.
- Protocol problems: type `@v` and include the frames around the failure.
-->

**`@s` output:**

```
```

**Web log (`http://<host>.local/log`):**

```
```

**Serial output or `@v` frames:**

```
```

## Already tried

- [ ] The Troubleshooting table in README.md
- [ ] Power-cycled the board and the AC
- [ ] Removed the accessory from the Home app and paired again

## Anything else

<!-- Screenshots, photos of the wiring, or anything that seems related. -->
