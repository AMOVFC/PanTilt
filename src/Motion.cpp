#include "Motion.h"

Motion motion_ctl;

void Motion::lockBus() {
  if (busMutex_ != nullptr) xSemaphoreTakeRecursive(busMutex_, portMAX_DELAY);
}

void Motion::unlockBus() {
  if (busMutex_ != nullptr) xSemaphoreGiveRecursive(busMutex_);
}

void Motion::begin() {
  busMutex_ = xSemaphoreCreateRecursiveMutex();

  pinMode(settings.driverEnablePin, OUTPUT);
  setDriversEnabled(false);  // stay de-energised until everything is up

  engine_.init();

  Wire.begin(settings.muxSdaPin, settings.muxSclPin, 400000);

  // All four drivers share one half-duplex UART. Serial1 is free on this
  // board (Serial0 is the USB console, Serial2 is unused).
  bool wantTmc = false;
  for (const auto &a : settings.axes) wantTmc |= a.enabled && a.tmcEnabled;
  if (wantTmc) {
    Serial1.begin(settings.tmcBaud, SERIAL_8N1, settings.tmcRxPin,
                  settings.tmcTxPin);
    tmcSerial_ = &Serial1;
  }

  for (uint8_t i = 0; i < fw::AXIS_COUNT; ++i) {
    axes_[i].begin(i, engine_, &Wire, tmcSerial_);
  }

  setDriversEnabled(true);
}

void Motion::setDriversEnabled(bool on) {
  driversEnabled_ = on;
  const bool level = settings.driverEnableActiveLow ? !on : on;
  digitalWrite(settings.driverEnablePin, level ? HIGH : LOW);
}

void Motion::update(uint32_t nowMs) {
  for (auto &a : axes_) a.update(nowMs);
}

void Motion::homeAll() {
  if (estopped_) return;
  for (auto &a : axes_) a.startHoming();
}

void Motion::stopAll() {
  for (auto &a : axes_) a.stop();
}

void Motion::estop() {
  for (auto &a : axes_) a.forceStop();
  setDriversEnabled(false);
  estopped_ = true;
  Serial.println("[motion] E-STOP: drivers de-energised");
}

void Motion::clearEstop() {
  if (!estopped_) return;
  estopped_ = false;
  setDriversEnabled(true);
  // Steppers can and do slip while de-energised, so nothing may be where the
  // firmware last left it. An axis with an absolute encoder just re-reads it
  // (no movement); the rest are marked un-homed and wait for the operator,
  // because starting a homing move on its own after an e-stop would be
  // exactly the surprise motion the e-stop existed to prevent.
  for (auto &a : axes_) {
    if (a.cfg().feedback == FeedbackType::AS5600) {
      a.startHoming();
    } else if (a.cfg().homing != HomingMode::NONE) {
      a.invalidateHoming();
    }
  }
  Serial.println("[motion] E-STOP cleared");
}

bool Motion::anyRunning() const {
  for (const auto &a : axes_) {
    if (a.isRunning()) return true;
  }
  return false;
}

bool Motion::allHomed() const {
  for (uint8_t i = 0; i < fw::AXIS_COUNT; ++i) {
    if (settings.axes[i].enabled && !axes_[i].isHomed()) return false;
  }
  return true;
}

void Motion::telemetryJson(JsonObject out) const {
  out["drivers_enabled"] = driversEnabled_;
  out["estop"] = estopped_;
  out["running"] = anyRunning();
  out["homed"] = allHomed();
  JsonArray arr = out["axes"].to<JsonArray>();
  for (const auto &a : axes_) a.telemetryJson(arr.add<JsonObject>());
}
