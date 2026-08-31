#include "Settings.h"

#include <LittleFS.h>

Settings settings;

namespace {

const char *const kAxisKindNames[] = {"linear", "rotary"};
const char *const kHomingNames[] = {"none", "limit_min", "limit_max", "sensor"};
const char *const kFeedbackNames[] = {"none", "as5600"};
const char *const kEncoderModeNames[] = {"off", "velocity", "position"};
const char *const kActionNames[] = {
    "none",         "play_pause",   "seq_stop",       "seq_restart",
    "keyframe_add", "keyframe_del", "keyframe_clear", "goto_start",
    "home_all",     "home_axis",    "estop",          "enable_toggle",
    "rec_toggle",   "rec_resync",   "select_next_axis", "zero_axis"};
static_assert(sizeof(kActionNames) / sizeof(kActionNames[0]) ==
                  static_cast<size_t>(ButtonAction::ACTION_COUNT),
              "kActionNames is out of sync with ButtonAction");

// Enum <-> string. An unknown string keeps the caller's existing value rather
// than silently snapping to 0, so a typo in a PATCH cannot quietly disable an
// axis or unbind a button.
template <typename E, size_t N>
E parseEnum(const char *s, const char *const (&names)[N], E fallback) {
  if (s == nullptr) return fallback;
  for (size_t i = 0; i < N; ++i) {
    if (strcmp(s, names[i]) == 0) return static_cast<E>(i);
  }
  return fallback;
}

void copyStr(char *dst, size_t cap, const char *src) {
  if (src == nullptr) return;
  strlcpy(dst, src, cap);
}

// PATCH-friendly readers: only touch the destination when the key is present.
void mergeBool(JsonObjectConst o, const char *k, bool &dst) {
  if (o[k].is<bool>()) dst = o[k].as<bool>();
}
void mergeFloat(JsonObjectConst o, const char *k, float &dst) {
  if (o[k].is<float>()) dst = o[k].as<float>();
}
void mergeU8(JsonObjectConst o, const char *k, uint8_t &dst) {
  if (o[k].is<int>()) dst = static_cast<uint8_t>(o[k].as<int>());
}
void mergeU16(JsonObjectConst o, const char *k, uint16_t &dst) {
  if (o[k].is<int>()) dst = static_cast<uint16_t>(o[k].as<int>());
}
void mergeU32(JsonObjectConst o, const char *k, uint32_t &dst) {
  if (o[k].is<uint32_t>()) dst = o[k].as<uint32_t>();
}
void mergeI32(JsonObjectConst o, const char *k, int32_t &dst) {
  if (o[k].is<int32_t>()) dst = o[k].as<int32_t>();
}
void mergeStr(JsonObjectConst o, const char *k, char *dst, size_t cap) {
  if (o[k].is<const char *>()) copyStr(dst, cap, o[k].as<const char *>());
}

bool isValidMicrostep(uint16_t v) {
  return v == 1 || v == 2 || v == 4 || v == 8 || v == 16 || v == 32 ||
         v == 64 || v == 128 || v == 256;
}

// GPIOs that are physically unusable on an ESP32-S3-WROOM-1 N16R8: 22-25 do
// not exist, 26-32 are the SPI flash bus and 33-37 are the octal PSRAM bus.
// Letting the UI assign one of these would brick the board until a factory
// reset, so they are rejected at the API boundary.
bool isReservedPin(uint8_t pin) {
  if (pin > 48) return true;
  if (pin >= 22 && pin <= 25) return true;
  if (pin >= 26 && pin <= 37) return true;
  return false;
}

bool validatePin(JsonObjectConst o, const char *k, uint8_t &dst, String &error) {
  if (!o[k].is<int>()) return true;
  const int v = o[k].as<int>();
  if (v < 0 || isReservedPin(static_cast<uint8_t>(v))) {
    error = String("pin ") + v + " is reserved or out of range (" + k + ")";
    return false;
  }
  dst = static_cast<uint8_t>(v);
  return true;
}

void axisToJson(const AxisConfig &a, JsonObject o) {
  o["name"] = a.name;
  o["enabled"] = a.enabled;
  o["kind"] = axisKindName(a.kind);
  o["step_pin"] = a.stepPin;
  o["dir_pin"] = a.dirPin;
  o["invert_dir"] = a.invertDir;
  o["tmc_enabled"] = a.tmcEnabled;
  o["tmc_address"] = a.tmcAddress;
  o["microsteps"] = a.microsteps;
  o["run_current_ma"] = a.runCurrentMa;
  o["hold_current_pct"] = a.holdCurrentPct;
  o["stealthchop"] = a.stealthChop;
  o["motor_steps_per_rev"] = a.motorStepsPerRev;
  o["belt_pitch_mm"] = a.beltPitchMm;
  o["pulley_teeth"] = a.pulleyTeeth;
  o["gear_ratio"] = a.gearRatio;
  o["max_speed"] = a.maxSpeed;
  o["accel"] = a.accel;
  o["min_limit"] = a.minLimit;
  o["max_limit"] = a.maxLimit;
  o["soft_limits"] = a.softLimits;
  o["homing"] = homingModeName(a.homing);
  o["limit_min_pin"] = a.limitMinPin;
  o["limit_max_pin"] = a.limitMaxPin;
  o["limit_active_low"] = a.limitActiveLow;
  o["homing_speed"] = a.homingSpeed;
  o["homing_backoff"] = a.homingBackoff;
  o["feedback"] = feedbackTypeName(a.feedback);
  o["mux_channel"] = a.muxChannel;
  o["zero_offset_deg"] = a.zeroOffsetDeg;
  o["drift_check_ms"] = a.driftCheckMs;
  o["drift_threshold_deg"] = a.driftThresholdDeg;
  // Read-only, for display: the UI shows the resulting resolution so the
  // user can sanity-check the mechanics they typed in.
  o["steps_per_unit"] = a.stepsPerUnit();
  o["units"] = a.unitLabel();
}

bool axisFromJson(AxisConfig &a, JsonObjectConst o, String &error) {
  mergeStr(o, "name", a.name, sizeof(a.name));
  mergeBool(o, "enabled", a.enabled);
  a.kind = parseEnum(o["kind"], kAxisKindNames, a.kind);
  if (!validatePin(o, "step_pin", a.stepPin, error)) return false;
  if (!validatePin(o, "dir_pin", a.dirPin, error)) return false;
  mergeBool(o, "invert_dir", a.invertDir);
  mergeBool(o, "tmc_enabled", a.tmcEnabled);
  mergeU8(o, "tmc_address", a.tmcAddress);
  if (a.tmcAddress > 3) {
    error = "tmc_address must be 0-3 (it is set by the MS1/MS2 straps)";
    return false;
  }
  mergeU16(o, "microsteps", a.microsteps);
  if (!isValidMicrostep(a.microsteps)) {
    error = "microsteps must be a power of two from 1 to 256";
    return false;
  }
  mergeU16(o, "run_current_ma", a.runCurrentMa);
  if (a.runCurrentMa > 2000) {
    error = "run_current_ma above 2000 will cook a SilentStepStick";
    return false;
  }
  mergeU8(o, "hold_current_pct", a.holdCurrentPct);
  if (a.holdCurrentPct > 100) a.holdCurrentPct = 100;
  mergeBool(o, "stealthchop", a.stealthChop);
  mergeFloat(o, "motor_steps_per_rev", a.motorStepsPerRev);
  mergeFloat(o, "belt_pitch_mm", a.beltPitchMm);
  mergeFloat(o, "pulley_teeth", a.pulleyTeeth);
  mergeFloat(o, "gear_ratio", a.gearRatio);
  mergeFloat(o, "max_speed", a.maxSpeed);
  mergeFloat(o, "accel", a.accel);
  mergeFloat(o, "min_limit", a.minLimit);
  mergeFloat(o, "max_limit", a.maxLimit);
  mergeBool(o, "soft_limits", a.softLimits);
  a.homing = parseEnum(o["homing"], kHomingNames, a.homing);
  if (!validatePin(o, "limit_min_pin", a.limitMinPin, error)) return false;
  if (!validatePin(o, "limit_max_pin", a.limitMaxPin, error)) return false;
  mergeBool(o, "limit_active_low", a.limitActiveLow);
  mergeFloat(o, "homing_speed", a.homingSpeed);
  mergeFloat(o, "homing_backoff", a.homingBackoff);
  a.feedback = parseEnum(o["feedback"], kFeedbackNames, a.feedback);
  mergeU8(o, "mux_channel", a.muxChannel);
  mergeFloat(o, "zero_offset_deg", a.zeroOffsetDeg);
  mergeU32(o, "drift_check_ms", a.driftCheckMs);
  mergeFloat(o, "drift_threshold_deg", a.driftThresholdDeg);

  if (a.motorStepsPerRev <= 0.0f) {
    error = "motor_steps_per_rev must be > 0";
    return false;
  }
  if (a.stepsPerUnit() <= 0.0f) {
    error = String(a.name) + ": those mechanics give a non-positive steps/unit";
    return false;
  }
  if (a.maxSpeed <= 0.0f || a.accel <= 0.0f) {
    error = String(a.name) + ": max_speed and accel must be > 0";
    return false;
  }
  if (a.minLimit >= a.maxLimit) {
    error = String(a.name) + ": min_limit must be below max_limit";
    return false;
  }
  return true;
}

void encoderToJson(const EncoderConfig &e, JsonObject o) {
  o["name"] = e.name;
  o["enabled"] = e.enabled;
  o["pin_a"] = e.pinA;
  o["pin_b"] = e.pinB;
  o["mode"] = encoderModeName(e.mode);
  o["axis"] = e.axis;
  o["invert"] = e.invert;
  o["counts_per_detent"] = e.countsPerDetent;
  o["units_per_detent"] = e.unitsPerDetent;
  o["detents_for_max_speed"] = e.detentsForMaxSpeed;
}

bool encoderFromJson(EncoderConfig &e, JsonObjectConst o, String &error) {
  mergeStr(o, "name", e.name, sizeof(e.name));
  mergeBool(o, "enabled", e.enabled);
  if (!validatePin(o, "pin_a", e.pinA, error)) return false;
  if (!validatePin(o, "pin_b", e.pinB, error)) return false;
  e.mode = parseEnum(o["mode"], kEncoderModeNames, e.mode);
  mergeU8(o, "axis", e.axis);
  if (e.axis >= fw::AXIS_COUNT) {
    error = "encoder axis index out of range";
    return false;
  }
  mergeBool(o, "invert", e.invert);
  mergeU8(o, "counts_per_detent", e.countsPerDetent);
  if (e.countsPerDetent == 0) e.countsPerDetent = 1;
  mergeFloat(o, "units_per_detent", e.unitsPerDetent);
  mergeI32(o, "detents_for_max_speed", e.detentsForMaxSpeed);
  if (e.detentsForMaxSpeed < 1) e.detentsForMaxSpeed = 1;
  return true;
}

void buttonToJson(const ButtonConfig &b, JsonObject o) {
  o["name"] = b.name;
  o["enabled"] = b.enabled;
  o["pin"] = b.pin;
  o["active_low"] = b.activeLow;
  o["short_press"] = buttonActionName(b.shortPress);
  o["long_press"] = buttonActionName(b.longPress);
  o["axis_arg"] = b.axisArg;
}

bool buttonFromJson(ButtonConfig &b, JsonObjectConst o, String &error) {
  mergeStr(o, "name", b.name, sizeof(b.name));
  mergeBool(o, "enabled", b.enabled);
  if (!validatePin(o, "pin", b.pin, error)) return false;
  mergeBool(o, "active_low", b.activeLow);
  b.shortPress = parseEnum(o["short_press"], kActionNames, b.shortPress);
  b.longPress = parseEnum(o["long_press"], kActionNames, b.longPress);
  mergeU8(o, "axis_arg", b.axisArg);
  if (b.axisArg >= fw::AXIS_COUNT) {
    error = "button axis_arg out of range";
    return false;
  }
  return true;
}

}  // namespace

const char *axisKindName(AxisKind v) {
  return kAxisKindNames[static_cast<uint8_t>(v)];
}
const char *homingModeName(HomingMode v) {
  return kHomingNames[static_cast<uint8_t>(v)];
}
const char *feedbackTypeName(FeedbackType v) {
  return kFeedbackNames[static_cast<uint8_t>(v)];
}
const char *encoderModeName(EncoderMode v) {
  return kEncoderModeNames[static_cast<uint8_t>(v)];
}
const char *buttonActionName(ButtonAction v) {
  const uint8_t i = static_cast<uint8_t>(v);
  return i < static_cast<uint8_t>(ButtonAction::ACTION_COUNT) ? kActionNames[i]
                                                             : kActionNames[0];
}

void Settings::setDefaults() {
  memset(this, 0, sizeof(*this));
  version = fw::SETTINGS_VERSION;

  for (auto &a : axes) {
    a.enabled = true;
    a.tmcEnabled = true;
    a.microsteps = tmc::MICROSTEPS;
    a.runCurrentMa = tmc::RUN_CURRENT_MA;
    a.holdCurrentPct = tmc::HOLD_CURRENT_PCT;
    a.stealthChop = true;
    a.motorStepsPerRev = mech::MOTOR_STEPS_PER_REV;
    a.beltPitchMm = mech::SLIDE_BELT_PITCH_MM;
    a.pulleyTeeth = mech::SLIDE_PULLEY_TEETH;
    a.gearRatio = mech::ROTARY_GEAR_RATIO;
    a.softLimits = true;
    a.limitActiveLow = true;
    a.homing = HomingMode::NONE;
    a.feedback = FeedbackType::NONE;
    a.driftCheckMs = motion::DRIFT_CHECK_INTERVAL_MS;
    a.driftThresholdDeg = motion::DRIFT_THRESHOLD_DEG;
  }

  AxisConfig &slide = axes[fw::AXIS_SLIDE];
  copyStr(slide.name, sizeof(slide.name), "Slide");
  slide.kind = AxisKind::LINEAR;
  slide.stepPin = pins::SLIDE_STEP;
  slide.dirPin = pins::SLIDE_DIR;
  slide.tmcAddress = 0;
  slide.maxSpeed = motion::SLIDE_MAX_SPEED_MM_S;
  slide.accel = motion::SLIDE_ACCEL_MM_S2;
  slide.minLimit = motion::SLIDE_MIN_MM;
  slide.maxLimit = motion::SLIDE_MAX_MM;
  slide.homing = HomingMode::LIMIT_MIN;
  slide.limitMinPin = pins::LIMIT_SLIDE_MIN;
  slide.limitMaxPin = pins::LIMIT_SLIDE_MAX;
  slide.homingSpeed = motion::SLIDE_HOMING_SPEED_MM_S;
  slide.homingBackoff = motion::SLIDE_HOMING_BACKOFF_MM;

  AxisConfig &pan = axes[fw::AXIS_PAN];
  copyStr(pan.name, sizeof(pan.name), "Pan");
  pan.kind = AxisKind::ROTARY;
  pan.stepPin = pins::PAN_STEP;
  pan.dirPin = pins::PAN_DIR;
  pan.tmcAddress = 1;
  pan.maxSpeed = motion::ROTARY_MAX_SPEED_DEG_S;
  pan.accel = motion::ROTARY_ACCEL_DEG_S2;
  pan.minLimit = motion::PAN_MIN_DEG;
  pan.maxLimit = motion::PAN_MAX_DEG;
  pan.homing = HomingMode::SENSOR;
  pan.feedback = FeedbackType::AS5600;
  pan.muxChannel = mux_channel::PAN;

  AxisConfig &tilt = axes[fw::AXIS_TILT];
  copyStr(tilt.name, sizeof(tilt.name), "Tilt");
  tilt.kind = AxisKind::ROTARY;
  tilt.stepPin = pins::TILT_STEP;
  tilt.dirPin = pins::TILT_DIR;
  tilt.tmcAddress = 2;
  tilt.maxSpeed = motion::ROTARY_MAX_SPEED_DEG_S;
  tilt.accel = motion::ROTARY_ACCEL_DEG_S2;
  tilt.minLimit = motion::TILT_MIN_DEG;
  tilt.maxLimit = motion::TILT_MAX_DEG;
  tilt.homing = HomingMode::SENSOR;
  tilt.feedback = FeedbackType::AS5600;
  tilt.muxChannel = mux_channel::TILT;

  // Z is a leadscrew column, homed once against a single switch and then
  // step-counted. The linear model takes mm/rev as belt_pitch * teeth, so an
  // 8mm lead is expressed as 8.0 x 1.
  AxisConfig &z = axes[fw::AXIS_Z];
  copyStr(z.name, sizeof(z.name), "Z");
  z.kind = AxisKind::LINEAR;
  z.stepPin = pins::Z_STEP;
  z.dirPin = pins::Z_DIR;
  z.tmcAddress = 3;
  z.beltPitchMm = mech::Z_LEAD_MM;
  z.pulleyTeeth = 1.0f;
  z.maxSpeed = motion::Z_MAX_SPEED_MM_S;
  z.accel = motion::Z_ACCEL_MM_S2;
  z.minLimit = motion::Z_MIN_MM;
  z.maxLimit = motion::Z_MAX_MM;
  z.homing = HomingMode::LIMIT_MIN;
  z.limitMinPin = pins::LIMIT_Z_HOME;
  z.limitMaxPin = pins::LIMIT_Z_HOME;
  z.homingSpeed = motion::Z_HOMING_SPEED_MM_S;
  z.homingBackoff = motion::Z_HOMING_BACKOFF_MM;

  // The board carries two encoders, each with a push switch: a jog wheel and
  // an angle wheel. Encoder slots 2 and 3 exist in the config model but have
  // no connector on this board, so they ship disabled.
  const uint8_t encPins[fw::ENCODER_COUNT][2] = {
      {pins::ENC_JOG_A, pins::ENC_JOG_B},
      {pins::ENC_ANGLE_A, pins::ENC_ANGLE_B},
      {0, 0},
      {0, 0}};
  const char *encNames[fw::ENCODER_COUNT] = {"Jog wheel", "Angle wheel",
                                             "Enc 3 (n/c)", "Enc 4 (n/c)"};
  for (uint8_t i = 0; i < fw::ENCODER_COUNT; ++i) {
    EncoderConfig &e = encoders[i];
    copyStr(e.name, sizeof(e.name), encNames[i]);
    e.pinA = encPins[i][0];
    e.pinB = encPins[i][1];
    e.countsPerDetent = controls::COUNTS_PER_DETENT;
    e.detentsForMaxSpeed = controls::COUNTS_FOR_MAX_SPEED;
    e.enabled = i < 2;
    if (i == 0) {
      // Jog wheel drives the slide as a speed dial.
      e.axis = fw::AXIS_SLIDE;
      e.mode = EncoderMode::VELOCITY;
      e.unitsPerDetent = controls::MM_PER_DETENT;
    } else if (i == 1) {
      // Angle wheel steps the selected rotary axis' target per click. Which
      // axis that is follows the "select next axis" action, but it has to
      // start somewhere.
      e.axis = fw::AXIS_PAN;
      e.mode = EncoderMode::POSITION;
      e.unitsPerDetent = controls::DEG_PER_DETENT;
    } else {
      e.axis = i;
      e.mode = EncoderMode::OFF;
      e.unitsPerDetent = controls::DEG_PER_DETENT;
    }
  }

  // The only buttons on the board are the two encoder push switches. Slots 2
  // and 3 have no connector and ship disabled.
  const uint8_t btnPins[fw::BUTTON_COUNT] = {pins::ENC_JOG_SW, pins::ENC_ANGLE_SW,
                                             0, 0};
  const char *btnNames[fw::BUTTON_COUNT] = {"Jog push", "Angle push",
                                            "Btn 3 (n/c)", "Btn 4 (n/c)"};
  const ButtonAction shortActions[fw::BUTTON_COUNT] = {
      ButtonAction::PLAY_PAUSE, ButtonAction::SELECT_NEXT_AXIS,
      ButtonAction::NONE, ButtonAction::NONE};
  const ButtonAction longActions[fw::BUTTON_COUNT] = {
      ButtonAction::REC_TOGGLE, ButtonAction::HOME_ALL,
      ButtonAction::NONE, ButtonAction::NONE};
  for (uint8_t i = 0; i < fw::BUTTON_COUNT; ++i) {
    ButtonConfig &b = buttons[i];
    copyStr(b.name, sizeof(b.name), btnNames[i]);
    b.enabled = i < 2;
    b.pin = btnPins[i];
    b.activeLow = true;
    b.shortPress = shortActions[i];
    b.longPress = longActions[i];
  }

  wifi.staEnabled = false;
  wifi.apFallback = true;
  copyStr(wifi.apSsid, sizeof(wifi.apSsid), net::AP_SSID);
  copyStr(wifi.apPass, sizeof(wifi.apPass), net::AP_PASS);
  copyStr(wifi.hostname, sizeof(wifi.hostname), net::HOSTNAME);

  driverEnablePin = pins::DRIVER_EN;
  driverEnableActiveLow = true;
  tmcTxPin = pins::TMC_TX;
  tmcRxPin = pins::TMC_RX;
  tmcBaud = tmc::BAUD;
  tmcRSense = tmc::R_SENSE;

  muxSdaPin = pins::I2C_MUX_SDA;
  muxSclPin = pins::I2C_MUX_SCL;
  muxAddress = i2c_addr::TCA9548A;
  oledSdaPin = pins::I2C_OLED_SDA;
  oledSclPin = pins::I2C_OLED_SCL;
  oledAddress = i2c_addr::OLED;
  displayEnabled = true;
  displayRefreshMs = ui::OLED_REFRESH_INTERVAL_MS;

  bleEnabled = true;
  copyStr(bleName, sizeof(bleName), ble::DEVICE_NAME);

  sequencerLoop = false;
  sequencerEase = true;
}

void Settings::toJson(JsonObject root) const {
  root["version"] = version;
  root["fw_version"] = fw::VERSION;

  JsonArray ja = root["axes"].to<JsonArray>();
  for (const auto &a : axes) axisToJson(a, ja.add<JsonObject>());
  JsonArray je = root["encoders"].to<JsonArray>();
  for (const auto &e : encoders) encoderToJson(e, je.add<JsonObject>());
  JsonArray jb = root["buttons"].to<JsonArray>();
  for (const auto &b : buttons) buttonToJson(b, jb.add<JsonObject>());

  JsonObject w = root["wifi"].to<JsonObject>();
  w["sta_enabled"] = wifi.staEnabled;
  w["ap_fallback"] = wifi.apFallback;
  w["ssid"] = wifi.ssid;
  w["pass"] = "";  // never echo the joined network's password back
  w["pass_set"] = wifi.pass[0] != '\0';
  w["ap_ssid"] = wifi.apSsid;
  w["ap_pass"] = wifi.apPass;
  w["hostname"] = wifi.hostname;

  JsonObject h = root["hardware"].to<JsonObject>();
  h["driver_enable_pin"] = driverEnablePin;
  h["driver_enable_active_low"] = driverEnableActiveLow;
  h["tmc_tx_pin"] = tmcTxPin;
  h["tmc_rx_pin"] = tmcRxPin;
  h["tmc_baud"] = tmcBaud;
  h["tmc_rsense"] = tmcRSense;
  h["mux_sda_pin"] = muxSdaPin;
  h["mux_scl_pin"] = muxSclPin;
  h["mux_address"] = muxAddress;
  h["oled_sda_pin"] = oledSdaPin;
  h["oled_scl_pin"] = oledSclPin;
  h["oled_address"] = oledAddress;
  h["display_enabled"] = displayEnabled;
  h["display_refresh_ms"] = displayRefreshMs;

  JsonObject b = root["ble"].to<JsonObject>();
  b["enabled"] = bleEnabled;
  b["name"] = bleName;

  JsonObject s = root["sequencer"].to<JsonObject>();
  s["loop"] = sequencerLoop;
  s["ease"] = sequencerEase;
}

bool Settings::applyJson(JsonObjectConst root, String &error) {
  // Validate against a scratch copy so a rejected field can never leave the
  // live settings half-updated.
  Settings draft = *this;

  if (root["axes"].is<JsonArrayConst>()) {
    JsonArrayConst ja = root["axes"];
    uint8_t i = 0;
    for (JsonObjectConst o : ja) {
      if (i >= fw::AXIS_COUNT) break;
      if (!axisFromJson(draft.axes[i], o, error)) return false;
      ++i;
    }
  }
  if (root["encoders"].is<JsonArrayConst>()) {
    JsonArrayConst je = root["encoders"];
    uint8_t i = 0;
    for (JsonObjectConst o : je) {
      if (i >= fw::ENCODER_COUNT) break;
      if (!encoderFromJson(draft.encoders[i], o, error)) return false;
      ++i;
    }
  }
  if (root["buttons"].is<JsonArrayConst>()) {
    JsonArrayConst jb = root["buttons"];
    uint8_t i = 0;
    for (JsonObjectConst o : jb) {
      if (i >= fw::BUTTON_COUNT) break;
      if (!buttonFromJson(draft.buttons[i], o, error)) return false;
      ++i;
    }
  }

  if (root["wifi"].is<JsonObjectConst>()) {
    JsonObjectConst w = root["wifi"];
    mergeBool(w, "sta_enabled", draft.wifi.staEnabled);
    mergeBool(w, "ap_fallback", draft.wifi.apFallback);
    mergeStr(w, "ssid", draft.wifi.ssid, sizeof(draft.wifi.ssid));
    // An empty "pass" means "leave it alone" -- toJson deliberately blanks
    // it, so a round-tripped save must not wipe the stored password.
    if (w["pass"].is<const char *>() && w["pass"].as<const char *>()[0] != '\0') {
      copyStr(draft.wifi.pass, sizeof(draft.wifi.pass), w["pass"]);
    }
    mergeStr(w, "ap_ssid", draft.wifi.apSsid, sizeof(draft.wifi.apSsid));
    mergeStr(w, "ap_pass", draft.wifi.apPass, sizeof(draft.wifi.apPass));
    mergeStr(w, "hostname", draft.wifi.hostname, sizeof(draft.wifi.hostname));
    if (draft.wifi.apPass[0] != '\0' && strlen(draft.wifi.apPass) < 8) {
      error = "AP password must be empty (open) or at least 8 characters";
      return false;
    }
  }

  if (root["hardware"].is<JsonObjectConst>()) {
    JsonObjectConst h = root["hardware"];
    if (!validatePin(h, "driver_enable_pin", draft.driverEnablePin, error)) return false;
    mergeBool(h, "driver_enable_active_low", draft.driverEnableActiveLow);
    if (!validatePin(h, "tmc_tx_pin", draft.tmcTxPin, error)) return false;
    if (!validatePin(h, "tmc_rx_pin", draft.tmcRxPin, error)) return false;
    mergeU32(h, "tmc_baud", draft.tmcBaud);
    mergeFloat(h, "tmc_rsense", draft.tmcRSense);
    if (draft.tmcRSense <= 0.0f) {
      error = "tmc_rsense must be > 0";
      return false;
    }
    if (!validatePin(h, "mux_sda_pin", draft.muxSdaPin, error)) return false;
    if (!validatePin(h, "mux_scl_pin", draft.muxSclPin, error)) return false;
    mergeU8(h, "mux_address", draft.muxAddress);
    if (!validatePin(h, "oled_sda_pin", draft.oledSdaPin, error)) return false;
    if (!validatePin(h, "oled_scl_pin", draft.oledSclPin, error)) return false;
    mergeU8(h, "oled_address", draft.oledAddress);
    mergeBool(h, "display_enabled", draft.displayEnabled);
    mergeU16(h, "display_refresh_ms", draft.displayRefreshMs);
    if (draft.displayRefreshMs < 50) draft.displayRefreshMs = 50;
  }

  if (root["ble"].is<JsonObjectConst>()) {
    JsonObjectConst b = root["ble"];
    mergeBool(b, "enabled", draft.bleEnabled);
    mergeStr(b, "name", draft.bleName, sizeof(draft.bleName));
  }

  if (root["sequencer"].is<JsonObjectConst>()) {
    JsonObjectConst s = root["sequencer"];
    mergeBool(s, "loop", draft.sequencerLoop);
    mergeBool(s, "ease", draft.sequencerEase);
  }

  if (!draft.checkPinConflicts(error)) return false;

  *this = draft;
  return true;
}

// Every GPIO that is actually driven or read, with the role that claims it.
// Only roles that are live count: a disabled axis, an off encoder or the
// limit pins of an axis that does not home against switches claim nothing,
// which is why all four axes can keep the same default limit pins.
bool Settings::checkPinConflicts(String &error) const {
  struct Claim {
    uint8_t pin;
    const char *role;
    const char *owner;
  };
  Claim claims[fw::AXIS_COUNT * 4 + fw::ENCODER_COUNT * 2 + fw::BUTTON_COUNT + 8];
  uint8_t n = 0;
  auto claim = [&](uint8_t pin, const char *role, const char *owner) {
    claims[n++] = {pin, role, owner};
  };

  for (const auto &a : axes) {
    if (!a.enabled) continue;
    claim(a.stepPin, "STEP", a.name);
    claim(a.dirPin, "DIR", a.name);
    if (a.homing == HomingMode::LIMIT_MIN || a.homing == HomingMode::LIMIT_MAX) {
      claim(a.limitMinPin, "limit min", a.name);
      // A single-switch axis sets min == max; claiming it twice would make
      // the axis collide with itself.
      if (a.limitMaxPin != a.limitMinPin) claim(a.limitMaxPin, "limit max", a.name);
    }
  }
  for (const auto &e : encoders) {
    if (!e.enabled || e.mode == EncoderMode::OFF) continue;
    claim(e.pinA, "encoder A", e.name);
    claim(e.pinB, "encoder B", e.name);
  }
  for (const auto &b : buttons) {
    if (!b.enabled) continue;
    claim(b.pin, "button", b.name);
  }
  claim(driverEnablePin, "driver enable", "shared");
  claim(tmcTxPin, "TMC UART TX", "shared");
  claim(tmcRxPin, "TMC UART RX", "shared");
  claim(muxSdaPin, "mux SDA", "shared");
  claim(muxSclPin, "mux SCL", "shared");
  if (displayEnabled) {
    claim(oledSdaPin, "OLED SDA", "shared");
    claim(oledSclPin, "OLED SCL", "shared");
  }

  for (uint8_t i = 0; i < n; ++i) {
    for (uint8_t j = i + 1; j < n; ++j) {
      if (claims[i].pin != claims[j].pin) continue;
      error = String("GPIO") + claims[i].pin + " is claimed twice: " +
              claims[i].owner + " " + claims[i].role + " and " +
              claims[j].owner + " " + claims[j].role;
      return false;
    }
  }
  return true;
}

bool Settings::load() {
  File f = LittleFS.open(path(), "r");
  if (!f) {
    Serial.println("[settings] no saved config, using defaults");
    return false;
  }
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[settings] config.json parse failed (%s), using defaults\n",
                  err.c_str());
    return false;
  }
  const uint16_t fileVersion = doc["version"] | 0;
  if (fileVersion != fw::SETTINGS_VERSION) {
    // Older layouts are merged onto current defaults rather than rejected:
    // fields that still exist carry over, new ones take their default.
    Serial.printf("[settings] migrating config from v%u to v%u\n", fileVersion,
                  fw::SETTINGS_VERSION);
  }
  String error;
  if (!applyJson(doc.as<JsonObjectConst>(), error)) {
    Serial.printf("[settings] saved config rejected (%s), using defaults\n",
                  error.c_str());
    setDefaults();
    return false;
  }
  version = fw::SETTINGS_VERSION;
  Serial.println("[settings] loaded /config.json");
  return true;
}

bool Settings::save() const {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  toJson(root);
  // toJson blanks the STA password for the HTTP response; the on-disk copy
  // has to keep the real one or saving would lock us out of the network.
  root["wifi"]["pass"] = wifi.pass;

  File f = LittleFS.open(path(), "w");
  if (!f) {
    Serial.println("[settings] ERROR: cannot open config.json for write");
    return false;
  }
  const size_t written = serializeJson(doc, f);
  f.close();
  if (written == 0) {
    Serial.println("[settings] ERROR: config.json write produced 0 bytes");
    return false;
  }
  Serial.printf("[settings] saved %u bytes to config.json\n",
                static_cast<unsigned>(written));
  return true;
}
