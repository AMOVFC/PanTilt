// Firmware for the 4-axis camera slider (slide / pan / tilt / aux).
//
// Boot order matters: LittleFS -> Settings -> Motion -> Inputs -> Display ->
// BLE -> WiFi. Everything after Settings reads its pin map and tuning out of
// it, so a config saved from the web UI fully defines the machine.
//
// The main loop is deliberately kept free of anything blocking. Step
// generation itself runs in FastAccelStepper's RMT/timer backend, but the
// limit-switch cutoff, the jog encoders and the sequencer's leg transitions
// are all polled here, so a stall in loop() is a stall in the safety checks.

#include <Arduino.h>
#include <LittleFS.h>

#include "BleRecorder.h"
#include "CurveSequence.h"
#include "Display.h"
#include "Inputs.h"
#include "Motion.h"
#include "Sequencer.h"
#include "Settings.h"
#include "WebUI.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== Camera Slider firmware %s ===\n", fw::VERSION);

  // format-on-fail: a blank or corrupted filesystem should self-heal into a
  // defaults-only machine rather than bricking the web UI.
  if (!LittleFS.begin(true)) {
    Serial.println("[fs] ERROR: LittleFS mount failed; running on defaults");
  }

  settings.setDefaults();
  settings.load();

  motion_ctl.begin();
  sequencer.begin(motion_ctl);
  curves.begin(motion_ctl);
  inputs.begin();
  display.begin();
  bleRecorder.begin();
  webui.begin();
  display.setNetworkLine(webui.networkSummary());

  Serial.printf("[boot] free heap %u, free psram %u\n", ESP.getFreeHeap(),
                ESP.getFreePsram());
  Serial.println("[boot] ready");
}

void loop() {
  const uint32_t nowMs = millis();

  // Queued web commands first: they include E-stop, so draining them ahead of
  // the motion update keeps the worst-case stop latency at one loop pass.
  webui.update(nowMs);
  motion_ctl.update(nowMs);
  inputs.update(nowMs);
  sequencer.update(nowMs);
  curves.update(nowMs);
  display.update(nowMs);
}
