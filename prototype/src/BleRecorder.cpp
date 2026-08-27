#include "BleRecorder.h"

void BleRecorder::begin() { keyboard_.begin(); }

void BleRecorder::toggleRecording() {
  if (!keyboard_.isConnected()) {
    Serial.println("BLE not connected; ignoring record toggle");
    return;
  }
  keyboard_.write(KEY_MEDIA_VOLUME_UP);
  isRecording_ = !isRecording_;
  Serial.printf("Recording toggled -> %s\n", isRecording_ ? "ON" : "OFF");
}

void BleRecorder::resync() {
  isRecording_ = !isRecording_;
  Serial.printf("Recording state resynced (no keypress) -> %s\n",
                isRecording_ ? "ON" : "OFF");
}
