// User configuration. Edit this file, or override any value in secrets.h
// (git-ignored; it is read first, so its #defines win).
#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#define FW_VERSION "1.0.0"

// ---------------------------------------------------------------------------
// HomeKit identity
// ---------------------------------------------------------------------------

// Name shown in the Home app when the accessory is discovered.
#ifndef DEVICE_NAME
#define DEVICE_NAME "Mini-Split"
#endif

// mDNS host name base. HomeSpan appends the device ID, e.g. MrCool-AC-XXXXXXXXXXXX.local
#ifndef DEVICE_HOSTNAME
#define DEVICE_HOSTNAME "MrCool-AC"
#endif

#ifndef DEVICE_MODEL
#define DEVICE_MODEL "O-ES-09-HP-230E"
#endif

// HomeKit setup (pairing) code.
//   ""          -> a random code is generated on first boot, saved in flash,
//                  and printed on the serial console at every boot.
//   "12345670"  -> this exact code is always used (8 digits; Apple rejects
//                  trivial codes like 12345678 or 11111111).
// It can also be set from the command line: `make flash SETUP_CODE=12345670`.
#ifndef HOMEKIT_SETUP_CODE
#ifdef HOMEKIT_SETUP_CODE_NUM            // injected by the Makefile
#define MRCOOL_STR_(x) #x
#define MRCOOL_STR(x) MRCOOL_STR_(x)
#define HOMEKIT_SETUP_CODE MRCOOL_STR(HOMEKIT_SETUP_CODE_NUM)
#else
#define HOMEKIT_SETUP_CODE ""
#endif
#endif

// 4-character Setup ID embedded in the pairing QR code. Give each device
// a different one if you build more than one controller.
#ifndef HOMEKIT_SETUP_ID
#define HOMEKIT_SETUP_ID "MRCL"
#endif

// ---------------------------------------------------------------------------
// Air conditioner UART (the unit's "USB" port: 9600 8N1)
// ---------------------------------------------------------------------------

#include <soc/soc_caps.h>

// Chips with native USB (ESP32-C3, C6, S3) can talk to the AC through their
// own USB socket: one USB-A to USB-C cable from the AC to the board, no
// wiring. At boot the firmware checks the USB port:
//   - a computer is attached -> USB stays a serial console (flash, setup,
//     setup code); the AC link is off.
//   - anything else (the AC) -> the USB data pins become the AC's UART.
// Set to 0 to use the wired pins below instead (needed for the original
// ESP32, e.g. NodeMCU-32S, which has no native USB).
// WARNING: this connects the AC's data lines straight to the chip, which
// is rated for 3.3 V. Measure first - see docs/wiring.md.
#if defined(SOC_USB_SERIAL_JTAG_SUPPORTED) && ARDUINO_USB_MODE == 1 && ARDUINO_USB_CDC_ON_BOOT
#define AC_USB_PORT_AVAILABLE 1
#else
#define AC_USB_PORT_AVAILABLE 0
#endif
#ifndef AC_LINK_ON_USB_PORT
#define AC_LINK_ON_USB_PORT AC_USB_PORT_AVAILABLE
#endif
#if AC_LINK_ON_USB_PORT && !AC_USB_PORT_AVAILABLE
#error "AC_LINK_ON_USB_PORT needs a chip with native USB (C3/C6/S3) built with USB CDC On Boot enabled"
#endif

// How long to look for a computer on the USB port at boot.
#ifndef USB_HOST_DETECT_MS
#define USB_HOST_DETECT_MS 1500
#endif

// Wired mode: pins connected (through the level shifter) to the AC's
// D+/D- lines. The defaults avoid the USB data pins, strapping pins, and
// the UART0 pins that Espressif DevKits wire to their USB-to-serial chip.
//   ESP32 (NodeMCU-32S, DevKit): GPIO16/17, labelled RX2/TX2 (on WROVER
//                                modules they drive PSRAM; use 25/26)
//   C3: GPIO4/5   C5: GPIO4/5   C6: GPIO2/3   S3: GPIO18/17
#if defined(CONFIG_IDF_TARGET_ESP32)
#define DEFAULT_AC_RX_PIN 16
#define DEFAULT_AC_TX_PIN 17
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
#define DEFAULT_AC_RX_PIN 2
#define DEFAULT_AC_TX_PIN 3
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define DEFAULT_AC_RX_PIN 18
#define DEFAULT_AC_TX_PIN 17
#else  // C3, C5
#define DEFAULT_AC_RX_PIN 4
#define DEFAULT_AC_TX_PIN 5
#endif
#ifndef AC_UART_RX_PIN
#define AC_UART_RX_PIN DEFAULT_AC_RX_PIN
#endif
#ifndef AC_UART_TX_PIN
#define AC_UART_TX_PIN DEFAULT_AC_TX_PIN
#endif

// Sources disagree on which data line is TX. With this on, the firmware
// swaps RX/TX in software when the unit stays silent, and remembers the
// orientation that works.
#ifndef AC_AUTO_SWAP_LINES
#define AC_AUTO_SWAP_LINES 1
#endif

// Beep on the indoor unit when a command is accepted.
#ifndef AC_BEEPER
#define AC_BEEPER true
#endif

// Set-point limits in Celsius (Midea units accept 17-30 C / 62-86 F).
#ifndef AC_MIN_TEMP_C
#define AC_MIN_TEMP_C 17.0f
#endif
#ifndef AC_MAX_TEMP_C
#define AC_MAX_TEMP_C 30.0f
#endif

// Set to 0 for a cooling-only unit (hides "Heat" in the Home app).
#ifndef AC_HAS_HEAT
#define AC_HAS_HEAT 1
#endif

// Louver movement used when "Oscillate" is switched on in the Home app:
// SWING_VERTICAL, SWING_HORIZONTAL or SWING_BOTH.
#ifndef AC_SWING_MODE
#define AC_SWING_MODE SWING_VERTICAL
#endif

// ---------------------------------------------------------------------------
// Extra HomeKit services (1 = shown in the Home app, 0 = hidden)
// ---------------------------------------------------------------------------
#ifndef ENABLE_FAN_ONLY_SERVICE
#define ENABLE_FAN_ONLY_SERVICE 1   // Fan tile that runs the unit in fan-only mode
#endif
#ifndef ENABLE_DRY_SWITCH
#define ENABLE_DRY_SWITCH 1         // Switch for dehumidify (dry) mode
#endif
#ifndef ENABLE_AUTO_FAN_SWITCH
#define ENABLE_AUTO_FAN_SWITCH 1    // Switch for automatic fan speed
#endif
#ifndef ENABLE_ECO_SWITCH
#define ENABLE_ECO_SWITCH 1         // ECO preset (cool mode only)
#endif
#ifndef ENABLE_TURBO_SWITCH
#define ENABLE_TURBO_SWITCH 1       // TURBO / boost preset (heat or cool)
#endif
#ifndef ENABLE_SLEEP_SWITCH
#define ENABLE_SLEEP_SWITCH 1       // SLEEP preset
#endif
#ifndef ENABLE_OUTDOOR_SENSOR
#define ENABLE_OUTDOOR_SENSOR 1     // Outdoor temperature reported by the condenser
#endif

// ---------------------------------------------------------------------------
// Controller hardware and networking
// ---------------------------------------------------------------------------

// On-board LED that shows HomeSpan status. -1 = none.
//   ESP32 DevKit / NodeMCU-32S: GPIO2, plain LED, lights when driven high
//   ESP32-C3 SuperMini:         GPIO8, plain LED, lights when driven low
//   ESP32-C3-DevKitM-1:         GPIO8 is an RGB pixel - set STATUS_LED_PIXEL 1
//   ESP32-C5-DevKitC-1:         GPIO27 RGB pixel
//   ESP32-C6-DevKitC-1:         GPIO8 RGB pixel
//   ESP32-S3-DevKitC-1:         GPIO48 RGB pixel (GPIO38 on board v1.1)
// Push button for HomeSpan's command mode: the BOOT button. Hold 3 s for
// command mode (unpair, setup AP...). GPIO0 on ESP32/S3, 9 on C3/C6,
// 28 on C5.
#if defined(CONFIG_IDF_TARGET_ESP32)
#define DEFAULT_LED_PIN 2
#define DEFAULT_LED_ACTIVE_LOW 0
#define DEFAULT_LED_PIXEL 0
#define DEFAULT_BUTTON_PIN 0
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define DEFAULT_LED_PIN 8
#define DEFAULT_LED_ACTIVE_LOW 1
#define DEFAULT_LED_PIXEL 0
#define DEFAULT_BUTTON_PIN 9
#elif defined(CONFIG_IDF_TARGET_ESP32C5)
#define DEFAULT_LED_PIN 27
#define DEFAULT_LED_ACTIVE_LOW 0
#define DEFAULT_LED_PIXEL 1
#define DEFAULT_BUTTON_PIN 28
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
#define DEFAULT_LED_PIN 8
#define DEFAULT_LED_ACTIVE_LOW 0
#define DEFAULT_LED_PIXEL 1
#define DEFAULT_BUTTON_PIN 9
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define DEFAULT_LED_PIN 48
#define DEFAULT_LED_ACTIVE_LOW 0
#define DEFAULT_LED_PIXEL 1
#define DEFAULT_BUTTON_PIN 0
#else
#define DEFAULT_LED_PIN -1
#define DEFAULT_LED_ACTIVE_LOW 0
#define DEFAULT_LED_PIXEL 0
#define DEFAULT_BUTTON_PIN 0
#endif
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN DEFAULT_LED_PIN
#endif
#ifndef STATUS_LED_ACTIVE_LOW
#define STATUS_LED_ACTIVE_LOW DEFAULT_LED_ACTIVE_LOW
#endif
#ifndef STATUS_LED_PIXEL
#define STATUS_LED_PIXEL DEFAULT_LED_PIXEL   // 1 = addressable RGB LED (WS2812)
#endif
#ifndef CONTROL_BUTTON_PIN
#define CONTROL_BUTTON_PIN DEFAULT_BUTTON_PIN
#endif

// When no Wi-Fi credentials are stored, open a setup access point
// ("MrCool-Setup") where Wi-Fi and the setup code can be entered.
#ifndef ENABLE_SETUP_AP
#define ENABLE_SETUP_AP 1
#endif
#ifndef SETUP_AP_SSID
#define SETUP_AP_SSID "MrCool-Setup"
#endif
#ifndef SETUP_AP_PASSWORD
#define SETUP_AP_PASSWORD "homespan"   // at least 8 characters
#endif

// Over-the-air updates (`make ota`). Password defaults to HomeSpan's
// "homespan-ota" unless OTA_PASSWORD is defined in secrets.h.
#ifndef ENABLE_OTA
#define ENABLE_OTA 1
#endif

// Rolling log viewable at http://<host>.local/log
#ifndef ENABLE_WEB_LOG
#define ENABLE_WEB_LOG 1
#endif
