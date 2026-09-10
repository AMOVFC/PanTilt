#include "Sequencer.h"

#include <LittleFS.h>

Sequencer sequencer;

namespace {
constexpr float DEFAULT_DURATION_S = 3.0f;
constexpr float MIN_DURATION_S = 0.05f;
constexpr uint32_t LEG_SETTLE_MS = 40;
}  // namespace

void Sequencer::begin(Motion &motion) {
  motion_ = &motion;
  load();
}

bool Sequencer::addFromCurrent(String &error) {
  if (count_ >= fw::MAX_KEYFRAMES) {
    error = "keyframe list is full";
    return false;
  }
  Keyframe kf{};
  for (uint8_t i = 0; i < fw::AXIS_COUNT; ++i) {
    kf.pos[i] = motion_->axis(i).positionUnits();
  }
  kf.durationS = DEFAULT_DURATION_S;
  kf.holdS = 0.0f;
  frames_[count_++] = kf;
  Serial.printf("[seq] captured keyframe %u\n", count_ - 1);
  return true;
}

bool Sequencer::insert(uint8_t at, const Keyframe &kf, String &error) {
  if (count_ >= fw::MAX_KEYFRAMES) {
    error = "keyframe list is full";
    return false;
  }
  if (at > count_) at = count_;
  for (uint8_t i = count_; i > at; --i) frames_[i] = frames_[i - 1];
  frames_[at] = kf;
  ++count_;
  return true;
}

bool Sequencer::replace(uint8_t at, const Keyframe &kf, String &error) {
  if (at >= count_) {
    error = "keyframe index out of range";
    return false;
  }
  frames_[at] = kf;
  return true;
}

bool Sequencer::remove(uint8_t at) {
  if (at >= count_) return false;
  for (uint8_t i = at; i + 1 < count_; ++i) frames_[i] = frames_[i + 1];
  --count_;
  if (index_ >= count_) index_ = count_ ? count_ - 1 : 0;
  return true;
}

void Sequencer::removeLast() {
  if (count_ > 0) remove(count_ - 1);
}

void Sequencer::clear() {
  stop();
  count_ = 0;
  index_ = 0;
}

// Commands every axis toward keyframe `target`, stretching the leg so the
// slowest axis sets the pace.
bool Sequencer::startLeg(uint8_t target, String &error) {
  if (target >= count_) {
    error = "keyframe index out of range";
    return false;
  }
  if (motion_->estopped()) {
    error = "E-stop is latched; clear it before playing";
    return false;
  }
  if (!motion_->driversEnabled()) motion_->setDriversEnabled(true);

  const Keyframe &kf = frames_[target];
  float duration = max(kf.durationS, MIN_DURATION_S);
  const bool ease = settings.sequencerEase;

  for (uint8_t i = 0; i < fw::AXIS_COUNT; ++i) {
    Axis &ax = motion_->axis(i);
    if (!ax.available()) continue;
    duration = max(duration,
                   ax.minMoveTime(ax.positionUnits(), kf.pos[i], ease));
  }

  bool ok = true;
  for (uint8_t i = 0; i < fw::AXIS_COUNT; ++i) {
    Axis &ax = motion_->axis(i);
    if (!ax.available()) continue;
    String axError;
    if (!ax.moveTimed(kf.pos[i], duration, ease, axError)) {
      // A clamped axis is a warning, not a stop: the rest of the move still
      // makes sense and the operator sees the message.
      Serial.printf("[seq] %s\n", axError.c_str());
      if (error.isEmpty()) error = axError;
      ok = false;
    }
  }

  index_ = target;
  legDurationS_ = duration;
  legStartedMs_ = millis();
  state_ = SeqState::MOVING;
  return ok;
}

bool Sequencer::play(String &error) {
  if (count_ == 0) {
    error = "no keyframes recorded";
    return false;
  }
  if (state_ == SeqState::MOVING) return true;
  if (state_ == SeqState::PAUSED) {
    // Resume by re-issuing the leg we were interrupted on; positions have
    // not changed, so this just re-derives the remaining move.
    String e;
    oneShot_ = false;
    startLeg(index_, e);
    return true;
  }
  // Fresh start: drive to the first pose, then run the list from there.
  index_ = 0;
  oneShot_ = false;
  return startLeg(0, error);
}

void Sequencer::pause() {
  if (state_ != SeqState::MOVING && state_ != SeqState::HOLDING) return;
  motion_->stopAll();
  state_ = SeqState::PAUSED;
}

void Sequencer::togglePlayPause() {
  if (state_ == SeqState::MOVING || state_ == SeqState::HOLDING) {
    pause();
  } else {
    String error;
    if (!play(error)) Serial.printf("[seq] %s\n", error.c_str());
  }
}

void Sequencer::stop() {
  if (motion_ != nullptr) motion_->stopAll();
  state_ = SeqState::IDLE;
  index_ = 0;
  oneShot_ = false;
}

void Sequencer::restart() {
  stop();
  String error;
  if (!play(error)) Serial.printf("[seq] %s\n", error.c_str());
}

bool Sequencer::gotoKeyframe(uint8_t index, String &error) {
  oneShot_ = true;
  if (!startLeg(index, error)) {
    oneShot_ = false;
    return false;
  }
  return true;
}

void Sequencer::advance() {
  if (index_ + 1 < count_) {
    String error;
    startLeg(index_ + 1, error);
    return;
  }
  if (settings.sequencerLoop && count_ > 0) {
    String error;
    startLeg(0, error);
    return;
  }
  state_ = SeqState::IDLE;
  Serial.println("[seq] sequence complete");
}

void Sequencer::update(uint32_t nowMs) {
  if (motion_ == nullptr) return;
  switch (state_) {
    case SeqState::MOVING:
      if (motion_->estopped()) {
        state_ = SeqState::PAUSED;
        break;
      }
      // FastAccelStepper needs a moment to report a freshly commanded move as
      // running; without this guard a leg can look complete on the very pass
      // that started it, and the whole sequence flashes past in one loop.
      if (nowMs - legStartedMs_ < LEG_SETTLE_MS) break;
      if (!motion_->anyRunning()) {
        if (oneShot_) {
          // A manual jump ends here rather than rolling into the next leg.
          oneShot_ = false;
          state_ = SeqState::IDLE;
          break;
        }
        const float holdS = frames_[index_].holdS;
        if (holdS > 0.0f) {
          holdUntilMs_ = nowMs + static_cast<uint32_t>(holdS * 1000.0f);
          state_ = SeqState::HOLDING;
        } else {
          advance();
        }
      }
      break;
    case SeqState::HOLDING:
      if (motion_->estopped()) {
        state_ = SeqState::PAUSED;
        break;
      }
      // Signed compare so the wrap of millis() cannot strand a hold forever.
      if (static_cast<int32_t>(nowMs - holdUntilMs_) >= 0) advance();
      break;
    default:
      break;
  }
}

void Sequencer::toJson(JsonObject out) const {
  out["count"] = count_;
  out["loop"] = settings.sequencerLoop;
  out["ease"] = settings.sequencerEase;
  JsonArray arr = out["keyframes"].to<JsonArray>();
  for (uint8_t i = 0; i < count_; ++i) {
    JsonObject o = arr.add<JsonObject>();
    JsonArray p = o["pos"].to<JsonArray>();
    for (uint8_t a = 0; a < fw::AXIS_COUNT; ++a) p.add(frames_[i].pos[a]);
    o["duration_s"] = frames_[i].durationS;
    o["hold_s"] = frames_[i].holdS;
  }
}

bool Sequencer::fromJson(JsonObjectConst in, String &error) {
  if (!in["keyframes"].is<JsonArrayConst>()) {
    error = "expected a keyframes array";
    return false;
  }
  JsonArrayConst arr = in["keyframes"];
  if (arr.size() > fw::MAX_KEYFRAMES) {
    error = String("too many keyframes (max ") + fw::MAX_KEYFRAMES + ")";
    return false;
  }
  Keyframe staging[fw::MAX_KEYFRAMES]{};
  uint8_t n = 0;
  for (JsonObjectConst o : arr) {
    JsonArrayConst p = o["pos"];
    uint8_t a = 0;
    for (JsonVariantConst v : p) {
      if (a >= fw::AXIS_COUNT) break;
      staging[n].pos[a++] = v.as<float>();
    }
    staging[n].durationS = o["duration_s"] | DEFAULT_DURATION_S;
    staging[n].holdS = o["hold_s"] | 0.0f;
    if (staging[n].durationS < MIN_DURATION_S) staging[n].durationS = MIN_DURATION_S;
    if (staging[n].holdS < 0.0f) staging[n].holdS = 0.0f;
    ++n;
  }
  stop();
  memcpy(frames_, staging, sizeof(frames_));
  count_ = n;
  return true;
}

bool Sequencer::load() {
  File f = LittleFS.open(path(), "r");
  if (!f) return false;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[seq] sequence.json parse failed: %s\n", err.c_str());
    return false;
  }
  String error;
  if (!fromJson(doc.as<JsonObjectConst>(), error)) {
    Serial.printf("[seq] sequence.json rejected: %s\n", error.c_str());
    return false;
  }
  Serial.printf("[seq] loaded %u keyframes\n", count_);
  return true;
}

bool Sequencer::save() const {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  toJson(root);
  File f = LittleFS.open(path(), "w");
  if (!f) {
    Serial.println("[seq] ERROR: cannot open sequence.json for write");
    return false;
  }
  serializeJson(doc, f);
  f.close();
  return true;
}

void Sequencer::telemetryJson(JsonObject out) const {
  switch (state_) {
    case SeqState::IDLE: out["state"] = "idle"; break;
    case SeqState::MOVING: out["state"] = "moving"; break;
    case SeqState::HOLDING: out["state"] = "holding"; break;
    case SeqState::PAUSED: out["state"] = "paused"; break;
  }
  out["index"] = index_;
  out["count"] = count_;
  out["leg_duration_s"] = legDurationS_;
}
