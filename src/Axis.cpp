#include "Axis.h"

#include "Motion.h"
#include "Mux.h"

namespace {
// Homing gives up rather than grinding into a rail forever if the switch is
// unplugged, miswired, or the belt has slipped off.
constexpr uint32_t HOMING_TIMEOUT_MS = 60000;

float normalizeDeg(float deg) {
  while (deg <= -180.0f) deg += 360.0f;
  while (deg > 180.0f) deg -= 360.0f;
  return deg;
}
}  // namespace

const AxisConfig &Axis::cfg() const { return settings.axes[index_]; }

int32_t Axis::unitsToSteps(float units) const {
  return lroundf(units * cfg().stepsPerUnit());
}

float Axis::stepsToUnits(int32_t steps) const {
  const float spu = cfg().stepsPerUnit();
  return spu > 0.0f ? steps / spu : 0.0f;
}

uint32_t Axis::unitsPerSecToHz(float unitsPerSec) const {
  const float hz = fabsf(unitsPerSec) * cfg().stepsPerUnit();
  if (hz < 1.0f) return 1;
  if (hz > motion::MAX_STEP_RATE_HZ) return motion::MAX_STEP_RATE_HZ;
  return static_cast<uint32_t>(hz);
}

void Axis::begin(uint8_t index, FastAccelStepperEngine &engine, TwoWire *muxBus,
                 HardwareSerial *tmcSerial) {
  index_ = index;
  muxBus_ = muxBus;
  const AxisConfig &c = cfg();

  if (!c.enabled) {
    Serial.printf("[axis %u] %s disabled by config\n", index_, c.name);
    return;
  }

  stepper_ = engine.stepperConnectToPin(c.stepPin);
  if (stepper_ == nullptr) {
    Serial.printf("[axis %u] ERROR: no free step-generator for pin %u\n", index_,
                  c.stepPin);
    return;
  }
  // FastAccelStepper's second argument is "direction pin HIGH counts up", so
  // inverting the axis is just passing the negation.
  stepper_->setDirectionPin(c.dirPin, !c.invertDir);
  stepper_->setAcceleration(
      static_cast<uint32_t>(max(1.0f, c.accel * c.stepsPerUnit())));
  stepper_->setSpeedInHz(unitsPerSecToHz(c.maxSpeed));

  if (c.homing == HomingMode::LIMIT_MIN || c.homing == HomingMode::LIMIT_MAX) {
    pinMode(c.limitMinPin, INPUT_PULLUP);
    pinMode(c.limitMaxPin, INPUT_PULLUP);
  }

  if (c.tmcEnabled && tmcSerial != nullptr) {
    BusLock lock(motion_ctl);
    tmc_ = new TMC2209Stepper(tmcSerial, settings.tmcRSense, c.tmcAddress);
    tmc_->begin();
    // Required for UART control: PDN must be released, and MRES only comes
    // from the register once mstep_reg_select is set. Without the second
    // call the MS1/MS2 straps -- which this board uses for the slave address
    // -- would still be picking the microstep count, giving each driver a
    // different (and wrong) resolution.
    tmc_->pdn_disable(true);
    tmc_->mstep_reg_select(true);
    tmc_->I_scale_analog(false);  // ignore the VREF pot, use the UART current
    tmcPresent_ = tmc_->test_connection() == 0;
    if (!tmcPresent_) {
      Serial.printf("[axis %u] WARNING: TMC2209 at address %u did not answer\n",
                    index_, c.tmcAddress);
    }
    applyDriverConfig();
  }

  // An absolute encoder means the axis knows where it is the moment it powers
  // up -- no homing move, no "please jog to the middle first".
  if (c.feedback == FeedbackType::AS5600) {
    float deg = 0.0f;
    if (readSensorDeg(deg)) {
      stepper_->setCurrentPosition(unitsToSteps(deg));
      targetUnits_ = deg;
      homed_ = true;
      homingState_ = HomingState::DONE;
      Serial.printf("[axis %u] %s absolute position from AS5600: %.2f deg\n",
                    index_, c.name, deg);
    } else {
      Serial.printf("[axis %u] WARNING: AS5600 on mux ch%u did not answer\n",
                    index_, c.muxChannel);
    }
  } else if (c.homing == HomingMode::NONE) {
    // Nothing to home against: trust wherever it happens to be sitting.
    homed_ = true;
    homingState_ = HomingState::DONE;
    targetUnits_ = positionUnits();
  }

  Serial.printf("[axis %u] %s ready: %.3f steps/%s, step=%u dir=%u tmc=%s\n",
                index_, c.name, c.stepsPerUnit(), c.unitLabel(), c.stepPin,
                c.dirPin, tmcPresent_ ? "ok" : "absent");
}

void Axis::applyDriverConfig() {
  if (tmc_ == nullptr) return;
  const AxisConfig &c = cfg();
  BusLock lock(motion_ctl);
  tmc_->toff(5);                       // enable the chopper
  tmc_->rms_current(c.runCurrentMa, c.holdCurrentPct / 100.0f);
  tmc_->microsteps(c.microsteps);
  tmc_->en_spreadCycle(!c.stealthChop);
  tmc_->blank_time(24);
  tmc_->iholddelay(6);
  tmc_->TPOWERDOWN(20);

  // Changing microsteps changes steps/unit, so the ramp parameters that were
  // derived from it have to be recomputed or the axis silently runs at the
  // wrong speed.
  if (stepper_ != nullptr) {
    stepper_->setAcceleration(
        static_cast<uint32_t>(max(1.0f, c.accel * c.stepsPerUnit())));
    stepper_->setSpeedInHz(unitsPerSecToHz(c.maxSpeed));
    stepper_->applySpeedAcceleration();
  }
}

void Axis::driverStatusJson(JsonObject out) {
  const AxisConfig &c = cfg();
  out["axis"] = index_;
  out["name"] = c.name;
  out["address"] = c.tmcAddress;
  if (tmc_ == nullptr) {
    out["present"] = false;
    out["note"] = "UART disabled for this axis";
    return;
  }
  // test_connection() is a live read; refresh it here so the UI reflects a
  // driver that was plugged in (or fell out) since boot.
  BusLock lock(motion_ctl);
  tmcPresent_ = tmc_->test_connection() == 0;
  out["present"] = tmcPresent_;
  if (!tmcPresent_) {
    out["note"] = "no reply -- check the module is seated and MS1/MS2 match";
    return;
  }
  const uint32_t drv = tmc_->DRV_STATUS();
  out["microsteps"] = tmc_->microsteps();
  out["rms_current_ma"] = tmc_->rms_current();
  out["stealthchop"] = !tmc_->en_spreadCycle();
  out["cs_actual"] = tmc_->cs_actual();
  out["stallguard"] = tmc_->SG_RESULT();
  out["overtemp_warn"] = tmc_->otpw();
  out["overtemp"] = tmc_->ot();
  out["open_load_a"] = tmc_->ola();
  out["open_load_b"] = tmc_->olb();
  out["short_a"] = tmc_->s2ga();
  out["short_b"] = tmc_->s2gb();
  out["standstill"] = (drv & (1UL << 31)) != 0;
}

bool Axis::isRunning() const {
  return stepper_ != nullptr && stepper_->isRunning();
}

float Axis::positionUnits() const {
  if (stepper_ == nullptr) return 0.0f;
  return stepsToUnits(stepper_->getCurrentPosition());
}

float Axis::speedUnitsPerSec() const {
  if (stepper_ == nullptr) return 0.0f;
  const float spu = cfg().stepsPerUnit();
  if (spu <= 0.0f) return 0.0f;
  return (stepper_->getCurrentSpeedInMilliHz() / 1000.0f) / spu;
}

bool Axis::limitTriggered(uint8_t pin) const {
  return digitalRead(pin) == (cfg().limitActiveLow ? LOW : HIGH);
}

bool Axis::limitMinTriggered() const {
  const AxisConfig &c = cfg();
  if (c.homing != HomingMode::LIMIT_MIN && c.homing != HomingMode::LIMIT_MAX) {
    return false;
  }
  return limitTriggered(c.limitMinPin);
}

bool Axis::limitMaxTriggered() const {
  const AxisConfig &c = cfg();
  if (c.homing != HomingMode::LIMIT_MIN && c.homing != HomingMode::LIMIT_MAX) {
    return false;
  }
  return limitTriggered(c.limitMaxPin);
}

bool Axis::clampToSoftLimits(float &units, String &error) const {
  const AxisConfig &c = cfg();
  if (!c.softLimits) return true;
  if (units < c.minLimit || units > c.maxLimit) {
    error = String(c.name) + ": " + String(units, 2) + c.unitLabel() +
            " is outside the soft limits (" + String(c.minLimit, 2) + " to " +
            String(c.maxLimit, 2) + ")";
    units = constrain(units, c.minLimit, c.maxLimit);
    return false;
  }
  return true;
}

bool Axis::moveTo(float units, String &error) {
  return moveTo(units, cfg().maxSpeed, error);
}

void Axis::applyRamp(float unitsPerSec, float accelUnitsPerSec2) {
  const float spu = cfg().stepsPerUnit();
  stepper_->setSpeedInHz(unitsPerSecToHz(unitsPerSec));
  stepper_->setAcceleration(
      static_cast<uint32_t>(max(1.0f, fabsf(accelUnitsPerSec2) * spu)));
}

bool Axis::moveTo(float units, float unitsPerSec, String &error) {
  if (stepper_ == nullptr) {
    error = String(cfg().name) + " is not available";
    return false;
  }
  const bool inRange = clampToSoftLimits(units, error);
  jogDir_ = 0;
  jogHz_ = 0;
  targetUnits_ = units;
  // Always restore the configured acceleration: a previous eased move may
  // have left a much gentler ramp behind.
  applyRamp(unitsPerSec, cfg().accel);
  stepper_->moveTo(unitsToSteps(units));
  return inRange;
}

float Axis::minMoveTime(float fromUnits, float toUnits, bool ease) const {
  const AxisConfig &c = cfg();
  const float dist = fabsf(toUnits - fromUnits);
  if (dist <= 0.0f) return 0.0f;
  if (ease) {
    // Triangular profile: peak = 2d/t and a = 4d/t^2, so both constraints
    // become lower bounds on t.
    return max(2.0f * dist / c.maxSpeed, 2.0f * sqrtf(dist / c.accel));
  }
  // Trapezoid: cruise time plus the two ramps.
  return dist / c.maxSpeed + c.maxSpeed / c.accel;
}

bool Axis::moveTimed(float units, float durationS, bool ease, String &error) {
  if (stepper_ == nullptr) {
    error = String(cfg().name) + " is not available";
    return false;
  }
  const bool inRange = clampToSoftLimits(units, error);
  jogDir_ = 0;
  jogHz_ = 0;
  targetUnits_ = units;

  const AxisConfig &c = cfg();
  const float dist = fabsf(units - positionUnits());
  if (dist <= 0.0f || durationS <= 0.0f) {
    applyRamp(c.maxSpeed, c.accel);
    stepper_->moveTo(unitsToSteps(units));
    return inRange;
  }

  float speed, accel;
  if (ease) {
    speed = 2.0f * dist / durationS;
    accel = 4.0f * dist / (durationS * durationS);
  } else {
    speed = dist / durationS;
    accel = c.accel;
  }
  // If the caller asked for something the axis cannot do, run it flat out --
  // the move takes longer than requested, which the sequencer avoids by
  // pre-stretching the duration via minMoveTime().
  applyRamp(min(speed, c.maxSpeed), min(accel, c.accel));
  stepper_->moveTo(unitsToSteps(units));
  return inRange;
}

bool Axis::nudge(float deltaUnits, String &error) {
  // Nudges accumulate on the *target*, not the live position, so spinning an
  // encoder fast does not lose clicks to an in-progress move.
  return moveTo(targetUnits_ + deltaUnits, error);
}

void Axis::jog(int8_t dir, float unitsPerSec) {
  if (stepper_ == nullptr || dir == 0) {
    if (jogDir_ != 0) stop();
    return;
  }
  const AxisConfig &c = cfg();
  // Refuse to jog further into a soft limit or a tripped switch.
  if (c.softLimits) {
    const float pos = positionUnits();
    if ((dir > 0 && pos >= c.maxLimit) || (dir < 0 && pos <= c.minLimit)) {
      stop();
      return;
    }
  }
  if ((dir < 0 && limitMinTriggered()) || (dir > 0 && limitMaxTriggered())) {
    stop();
    return;
  }

  const uint32_t hz = unitsPerSecToHz(unitsPerSec);
  if (dir == jogDir_ && hz == jogHz_) return;  // already running like this
  jogDir_ = dir;
  jogHz_ = hz;
  stepper_->setSpeedInHz(hz);
  if (dir > 0) {
    stepper_->runForward();
  } else {
    stepper_->runBackward();
  }
}

void Axis::stop() {
  jogDir_ = 0;
  jogHz_ = 0;
  if (stepper_ == nullptr) return;
  stepper_->stopMove();
  targetUnits_ = stepsToUnits(stepper_->getPositionAfterCommandsCompleted());
}

void Axis::forceStop() {
  jogDir_ = 0;
  jogHz_ = 0;
  if (stepper_ == nullptr) return;
  stepper_->forceStopAndNewPosition(stepper_->getCurrentPosition());
  targetUnits_ = positionUnits();
}

void Axis::setPositionUnits(float units) {
  if (stepper_ == nullptr) return;
  stepper_->forceStopAndNewPosition(unitsToSteps(units));
  targetUnits_ = units;
  homed_ = true;
  homingState_ = HomingState::DONE;
}

void Axis::invalidateHoming() {
  homed_ = false;
  homingState_ = HomingState::IDLE;
}

void Axis::startHoming() {
  const AxisConfig &c = cfg();
  if (stepper_ == nullptr) return;

  if (c.feedback == FeedbackType::AS5600) {
    float deg = 0.0f;
    if (readSensorDeg(deg)) {
      setPositionUnits(deg);
      Serial.printf("[axis %u] %s homed from AS5600 at %.2f deg\n", index_,
                    c.name, deg);
    } else {
      homingState_ = HomingState::FAILED;
      Serial.printf("[axis %u] %s homing failed: AS5600 silent\n", index_, c.name);
    }
    return;
  }

  if (c.homing != HomingMode::LIMIT_MIN && c.homing != HomingMode::LIMIT_MAX) {
    // Nothing to seek; treat the current spot as home.
    setPositionUnits(0.0f);
    return;
  }

  homed_ = false;
  homingState_ = HomingState::SEEKING;
  homingStartedMs_ = millis();
  jogDir_ = 0;
  jogHz_ = 0;
  stepper_->setSpeedInHz(unitsPerSecToHz(c.homingSpeed));
  if (c.homing == HomingMode::LIMIT_MIN) {
    stepper_->runBackward();
  } else {
    stepper_->runForward();
  }
}

void Axis::updateHoming(uint32_t nowMs) {
  const AxisConfig &c = cfg();
  switch (homingState_) {
    case HomingState::SEEKING: {
      const bool hit = c.homing == HomingMode::LIMIT_MIN ? limitMinTriggered()
                                                         : limitMaxTriggered();
      if (hit) {
        // Zero at the switch, then back off so the axis rests off the
        // trigger -- otherwise the overrun cutoff fires on the first move.
        const float homePos =
            c.homing == HomingMode::LIMIT_MIN ? c.minLimit : c.maxLimit;
        stepper_->forceStopAndNewPosition(unitsToSteps(homePos));
        const float backoff = c.homing == HomingMode::LIMIT_MIN
                                  ? fabsf(c.homingBackoff)
                                  : -fabsf(c.homingBackoff);
        targetUnits_ = homePos + backoff;
        stepper_->moveTo(unitsToSteps(targetUnits_));
        homingState_ = HomingState::BACKING_OFF;
      } else if (nowMs - homingStartedMs_ > HOMING_TIMEOUT_MS) {
        stepper_->forceStopAndNewPosition(stepper_->getCurrentPosition());
        homingState_ = HomingState::FAILED;
        Serial.printf("[axis %u] %s homing timed out -- check the switch\n",
                      index_, c.name);
      }
      break;
    }
    case HomingState::BACKING_OFF:
      if (!stepper_->isRunning()) {
        homingState_ = HomingState::DONE;
        homed_ = true;
        Serial.printf("[axis %u] %s homed\n", index_, c.name);
      }
      break;
    default:
      break;
  }
}

// Overrun cutoff: stop the instant the axis is moving toward a switch that is
// already triggered. Runs every loop, independent of homing.
void Axis::enforceLimitSwitches() {
  if (stepper_ == nullptr) return;
  if (homingState_ == HomingState::SEEKING) return;  // homing drives into one
  const AxisConfig &c = cfg();
  if (c.homing != HomingMode::LIMIT_MIN && c.homing != HomingMode::LIMIT_MAX) {
    return;
  }
  const int32_t speedMilliHz = stepper_->getCurrentSpeedInMilliHz();
  if (speedMilliHz == 0) return;
  if ((speedMilliHz < 0 && limitMinTriggered()) ||
      (speedMilliHz > 0 && limitMaxTriggered())) {
    stepper_->forceStopAndNewPosition(stepper_->getCurrentPosition());
    targetUnits_ = positionUnits();
    jogDir_ = 0;
    jogHz_ = 0;
    Serial.printf("[axis %u] %s limit switch cutoff\n", index_, c.name);
  }
}

bool Axis::readSensorDeg(float &outDeg) {
  const AxisConfig &c = cfg();
  if (c.feedback != FeedbackType::AS5600 || muxBus_ == nullptr) return false;
  float raw = 0.0f;
  {
    BusLock lock(motion_ctl);
    if (!readAS5600Degrees(*muxBus_, settings.muxAddress, c.muxChannel, raw)) {
      return false;
    }
  }
  outDeg = normalizeDeg(raw - c.zeroOffsetDeg);
  sensorDeg_ = outDeg;
  return true;
}

// With standalone step/dir there is no way to notice lost steps except by
// asking the absolute encoder. Only correct while idle, so a resync can never
// yank an in-progress move.
void Axis::updateDrift(uint32_t nowMs) {
  const AxisConfig &c = cfg();
  if (c.feedback != FeedbackType::AS5600 || c.driftCheckMs == 0) return;
  if (nowMs - lastDriftCheckMs_ < c.driftCheckMs) return;
  lastDriftCheckMs_ = nowMs;
  if (stepper_ == nullptr || stepper_->isRunning()) return;

  float deg = 0.0f;
  if (!readSensorDeg(deg)) return;
  const float errorDeg = deg - positionUnits();
  if (fabsf(errorDeg) > c.driftThresholdDeg) {
    Serial.printf("[axis %u] %s drift %.2f deg, resyncing to AS5600\n", index_,
                  c.name, errorDeg);
    stepper_->setCurrentPosition(unitsToSteps(deg));
    // The target follows the correction, otherwise the axis would immediately
    // drive back to the position we just decided was wrong.
    targetUnits_ = deg;
  }
}

void Axis::update(uint32_t nowMs) {
  if (stepper_ == nullptr) return;
  updateHoming(nowMs);
  enforceLimitSwitches();
  updateDrift(nowMs);

  // A soft-limited axis in continuous jog has to be stopped at the boundary;
  // there is no move target to do it for us.
  const AxisConfig &c = cfg();
  if (jogDir_ != 0 && c.softLimits) {
    const float pos = positionUnits();
    if ((jogDir_ > 0 && pos >= c.maxLimit) || (jogDir_ < 0 && pos <= c.minLimit)) {
      stop();
    }
  }
}

void Axis::telemetryJson(JsonObject out) const {
  const AxisConfig &c = cfg();
  out["name"] = c.name;
  out["enabled"] = c.enabled;
  out["available"] = available();
  out["units"] = c.unitLabel();
  out["pos"] = positionUnits();
  out["target"] = targetUnits_;
  out["speed"] = speedUnitsPerSec();
  out["running"] = isRunning();
  out["homed"] = homed_;
  out["min"] = c.minLimit;
  out["max"] = c.maxLimit;
  out["soft_limits"] = c.softLimits;
  out["limit_min"] = limitMinTriggered();
  out["limit_max"] = limitMaxTriggered();
  out["tmc"] = tmcPresent_;
  if (!isnan(sensorDeg_)) out["sensor_deg"] = sensorDeg_;
  switch (homingState_) {
    case HomingState::IDLE: out["homing"] = "idle"; break;
    case HomingState::SEEKING: out["homing"] = "seeking"; break;
    case HomingState::BACKING_OFF: out["homing"] = "backoff"; break;
    case HomingState::DONE: out["homing"] = "done"; break;
    case HomingState::FAILED: out["homing"] = "failed"; break;
  }
}
