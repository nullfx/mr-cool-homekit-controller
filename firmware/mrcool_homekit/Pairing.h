// HomeKit setup-code management.
//
// HomeSpan only stores a *hash* of the setup code, so it cannot print the
// code back later. This module remembers the plain code in its own NVS
// namespace, so the code can be shown at every boot, and it notices when
// the code was changed elsewhere (HomeSpan's `S` command or the setup
// access point).
#pragma once

#include <Arduino.h>

namespace mrcool {
namespace pairing {

// Call before homeSpan.begin(). fixedCode is "" for a random code that is
// generated once, or an 8-digit code that is always enforced.
void begin(const char *fixedCode, const char *setupId);

// Validate, apply and remember a code. Accepts "12345670" or "123-45-670".
bool setCode(const char *code, Print &out);

// Generate and apply a new random code.
void regenerate(Print &out);

// Print the setup code, QR payload and how to pair.
void printBanner(Print &out);

bool isAllowed(const char *digits);

}  // namespace pairing
}  // namespace mrcool
