// Thin layer over MideaUART: owns the UART, merges HomeKit writes into a
// single control frame, and tracks whether the indoor unit is answering.
#pragma once

#include <Arduino.h>
#include <MideaUART.h>

namespace mrcool {

using dudanov::midea::ac::Control;
using dudanov::midea::ac::FanMode;
using dudanov::midea::ac::Mode;
using dudanov::midea::ac::Preset;
using dudanov::midea::ac::SwingMode;

// Wraps the UART and counts traffic, so we can tell a wiring problem
// (nothing received) from a protocol problem.
class TapStream : public Stream {
 public:
  explicit TapStream(Stream &inner) : inner_(inner) {}
  int available() override { return inner_.available(); }
  int peek() override { return inner_.peek(); }
  int read() override {
    int c = inner_.read();
    if (c >= 0) {
      if (rxBytes_ < kFirstRxLen)
        firstRx_[rxBytes_] = (uint8_t)c;
      rxBytes_++;
      lastRxMs_ = millis();
    }
    return c;
  }
  size_t write(uint8_t b) override {
    txBytes_++;
    return inner_.write(b);
  }
  size_t write(const uint8_t *buf, size_t len) override {
    txBytes_ += len;
    return inner_.write(buf, len);
  }
  void flush() override { inner_.flush(); }

  uint32_t rxBytes() const { return rxBytes_; }
  uint32_t txBytes() const { return txBytes_; }
  uint32_t lastRxMs() const { return lastRxMs_; }
  // The first bytes ever received, as hex, for the web log. Noise and a
  // real frame (which starts with 0xAA) look very different.
  void firstRxHex(char *out, size_t len) const;

 private:
  static constexpr uint32_t kFirstRxLen = 12;

  Stream &inner_;
  uint32_t rxBytes_ = 0;
  uint32_t txBytes_ = 0;
  uint32_t lastRxMs_ = 0;
  uint8_t firstRx_[kFirstRxLen] = {};
};

// Exposes whether a control frame is still waiting for its answer.
// MideaUART silently drops control() calls made during that window.
class AirCon : public dudanov::midea::ac::AirConditioner {
 public:
  bool controlInFlight() const { return m_sendControl; }
};

// Last state reported by the indoor unit. Temperatures are Celsius.
struct AcState {
  Mode mode;
  float targetTemp;
  float indoorTemp;
  float outdoorTemp;
  FanMode fan;
  SwingMode swing;
  Preset preset;
};

class MideaLink {
 public:
  // autoSwap: if the unit stays silent, swap RX/TX and remember what works.
  void begin(HardwareSerial &port, int rxPin, int txPin, bool beeper, bool autoSwap);
  // Leave the AC link off (e.g. the USB port is in use as a console).
  void disable(const char *reason);
  bool enabled() const { return enabled_; }
  void loop();

  // Requests from HomeKit. They are merged and sent as one frame once
  // the bus is free and writes have settled for a moment. Until the unit
  // has sent its status they are dropped (see flushPending()).
  void requestMode(Mode mode);
  void requestTargetTemp(float celsius);
  void requestFan(FanMode fan);
  void requestSwing(SwingMode swing);
  void requestPreset(Preset preset);
  void toggleDisplay() { ac_.displayToggle(); }

  AcState state() const;
  // True once the unit has answered at least one status query.
  bool hasStatus() const { return hasStatus_; }
  // True while a request is queued or in flight. HomeKit values should not
  // be overwritten with stale AC state during this window.
  bool busy() const { return hasPending_ || ac_.controlInFlight(); }
  // Bumps whenever HomeKit should re-read the AC state.
  uint32_t revision() const { return revision_; }
  // True if bytes arrived from the unit in the last 30 seconds.
  bool linkUp() const;

  void setDebug(bool on) { debug_ = on; }
  bool debug() const { return debug_; }
  void printStatus(Print &out) const;

 private:
  void flushPending();
  void onAcStateChanged();
  void checkOrientation(uint32_t now);
  void beginUart();

  AirCon ac_;
  TapStream *tap_ = nullptr;
  HardwareSerial *port_ = nullptr;
  bool enabled_ = false;
  const char *disabledReason_ = nullptr;
  int cfgRxPin_ = -1, cfgTxPin_ = -1;  // as configured
  int rxPin_ = -1, txPin_ = -1;        // in use (may be swapped)
  bool autoSwap_ = false;
  bool orientConfirmed_ = false;
  uint32_t orientSinceMs_ = 0;
  uint32_t rxAtOrient_ = 0;
  uint32_t swapAttempts_ = 0;
  Control pending_{};
  bool hasPending_ = false;
  uint32_t lastRequestMs_ = 0;
  uint32_t resyncAtMs_ = 0;
  uint32_t lastPeriodicMs_ = 0;
  uint32_t revision_ = 0;
  bool hasStatus_ = false;
  bool wasLinkUp_ = false;
  bool warnedNoStatus_ = false;
  uint32_t droppedNotedMs_ = 0;
  bool debug_ = false;
};

const char *modeName(Mode mode);
const char *fanName(FanMode fan);
const char *presetName(Preset preset);
const char *swingName(SwingMode swing);

}  // namespace mrcool
