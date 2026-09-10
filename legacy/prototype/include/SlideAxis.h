#pragma once
// Slide axis: FastAccelStepper driven by the jog encoder's signed
// accumulated value as a live velocity target, with startup homing against
// the two physical limit switches and a continuous overrun safety cutoff.
//
// Unchanged from the final rig's SlideAxis — the slide axis's hardware and
// control scheme are identical between the two builds.

#include <Arduino.h>
#include <ESP32Encoder.h>
#include <FastAccelStepper.h>

class SlideAxis {
 public:
  void begin(FastAccelStepperEngine &engine);
  void update();  // call every loop iteration; fully non-blocking

  bool isHomed() const { return homed_; }
  float positionMm() const;
  int32_t positionSteps() const;
  int32_t jogSignedHz() const { return static_cast<int32_t>(lastSign_) * lastHz_; }

  // Programmed-move interface, kept for parity with the final rig's Shot
  // system even though the prototype doesn't wire up a ShotSequencer yet.
  void beginProgrammedMove(int32_t targetSteps, uint32_t speedHz, uint32_t accelHz);
  void endProgrammedMove(bool stopImmediately);
  bool isMoveComplete() const;

 private:
  enum class HomingState : uint8_t { DRIVING, BACKING_OFF, DONE };
  enum class ControlMode : uint8_t { MANUAL, PROGRAMMED };

  FastAccelStepper *stepper_ = nullptr;
  ESP32Encoder jogEncoder_;
  HomingState homingState_ = HomingState::DRIVING;
  ControlMode mode_ = ControlMode::MANUAL;
  bool homed_ = false;
  int8_t lastSign_ = 0;
  uint32_t lastHz_ = 0;

  uint8_t homingLimitPin() const;
  void startHoming();
  void updateHoming();
  void updateJog();
  void checkLimitSwitches();
};
