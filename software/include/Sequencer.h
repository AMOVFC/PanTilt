#pragma once
// Keyframe sequencer: record a list of 4-axis poses, then play them back as
// coordinated moves so every axis starts and finishes together. This is what
// the board's Set-keyframe / Clear / Play-pause / Reset buttons drive, and
// what the web UI's Sequence tab edits.
//
// Timing note: durations are honoured by stretching, never by compressing. If
// a leg is asked for faster than the slowest axis can manage, the whole leg
// is slowed to the feasible time -- all axes stay in sync and the move just
// takes longer than asked, rather than one axis silently lagging.

#include <Arduino.h>
#include <ArduinoJson.h>

#include "Motion.h"
#include "Settings.h"

struct Keyframe {
  float pos[fw::AXIS_COUNT];
  float durationS;  // travel time from the previous keyframe
  float holdS;      // dwell after arriving, before the next leg
};

enum class SeqState : uint8_t { IDLE, MOVING, HOLDING, PAUSED };

class Sequencer {
 public:
  void begin(Motion &motion);
  void update(uint32_t nowMs);

  // --- editing ---
  bool addFromCurrent(String &error);  // capture the live pose
  bool insert(uint8_t at, const Keyframe &kf, String &error);
  bool replace(uint8_t at, const Keyframe &kf, String &error);
  bool remove(uint8_t at);
  void removeLast();
  void clear();
  uint8_t count() const { return count_; }
  const Keyframe &at(uint8_t i) const { return frames_[i < count_ ? i : 0]; }

  // --- transport ---
  bool play(String &error);
  void pause();
  void togglePlayPause();
  void stop();
  void restart();
  bool gotoKeyframe(uint8_t index, String &error);
  SeqState state() const { return state_; }
  uint8_t currentIndex() const { return index_; }

  // --- persistence ---
  bool load();
  bool save() const;
  static const char *path() { return "/sequence.json"; }

  void toJson(JsonObject out) const;
  bool fromJson(JsonObjectConst in, String &error);
  void telemetryJson(JsonObject out) const;

 private:
  Motion *motion_ = nullptr;
  Keyframe frames_[fw::MAX_KEYFRAMES];
  uint8_t count_ = 0;
  uint8_t index_ = 0;
  SeqState state_ = SeqState::IDLE;
  uint32_t holdUntilMs_ = 0;
  uint32_t legStartedMs_ = 0;
  float legDurationS_ = 0.0f;
  // A "go to this pose" jump must not roll on into the rest of the sequence.
  bool oneShot_ = false;

  bool startLeg(uint8_t target, String &error);
  void advance();
};

extern Sequencer sequencer;
