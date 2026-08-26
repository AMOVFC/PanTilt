#pragma once
// TCA9548A channel select + AS5600 raw angle read.
//
// There is deliberately no standalone "read AS5600" entry point: channel
// select must precede every read, or you silently read whichever channel was
// last selected rather than getting a bus error. Wrapping both steps in one
// call makes that mistake impossible to make at the caller.

#include <Arduino.h>
#include <Wire.h>

bool selectMuxChannel(TwoWire &bus, uint8_t muxAddress, uint8_t channel);

// Returns false (and leaves outDeg untouched) if the mux or the AS5600 did
// not acknowledge, so callers can tell "sensor missing" from "reads 0 deg".
bool readAS5600Degrees(TwoWire &bus, uint8_t muxAddress, uint8_t channel,
                       float &outDeg);

// AS5600 STATUS register: magnet detected / too weak / too strong. Used by
// the web UI's sensor check during mechanical bring-up.
struct AS5600Status {
  bool responded = false;
  bool magnetDetected = false;
  bool magnetTooWeak = false;
  bool magnetTooStrong = false;
  uint16_t rawAngle = 0;
};
AS5600Status readAS5600Status(TwoWire &bus, uint8_t muxAddress, uint8_t channel);
