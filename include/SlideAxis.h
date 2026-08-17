#pragma once
// Slide axis: FastAccelStepper driven by the jog encoder's signed
// accumulated value as a live velocity target, with startup homing against
// the two physical limit switches and a continuous overrun safety cutoff.

#include <Arduino.h>
#include <ESP32Encoder.h>
#include <FastAccelStepper.h>

class SlideAxis {
 public:
  void begin(FastAccelStepperEngine &engine);
  void update();  // call every loop iteration; fully non-blocking

  bool isHomed() const { return homed_; }
  float positionMm() const;
  int32_t jogSignedHz() const { return static_cast<int32_t>(lastSign_) * lastHz_; }

 private:
  enum class HomingState : uint8_t { DRIVING, BACKING_OFF, DONE };

  FastAccelStepper *stepper_ = nullptr;
  ESP32Encoder jogEncoder_;
  HomingState homingState_ = HomingState::DRIVING;
  bool homed_ = false;
  int8_t lastSign_ = 0;
  uint32_t lastHz_ = 0;

  uint8_t homingLimitPin() const;
  void startHoming();
  void updateHoming();
  void updateJog();
  void checkLimitSwitches();
};
