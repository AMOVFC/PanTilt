#pragma once
// Curve sequences: the second kind of move the rig can play.
//
// A keyframe (see Sequencer.h) is a *pose* -- one value per axis, and every
// axis is time-dilated so they all arrive together. That is the right model
// for "go from here to there, together", and useless for "let the slide run
// steadily while pan holds, then whips, then settles". So a curve sequence
// gives every axis its own independent channel of keys on one shared clock.
// Channels do not have to share key times, or even have the same number of
// keys; an axis with no keys is simply not part of the move and is never
// commanded.
//
// Between two keys the axis follows a cubic Bezier whose handles the operator
// drags in the web UI. Handles are stored normalised to the leg they shape --
// x as a fraction of that leg's duration, y as a fraction of its
// displacement -- exactly like CSS cubic-bezier(). That way a handle means
// the same shape on a 2-second leg as on a 20-second one, and retiming a leg
// does not silently restyle it.
//
// Playback is waypoint dispatch on a wall clock: every ui::CURVE_TICK_MS the
// player asks each channel where it should be one tick from now and hands
// that to the axis as a short timed move. FastAccelStepper's ramp generator
// smooths between waypoints, so an arbitrary drawn curve is reproducible
// without needing a jerk-limited planner in firmware.

#include <Arduino.h>
#include <ArduinoJson.h>

#include "Motion.h"
#include "Settings.h"

struct CurveKey {
  float t = 0.0f;  // seconds from the start of the sequence
  float v = 0.0f;  // axis units (mm or deg)
  // Handle shaping the leg *leaving* this key, and the one *arriving* at it.
  // outY/inY are fractions of the leg's displacement, so 0 is a flat handle
  // (a dead stop at this end, i.e. ease) and matching x makes the leg linear.
  float outX = 0.33f, outY = 0.0f;
  float inX = 0.33f, inY = 0.0f;
};

struct CurveChannel {
  CurveKey keys[fw::MAX_CURVE_KEYS];
  uint8_t count = 0;

  float endTime() const { return count ? keys[count - 1].t : 0.0f; }
  // Value at time `t`, holding the first/last key outside the channel's span.
  float valueAt(float t) const;
  // Fastest units/s this channel ever demands, and the largest |accel|. Both
  // are sampled rather than solved: the closed form for a cubic Bezier's
  // extrema is exact but fiddly, and this runs once per play, not per tick.
  void peaks(float &peakSpeed, float &peakAccel) const;
};

class CurveSequence {
 public:
  char name[fw::CURVE_NAME_LEN] = "";
  CurveChannel channels[fw::AXIS_COUNT];

  void clear();
  float duration() const;
  uint16_t totalKeys() const;
  bool empty() const { return totalKeys() == 0; }

  void toJson(JsonObject out) const;
  bool fromJson(JsonObjectConst in, String &error);
};

enum class CurveState : uint8_t { IDLE, PLAYING, PAUSED };

class CurveSequencer {
 public:
  void begin(Motion &motion);
  void update(uint32_t nowMs);

  CurveSequence &active() { return seq_; }
  const CurveSequence &active() const { return seq_; }

  // --- saved slots ---
  bool loadSlot(uint8_t slot, String &error);
  bool saveSlot(uint8_t slot, const char *name, String &error);
  bool deleteSlot(uint8_t slot, String &error);
  int8_t activeSlot() const { return activeSlot_; }
  void refreshSlotNames();
  const char *slotName(uint8_t slot) const {
    return slot < fw::MAX_CURVE_SEQUENCES ? slotNames_[slot] : "";
  }

  // --- transport ---
  bool play(String &error);
  void pause();
  void stop();
  bool gotoTime(float t, String &error);
  CurveState state() const { return state_; }
  float elapsed() const { return elapsedS_; }

  // Refuses a sequence the machine cannot physically track, naming the axis
  // and what it would have needed. Silently running slower than drawn would
  // desynchronise the channels from each other, which is exactly the failure
  // the shared clock exists to prevent.
  bool checkFeasible(String &error) const;

  void slotsJson(JsonObject out) const;
  void toJson(JsonObject out) const;
  void telemetryJson(JsonObject out) const;

  static void slotPath(uint8_t slot, char *buf, size_t n);

 private:
  Motion *motion_ = nullptr;
  CurveSequence seq_;
  char slotNames_[fw::MAX_CURVE_SEQUENCES][fw::CURVE_NAME_LEN] = {};
  int8_t activeSlot_ = -1;
  CurveState state_ = CurveState::IDLE;
  uint32_t tickBaseMs_ = 0;   // millis() at the moment elapsedS_ was last set
  uint32_t lastTickMs_ = 0;
  float elapsedS_ = 0.0f;
  float durationS_ = 0.0f;

  void commandAt(float t, bool exact);
  void finish();
};

extern CurveSequencer curves;
