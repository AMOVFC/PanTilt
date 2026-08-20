#pragma once
// TCA9548A channel select + AS5600 raw angle read.
//
// There is deliberately no standalone "read AS5600" entry point: channel
// select must precede every read or you silently read whichever channel was
// last selected, not a bus error. Wrapping both steps in one call makes
// that mistake impossible to make at the caller.
//
// Unchanged from the final rig's Mux — identical mux/AS5600 hardware.

#include <Arduino.h>
#include <Wire.h>

void selectMuxChannel(TwoWire &bus, uint8_t channel);
float readAS5600DegreesOnChannel(TwoWire &bus, uint8_t channel);
