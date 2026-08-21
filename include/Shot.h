#pragma once
// Programmed multi-axis "shot" playback (project brief §6). Separate
// subsystem from live jog/angle-setpoint control: steps through an ordered
// list of keyframes, executing a synchronized move between each consecutive
// pair so every axis starts, eases, and arrives together regardless of how
// far each individually travels. All 4 axes (slide/pan/tilt/z) move within
// the same keyframe move — they share one governing duration and one S-curve
// timeline, not 4 independent moves that happen to overlap.
//
// EASE_IN_OUT moves are driven as a jerk-limited S-curve (§6.4): a quintic
// (minimum-jerk) position profile sampled into a short waypoint sequence and
// replayed as successive FastAccelStepper moveTo() calls timed against the
// move's duration, rather than relying on FastAccelStepper's own trapezoidal
// ramp for the whole move. FastAccelStepper has no native jerk-limited ramp,
// so this is the brief's explicitly-sanctioned fallback: "scaling a
// precomputed S-curve lookup table across the move's duration" instead of a
// custom step-pulse ISR. LINEAR moves are unchanged: a single trapezoidal
// move with a boosted acceleration for a sharp, non-cinematic snap.

#include <cstdint>

#include "RotaryAxis.h"
#include "SlideAxis.h"
#include "ZAxis.h"

enum class EaseType : uint8_t {
  LINEAR,       // near-instant accel ramp; sharp start/stop
  EASE_IN_OUT,  // jerk-limited S-curve; acceleration itself ramps smoothly
};

struct Keyframe {
  float slideMm;
  float panDeg;
  float tiltDeg;
  float zMm;
  float durationS;  // time to reach this keyframe from the previous one
  EaseType ease;
};

class ShotSequencer {
 public:
  void begin(SlideAxis &slide, RotaryAxis &pan, RotaryAxis &tilt, ZAxis &z);

  // Starts playback from wherever each axis currently is toward
  // keyframes[0], then on through the rest of the list. `keyframes` must
  // outlive playback (no copy is taken).
  void start(const Keyframe *keyframes, uint8_t count, uint32_t nowMs);
  void cancel();
  void update(uint32_t nowMs);

  bool isActive() const { return active_; }
  uint8_t currentKeyframeIndex() const { return keyframeIndex_; }

 private:
  enum class MoveMode : uint8_t { NONE, LINEAR_SINGLE, SCURVE_SEGMENTED };

  // Per-axis plan for the S-curve waypoint sequence within the current
  // keyframe move. Steps, not domain units (mm/deg), so segment math is
  // identical across axis types.
  struct AxisPlan {
    int32_t startSteps;
    int32_t targetSteps;
    bool moving;
  };

  SlideAxis *slide_ = nullptr;
  RotaryAxis *pan_ = nullptr;
  RotaryAxis *tilt_ = nullptr;
  ZAxis *z_ = nullptr;

  const Keyframe *keyframes_ = nullptr;
  uint8_t keyframeCount_ = 0;
  uint8_t keyframeIndex_ = 0;
  bool active_ = false;

  // S-curve segment-playback state for the move currently in flight.
  MoveMode moveMode_ = MoveMode::NONE;
  uint32_t moveStartMs_ = 0;
  float moveDurationS_ = 0.0f;
  uint8_t scurveSegments_ = 0;
  uint8_t scurveNextSegment_ = 0;
  AxisPlan slidePlan_;
  AxisPlan panPlan_;
  AxisPlan tiltPlan_;
  AxisPlan zPlan_;

  void beginMoveToKeyframe(const Keyframe &kf, uint32_t nowMs);
  void dispatchScurveSegment(uint8_t segment);
  void finish();
};
