#pragma once
// One motion axis: a FastAccelStepper channel, an optional TMC2209 on the
// shared UART bus, optional limit switches and an optional AS5600 absolute
// encoder. Both linear (mm) and rotary (deg) axes use this class -- the only
// difference is AxisConfig::stepsPerUnit() and the unit label.
//
// All public setpoints are in user units. Steps exist only inside this file.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastAccelStepper.h>
#include <TMCStepper.h>
#include <Wire.h>

#include "Settings.h"

enum class HomingState : uint8_t { IDLE, SEEKING, BACKING_OFF, DONE, FAILED };

class Axis {
 public:
  // `muxBus` may be null when the axis has no AS5600; `tmcSerial` may be null
  // when the axis is configured with tmc_enabled = false.
  void begin(uint8_t index, FastAccelStepperEngine &engine, TwoWire *muxBus,
             HardwareSerial *tmcSerial);
  void update(uint32_t nowMs);

  // --- commands (all in user units) ---
  bool moveTo(float units, String &error);
  bool moveTo(float units, float unitsPerSec, String &error);
  // Move that is scheduled to take `durationS`, so several axes commanded
  // together arrive together. With `ease` the ramp is stretched across the
  // whole move (triangular profile) instead of snapping to cruise speed --
  // that is the difference between a usable camera move and a lurch.
  bool moveTimed(float units, float durationS, bool ease, String &error);
  // Shortest time this axis can cover from->to without exceeding its
  // configured speed and acceleration. The sequencer uses it to stretch a
  // too-fast keyframe rather than let one axis quietly fall behind.
  float minMoveTime(float fromUnits, float toUnits, bool ease) const;
  bool nudge(float deltaUnits, String &error);
  // Continuous run until jogStop()/stop(). `dir` is +1 or -1.
  void jog(int8_t dir, float unitsPerSec);
  void stop();       // decelerate to a halt
  void forceStop();  // immediate, drops the remaining ramp
  void startHoming();
  void setPositionUnits(float units);  // redefine "where I am" without moving
  // Forget the homing reference, without moving. Used after the drivers have
  // been de-energised, when the axis may have been shifted by hand or gravity.
  void invalidateHoming();

  // --- state ---
  bool available() const { return stepper_ != nullptr; }
  bool isRunning() const;
  bool isHomed() const { return homed_; }
  HomingState homingState() const { return homingState_; }
  float positionUnits() const;
  float targetUnits() const { return targetUnits_; }
  float speedUnitsPerSec() const;
  bool limitMinTriggered() const;
  bool limitMaxTriggered() const;
  // Last AS5600 reading in degrees after the zero offset, or NAN when the
  // axis has no feedback sensor.
  float sensorDeg() const { return sensorDeg_; }
  const AxisConfig &cfg() const;

  // --- TMC2209 ---
  bool tmcPresent() const { return tmcPresent_; }
  // Pushes current/microstep/stealthChop settings to the driver. Safe to call
  // at run time; the UI calls it after an axis config change.
  void applyDriverConfig();
  void driverStatusJson(JsonObject out);

  void telemetryJson(JsonObject out) const;

 private:
  uint8_t index_ = 0;
  FastAccelStepper *stepper_ = nullptr;
  TwoWire *muxBus_ = nullptr;
  TMC2209Stepper *tmc_ = nullptr;
  bool tmcPresent_ = false;

  bool homed_ = false;
  HomingState homingState_ = HomingState::IDLE;
  uint32_t homingStartedMs_ = 0;

  float targetUnits_ = 0.0f;
  float sensorDeg_ = NAN;
  uint32_t lastDriftCheckMs_ = 0;

  // Live jog state, so a repeated jog command at the same speed does not
  // re-issue runForward() and restart the ramp every telemetry tick.
  int8_t jogDir_ = 0;
  uint32_t jogHz_ = 0;

  int32_t unitsToSteps(float units) const;
  float stepsToUnits(int32_t steps) const;
  uint32_t unitsPerSecToHz(float unitsPerSec) const;
  void applyRamp(float unitsPerSec, float accelUnitsPerSec2);
  bool limitTriggered(uint8_t pin) const;
  void updateHoming(uint32_t nowMs);
  void enforceLimitSwitches();
  void updateDrift(uint32_t nowMs);
  bool readSensorDeg(float &outDeg);
  bool clampToSoftLimits(float &units, String &error) const;
};
