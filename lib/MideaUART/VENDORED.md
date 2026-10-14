# Vendored: MideaUART v1.1.9

- Upstream: https://github.com/dudanov/MideaUART
- Tag: `v1.1.9` (commit `12f3b887cf9d5f0538b5e3e2c16de5a1af292b2e`). ESPHome's `midea` climate component pins this same version.
- License: MIT, as declared in the upstream `library.json`. Copyright (c) Sergey Dudanov.

## What changed

The upstream source files are unmodified. The upstream repo is a PlatformIO-style library with separate `include/` and `src/` trees, which the Arduino builder doesn't support. To fit the Arduino layout:

- Both trees were merged into `src/`, so the `#include "Appliance/..."` paths still resolve.
- `library.properties` was added.
- `src/MideaUART.h` was added. It includes `AirConditioner.h`, and it exists because the Arduino builder only finds a library through a header at the top of `src/`.

To re-vendor (for example, to take a newer tag), run `tools/vendor-mideauart.sh <tag>`.
