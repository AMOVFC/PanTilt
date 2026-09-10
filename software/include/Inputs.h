#pragma once
// Physical controls: four quadrature encoders and four push buttons, both
// fully remappable from the web UI.
//
// Nothing here knows what an encoder or a button "is for" -- each one carries
// a target axis and a mode/action out of Settings, and this class only
// translates edges into calls on Motion, Sequencer and BleRecorder. Rebinding
// a control is therefore a config change, not a firmware change.
//
// Encoders are decoded in GPIO interrupts rather than with the PCNT
// peripheral -- see QuadEncoder.h for why that matters on a four-axis S3.

#include <Arduino.h>
#include <ArduinoJson.h>
#include "QuadEncoder.h"

#include "DebouncedButton.h"
#include "Settings.h"

class Inputs {
 public:
  void begin();
  void update(uint32_t nowMs);

  // Runs a bound action by name, so the web UI can fire the same actions the
  // physical buttons do.
  void runAction(ButtonAction action, uint8_t axisArg);

  uint8_t selectedAxis() const { return selectedAxis_; }
  void setSelectedAxis(uint8_t a) { if (a < fw::AXIS_COUNT) selectedAxis_ = a; }

  void telemetryJson(JsonObject out) const;

 private:
  // A velocity knob left off its centre detent is a standing "go" command.
  // After an e-stop clears, that would restart motion the instant the drivers
  // come back -- so the knob's current position is redefined as stop.
  void rezeroVelocityEncoders();

  QuadEncoder encoders_[fw::ENCODER_COUNT];
  int32_t lastCount_[fw::ENCODER_COUNT] = {0};
  int32_t detentRemainder_[fw::ENCODER_COUNT] = {0};
  DebouncedButton buttons_[fw::BUTTON_COUNT];
  uint8_t selectedAxis_ = 0;
  bool lastEstop_ = false;

  void updateEncoder(uint8_t i);
  void interruptPlayback();
};

extern Inputs inputs;
