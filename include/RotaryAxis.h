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

 private:
  const uint8_t stepPin_;
  const uint8_t dirPin_;
  const uint8_t muxChannel_;
  const float zeroOffsetDeg_;
  const float minDeg_;
  const float maxDeg_;

  FastAccelStepper *stepper_ = nullptr;
  TwoWire *muxBus_ = nullptr;
  float targetDeg_ = 0.0f;
  uint32_t lastDriftCheckMs_ = 0;

  void commandTarget();
};
