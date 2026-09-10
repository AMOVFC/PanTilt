#include "CurveSequence.h"

#include <LittleFS.h>

CurveSequencer curves;

namespace {
constexpr uint8_t PEAK_SAMPLES = 48;   // per leg, for the feasibility check
constexpr float MIN_LEG_S = 0.01f;

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// One coordinate of a cubic Bezier.
float bez(float p0, float p1, float p2, float p3, float u) {
  const float m = 1.0f - u;
  return m * m * m * p0 + 3.0f * m * m * u * p1 + 3.0f * m * u * u * p2 +
         u * u * u * p3;
}

// The control points of the leg between two keys, in real (seconds, units)
// space. Handles are stored normalised, so this is where they become absolute.
struct Leg {
  float t0, t1, v0, v1;   // endpoints
  float cx1, cy1, cx2, cy2;  // interior control points
};

Leg legBetween(const CurveKey &a, const CurveKey &b) {
  Leg l;
  l.t0 = a.t;
  l.t1 = b.t;
  l.v0 = a.v;
  l.v1 = b.v;
  const float dt = b.t - a.t;
  const float dv = b.v - a.v;
  l.cx1 = a.t + clampf(a.outX, 0.0f, 1.0f) * dt;
  l.cy1 = a.v + a.outY * dv;
  l.cx2 = b.t - clampf(b.inX, 0.0f, 1.0f) * dt;
  l.cy2 = b.v - b.inY * dv;
  return l;
}

// Bezier time is not wall time: solve Bx(u) = t for u, then read By(u).
// Bx is monotonic because both handle x fractions are clamped into [0,1], so
// Newton with a bisection guard always converges. This is the same approach
// browsers use for cubic-bezier() timing functions.
float uAtTime(const Leg &l, float t) {
  const float span = l.t1 - l.t0;
  if (span <= MIN_LEG_S) return 1.0f;
  float u = (t - l.t0) / span;  // linear guess, exact for a linear leg
  float lo = 0.0f, hi = 1.0f;
  for (uint8_t i = 0; i < 12; ++i) {
    const float x = bez(l.t0, l.cx1, l.cx2, l.t1, u);
    const float err = x - t;
    if (fabsf(err) < 1e-5f) break;
    if (err > 0.0f) hi = u; else lo = u;
    const float m = 1.0f - u;
    const float dx = 3.0f * m * m * (l.cx1 - l.t0) + 6.0f * m * u * (l.cx2 - l.cx1) +
                     3.0f * u * u * (l.t1 - l.cx2);
    // A near-flat derivative means Newton would fling u out of range; the
    // bracket is always safe, just slower.
    u = (fabsf(dx) > 1e-6f) ? u - err / dx : 0.5f * (lo + hi);
    if (u < lo || u > hi) u = 0.5f * (lo + hi);
  }
  return clampf(u, 0.0f, 1.0f);
}
}  // namespace

float CurveChannel::valueAt(float t) const {
  if (count == 0) return 0.0f;
  if (count == 1 || t <= keys[0].t) return keys[0].v;
  if (t >= keys[count - 1].t) return keys[count - 1].v;
  uint8_t i = 0;
  while (i + 1 < count && keys[i + 1].t <= t) ++i;
  if (i + 1 >= count) return keys[count - 1].v;
  const Leg l = legBetween(keys[i], keys[i + 1]);
  if (l.t1 - l.t0 <= MIN_LEG_S) return l.v1;
  return bez(l.v0, l.cy1, l.cy2, l.v1, uAtTime(l, t));
}

void CurveChannel::peaks(float &peakSpeed, float &peakAccel) const {
  peakSpeed = 0.0f;
  peakAccel = 0.0f;
  for (uint8_t i = 0; i + 1 < count; ++i) {
    const Leg l = legBetween(keys[i], keys[i + 1]);
    const float span = l.t1 - l.t0;
    if (span <= MIN_LEG_S) continue;
    const float dt = span / PEAK_SAMPLES;
    float prevV = valueAt(l.t0);
    float prevSpeed = 0.0f;
    for (uint8_t s = 1; s <= PEAK_SAMPLES; ++s) {
      const float v = valueAt(l.t0 + dt * s);
      const float speed = fabsf(v - prevV) / dt;
      if (speed > peakSpeed) peakSpeed = speed;
      if (s > 1) {
        const float accel = fabsf(speed - prevSpeed) / dt;
        if (accel > peakAccel) peakAccel = accel;
      }
      prevSpeed = speed;
      prevV = v;
    }
  }
}

void CurveSequence::clear() {
  name[0] = '\0';
  for (uint8_t a = 0; a < fw::AXIS_COUNT; ++a) channels[a].count = 0;
}

float CurveSequence::duration() const {
  float d = 0.0f;
  for (uint8_t a = 0; a < fw::AXIS_COUNT; ++a) d = max(d, channels[a].endTime());
  return d;
}

uint16_t CurveSequence::totalKeys() const {
  uint16_t n = 0;
  for (uint8_t a = 0; a < fw::AXIS_COUNT; ++a) n += channels[a].count;
  return n;
}

void CurveSequence::toJson(JsonObject out) const {
  out["name"] = name;
  out["duration_s"] = duration();
  JsonArray chans = out["channels"].to<JsonArray>();
  for (uint8_t a = 0; a < fw::AXIS_COUNT; ++a) {
    JsonObject c = chans.add<JsonObject>();
    c["axis"] = a;
    c["name"] = settings.axes[a].name;
    c["units"] = settings.axes[a].unitLabel();
    JsonArray ks = c["keys"].to<JsonArray>();
    for (uint8_t i = 0; i < channels[a].count; ++i) {
      const CurveKey &k = channels[a].keys[i];
      JsonObject o = ks.add<JsonObject>();
      o["t"] = k.t;
      o["v"] = k.v;
      o["out_x"] = k.outX;
      o["out_y"] = k.outY;
      o["in_x"] = k.inX;
      o["in_y"] = k.inY;
    }
  }
}

bool CurveSequence::fromJson(JsonObjectConst in, String &error) {
  if (!in["channels"].is<JsonArrayConst>()) {
    error = "expected a channels array";
    return false;
  }
  CurveSequence staging;
  const char *n = in["name"] | "";
  strlcpy(staging.name, n, sizeof(staging.name));

  for (JsonObjectConst c : in["channels"].as<JsonArrayConst>()) {
    const int a = c["axis"] | -1;
    if (a < 0 || a >= fw::AXIS_COUNT) {
      error = "channel names an axis that does not exist";
      return false;
    }
    JsonArrayConst ks = c["keys"];
    if (ks.size() > fw::MAX_CURVE_KEYS) {
      error = String(settings.axes[a].name) + ": too many keys (max " +
              fw::MAX_CURVE_KEYS + ")";
      return false;
    }
    CurveChannel &ch = staging.channels[a];
    ch.count = 0;
    float lastT = -1.0f;
    for (JsonObjectConst o : ks) {
      CurveKey k;
      k.t = o["t"] | 0.0f;
      k.v = o["v"] | 0.0f;
      k.outX = clampf(o["out_x"] | 0.33f, 0.0f, 1.0f);
      k.outY = clampf(o["out_y"] | 0.0f, -2.0f, 3.0f);
      k.inX = clampf(o["in_x"] | 0.33f, 0.0f, 1.0f);
      k.inY = clampf(o["in_y"] | 0.0f, -2.0f, 3.0f);
      if (k.t < 0.0f) k.t = 0.0f;
      // Keys must be strictly ordered in time: the player walks them in order
      // and the Bezier solve assumes a positive span.
      if (k.t <= lastT) k.t = lastT + MIN_LEG_S;
      lastT = k.t;
      ch.keys[ch.count++] = k;
    }
  }
  *this = staging;
  return true;
}

// ---------------------------------------------------------------- sequencer

void CurveSequencer::slotPath(uint8_t slot, char *buf, size_t n) {
  // Flat names, not a /curves/ directory: LittleFS will not create a missing
  // parent directory on open(), and a slot file failing to save because of
  // that would be a miserable thing to debug on location.
  snprintf(buf, n, "/curve%u.json", static_cast<unsigned>(slot));
}

void CurveSequencer::begin(Motion &motion) {
  motion_ = &motion;
  refreshSlotNames();
  // Come back up on whatever was last loaded, so a power cycle between takes
  // does not lose the operator's place.
  String error;
  for (uint8_t s = 0; s < fw::MAX_CURVE_SEQUENCES; ++s) {
    if (slotNames_[s][0] != '\0') {
      loadSlot(s, error);
      break;
    }
  }
}

void CurveSequencer::refreshSlotNames() {
  char path[24];
  for (uint8_t s = 0; s < fw::MAX_CURVE_SEQUENCES; ++s) {
    slotNames_[s][0] = '\0';
    slotPath(s, path, sizeof(path));
    File f = LittleFS.open(path, "r");
    if (!f) continue;
    // Only the name is wanted here, but the documents are a few kB at most and
    // this runs on slot changes, not per frame.
    JsonDocument doc;
    if (deserializeJson(doc, f) == DeserializationError::Ok) {
      strlcpy(slotNames_[s], doc["name"] | "", sizeof(slotNames_[s]));
      if (slotNames_[s][0] == '\0') snprintf(slotNames_[s], sizeof(slotNames_[s]), "Sequence %u", s + 1);
    }
    f.close();
  }
}

bool CurveSequencer::loadSlot(uint8_t slot, String &error) {
  if (slot >= fw::MAX_CURVE_SEQUENCES) {
    error = "slot out of range";
    return false;
  }
  char path[24];
  slotPath(slot, path, sizeof(path));
  File f = LittleFS.open(path, "r");
  if (!f) {
    error = "that slot is empty";
    return false;
  }
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    error = String("slot ") + (slot + 1) + " is corrupt: " + err.c_str();
    return false;
  }
  if (!seq_.fromJson(doc.as<JsonObjectConst>(), error)) return false;
  stop();
  activeSlot_ = static_cast<int8_t>(slot);
  Serial.printf("[curve] loaded slot %u \"%s\" (%u keys)\n", slot, seq_.name,
                seq_.totalKeys());
  return true;
}

bool CurveSequencer::saveSlot(uint8_t slot, const char *name, String &error) {
  if (slot >= fw::MAX_CURVE_SEQUENCES) {
    error = "slot out of range";
    return false;
  }
  if (name != nullptr && name[0] != '\0') strlcpy(seq_.name, name, sizeof(seq_.name));
  if (seq_.name[0] == '\0') {
    snprintf(seq_.name, sizeof(seq_.name), "Sequence %u", slot + 1);
  }
  char path[24];
  slotPath(slot, path, sizeof(path));
  File f = LittleFS.open(path, "w");
  if (!f) {
    error = "cannot open the slot file for writing";
    return false;
  }
  JsonDocument doc;
  seq_.toJson(doc.to<JsonObject>());
  const size_t written = serializeJson(doc, f);
  f.close();
  if (written == 0) {
    error = "write failed -- the filesystem may be full";
    return false;
  }
  activeSlot_ = static_cast<int8_t>(slot);
  refreshSlotNames();
  Serial.printf("[curve] saved slot %u \"%s\" (%u bytes)\n", slot, seq_.name, written);
  return true;
}

bool CurveSequencer::deleteSlot(uint8_t slot, String &error) {
  if (slot >= fw::MAX_CURVE_SEQUENCES) {
    error = "slot out of range";
    return false;
  }
  char path[24];
  slotPath(slot, path, sizeof(path));
  if (!LittleFS.remove(path)) {
    error = "that slot is already empty";
    return false;
  }
  if (activeSlot_ == static_cast<int8_t>(slot)) activeSlot_ = -1;
  refreshSlotNames();
  return true;
}

bool CurveSequencer::checkFeasible(String &error) const {
  for (uint8_t a = 0; a < fw::AXIS_COUNT; ++a) {
    const CurveChannel &ch = seq_.channels[a];
    if (ch.count < 2) continue;
    const AxisConfig &c = settings.axes[a];
    if (!c.enabled) {
      error = String(c.name) + " has curve keys but is disabled";
      return false;
    }
    float peakSpeed = 0.0f, peakAccel = 0.0f;
    ch.peaks(peakSpeed, peakAccel);
    if (peakSpeed > c.maxSpeed * 1.02f) {
      error = String(c.name) + " needs " + String(peakSpeed, 1) + " " +
              c.unitLabel() + "/s but tops out at " + String(c.maxSpeed, 1) +
              " -- lengthen that leg or flatten its handles";
      return false;
    }
    if (peakAccel > c.accel * 1.05f) {
      error = String(c.name) + " needs " + String(peakAccel, 0) + " " +
              c.unitLabel() + "/s2 of acceleration but is limited to " +
              String(c.accel, 0) + " -- ease the corner or slow the leg";
      return false;
    }
  }
  return true;
}

bool CurveSequencer::play(String &error) {
  if (motion_ == nullptr) return false;
  if (seq_.empty()) {
    error = "this sequence has no keys";
    return false;
  }
  if (motion_->estopped()) {
    error = "E-stop is latched; clear it before playing";
    return false;
  }
  if (!checkFeasible(error)) return false;
  durationS_ = seq_.duration();
  if (durationS_ <= 0.0f) {
    error = "this sequence has no length";
    return false;
  }
  if (!motion_->driversEnabled()) motion_->setDriversEnabled(true);
  if (state_ != CurveState::PAUSED) elapsedS_ = 0.0f;
  tickBaseMs_ = millis();
  lastTickMs_ = 0;
  state_ = CurveState::PLAYING;
  Serial.printf("[curve] playing \"%s\" (%.2fs)\n", seq_.name, durationS_);
  return true;
}

void CurveSequencer::pause() {
  if (state_ != CurveState::PLAYING) return;
  elapsedS_ += (millis() - tickBaseMs_) / 1000.0f;
  motion_->stopAll();
  state_ = CurveState::PAUSED;
}

void CurveSequencer::stop() {
  if (motion_ != nullptr && state_ != CurveState::IDLE) motion_->stopAll();
  state_ = CurveState::IDLE;
  elapsedS_ = 0.0f;
}

bool CurveSequencer::gotoTime(float t, String &error) {
  if (motion_ == nullptr) return false;
  if (seq_.empty()) {
    error = "this sequence has no keys";
    return false;
  }
  if (motion_->estopped()) {
    error = "E-stop is latched";
    return false;
  }
  if (!motion_->driversEnabled()) motion_->setDriversEnabled(true);
  stop();
  elapsedS_ = max(0.0f, t);
  commandAt(elapsedS_, true);
  return true;
}

// `exact` sends a normal full-speed move to the sampled pose (used for
// scrubbing); otherwise the move is timed to land exactly one tick ahead,
// which is what makes the playback follow the drawn curve.
void CurveSequencer::commandAt(float t, bool exact) {
  const float tickS = ui::CURVE_TICK_MS / 1000.0f;
  String err;
  for (uint8_t a = 0; a < fw::AXIS_COUNT; ++a) {
    const CurveChannel &ch = seq_.channels[a];
    if (ch.count == 0) continue;  // axis is not part of this move
    Axis &ax = motion_->axis(a);
    if (!ax.available()) continue;
    const float target = ch.valueAt(t);
    if (exact) {
      ax.moveTo(target, err);
    } else {
      // Distance is measured from where the axis actually is, so a lagging
      // axis speeds up to catch the curve instead of falling further behind.
      ax.moveTimed(target, tickS, false, err);
    }
  }
}

void CurveSequencer::update(uint32_t nowMs) {
  if (state_ != CurveState::PLAYING) return;
  if (motion_->estopped()) {
    elapsedS_ += (nowMs - tickBaseMs_) / 1000.0f;
    state_ = CurveState::PAUSED;
    return;
  }
  if (lastTickMs_ != 0 && nowMs - lastTickMs_ < ui::CURVE_TICK_MS) return;
  lastTickMs_ = nowMs;

  const float t = elapsedS_ + (nowMs - tickBaseMs_) / 1000.0f;
  if (t >= durationS_) {
    finish();
    return;
  }
  // Aim one tick ahead: the axis should arrive where the curve will be by the
  // time the next waypoint is issued, not where it was when this one was.
  commandAt(min(t + ui::CURVE_TICK_MS / 1000.0f, durationS_), false);
}

void CurveSequencer::finish() {
  // Land exactly on the final key rather than wherever the last waypoint's
  // ramp happened to leave the axis.
  commandAt(durationS_, true);
  state_ = CurveState::IDLE;
  elapsedS_ = 0.0f;
  Serial.println("[curve] sequence complete");
}

void CurveSequencer::slotsJson(JsonObject out) const {
  out["active"] = activeSlot_;
  out["max_keys"] = fw::MAX_CURVE_KEYS;
  JsonArray arr = out["slots"].to<JsonArray>();
  for (uint8_t s = 0; s < fw::MAX_CURVE_SEQUENCES; ++s) {
    JsonObject o = arr.add<JsonObject>();
    o["slot"] = s;
    o["name"] = slotNames_[s];
    o["empty"] = slotNames_[s][0] == '\0';
  }
}

void CurveSequencer::toJson(JsonObject out) const {
  seq_.toJson(out);
  out["active_slot"] = activeSlot_;
  out["max_keys"] = fw::MAX_CURVE_KEYS;
}

void CurveSequencer::telemetryJson(JsonObject out) const {
  switch (state_) {
    case CurveState::IDLE: out["state"] = "idle"; break;
    case CurveState::PLAYING: out["state"] = "playing"; break;
    case CurveState::PAUSED: out["state"] = "paused"; break;
  }
  const float t = state_ == CurveState::PLAYING
                      ? elapsedS_ + (millis() - tickBaseMs_) / 1000.0f
                      : elapsedS_;
  out["t"] = t;
  out["duration_s"] = seq_.duration();
  out["slot"] = activeSlot_;
  out["name"] = seq_.name;
  out["keys"] = seq_.totalKeys();
}
