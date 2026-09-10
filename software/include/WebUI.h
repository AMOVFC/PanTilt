#pragma once
// WiFi + HTTP/WebSocket control surface.
//
// Concurrency: ESPAsyncWebServer runs its handlers on the AsyncTCP task, not
// in loop(). Nothing here touches Motion, Sequencer or Settings directly --
// requests are validated on the web task and then queued as plain-old-data
// commands that loop() drains. That keeps every mutation of machine state on
// one thread while still letting the HTTP layer reject bad input with a real
// error message instead of an optimistic 200.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "Settings.h"

enum class CmdType : uint8_t {
  NONE,
  JOG,
  MOVE,
  NUDGE,
  STOP_AXIS,
  STOP_ALL,
  HOME_AXIS,
  HOME_ALL,
  ESTOP,
  CLEAR_ESTOP,
  SET_ENABLE,
  ZERO_AXIS,
  SEQ_PLAY,
  SEQ_PAUSE,
  SEQ_STOP,
  SEQ_RESTART,
  SEQ_GOTO,
  SEQ_CAPTURE,
  SEQ_APPLY,
  SEQ_SAVE,
  CURVE_PLAY,
  CURVE_PAUSE,
  CURVE_STOP,
  CURVE_GOTO,
  CURVE_SELECT,
  CURVE_SAVE,
  CURVE_DELETE,
  CURVE_NEW,
  CURVE_APPLY,
  RUN_ACTION,
  APPLY_CONFIG,
  FACTORY_RESET,
  TMC_APPLY,
  REBOOT
};

struct WebCommand {
  CmdType type = CmdType::NONE;
  uint8_t axis = 0;
  int8_t dir = 0;
  float value = 0.0f;
  float value2 = 0.0f;
  bool flag = false;
};

class WebUI {
 public:
  void begin();
  // Drains the command queue and pushes telemetry. Call every loop iteration.
  void update(uint32_t nowMs);

  bool queueCommand(const WebCommand &cmd);
  String networkSummary() const { return networkSummary_; }

 private:
  AsyncWebServer server_{80};
  AsyncWebSocket ws_{"/ws"};
  QueueHandle_t queue_ = nullptr;
  uint32_t lastTelemetryMs_ = 0;
  String networkSummary_ = "no network";

  // Staging for the two commands that carry more than a few floats. Written
  // by the web task after validation, consumed by loop().
  Settings pendingSettings_;
  volatile bool pendingSettingsValid_ = false;
  String pendingSequenceJson_;
  volatile bool pendingSequenceValid_ = false;
  String pendingCurveJson_;
  volatile bool pendingCurveValid_ = false;
  char pendingCurveName_[fw::CURVE_NAME_LEN] = "";

  void startNetwork();
  void registerRoutes();
  void handleCommand(const WebCommand &cmd);
  void buildStatus(JsonObject out);
  void pushTelemetry();
  void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                 AwsEventType type, void *arg, uint8_t *data, size_t len);
  void handleWsMessage(const char *payload, size_t len);
};

extern WebUI webui;
