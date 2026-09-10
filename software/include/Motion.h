#pragma once
// Owns the four axes plus everything they share: the FastAccelStepper engine,
// the TCA9548A I2C bus, the half-duplex TMC2209 UART bus, and the single
// active-low enable line that gates all four drivers at once.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastAccelStepper.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "Axis.h"
#include "Settings.h"

class Motion {
 public:
  void begin();
  void update(uint32_t nowMs);

  Axis &axis(uint8_t i) { return axes_[i < fw::AXIS_COUNT ? i : 0]; }
  const Axis &axis(uint8_t i) const { return axes_[i < fw::AXIS_COUNT ? i : 0]; }

  // The enable line is shared by all four TMC2209s, so it is owned here
  // rather than by any one axis. FastAccelStepper's per-stepper auto-enable
  // is deliberately not used: it assumes exclusive ownership of the pin.
  void setDriversEnabled(bool on);
  bool driversEnabled() const { return driversEnabled_; }

  void homeAll();
  void stopAll();
  // Hard stop plus de-energise. Latches until clearEstop(), so nothing --
  // encoder, web UI or sequencer -- can start the machine moving again by
  // accident.
  void estop();
  void clearEstop();
  bool estopped() const { return estopped_; }

  bool anyRunning() const;
  bool allHomed() const;

  TwoWire &muxBus() { return Wire; }
  bool tmcBusStarted() const { return tmcSerial_ != nullptr; }

  // The TCA9548A and the TMC UART are both multi-step, stateful protocols
  // shared between loop() and the web server's task: a mux channel select
  // followed by a read, and a UART request followed by its reply. Interleave
  // two of those and you read the wrong sensor or a corrupted register, so
  // every such sequence takes this lock. See BusLock below.
  void lockBus();
  void unlockBus();

  void telemetryJson(JsonObject out) const;

 private:
  FastAccelStepperEngine engine_;
  Axis axes_[fw::AXIS_COUNT];
  HardwareSerial *tmcSerial_ = nullptr;
  SemaphoreHandle_t busMutex_ = nullptr;
  bool driversEnabled_ = false;
  bool estopped_ = false;
};

// Scoped helper for Motion::lockBus(). Recursive, so a locked operation can
// safely call another one.
class BusLock {
 public:
  explicit BusLock(Motion &m) : m_(m) { m_.lockBus(); }
  ~BusLock() { m_.unlockBus(); }
  BusLock(const BusLock &) = delete;
  BusLock &operator=(const BusLock &) = delete;

 private:
  Motion &m_;
};

extern Motion motion_ctl;
