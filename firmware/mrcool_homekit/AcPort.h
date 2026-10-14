// Decides where the AC's UART lives: the board's own USB socket (C3/C6/S3)
// or wired GPIO pins.
#pragma once

#include <Arduino.h>

namespace mrcool {

struct AcPort {
  bool enabled;       // false: a computer is on the USB port, AC link stays off
  int rxPin;
  int txPin;
  const char *where;  // human-readable description for the console
};

// In USB-port mode this waits up to USB_HOST_DETECT_MS for a computer.
// If none shows up, it releases the USB data pins for the UART. Call after
// Serial.begin() and before anything else touches GPIO 18/19 (C3).
AcPort chooseAcPort();

}  // namespace mrcool
