#pragma once
// Z (height) axis: homed once against a single limit switch at startup,
// then tracked purely by step count (no absolute encoder, unlike pan/tilt —
// see brief §2/§6.5). No live jog control: the pin map has no spare encoder
// assigned to Z, so it only moves via ShotSequencer programmed moves.
//
// Leadscrew self-locking under gravity load isn't guaranteed (brief §6.5).
// Holding torque is handled globally, not per-axis: EN is a single line
// shared across all 4 TMC2209s and held enabled continuously from setup()
// onward (see main.cpp's setupDriverEnable()) — there's no way to disable Z
// independently of the other 3 axes, so the driver never loses holding
// torque between keyframes regardless of whether the leadscrew self-locks.

#include <Arduino.h>
#include <FastAccelStepper.h>

class ZAxis {
 public:
  void begin(FastAccelStepperEngine &engine);
  void update();  // call every loop iteration; fully non-blocking

  bool isHomed() const { return homed_; }
  float positionMm() const;
  int32_t positionSteps() const;

  // Programmed-move interface for ShotSequencer (see Shot.h).
  void beginProgrammedMove(int32_t targetSteps, uint32_t speedHz, uint32_t accelHz);
  void endProgrammedMove(bool stopImmediately);
  bool isMoveComplete() const;

 private:
  enum class HomingState : uint8_t { DRIVING, BACKING_OFF, DONE };

  FastAccelStepper *stepper_ = nullptr;
  HomingState homingState_ = HomingState::DRIVING;
  bool homed_ = false;

  void startHoming();
  void updateHoming();
  void checkLimitSwitch();
};
