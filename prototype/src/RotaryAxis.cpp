#include "RotaryAxis.h"

#include "Mux.h"
#include "config.h"

namespace {
// Normalizes to (-180, 180]; matches the signed soft-limit ranges in config.h.
float normalizeDeg(float deg) {
  while (deg <= -180.0f) deg += 360.0f;
  while (deg > 180.0f) deg -= 360.0f;
  return deg;
}

int32_t degToSteps(float deg) {
  return lroundf(deg * mech::ROTARY_STEPS_PER_DEGREE);
}

float stepsToDeg(int32_t steps) { return steps / mech::ROTARY_STEPS_PER_DEGREE; }
}  // namespace

RotaryAxis::RotaryAxis(uint8_t stepPin, uint8_t dirPin, uint8_t muxChannel,
                        const float &zeroOffsetDeg, const float &minDeg,
                        const float &maxDeg)
    : stepPin_(stepPin),
      dirPin_(dirPin),
      muxChannel_(muxChannel),
      zeroOffsetDeg_(&zeroOffsetDeg),
      minDeg_(&minDeg),
      maxDeg_(&maxDeg) {}

void RotaryAxis::begin(FastAccelStepperEngine &engine, TwoWire &muxBus) {
  muxBus_ = &muxBus;
  stepper_ = engine.stepperConnectToPin(stepPin_);
  if (stepper_ == nullptr) {
    Serial.println("ERROR: failed to attach rotary stepper to STEP pin");
    return;
  }
  stepper_->setDirectionPin(dirPin_);
  stepper_->setAcceleration(motion::ROTARY_ACCEL_HZ_PER_S);
  stepper_->setSpeedInHz(motion::ROTARY_MAX_SPEED_HZ);

  // No homing move: the AS5600 gives absolute position on every power-up.
  const float sensorDeg = readAS5600DegreesOnChannel(*muxBus_, muxChannel_);
  const float correctedDeg = normalizeDeg(sensorDeg - *zeroOffsetDeg_);
  stepper_->setCurrentPosition(degToSteps(correctedDeg));
  targetDeg_ = correctedDeg;
}

void RotaryAxis::nudgeTargetDeg(float deltaDeg) {
  if (stepper_ == nullptr || mode_ == ControlMode::PROGRAMMED) return;
  targetDeg_ = constrain(targetDeg_ + deltaDeg, *minDeg_, *maxDeg_);
  commandTarget();
}

void RotaryAxis::setTargetDeg(float deg) {
  if (stepper_ == nullptr || mode_ == ControlMode::PROGRAMMED) return;
  targetDeg_ = constrain(deg, *minDeg_, *maxDeg_);
  commandTarget();
}

void RotaryAxis::commandTarget() { stepper_->moveTo(degToSteps(targetDeg_)); }

float RotaryAxis::currentDeg() const {
  if (stepper_ == nullptr) return 0.0f;
  return stepsToDeg(stepper_->getCurrentPosition());
}

int32_t RotaryAxis::positionSteps() const {
  return stepper_ == nullptr ? 0 : stepper_->getCurrentPosition();
}

void RotaryAxis::beginProgrammedMove(int32_t targetSteps, uint32_t speedHz,
                                      uint32_t accelHz) {
  if (stepper_ == nullptr) return;
  mode_ = ControlMode::PROGRAMMED;
  stepper_->setSpeedInHz(speedHz);
  stepper_->setAcceleration(static_cast<int32_t>(accelHz));
  stepper_->moveTo(targetSteps);
}

void RotaryAxis::endProgrammedMove(bool stopImmediately) {
  if (stepper_ != nullptr) {
    if (stopImmediately) {
      stepper_->forceStopAndNewPosition(stepper_->getCurrentPosition());
    }
    stepper_->setAcceleration(motion::ROTARY_ACCEL_HZ_PER_S);
    stepper_->setSpeedInHz(motion::ROTARY_MAX_SPEED_HZ);
    // Resync the angle-setpoint target to wherever the move left the axis,
    // so the next nudgeTargetDeg() call doesn't jump back to a stale target.
    targetDeg_ = currentDeg();
  }
  mode_ = ControlMode::MANUAL;
}

bool RotaryAxis::isMoveComplete() const {
  return stepper_ == nullptr || !stepper_->isRunning();
}

void RotaryAxis::update(uint32_t nowMs) {
  if (stepper_ == nullptr || muxBus_ == nullptr) return;
  if (mode_ == ControlMode::PROGRAMMED) return;  // drift-check is idle-only anyway; skip explicitly
  if (nowMs - lastDriftCheckMs_ < motion::ROTARY_DRIFT_CHECK_INTERVAL_MS) return;
  lastDriftCheckMs_ = nowMs;

  // Only resync while idle: correcting mid-move would yank the target.
  if (stepper_->isRunning()) return;

  const float sensorDeg = readAS5600DegreesOnChannel(*muxBus_, muxChannel_);
  const float correctedDeg = normalizeDeg(sensorDeg - *zeroOffsetDeg_);
  const int32_t sensorSteps = degToSteps(correctedDeg);
  const int32_t stepDiff = sensorSteps - stepper_->getCurrentPosition();

  if (abs(stepDiff) > motion::ROTARY_DRIFT_THRESHOLD_STEPS) {
    Serial.printf("Rotary axis drift detected: %ld steps, resyncing to AS5600\n",
                  static_cast<long>(stepDiff));
    stepper_->setCurrentPosition(sensorSteps);
  }
}
