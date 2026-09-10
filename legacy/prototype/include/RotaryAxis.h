#pragma once
// Pan/tilt axis: angle-setpoint control via FastAccelStepper, with absolute
// position on every boot from an AS5600 (no homing move) and periodic
// idle-only drift correction against that same sensor.
//
// Ported from the final rig's RotaryAxis with one addition: setTargetDeg(),
// used by the prototype's per-axis encoder push-button (recenter to 0deg).
// Everything else is unchanged, so this stays a drop-in match for the final
// rig's version if the two ever need to be reconciled.

#include <Arduino.h>
#include <FastAccelStepper.h>
#include <Wire.h>

class RotaryAxis {
 public:
  // The calibration offset and soft limits are bound by reference, not
  // copied: these objects are constructed at static-init time, before
  // settings are loaded from NVS, and the values stay editable from the web
  // UI afterwards. Binding the address is safe at static-init time; only
  // the read has to happen later.
  RotaryAxis(uint8_t stepPin, uint8_t dirPin, uint8_t muxChannel,
             const float &zeroOffsetDeg, const float &minDeg, const float &maxDeg);

  void begin(FastAccelStepperEngine &engine, TwoWire &muxBus);
  void nudgeTargetDeg(float deltaDeg);
  void setTargetDeg(float deg);  // absolute set, e.g. recenter-button
  void update(uint32_t nowMs);

  float targetDeg() const { return targetDeg_; }
  float currentDeg() const;
  float minDeg() const { return *minDeg_; }
  float maxDeg() const { return *maxDeg_; }
  int32_t positionSteps() const;

  // Programmed-move interface, kept for parity with the final rig's Shot
  // system even though the prototype doesn't wire up a ShotSequencer yet.
  void beginProgrammedMove(int32_t targetSteps, uint32_t speedHz, uint32_t accelHz);
  void endProgrammedMove(bool stopImmediately);
  bool isMoveComplete() const;

 private:
  enum class ControlMode : uint8_t { MANUAL, PROGRAMMED };

  const uint8_t stepPin_;
  const uint8_t dirPin_;
  const uint8_t muxChannel_;
  const float *const zeroOffsetDeg_;
  const float *const minDeg_;
  const float *const maxDeg_;

  FastAccelStepper *stepper_ = nullptr;
  TwoWire *muxBus_ = nullptr;
  ControlMode mode_ = ControlMode::MANUAL;
  float targetDeg_ = 0.0f;
  uint32_t lastDriftCheckMs_ = 0;

  void commandTarget();
};
