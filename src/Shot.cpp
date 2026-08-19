#include "Shot.h"

#include <Arduino.h>
#include <math.h>

#include "config.h"

namespace {

int32_t mmToSteps(float mm) { return lroundf(mm * mech::SLIDE_STEPS_PER_MM); }
int32_t degToSteps(float deg) { return lroundf(deg * mech::ROTARY_STEPS_PER_DEGREE); }
int32_t zMmToSteps(float mm) { return lroundf(mm * mech::Z_STEPS_PER_MM); }

// ---------- LINEAR (single trapezoidal move) ----------

// Time to cover `distanceSteps` under a standard trapezoidal (or, if too
// short to reach cruise speed, triangular) velocity profile at the given
// max speed/acceleration.
float trapezoidalTimeS(int32_t distanceSteps, float maxSpeedHz, float maxAccelHz) {
  const float d = fabsf(static_cast<float>(distanceSteps));
  if (d <= 0.0f || maxSpeedHz <= 0.0f || maxAccelHz <= 0.0f) return 0.0f;

  const float accelDist = (maxSpeedHz * maxSpeedHz) / (2.0f * maxAccelHz);
  if (2.0f * accelDist <= d) {
    return maxSpeedHz / maxAccelHz + d / maxSpeedHz;  // reaches cruise speed
  }
  return 2.0f * sqrtf(d / maxAccelHz);  // triangular: never reaches max speed
}

// Scales this axis's speed/acceleration down (time-dilating its natural
// trapezoidal profile by governingTimeS / its own natural time) so it takes
// exactly governingTimeS to cover its shorter travel, per brief §6.3 step 4.
// Time dilation (t -> t/s) is exact: it stretches duration by s while
// leaving distance covered unchanged, scaling velocity by 1/s and
// acceleration by 1/s^2.
template <typename Axis>
void executeLinearMove(Axis &axis, int32_t targetSteps, float maxSpeedHz,
                        float maxAccelHz, float governingTimeS) {
  const int32_t distance = targetSteps - axis.positionSteps();
  if (distance == 0) return;

  const float baseAccel = maxAccelHz * motion::SHOT_LINEAR_ACCEL_MULTIPLIER;
  const float naturalTimeS = trapezoidalTimeS(distance, maxSpeedHz, baseAccel);
  const float scale = governingTimeS > naturalTimeS ? governingTimeS / naturalTimeS : 1.0f;

  const uint32_t speedHz = max(1u, static_cast<uint32_t>(maxSpeedHz / scale));
  const uint32_t accelHz =
      max(1u, static_cast<uint32_t>(baseAccel / (scale * scale)));
  axis.beginProgrammedMove(targetSteps, speedHz, accelHz);
}

// ---------- EASE_IN_OUT (jerk-limited S-curve via segmented waypoints) ----------

// Quintic (minimum-jerk) smoothstep: zero velocity AND zero acceleration at
// both x=0 and x=1, so — unlike a trapezoid — acceleration itself ramps up
// from and back down to zero rather than snapping on. x is normalized time
// in [0,1]; returns normalized position in [0,1].
float quinticEase(float x) { return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f); }

// Peak values of the quintic profile's derivatives, in normalized units
// (multiply by distance/duration^n to get physical peak speed/accel).
// velocity'(x) = 30x^2(x-1)^2, peak 1.875 at x=0.5.
// accel''(x) = 60x(2x-1)(x-1), peak magnitude ~5.7735 at x = (3-sqrt(3))/6.
constexpr float kQuinticPeakVelCoeff = 1.875f;
constexpr float kQuinticPeakAccelCoeff = 5.7735f;

// Minimum duration for a quintic S-curve covering `distanceSteps` that
// keeps both peak velocity and peak acceleration within the axis's
// configured limits. This is the S-curve equivalent of trapezoidalTimeS()
// above, used the same way: to find each axis's own "natural" time so the
// governing (slowest) axis sets the move's shared duration.
float quinticNaturalTimeS(int32_t distanceSteps, float maxSpeedHz, float maxAccelHz) {
  const float d = fabsf(static_cast<float>(distanceSteps));
  if (d <= 0.0f || maxSpeedHz <= 0.0f || maxAccelHz <= 0.0f) return 0.0f;

  const float velLimitedTimeS = kQuinticPeakVelCoeff * d / maxSpeedHz;
  const float accelLimitedTimeS = sqrtf(kQuinticPeakAccelCoeff * d / maxAccelHz);
  return velLimitedTimeS > accelLimitedTimeS ? velLimitedTimeS : accelLimitedTimeS;
}

uint8_t computeSegmentCount(float governingTimeS) {
  const float maxByFloor =
      (governingTimeS * 1000.0f) / static_cast<float>(motion::SHOT_SCURVE_MIN_SEGMENT_MS);
  const uint8_t floored = maxByFloor < 1.0f ? 1 : static_cast<uint8_t>(maxByFloor);
  return floored < motion::SHOT_SCURVE_SEGMENTS ? floored : motion::SHOT_SCURVE_SEGMENTS;
}

}  // namespace

void ShotSequencer::begin(SlideAxis &slide, RotaryAxis &pan, RotaryAxis &tilt, ZAxis &z) {
  slide_ = &slide;
  pan_ = &pan;
  tilt_ = &tilt;
  z_ = &z;
}

void ShotSequencer::start(const Keyframe *keyframes, uint8_t count, uint32_t nowMs) {
  if (keyframes == nullptr || count == 0 || active_) return;
  keyframes_ = keyframes;
  keyframeCount_ = count;
  keyframeIndex_ = 0;
  active_ = true;
  Serial.println("Shot: starting");
  beginMoveToKeyframe(keyframes_[0], nowMs);
}

void ShotSequencer::cancel() {
  if (!active_) return;
  Serial.println("Shot: cancelled");
  slide_->endProgrammedMove(/*stopImmediately=*/true);
  pan_->endProgrammedMove(/*stopImmediately=*/true);
  tilt_->endProgrammedMove(/*stopImmediately=*/true);
  z_->endProgrammedMove(/*stopImmediately=*/true);
  active_ = false;
  moveMode_ = MoveMode::NONE;
  keyframes_ = nullptr;
  keyframeCount_ = 0;
}

void ShotSequencer::update(uint32_t nowMs) {
  if (!active_) return;

  if (moveMode_ == MoveMode::SCURVE_SEGMENTED && scurveNextSegment_ <= scurveSegments_) {
    const float elapsedS = static_cast<float>(nowMs - moveStartMs_) / 1000.0f;
    const float segmentDurationS = moveDurationS_ / static_cast<float>(scurveSegments_);
    while (scurveNextSegment_ <= scurveSegments_ &&
           elapsedS >= scurveNextSegment_ * segmentDurationS) {
      dispatchScurveSegment(scurveNextSegment_);
      scurveNextSegment_++;
    }
    return;  // still dispatching waypoints; completion is checked once all are sent
  }

  if (!slide_->isMoveComplete() || !pan_->isMoveComplete() || !tilt_->isMoveComplete() ||
      !z_->isMoveComplete()) {
    return;
  }

  keyframeIndex_++;
  if (keyframeIndex_ >= keyframeCount_) {
    finish();
    return;
  }
  beginMoveToKeyframe(keyframes_[keyframeIndex_], nowMs);
}

void ShotSequencer::finish() {
  Serial.println("Shot: complete");
  slide_->endProgrammedMove(/*stopImmediately=*/false);
  pan_->endProgrammedMove(/*stopImmediately=*/false);
  tilt_->endProgrammedMove(/*stopImmediately=*/false);
  z_->endProgrammedMove(/*stopImmediately=*/false);
  active_ = false;
  moveMode_ = MoveMode::NONE;
  keyframes_ = nullptr;
  keyframeCount_ = 0;
}

void ShotSequencer::beginMoveToKeyframe(const Keyframe &kf, uint32_t nowMs) {
  Serial.printf(
      "Shot: keyframe %u -> slide=%.1fmm pan=%.1fdeg tilt=%.1fdeg z=%.1fmm dur=%.2fs\n",
      static_cast<unsigned>(keyframeIndex_), kf.slideMm, kf.panDeg, kf.tiltDeg, kf.zMm,
      kf.durationS);

  const int32_t slideTarget = mmToSteps(kf.slideMm);
  const int32_t panTarget = degToSteps(constrain(kf.panDeg, pan_->minDeg(), pan_->maxDeg()));
  const int32_t tiltTarget =
      degToSteps(constrain(kf.tiltDeg, tilt_->minDeg(), tilt_->maxDeg()));
  const int32_t zTarget = zMmToSteps(constrain(kf.zMm, motion::Z_MIN_MM, motion::Z_MAX_MM));

  const int32_t slideStart = slide_->positionSteps();
  const int32_t panStart = pan_->positionSteps();
  const int32_t tiltStart = tilt_->positionSteps();
  const int32_t zStart = z_->positionSteps();

  const bool useScurve = kf.ease == EaseType::EASE_IN_OUT;
  const float slideAccel =
      motion::SLIDE_ACCEL_HZ_PER_S * (useScurve ? 1.0f : motion::SHOT_LINEAR_ACCEL_MULTIPLIER);
  const float rotaryAccel =
      motion::ROTARY_ACCEL_HZ_PER_S * (useScurve ? 1.0f : motion::SHOT_LINEAR_ACCEL_MULTIPLIER);
  const float zAccel =
      motion::Z_ACCEL_HZ_PER_S * (useScurve ? 1.0f : motion::SHOT_LINEAR_ACCEL_MULTIPLIER);

  const auto naturalTimeS = useScurve ? quinticNaturalTimeS : trapezoidalTimeS;
  const float slideTimeS = naturalTimeS(slideTarget - slideStart, motion::SLIDE_MAX_SPEED_HZ, slideAccel);
  const float panTimeS = naturalTimeS(panTarget - panStart, motion::ROTARY_MAX_SPEED_HZ, rotaryAccel);
  const float tiltTimeS = naturalTimeS(tiltTarget - tiltStart, motion::ROTARY_MAX_SPEED_HZ, rotaryAccel);
  const float zTimeS = naturalTimeS(zTarget - zStart, motion::Z_MAX_SPEED_HZ, zAccel);

  // Governing duration: the slowest axis at its own configured max
  // speed/accel, or the keyframe's explicit duration, whichever is longer
  // (you can't force a move to finish faster than an axis physically
  // allows). Per brief §6.3 steps 3-5, every axis is then scaled to match.
  float governingTimeS = slideTimeS;
  if (panTimeS > governingTimeS) governingTimeS = panTimeS;
  if (tiltTimeS > governingTimeS) governingTimeS = tiltTimeS;
  if (zTimeS > governingTimeS) governingTimeS = zTimeS;
  if (kf.durationS > governingTimeS) governingTimeS = kf.durationS;

  if (!useScurve) {
    moveMode_ = MoveMode::LINEAR_SINGLE;
    executeLinearMove(*slide_, slideTarget, motion::SLIDE_MAX_SPEED_HZ,
                       motion::SLIDE_ACCEL_HZ_PER_S, governingTimeS);
    executeLinearMove(*pan_, panTarget, motion::ROTARY_MAX_SPEED_HZ,
                       motion::ROTARY_ACCEL_HZ_PER_S, governingTimeS);
    executeLinearMove(*tilt_, tiltTarget, motion::ROTARY_MAX_SPEED_HZ,
                       motion::ROTARY_ACCEL_HZ_PER_S, governingTimeS);
    executeLinearMove(*z_, zTarget, motion::Z_MAX_SPEED_HZ, motion::Z_ACCEL_HZ_PER_S,
                       governingTimeS);
    return;
  }

  moveMode_ = MoveMode::SCURVE_SEGMENTED;
  moveStartMs_ = nowMs;
  moveDurationS_ = governingTimeS;
  scurveSegments_ = computeSegmentCount(governingTimeS);
  scurveNextSegment_ = 1;

  slidePlan_ = {slideStart, slideTarget, slideTarget != slideStart};
  panPlan_ = {panStart, panTarget, panTarget != panStart};
  tiltPlan_ = {tiltStart, tiltTarget, tiltTarget != tiltStart};
  zPlan_ = {zStart, zTarget, zTarget != zStart};

  // Dispatch segment 1 immediately rather than waiting for the first
  // update() tick, so the move starts moving in the same loop iteration it
  // was scheduled in.
  dispatchScurveSegment(scurveNextSegment_);
  scurveNextSegment_++;
}

void ShotSequencer::dispatchScurveSegment(uint8_t segment) {
  const float x = static_cast<float>(segment) / static_cast<float>(scurveSegments_);
  const float frac = quinticEase(x);
  const float segmentDurationS = moveDurationS_ / static_cast<float>(scurveSegments_);

  // All 4 axes share identical per-segment math but different concrete
  // types, so this can't be one loop over a common base without adding a
  // virtual interface just for this — four explicit blocks instead.
  if (slidePlan_.moving) {
    const int32_t target =
        slidePlan_.startSteps + lroundf((slidePlan_.targetSteps - slidePlan_.startSteps) * frac);
    const int32_t segmentDistance = target - slide_->positionSteps();
    const uint32_t speedHz = max(
        1u, min(static_cast<uint32_t>(motion::SLIDE_MAX_SPEED_HZ),
                static_cast<uint32_t>(fabsf(segmentDistance) / segmentDurationS)));
    slide_->beginProgrammedMove(target, speedHz,
                                 static_cast<uint32_t>(motion::SLIDE_ACCEL_HZ_PER_S));
  }
  if (panPlan_.moving) {
    const int32_t target =
        panPlan_.startSteps + lroundf((panPlan_.targetSteps - panPlan_.startSteps) * frac);
    const int32_t segmentDistance = target - pan_->positionSteps();
    const uint32_t speedHz = max(
        1u, min(static_cast<uint32_t>(motion::ROTARY_MAX_SPEED_HZ),
                static_cast<uint32_t>(fabsf(segmentDistance) / segmentDurationS)));
    pan_->beginProgrammedMove(target, speedHz,
                               static_cast<uint32_t>(motion::ROTARY_ACCEL_HZ_PER_S));
  }
  if (tiltPlan_.moving) {
    const int32_t target =
        tiltPlan_.startSteps + lroundf((tiltPlan_.targetSteps - tiltPlan_.startSteps) * frac);
    const int32_t segmentDistance = target - tilt_->positionSteps();
    const uint32_t speedHz = max(
        1u, min(static_cast<uint32_t>(motion::ROTARY_MAX_SPEED_HZ),
                static_cast<uint32_t>(fabsf(segmentDistance) / segmentDurationS)));
    tilt_->beginProgrammedMove(target, speedHz,
                                static_cast<uint32_t>(motion::ROTARY_ACCEL_HZ_PER_S));
  }
  if (zPlan_.moving) {
    const int32_t target =
        zPlan_.startSteps + lroundf((zPlan_.targetSteps - zPlan_.startSteps) * frac);
    const int32_t segmentDistance = target - z_->positionSteps();
    const uint32_t speedHz = max(
        1u, min(static_cast<uint32_t>(motion::Z_MAX_SPEED_HZ),
                static_cast<uint32_t>(fabsf(segmentDistance) / segmentDurationS)));
    z_->beginProgrammedMove(target, speedHz, static_cast<uint32_t>(motion::Z_ACCEL_HZ_PER_S));
  }
}
