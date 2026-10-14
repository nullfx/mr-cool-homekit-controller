#include "MideaLink.h"

#include <HomeSpan.h>  // homeSpan.addWebLog()
#include <Helpers/Log.h>
#include <Preferences.h>
#include <utility>

namespace mrcool {

using namespace dudanov::midea::ac;

// Wait this long after the last HomeKit write before sending, so that
// Active + mode + temperature arriving separately go out as one frame.
static constexpr uint32_t kSettleMs = 250;
// Re-read state into HomeKit this long after a command, even if nothing
// changed, so a command the unit rejected snaps the Home app back.
static constexpr uint32_t kResyncMs = 1500;
static constexpr uint32_t kPeriodicResyncMs = 60000;
static constexpr uint32_t kLinkTimeoutMs = 30000;
static constexpr uint32_t kNoRxWarningMs = 20000;
// With auto-swap on, try the other RX/TX orientation after this much silence.
static constexpr uint32_t kSwapAfterMs = 8000;

// Prints to the serial console and records in the web log.
static void note(const char *fmt, ...) {
  char buf[200];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.printf("[mrcool] %s\n", buf);
  homeSpan.addWebLog(false, "%s", buf);
}

// NVS key remembering that the configured pins work swapped.
static void swapKey(char out[16], int rxPin, int txPin) {
  snprintf(out, 16, "sw%d_%d", rxPin, txPin);
}

void MideaLink::begin(HardwareSerial &port, int rxPin, int txPin, bool beeper, bool autoSwap) {
  port_ = &port;
  cfgRxPin_ = rxPin_ = rxPin;
  cfgTxPin_ = txPin_ = txPin;
  autoSwap_ = autoSwap;
  enabled_ = true;

  if (autoSwap_) {
    char key[16];
    swapKey(key, cfgRxPin_, cfgTxPin_);
    Preferences prefs;
    prefs.begin("mrcool", false);
    if (prefs.getBool(key, false))
      std::swap(rxPin_, txPin_);
    prefs.end();
  }

  port.begin(9600, SERIAL_8N1, rxPin_, txPin_);
  tap_ = new TapStream(port);
  orientSinceMs_ = millis();

  dudanov::midea::ApplianceBase::setLogger(
      [this](int level, const char *tag, int line, String format, va_list args) {
        if (level > LOG_LEVEL_CONFIG && !debug_)
          return;
        char msg[160];
        vsnprintf(msg, sizeof(msg), format.c_str(), args);
        Serial.printf("[midea:%s] %s\n", tag, msg);
      });

  ac_.setStream(tap_);
  ac_.setBeeper(beeper);
  ac_.setAutoconf(true);  // ask the unit for its capability report (see `@s`)
  ac_.addOnStateCallback([this]() { onAcStateChanged(); });
  ac_.setup();
}

void MideaLink::disable(const char *reason) {
  enabled_ = false;
  disabledReason_ = reason;
}

void MideaLink::checkOrientation(uint32_t now) {
  if (!autoSwap_ || orientConfirmed_)
    return;
  if (tap_->rxBytes() > rxAtOrient_) {
    orientConfirmed_ = true;
    const bool swapped = rxPin_ != cfgRxPin_;
    char key[16];
    swapKey(key, cfgRxPin_, cfgTxPin_);
    Preferences prefs;
    prefs.begin("mrcool", false);
    if (prefs.getBool(key, false) != swapped)
      prefs.putBool(key, swapped);
    prefs.end();
    note("UART lines confirmed: RX=GPIO%d, TX=GPIO%d%s", rxPin_, txPin_,
         swapped ? " (swapped from config)" : "");
    return;
  }
  if (now - orientSinceMs_ < kSwapAfterMs)
    return;
  std::swap(rxPin_, txPin_);
  port_->end();
  port_->begin(9600, SERIAL_8N1, rxPin_, txPin_);
  orientSinceMs_ = now;
  rxAtOrient_ = tap_->rxBytes();
  swapAttempts_++;
  if (swapAttempts_ <= 4)
    note("No data from the indoor unit; swapping lines to RX=GPIO%d, TX=GPIO%d", rxPin_, txPin_);
}

void MideaLink::loop() {
  if (!enabled_)
    return;
  ac_.loop();
  flushPending();

  const uint32_t now = millis();
  if (resyncAtMs_ && (int32_t)(now - resyncAtMs_) >= 0 && !busy()) {
    resyncAtMs_ = 0;
    revision_++;
  }
  if (now - lastPeriodicMs_ >= kPeriodicResyncMs) {
    lastPeriodicMs_ = now;
    revision_++;
  }
  checkOrientation(now);

  const bool up = linkUp();
  if (up != wasLinkUp_) {
    wasLinkUp_ = up;
    if (up)
      note("Indoor unit is answering on the UART");
    else
      note("No data from the indoor unit for %lus", (unsigned long)(kLinkTimeoutMs / 1000));
  }
  if (!warnedNoRx_ && tap_->rxBytes() == 0 && now > kNoRxWarningMs) {
    warnedNoRx_ = true;
    if (autoSwap_)
      note("Nothing received from the indoor unit on either line orientation. "
           "Check the cable, 5V/GND and (if wired) the level shifter.");
    else
      note("Nothing received from the indoor unit yet. Check 5V/GND, the level shifter, "
           "and try swapping the D+/D- wires.");
  }
}

void MideaLink::onAcStateChanged() {
  if (!hasStatus_) {
    hasStatus_ = true;
    note("First status from the indoor unit: %s, set point %.1fC, room %.1fC",
           modeName(ac_.getMode()), ac_.getTargetTemp(), ac_.getIndoorTemp());
  }
  revision_++;
}

void MideaLink::requestMode(Mode mode) {
  pending_.mode = mode;
  hasPending_ = true;
  lastRequestMs_ = millis();
}

void MideaLink::requestTargetTemp(float celsius) {
  pending_.targetTemp = celsius;
  hasPending_ = true;
  lastRequestMs_ = millis();
}

void MideaLink::requestFan(FanMode fan) {
  pending_.fanMode = fan;
  hasPending_ = true;
  lastRequestMs_ = millis();
}

void MideaLink::requestSwing(SwingMode swing) {
  pending_.swingMode = swing;
  hasPending_ = true;
  lastRequestMs_ = millis();
}

void MideaLink::requestPreset(Preset preset) {
  pending_.preset = preset;
  hasPending_ = true;
  lastRequestMs_ = millis();
}

void MideaLink::flushPending() {
  if (!hasPending_ || ac_.controlInFlight() || millis() - lastRequestMs_ < kSettleMs)
    return;
  if (debug_) {
    Serial.printf("[mrcool] control:%s%s%s%s%s\n",
                  pending_.mode.hasValue() ? " mode" : "",
                  pending_.targetTemp.hasValue() ? " temp" : "",
                  pending_.fanMode.hasValue() ? " fan" : "",
                  pending_.swingMode.hasValue() ? " swing" : "",
                  pending_.preset.hasValue() ? " preset" : "");
  }
  ac_.control(pending_);
  pending_ = Control{};
  hasPending_ = false;
  resyncAtMs_ = millis() + kResyncMs;
  if (resyncAtMs_ == 0)
    resyncAtMs_ = 1;
}

AcState MideaLink::state() const {
  return AcState{ac_.getMode(),        ac_.getTargetTemp(), ac_.getIndoorTemp(),
                 ac_.getOutdoorTemp(), ac_.getFanMode(),    ac_.getSwingMode(),
                 ac_.getPreset()};
}

bool MideaLink::linkUp() const {
  return enabled_ && tap_ && tap_->lastRxMs() != 0 && millis() - tap_->lastRxMs() < kLinkTimeoutMs;
}

void MideaLink::printStatus(Print &out) const {
  out.printf("\n*** Indoor unit\n");
  if (!enabled_) {
    out.printf("  AC link off - %s.\n  Plug the board into the AC to control it.\n\n",
               disabledReason_ ? disabledReason_ : "not started");
    return;
  }
  const AcState s = state();
  out.printf("  UART pins   : RX=GPIO%d TX=GPIO%d%s\n", rxPin_, txPin_,
             !autoSwap_ ? "" : (orientConfirmed_ ? " (confirmed)" : " (auto-swap searching)"));
  out.printf("  UART link   : %s (rx %lu bytes, tx %lu bytes",
             linkUp() ? "UP" : "DOWN", (unsigned long)(tap_ ? tap_->rxBytes() : 0),
             (unsigned long)(tap_ ? tap_->txBytes() : 0));
  if (tap_ && tap_->lastRxMs())
    out.printf(", last rx %lus ago)\n", (unsigned long)((millis() - tap_->lastRxMs()) / 1000));
  else
    out.printf(", nothing received yet)\n");
  if (!hasStatus_) {
    out.printf("  No status received yet.%s\n\n",
               autoSwap_ ? " The firmware tries both line orientations by itself."
                         : " If rx stays at 0, swap the D+/D- wires.");
    return;
  }
  out.printf("  Mode        : %s\n", modeName(s.mode));
  out.printf("  Set point   : %.1f C (%.0f F)\n", s.targetTemp, s.targetTemp * 1.8f + 32.0f);
  out.printf("  Indoor temp : %.1f C\n", s.indoorTemp);
  out.printf("  Outdoor temp: %.1f C\n", s.outdoorTemp);
  out.printf("  Fan         : %s (%u)\n", fanName(s.fan), (unsigned)s.fan);
  out.printf("  Swing       : %s\n", swingName(s.swing));
  out.printf("  Preset      : %s\n", presetName(s.preset));
  switch (ac_.getAutoconfStatus()) {
    case dudanov::midea::AUTOCONF_OK:
      out.printf("  Capabilities: received (dump follows)\n");
      ac_.getCapabilities().dump();
      break;
    case dudanov::midea::AUTOCONF_ERROR:
      out.printf("  Capabilities: unit did not send a report (normal on some models)\n");
      break;
    default:
      out.printf("  Capabilities: pending\n");
      break;
  }
  out.printf("\n");
}

const char *modeName(Mode mode) {
  switch (mode) {
    case MODE_OFF: return "OFF";
    case MODE_AUTO: return "AUTO";
    case MODE_COOL: return "COOL";
    case MODE_DRY: return "DRY";
    case MODE_HEAT: return "HEAT";
    case MODE_FAN_ONLY: return "FAN_ONLY";
  }
  return "?";
}

const char *fanName(FanMode fan) {
  switch (fan) {
    case FAN_AUTO: return "AUTO";
    case FAN_SILENT: return "SILENT";
    case FAN_LOW: return "LOW";
    case FAN_MEDIUM: return "MEDIUM";
    case FAN_HIGH: return "HIGH";
    case FAN_TURBO: return "TURBO";
  }
  return "custom";
}

const char *presetName(Preset preset) {
  switch (preset) {
    case PRESET_NONE: return "NONE";
    case PRESET_SLEEP: return "SLEEP";
    case PRESET_TURBO: return "TURBO";
    case PRESET_ECO: return "ECO";
    case PRESET_FREEZE_PROTECTION: return "FREEZE_PROTECTION";
  }
  return "?";
}

const char *swingName(SwingMode swing) {
  switch (swing) {
    case SWING_OFF: return "OFF";
    case SWING_VERTICAL: return "VERTICAL";
    case SWING_HORIZONTAL: return "HORIZONTAL";
    case SWING_BOTH: return "BOTH";
  }
  return "?";
}

}  // namespace mrcool
