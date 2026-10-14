// HomeKit services for the mini-split, built on HomeSpan.
//
// The HeaterCooler service is the main tile (power, Auto/Heat/Cool, set
// point, fan speed, oscillation). Modes HomeKit has no word for (fan only,
// dry) and the unit's presets are exposed as extra services in the same
// accessory. Every service reads back from the AC, so the Home app always
// shows what the unit actually accepted.
#pragma once

#include <HomeSpan.h>
#include <math.h>

#include "MideaLink.h"
#include "config.h"

namespace mrcool {

using namespace dudanov::midea::ac;

// --- value mapping ---------------------------------------------------------

// The Home app fan slider snaps to 20% steps that line up with the unit's
// own speeds: 20 silent, 40 low, 60 medium, 80 high, 100 turbo.
inline FanMode fanFromPercent(float pct) {
  if (pct <= 30) return FAN_SILENT;
  if (pct <= 50) return FAN_LOW;
  if (pct <= 70) return FAN_MEDIUM;
  if (pct <= 90) return FAN_HIGH;
  return FAN_TURBO;
}

// Slider position for a fan mode, or -1 for automatic.
inline int percentFromFan(FanMode fan) {
  if (fan > 100)
    return -1;  // FAN_AUTO (102) and other non-speed values
  int pct = ((fan + 10) / 20) * 20;
  return pct < 20 ? 20 : pct;
}

// Modes that the HeaterCooler tile owns. Dry and fan-only belong to their
// own services when those are enabled.
inline bool isClimateMode(Mode m) {
  switch (m) {
    case MODE_AUTO:
    case MODE_COOL:
    case MODE_HEAT: return true;
    case MODE_DRY: return !ENABLE_DRY_SWITCH;
    case MODE_FAN_ONLY: return !ENABLE_FAN_ONLY_SERVICE;
    default: return false;
  }
}

inline float clampSetPoint(float c) {
  return c < AC_MIN_TEMP_C ? AC_MIN_TEMP_C : (c > AC_MAX_TEMP_C ? AC_MAX_TEMP_C : c);
}

template <typename T>
inline void syncVal(SpanCharacteristic *c, T v) {
  if (c->getVal<T>() != v)
    c->setVal(v);
}

inline void syncTemp(SpanCharacteristic *c, float v, float minC, float maxC) {
  if (!isfinite(v) || v < minC || v > maxC)
    return;
  if (fabsf(c->getVal<float>() - v) >= 0.05f)
    c->setVal(v);
}

// Lets a service skip work until the AC has news, and keeps it from
// overwriting a value the user just changed before the AC confirms.
class AcFollower {
 protected:
  explicit AcFollower(MideaLink &ac) : ac_(ac) {}
  bool shouldSync() {
    if (!ac_.hasStatus() || ac_.busy() || ac_.revision() == seen_)
      return false;
    seen_ = ac_.revision();
    return true;
  }
  MideaLink &ac_;

 private:
  uint32_t seen_ = 0;
};

// --- main tile -------------------------------------------------------------

struct ClimateService : Service::HeaterCooler, AcFollower {
  SpanCharacteristic *active;
  SpanCharacteristic *currentTemp;
  SpanCharacteristic *currentState;
  SpanCharacteristic *targetState;
  SpanCharacteristic *coolingThreshold;
  SpanCharacteristic *heatingThreshold;
  SpanCharacteristic *speed;
  SpanCharacteristic *swing;

  explicit ClimateService(MideaLink &ac) : Service::HeaterCooler(), AcFollower(ac) {
    active = new Characteristic::Active(0);
    currentTemp = (new Characteristic::CurrentTemperature(21))->setRange(-20, 60);
    currentState = new Characteristic::CurrentHeaterCoolerState(0);
    targetState = new Characteristic::TargetHeaterCoolerState(0);
#if !AC_HAS_HEAT
    targetState->setValidValues(2, 0, 2);  // AUTO and COOL, no HEAT
#endif
    coolingThreshold = (new Characteristic::CoolingThresholdTemperature(24))
                           ->setRange(AC_MIN_TEMP_C, AC_MAX_TEMP_C, 0.5);
    heatingThreshold = (new Characteristic::HeatingThresholdTemperature(21))
                           ->setRange(AC_MIN_TEMP_C, AC_MAX_TEMP_C, 0.5);
    speed = (new Characteristic::RotationSpeed(60))->setRange(0, 100, 20);
    swing = new Characteristic::SwingMode(0);
  }

  static Mode modeForTarget(int target) {
    switch (target) {
      case 1: return MODE_HEAT;
      case 2: return MODE_COOL;
      default: return MODE_AUTO;
    }
  }

  boolean update() override {
    if (active->updated() || targetState->updated()) {
      if (active->getNewVal() == 1)
        ac_.requestMode(modeForTarget(targetState->getNewVal()));
      else if (active->updated())
        ac_.requestMode(MODE_OFF);
    }

    // The unit has a single set point. In Auto the Home app sends a
    // heat/cool range; aim for its middle.
    const bool cool = coolingThreshold->updated();
    const bool heat = heatingThreshold->updated();
    if (cool || heat) {
      float t;
      if (cool && heat) {
        const int target = targetState->getNewVal();
        const float c = coolingThreshold->getNewVal<float>();
        const float h = heatingThreshold->getNewVal<float>();
        t = target == 1 ? h : (target == 2 ? c : (c + h) / 2.0f);
      } else {
        t = cool ? coolingThreshold->getNewVal<float>() : heatingThreshold->getNewVal<float>();
      }
      ac_.requestTargetTemp(clampSetPoint(t));
    }

    // 0% comes with Active=0 (turning off), so it needs no handling here.
    if (speed->updated() && speed->getNewVal<float>() > 0)
      ac_.requestFan(fanFromPercent(speed->getNewVal<float>()));

    if (swing->updated())
      ac_.requestSwing(swing->getNewVal() ? AC_SWING_MODE : SWING_OFF);

    return true;
  }

  void loop() override {
    if (!shouldSync())
      return;
    const AcState s = ac_.state();
    const bool on = isClimateMode(s.mode);

    syncVal(active, on ? 1 : 0);
    if (s.mode == MODE_HEAT)
      syncVal(targetState, 1);
    else if (s.mode == MODE_COOL || s.mode == MODE_DRY)
      syncVal(targetState, 2);
    else if (s.mode == MODE_AUTO)
      syncVal(targetState, 0);

    syncTemp(currentTemp, s.indoorTemp, -20, 60);
    syncTemp(coolingThreshold, clampSetPoint(s.targetTemp), AC_MIN_TEMP_C, AC_MAX_TEMP_C);
    syncTemp(heatingThreshold, clampSetPoint(s.targetTemp), AC_MIN_TEMP_C, AC_MAX_TEMP_C);
    syncVal(currentState, currentStateFor(s, on));

    const int pct = percentFromFan(s.fan);
    if (pct > 0)
      syncVal(speed, pct);
    syncVal(swing, s.swing != SWING_OFF ? 1 : 0);
  }

  // The UART status does not say whether the compressor is running, so
  // infer heating/cooling from room temperature versus the set point.
  static int currentStateFor(const AcState &s, bool on) {
    enum { INACTIVE = 0, IDLE = 1, HEATING = 2, COOLING = 3 };
    if (!on)
      return INACTIVE;
    const float diff = s.indoorTemp - s.targetTemp;
    switch (s.mode) {
      case MODE_HEAT: return diff < -0.5f ? HEATING : IDLE;
      case MODE_COOL:
      case MODE_DRY: return diff > 0.5f ? COOLING : IDLE;
      case MODE_AUTO: return diff < -1.0f ? HEATING : (diff > 1.0f ? COOLING : IDLE);
      default: return IDLE;
    }
  }
};

// --- fan-only mode ---------------------------------------------------------

struct FanOnlyService : Service::Fan, AcFollower {
  SpanCharacteristic *active;
  SpanCharacteristic *speed;

  FanOnlyService(MideaLink &ac, const char *name) : Service::Fan(), AcFollower(ac) {
    new Characteristic::ConfiguredName(name);
    active = new Characteristic::Active(0);
    speed = (new Characteristic::RotationSpeed(60))->setRange(0, 100, 20);
  }

  boolean update() override {
    if (active->updated()) {
      if (active->getNewVal() == 1)
        ac_.requestMode(MODE_FAN_ONLY);
      else if (ac_.state().mode == MODE_FAN_ONLY)
        ac_.requestMode(MODE_OFF);
    }
    if (speed->updated() && speed->getNewVal<float>() > 0)
      ac_.requestFan(fanFromPercent(speed->getNewVal<float>()));
    return true;
  }

  void loop() override {
    if (!shouldSync())
      return;
    const AcState s = ac_.state();
    syncVal(active, s.mode == MODE_FAN_ONLY ? 1 : 0);
    const int pct = percentFromFan(s.fan);
    if (pct > 0)
      syncVal(speed, pct);
  }
};

// --- simple switches -------------------------------------------------------

// On = unit in `mode`. Off = power off, but only if it is still in `mode`.
struct ModeSwitch : Service::Switch, AcFollower {
  SpanCharacteristic *on;
  Mode mode;

  ModeSwitch(MideaLink &ac, Mode m, const char *name) : Service::Switch(), AcFollower(ac), mode(m) {
    new Characteristic::ConfiguredName(name);
    on = new Characteristic::On(false);
  }

  boolean update() override {
    if (on->getNewVal())
      ac_.requestMode(mode);
    else if (ac_.state().mode == mode)
      ac_.requestMode(MODE_OFF);
    return true;
  }

  void loop() override {
    if (shouldSync())
      syncVal(on, ac_.state().mode == mode);
  }
};

// On = preset active. The unit only allows some presets in some modes
// (ECO: cool; TURBO: heat/cool; SLEEP: not dry/fan), so an impossible
// request snaps back to off after the next status read.
struct PresetSwitch : Service::Switch, AcFollower {
  SpanCharacteristic *on;
  Preset preset;

  PresetSwitch(MideaLink &ac, Preset p, const char *name)
      : Service::Switch(), AcFollower(ac), preset(p) {
    new Characteristic::ConfiguredName(name);
    on = new Characteristic::On(false);
  }

  boolean update() override {
    if (on->getNewVal())
      ac_.requestPreset(preset);
    else if (ac_.state().preset == preset)
      ac_.requestPreset(PRESET_NONE);
    return true;
  }

  void loop() override {
    if (shouldSync())
      syncVal(on, ac_.state().preset == preset);
  }
};

// On = automatic fan speed. Off goes back to the last manual speed.
struct AutoFanSwitch : Service::Switch, AcFollower {
  SpanCharacteristic *on;
  FanMode lastManual = FAN_MEDIUM;

  AutoFanSwitch(MideaLink &ac, const char *name) : Service::Switch(), AcFollower(ac) {
    new Characteristic::ConfiguredName(name);
    on = new Characteristic::On(true);
  }

  boolean update() override {
    ac_.requestFan(on->getNewVal() ? FAN_AUTO : lastManual);
    return true;
  }

  void loop() override {
    if (!shouldSync())
      return;
    const FanMode fan = ac_.state().fan;
    if (fan != FAN_AUTO && percentFromFan(fan) > 0)
      lastManual = fan;
    syncVal(on, fan == FAN_AUTO);
  }
};

// --- sensors ---------------------------------------------------------------

struct OutdoorTempService : Service::TemperatureSensor, AcFollower {
  SpanCharacteristic *temp;
  SpanCharacteristic *statusActive;

  OutdoorTempService(MideaLink &ac, const char *name) : Service::TemperatureSensor(), AcFollower(ac) {
    new Characteristic::ConfiguredName(name);
    temp = (new Characteristic::CurrentTemperature(10))->setRange(-40, 60);
    statusActive = new Characteristic::StatusActive(false);
  }

  void loop() override {
    if (!shouldSync())
      return;
    // The unit reports nonsense (e.g. 102.5 C) when it has no reading.
    const float t = ac_.state().outdoorTemp;
    const bool valid = isfinite(t) && t >= -40 && t <= 60;
    syncVal(statusActive, valid && ac_.linkUp());
    syncTemp(temp, t, -40, 60);
  }
};

}  // namespace mrcool
