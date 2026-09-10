#include "SlideAxis.h"

#include "config.h"

void SlideAxis::begin(FastAccelStepperEngine &engine) {
  pinMode(pins::LIMIT_SLIDE_MIN, INPUT_PULLUP);
  pinMode(pins::LIMIT_SLIDE_MAX, INPUT_PULLUP);

  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  jogEncoder_.attachHalfQuad(pins::SLIDE_ENC_A, pins::SLIDE_ENC_B);
  jogEncoder_.setCount(0);

  stepper_ = engine.stepperConnectToPin(pins::SLIDE_STEP);
  if (stepper_ == nullptr) {
    Serial.println("ERROR: failed to attach slide stepper to STEP pin");
    return;
  }
  stepper_->setDirectionPin(pins::SLIDE_DIR, !motion::SLIDE_DIR_INVERT);
  stepper_->setAcceleration(motion::SLIDE_ACCEL_HZ_PER_S);

  startHoming();
}

void SlideAxis::update() {
  updateHoming();
  checkLimitSwitches();  // overrun cutoff applies in every mode
  if (mode_ == ControlMode::MANUAL) {
    updateJog();
  }
}

float SlideAxis::positionMm() const {
  if (stepper_ == nullptr) return 0.0f;
  return stepper_->getCurrentPosition() / mech::SLIDE_STEPS_PER_MM;
}

int32_t SlideAxis::positionSteps() const {
  return stepper_ == nullptr ? 0 : stepper_->getCurrentPosition();
}

void SlideAxis::beginProgrammedMove(int32_t targetSteps, uint32_t speedHz,
                                     uint32_t accelHz) {
  if (stepper_ == nullptr || !homed_) return;
  mode_ = ControlMode::PROGRAMMED;
  stepper_->setSpeedInHz(speedHz);
  stepper_->setAcceleration(static_cast<int32_t>(accelHz));
  stepper_->moveTo(targetSteps);
}

void SlideAxis::endProgrammedMove(bool stopImmediately) {
  if (stepper_ != nullptr) {
    if (stopImmediately) {
      stepper_->forceStopAndNewPosition(stepper_->getCurrentPosition());
    }
    stepper_->setAcceleration(motion::SLIDE_ACCEL_HZ_PER_S);
  }
  // Ignore any knob movement that happened during the programmed move, same
  // as the homing path does, so jog doesn't resume with a phantom jump.
  jogEncoder_.setCount(0);
  lastSign_ = 0;
  lastHz_ = 0;
  mode_ = ControlMode::MANUAL;
}

bool SlideAxis::isMoveComplete() const {
  return stepper_ == nullptr || !stepper_->isRunning();
}

uint8_t SlideAxis::homingLimitPin() const {
  return motion::SLIDE_HOME_TOWARD_MIN ? pins::LIMIT_SLIDE_MIN
                                        : pins::LIMIT_SLIDE_MAX;
}

void SlideAxis::startHoming() {
  if (stepper_ == nullptr) return;
  stepper_->setSpeedInHz(motion::SLIDE_HOMING_SPEED_HZ);
  if (motion::SLIDE_HOME_TOWARD_MIN) {
    stepper_->runBackward();
  } else {
    stepper_->runForward();
  }
}

void SlideAxis::updateHoming() {
  if (homed_ || stepper_ == nullptr) return;

  switch (homingState_) {
    case HomingState::DRIVING:
      if (digitalRead(homingLimitPin()) == LOW) {
        stepper_->forceStopAndNewPosition(0);
        const int32_t backoffTarget = motion::SLIDE_HOME_TOWARD_MIN
                                           ? motion::SLIDE_HOMING_BACKOFF_STEPS
                                           : -motion::SLIDE_HOMING_BACKOFF_STEPS;
        stepper_->moveTo(backoffTarget);
        homingState_ = HomingState::BACKING_OFF;
      }
      break;
    case HomingState::BACKING_OFF:
      if (!stepper_->isRunning()) {
        jogEncoder_.setCount(0);  // ignore any knob movement during homing
        homingState_ = HomingState::DONE;
        homed_ = true;
      }
      break;
    case HomingState::DONE:
      break;
  }
}

// Maps the jog encoder's signed count directly to a live velocity target:
// CW increases speed positive, CCW increases speed negative, back to zero
// stops. Only touches the stepper when the target actually changes.
void SlideAxis::updateJog() {
  if (stepper_ == nullptr || !homed_) return;

  const int64_t rawCount = jogEncoder_.getCount();
  const int32_t clamped = constrain(
      static_cast<int32_t>(rawCount), -motion::JOG_COUNTS_PER_MAX_SPEED,
      motion::JOG_COUNTS_PER_MAX_SPEED);
  const uint32_t targetHz = static_cast<uint32_t>(
      (static_cast<int64_t>(abs(clamped)) * motion::SLIDE_MAX_SPEED_HZ) /
      motion::JOG_COUNTS_PER_MAX_SPEED);
  const int8_t sign = (clamped > 0) - (clamped < 0);

  if (sign == lastSign_ && targetHz == lastHz_) return;

  if (targetHz == 0) {
    stepper_->stopMove();
  } else {
    stepper_->setSpeedInHz(targetHz);
    if (sign > 0) {
      stepper_->runForward();
    } else {
      stepper_->runBackward();
    }
  }
  lastSign_ = sign;
  lastHz_ = targetHz;
}

// Overrun safety cutoff: stop immediately if the slide is moving toward a
// triggered limit switch. Active on every loop, independent of homing state.
void SlideAxis::checkLimitSwitches() {
  if (stepper_ == nullptr) return;

  const bool minTriggered = digitalRead(pins::LIMIT_SLIDE_MIN) == LOW;
  const bool maxTriggered = digitalRead(pins::LIMIT_SLIDE_MAX) == LOW;
  const int32_t speedMilliHz = stepper_->getCurrentSpeedInMilliHz();

  if ((minTriggered && speedMilliHz < 0) ||
      (maxTriggered && speedMilliHz > 0)) {
    stepper_->forceStopAndNewPosition(stepper_->getCurrentPosition());
  }
}
