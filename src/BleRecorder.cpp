#include "BleRecorder.h"

#include "Settings.h"

#if ENABLE_BLE_HID
#include <BleKeyboard.h>
namespace {
BleKeyboard *keyboard = nullptr;
}
#endif

BleRecorder bleRecorder;

void BleRecorder::begin() {
#if ENABLE_BLE_HID
  if (!settings.bleEnabled) {
    Serial.println("[ble] disabled in settings");
    return;
  }
  keyboard = new BleKeyboard(settings.bleName, ble::MANUFACTURER,
                             ble::BATTERY_LEVEL);
  keyboard->begin();
  started_ = true;
  Serial.printf("[ble] advertising as \"%s\"\n", settings.bleName);
#else
  Serial.println("[ble] compiled out (ENABLE_BLE_HID=0)");
#endif
}

bool BleRecorder::isConnected() {
#if ENABLE_BLE_HID
  return keyboard != nullptr && keyboard->isConnected();
#else
  return false;
#endif
}

void BleRecorder::toggleRecording() {
#if ENABLE_BLE_HID
  if (!isConnected()) {
    Serial.println("[ble] not connected; ignoring record toggle");
    return;
  }
  keyboard->write(KEY_MEDIA_VOLUME_UP);
  isRecording_ = !isRecording_;
  Serial.printf("[ble] recording toggled -> %s\n", isRecording_ ? "ON" : "OFF");
#endif
}

void BleRecorder::resync() {
  isRecording_ = !isRecording_;
  Serial.printf("[ble] record state resynced (no keypress) -> %s\n",
                isRecording_ ? "ON" : "OFF");
}
