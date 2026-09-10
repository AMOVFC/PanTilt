#pragma once
// Programmed multi-axis "shot" playback — 3-axis prototype version of the
// final rig's Shot system. Same algorithm, same interface, same tuning
// knobs; only the axis count differs (slide/pan/tilt here, no Z, since the
// prototype has no Z motor). A shot authored for this build ports to the
// final rig by adding a zMm field to each keyframe (and vice versa, a
// final-rig shot ports here by dropping zMm) — everything else about how a
// keyframe behaves is identical.
//
// Separate subsystem from live jog/angle-setpoint control: steps through an
// ordered list of keyframes, executing a synchronized move between each
// consecutive pair so every axis starts, eases, and arrives together
// regardless of how far each individually travels.
//
// EASE_IN_OUT moves are driven as a jerk-limited S-curve: a quintic
// (minimum-jerk) position profile sampled into a short waypoint sequence and
// replayed as successive FastAccelStepper moveTo() calls timed against the
// move's duration, rather than relying on FastAccelStepper's own trapezoidal
// ramp for the whole move. LINEAR moves are unchanged: a single trapezoidal
// move with a boosted acceleration for a sharp, non-cinematic snap.

#include <cstdint>

#include "RotaryAxis.h"
#include "SlideAxis.h"

enum class EaseType : uint8_t {
  LINEAR,       // near-instant accel ramp; sharp start/stop
  EASE_IN_OUT,  // jerk-limited S-curve; acceleration itself ramps smoothly
};

struct Keyframe {
  float slideMm;
  float panDeg;
  float tiltDeg;
  float durationS;  // time to reach this keyframe from the previous one
  EaseType ease;
};

class ShotSequencer {
 public:
  void begin(SlideAxis &slide, RotaryAxis &pan, RotaryAxis &tilt);

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

  void beginMoveToKeyframe(const Keyframe &kf, uint32_t nowMs);
  void dispatchScurveSegment(uint8_t segment);
  void finish();
};
