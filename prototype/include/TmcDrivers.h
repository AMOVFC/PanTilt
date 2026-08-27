#pragma once
// UART configuration plane for the three TMC2209s (slide/pan/tilt). Motion
// stays on STEP/DIR via FastAccelStepper — UART is used at boot to set
// current and microstepping in firmware, verify each driver actually
// responds, and confirm the microstep register took. Same config/motion
// split Marlin uses for TMC drivers.
//
// Trimmed from the final rig's TmcDrivers: 3 drivers instead of 4, no Z.

#include <cstdint>

namespace tmc_drivers {

// Initializes the shared half-duplex UART bus and configures all 3 drivers:
// current, microstepping, stealthChop, interpolation. Call once in setup(),
// after driver power is up, before any axis begin(). Serial must already be
// initialized (errors are reported there).
void beginAll();

// True if every driver responded on UART and accepted its configuration.
// Motion still works without UART (STEP/DIR is independent), but current
// and microstepping are then whatever the hardware defaults to — position
// math is NOT trustworthy until the wiring issue is fixed.
bool allOk();

}  // namespace tmc_drivers
