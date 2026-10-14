// MrCool / Midea mini-split HomeKit controller for ESP32.
//
// Plugs into the indoor unit's "USB" Wi-Fi-dongle port (really a 9600-baud
// UART), speaks the Midea UART protocol through MideaUART, and exposes the
// unit to Apple Home over Wi-Fi with HomeSpan.
//
// On an ESP32-C3 the board's own USB socket can be the AC link (see
// AC_LINK_ON_USB_PORT in config.h). Plugged into a computer, the same
// socket is the serial console below.
//
// Serial console (115200 baud) commands, in addition to HomeSpan's own
// (type ? for the full list):
//   @c            show the HomeKit setup code and QR payload
//   @n            generate a new random setup code
//   @p <code>     set a specific 8-digit setup code
//   @s            show indoor-unit status and UART link statistics
//   @v            toggle protocol debug logging
//   @d            toggle the indoor unit's display LEDs

#include "config.h"  // also pulls in secrets.h when present

#include <HomeSpan.h>

#include "AcPort.h"
#include "AcServices.h"
#include "MideaLink.h"
#include "Pairing.h"

static mrcool::MideaLink ac;

// Status LED for boards whose LED lights when the pin is driven low.
struct ActiveLowLED : Blinkable {
  int pin;
  explicit ActiveLowLED(int p) : pin(p) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
  void on() override { digitalWrite(pin, LOW); }
  void off() override { digitalWrite(pin, HIGH); }
  int getPin() override { return pin; }
};

static void printHeader(const mrcool::AcPort &port) {
  Serial.printf("\nMrCool HomeKit controller v%s  (%s)\n", FW_VERSION, DEVICE_MODEL);
  if (port.enabled)
    Serial.printf("AC link on %s: RX=GPIO%d TX=GPIO%d\n", port.where, port.rxPin, port.txPin);
  else
    Serial.printf("AC link off - %s.\nSet up Wi-Fi and pairing here, then plug the board into "
                  "the AC.\n", port.where);
}

static void addCommands() {
  new SpanUserCommand('c', "- show HomeKit setup code and QR payload",
                      [](const char *) { mrcool::pairing::printBanner(Serial); });
  new SpanUserCommand('n', "- generate a new random HomeKit setup code",
                      [](const char *) { mrcool::pairing::regenerate(Serial); });
  new SpanUserCommand('p', "<code> - set the HomeKit setup code, e.g. @p 31415926",
                      [](const char *buf) { mrcool::pairing::setCode(buf + 1, Serial); });
  new SpanUserCommand('s', "- show indoor unit status and UART link statistics",
                      [](const char *) { ac.printStatus(Serial); });
  new SpanUserCommand('v', "- toggle Midea protocol debug logging", [](const char *) {
    ac.setDebug(!ac.debug());
    Serial.printf("\n*** Protocol debug logging %s\n\n", ac.debug() ? "ON" : "OFF");
  });
  new SpanUserCommand('d', "- toggle the indoor unit's display LEDs", [](const char *) {
    ac.toggleDisplay();
    Serial.printf("\n*** Display toggle sent\n\n");
  });
}

static void buildAccessory() {
  new SpanAccessory();
  new Service::AccessoryInformation();
  new Characteristic::Identify();
  new Characteristic::Name(DEVICE_NAME);
  new Characteristic::Manufacturer("MRCOOL");
  new Characteristic::Model(DEVICE_MODEL);
  new Characteristic::FirmwareRevision(FW_VERSION);

  (new mrcool::ClimateService(ac))->setPrimary();
#if ENABLE_FAN_ONLY_SERVICE
  new mrcool::FanOnlyService(ac, "Fan Only");
#endif
#if ENABLE_DRY_SWITCH
  new mrcool::ModeSwitch(ac, mrcool::MODE_DRY, mrcool::MODE_COOL, "Dry Mode");
#endif
#if ENABLE_AUTO_FAN_SWITCH
  new mrcool::AutoFanSwitch(ac, "Auto Fan");
#endif
#if ENABLE_ECO_SWITCH
  new mrcool::PresetSwitch(ac, mrcool::PRESET_ECO, "Eco");
#endif
#if ENABLE_TURBO_SWITCH
  new mrcool::PresetSwitch(ac, mrcool::PRESET_TURBO, "Turbo");
#endif
#if ENABLE_SLEEP_SWITCH
  new mrcool::PresetSwitch(ac, mrcool::PRESET_SLEEP, "Sleep");
#endif
#if ENABLE_OUTDOOR_SENSOR
  new mrcool::OutdoorTempService(ac, "Outdoor Temperature");
#endif
}

void setup() {
  Serial.begin(115200);
  delay(500);  // let a USB-serial monitor attach

  // Must run before anything else touches the USB data pins.
  const mrcool::AcPort port = mrcool::chooseAcPort();
  printHeader(port);
  if (port.enabled)
    ac.begin(Serial1, port.rxPin, port.txPin, AC_BEEPER, AC_AUTO_SWAP_LINES);
  else
    ac.disable(port.where);
  mrcool::pairing::begin(HOMEKIT_SETUP_CODE, HOMEKIT_SETUP_ID);

  homeSpan.setQRID(HOMEKIT_SETUP_ID);
  homeSpan.setSketchVersion(FW_VERSION);
#if STATUS_LED_PIN >= 0 && STATUS_LED_PIXEL
  homeSpan.setStatusPixel(STATUS_LED_PIN);
#elif STATUS_LED_PIN >= 0 && STATUS_LED_ACTIVE_LOW
  homeSpan.setStatusDevice(new ActiveLowLED(STATUS_LED_PIN));
#elif STATUS_LED_PIN >= 0
  homeSpan.setStatusPin(STATUS_LED_PIN);
#endif
#if CONTROL_BUTTON_PIN >= 0
  homeSpan.setControlPin(CONTROL_BUTTON_PIN);
#endif
#if ENABLE_SETUP_AP
  homeSpan.setApSSID(SETUP_AP_SSID);
  homeSpan.setApPassword(SETUP_AP_PASSWORD);
  homeSpan.enableAutoStartAP();
#endif
#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
  homeSpan.setWifiCredentials(WIFI_SSID, WIFI_PASSWORD);
#endif
#if ENABLE_OTA
#ifdef OTA_PASSWORD
  homeSpan.enableOTA(OTA_PASSWORD);
#else
  homeSpan.enableOTA();
#endif
#endif
#if ENABLE_WEB_LOG
  homeSpan.enableWebLog(50, nullptr, "UTC", "log");
#endif
  homeSpan.setPairCallback([](boolean paired) {
    Serial.printf("\n*** HomeKit %s\n\n", paired ? "PAIRED" : "UNPAIRED - ready to pair again");
  });
  // HomeSpan loads its pairing records on the first poll(), so print the
  // banner (which reports pairing status) from there.
  homeSpan.setPollingCallback([]() { mrcool::pairing::printBanner(Serial); });

  homeSpan.begin(Category::AirConditioners, DEVICE_NAME, DEVICE_HOSTNAME, "MrCool-HomeKit");
  buildAccessory();
  addCommands();
}

void loop() {
  homeSpan.poll();
  ac.loop();
}
