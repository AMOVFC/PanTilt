#include "ZAxis.h"

#include "config.h"

void ZAxis::begin(FastAccelStepperEngine &engine) {
  pinMode(pins::LIMIT_Z, INPUT_PULLUP);

  stepper_ = engine.stepperConnectToPin(pins::Z_STEP);
  if (stepper_ == nullptr) {
    Serial.println("ERROR: failed to attach Z stepper to STEP pin");
    return;
  }
  stepper_->setDirectionPin(pins::Z_DIR);
  stepper_->setAcceleration(motion::Z_ACCEL_HZ_PER_S);

  startHoming();
}

void ZAxis::update() {
  updateHoming();
  checkLimitSwitch();  // overrun cutoff applies once homing is done
}

float ZAxis::positionMm() const {
  if (stepper_ == nullptr) return 0.0f;
  return stepper_->getCurrentPosition() / mech::Z_STEPS_PER_MM;
}

int32_t ZAxis::positionSteps() const {
  return stepper_ == nullptr ? 0 : stepper_->getCurrentPosition();
}

void ZAxis::startHoming() {
  if (stepper_ == nullptr) return;
  stepper_->setSpeedInHz(motion::Z_HOMING_SPEED_HZ);
  if (motion::Z_HOME_DIR_FORWARD) {
    stepper_->runForward();
  } else {
    stepper_->runBackward();
  }
}

void ZAxis::updateHoming() {
  if (homed_ || stepper_ == nullptr) return;

  switch (homingState_) {
    case HomingState::DRIVING:
      if (digitalRead(pins::LIMIT_Z) == LOW) {
        stepper_->forceStopAndNewPosition(0);
        const int32_t backoffTarget = motion::Z_HOME_DIR_FORWARD
                                           ? -motion::Z_HOMING_BACKOFF_STEPS
                                           : motion::Z_HOMING_BACKOFF_STEPS;
        stepper_->moveTo(backoffTarget);
        homingState_ = HomingState::BACKING_OFF;
      }
      break;
    case HomingState::BACKING_OFF:
      if (!stepper_->isRunning()) {
        homingState_ = HomingState::DONE;
        homed_ = true;
      }
      break;
    case HomingState::DONE:
      break;
  }
}

// Overrun safety cutoff: stop immediately if Z is driving back toward the
// home switch outside of the homing sequence (e.g. a bad keyframe target).
// There's no second switch at full extension (brief §2) — soft limits in
// config.h are what stop over-travel on that end instead (see Shot.cpp).
void ZAxis::checkLimitSwitch() {
  if (stepper_ == nullptr || homingState_ != HomingState::DONE) return;

  const bool triggered = digitalRead(pins::LIMIT_Z) == LOW;
  const int32_t speedMilliHz = stepper_->getCurrentSpeedInMilliHz();
  const bool movingTowardSwitch =
      motion::Z_HOME_DIR_FORWARD ? speedMilliHz > 0 : speedMilliHz < 0;

  if (triggered && movingTowardSwitch) {
    stepper_->forceStopAndNewPosition(stepper_->getCurrentPosition());
  }
}

void ZAxis::beginProgrammedMove(int32_t targetSteps, uint32_t speedHz, uint32_t accelHz) {
  if (stepper_ == nullptr || !homed_) return;
  stepper_->setSpeedInHz(speedHz);
  stepper_->setAcceleration(static_cast<int32_t>(accelHz));
  stepper_->moveTo(targetSteps);
}

void ZAxis::endProgrammedMove(bool stopImmediately) {
  if (stepper_ != nullptr) {
    if (stopImmediately) {
      stepper_->forceStopAndNewPosition(stepper_->getCurrentPosition());
    }
    stepper_->setAcceleration(motion::Z_ACCEL_HZ_PER_S);
  }
}

bool ZAxis::isMoveComplete() const {
  return stepper_ == nullptr || !stepper_->isRunning();
}
