#pragma once
// Pan/tilt axis: angle-setpoint control via FastAccelStepper, with absolute
// position on every boot from an AS5600 (no homing move) and periodic
// idle-only drift correction against that same sensor.

#include <Arduino.h>
#include <FastAccelStepper.h>
#include <Wire.h>

class RotaryAxis {
 public:
  RotaryAxis(uint8_t stepPin, uint8_t dirPin, uint8_t muxChannel,
             float zeroOffsetDeg, float minDeg, float maxDeg);

  void begin(FastAccelStepperEngine &engine, TwoWire &muxBus);
  void nudgeTargetDeg(float deltaDeg);
  void update(uint32_t nowMs);

  float targetDeg() const { return targetDeg_; }
  float currentDeg() const;
  float minDeg() const { return minDeg_; }
  float maxDeg() const { return maxDeg_; }
  int32_t positionSteps() const;

  // Programmed-move interface for ShotSequencer (see Shot.h). Suspends
  // nudgeTargetDeg() and idle drift-correction until endProgrammedMove().
  void beginProgrammedMove(int32_t targetSteps, uint32_t speedHz, uint32_t accelHz);
  void endProgrammedMove(bool stopImmediately);
  bool isMoveComplete() const;

 private:
  enum class ControlMode : uint8_t { MANUAL, PROGRAMMED };

  const uint8_t stepPin_;
  const uint8_t dirPin_;
  const uint8_t muxChannel_;
  const float zeroOffsetDeg_;
  const float minDeg_;
  const float maxDeg_;

  FastAccelStepper *stepper_ = nullptr;
  TwoWire *muxBus_ = nullptr;
  ControlMode mode_ = ControlMode::MANUAL;
  float targetDeg_ = 0.0f;
  uint32_t lastDriftCheckMs_ = 0;

  void commandTarget();
};
