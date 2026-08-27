#include "TmcDrivers.h"

#include <Arduino.h>
#include <TMCStepper.h>

#include "config.h"

namespace {

TMC2209Stepper slideDriver(&Serial1, tmc::R_SENSE_OHMS, tmc::ADDR_SLIDE);
TMC2209Stepper panDriver(&Serial1, tmc::R_SENSE_OHMS, tmc::ADDR_PAN);
TMC2209Stepper tiltDriver(&Serial1, tmc::R_SENSE_OHMS, tmc::ADDR_TILT);

bool allOk_ = false;

bool configureOne(TMC2209Stepper &driver, const char *name, uint8_t addr,
                  uint16_t rmsMa) {
  // 0 = ok; 1/2 = no reply / CRC error. Distinct per-driver addresses mean
  // a wrong MS1/MS2 address strap shows up here as that driver not
  // responding, not as silent misconfiguration.
  if (driver.test_connection() != 0) {
    Serial.printf("ERROR: TMC2209 '%s' (addr %u) not responding on UART\n", name, addr);
    return false;
  }

  driver.begin();
  driver.toff(5);                  // enable chopper
  driver.I_scale_analog(false);    // current from register, NOT the Vref trimpot
  driver.rms_current(rmsMa, tmc::HOLD_CURRENT_FRACTION);
  driver.mstep_reg_select(true);   // microstepping from register, NOT MS1/MS2
  driver.microsteps(tmc::MICROSTEPS);
  driver.intpol(true);             // interpolate to 1/256 between microsteps
  driver.en_spreadCycle(false);    // stealthChop: quiet, smooth — right for cinematic speeds
  driver.pwm_autoscale(true);      // stealthChop current regulation

  // Read back the microstep register: this is the value all step math in
  // config.h assumes, so a driver that didn't take the write must be loud.
  const uint16_t readback = driver.microsteps();
  if (readback != tmc::MICROSTEPS) {
    Serial.printf("ERROR: TMC2209 '%s' microsteps readback %u != configured %u\n",
                  name, readback, tmc::MICROSTEPS);
    return false;
  }

  Serial.printf("TMC2209 '%s': OK, %umA RMS, 1/%u microstepping\n", name, rmsMa,
                tmc::MICROSTEPS);
  return true;
}

}  // namespace

namespace tmc_drivers {

void beginAll() {
  Serial1.begin(115200, SERIAL_8N1, pins::TMC_UART_RX, pins::TMC_UART_TX);

  bool ok = configureOne(slideDriver, "slide", tmc::ADDR_SLIDE, tmc::SLIDE_RMS_MA);
  ok = configureOne(panDriver, "pan", tmc::ADDR_PAN, tmc::PAN_RMS_MA) && ok;
  ok = configureOne(tiltDriver, "tilt", tmc::ADDR_TILT, tmc::TILT_RMS_MA) && ok;
  allOk_ = ok;

  if (!ok) {
    Serial.println(
        "WARNING: one or more TMC2209s unconfigured — current/microstepping at "
        "hardware defaults, position math untrustworthy until fixed");
  }
}

bool allOk() { return allOk_; }

}  // namespace tmc_drivers
