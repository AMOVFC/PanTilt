#pragma once
// WiFi access point + HTTP config UI for every runtime setting in
// Settings.h. The rig hosts its own AP (see webcfg:: in config.h) so it's
// reachable on location without any existing network.
//
// Radio note: the ESP32-S3 shares one radio between WiFi and BLE, and this
// firmware already uses BLE HID for the camera trigger. They coexist, but
// both add interrupt load. The HTTP server is synchronous, so a request
// being served briefly blocks the main loop — which is why config writes are
// refused while a shot is running (see the isBusy callback), and why it's
// worth keeping the browser closed during a take you care about.

#include <cstdint>

namespace webconfig {

// isBusy: returns true when motion is in progress and settings must not
// change underneath it. Config writes are rejected with HTTP 409 while it
// returns true; reads stay available.
void begin(bool (*isBusy)());

// Call every loop iteration. Cheap when no client is connected.
void update();

}  // namespace webconfig
