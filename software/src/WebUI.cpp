#include "WebUI.h"

#include <AsyncJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "BleRecorder.h"
#include "CurveSequence.h"
#include "Inputs.h"
#include "Motion.h"
#include "Mux.h"
#include "Sequencer.h"
#include "web_index.h"

WebUI webui;

namespace {
constexpr uint32_t STA_CONNECT_TIMEOUT_MS = 15000;
constexpr UBaseType_t COMMAND_QUEUE_DEPTH = 24;

void sendJson(AsyncWebServerRequest *request, int code, JsonDocument &doc) {
  AsyncResponseStream *response =
      request->beginResponseStream("application/json");
  response->setCode(code);
  serializeJson(doc, *response);
  request->send(response);
}

void sendOk(AsyncWebServerRequest *request) {
  request->send(200, "application/json", "{\"ok\":true}");
}

void sendError(AsyncWebServerRequest *request, int code, const String &message) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = message;
  sendJson(request, code, doc);
}

ButtonAction parseAction(const char *name) {
  if (name == nullptr) return ButtonAction::NONE;
  for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonAction::ACTION_COUNT); ++i) {
    if (strcmp(name, buttonActionName(static_cast<ButtonAction>(i))) == 0) {
      return static_cast<ButtonAction>(i);
    }
  }
  return ButtonAction::NONE;
}
}  // namespace

void WebUI::begin() {
  queue_ = xQueueCreate(COMMAND_QUEUE_DEPTH, sizeof(WebCommand));
  startNetwork();
  registerRoutes();

  ws_.onEvent([this](AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType t,
                     void *arg, uint8_t *data, size_t len) {
    onWsEvent(s, c, t, arg, data, len);
  });
  server_.addHandler(&ws_);
  server_.begin();
  Serial.printf("[web] listening on %s\n", networkSummary_.c_str());
}

void WebUI::startNetwork() {
  WiFi.persistent(false);
  WiFi.setHostname(settings.wifi.hostname);

  bool staConnected = false;
  if (settings.wifi.staEnabled && settings.wifi.ssid[0] != '\0') {
    WiFi.mode(WIFI_STA);
    WiFi.begin(settings.wifi.ssid, settings.wifi.pass);
    Serial.printf("[net] joining \"%s\"", settings.wifi.ssid);
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < STA_CONNECT_TIMEOUT_MS) {
      delay(250);
      Serial.print('.');
    }
    Serial.println();
    staConnected = WiFi.status() == WL_CONNECTED;
    if (staConnected) {
      networkSummary_ = WiFi.localIP().toString();
      Serial.printf("[net] joined, IP %s\n", networkSummary_.c_str());
    } else {
      Serial.println("[net] join failed");
    }
  }

  // The AP is the safety net: if the configured network is gone (or was
  // mistyped in the web UI), the machine still comes up reachable instead of
  // needing a serial cable to recover.
  if (!staConnected && (settings.wifi.apFallback || !settings.wifi.staEnabled)) {
    WiFi.mode(staConnected ? WIFI_AP_STA : WIFI_AP);
    const char *pass = strlen(settings.wifi.apPass) >= 8 ? settings.wifi.apPass
                                                         : nullptr;
    if (WiFi.softAP(settings.wifi.apSsid, pass)) {
      networkSummary_ = WiFi.softAPIP().toString();
      Serial.printf("[net] AP \"%s\" up at %s\n", settings.wifi.apSsid,
                    networkSummary_.c_str());
    } else {
      Serial.println("[net] ERROR: softAP failed");
    }
  }

  if (MDNS.begin(settings.wifi.hostname)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[net] mDNS: http://%s.local\n", settings.wifi.hostname);
  }
}

bool WebUI::queueCommand(const WebCommand &cmd) {
  if (queue_ == nullptr) return false;
  return xQueueSend(queue_, &cmd, 0) == pdTRUE;
}

void WebUI::registerRoutes() {
  // Every route below is registered with an *exact* URI matcher. The library's
  // default matcher is "backward compatible", which also matches anything
  // under the path as if it were a folder -- so "/api/estop" would swallow
  // "/api/estop/clear" (and "/api/home" would swallow "/api/home/axis"),
  // whichever was registered first winning. The result was a Clear e-stop
  // button that cheerfully returned 200 while re-latching the e-stop.

  // ---- static page ----
  server_.on(AsyncURIMatcher::exact("/"), HTTP_GET, [](AsyncWebServerRequest *request) {
    // Served straight out of flash: a String copy of the page would be a
    // 30 kB heap allocation on a board that also runs WiFi and BLE.
    AsyncWebServerResponse *response = request->beginResponse(
        200, "text/html; charset=utf-8",
        reinterpret_cast<const uint8_t *>(INDEX_HTML), strlen(INDEX_HTML));
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // ---- read-only ----
  server_.on(AsyncURIMatcher::exact("/api/status"), HTTP_GET, [this](AsyncWebServerRequest *request) {
    JsonDocument doc;
    buildStatus(doc.to<JsonObject>());
    sendJson(request, 200, doc);
  });

  server_.on(AsyncURIMatcher::exact("/api/config"), HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    settings.toJson(doc.to<JsonObject>());
    sendJson(request, 200, doc);
  });

  server_.on(AsyncURIMatcher::exact("/api/actions"), HTTP_GET, [](AsyncWebServerRequest *request) {
    // Lets the UI build its dropdowns from the firmware's own action list, so
    // the two can never drift apart.
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonAction::ACTION_COUNT); ++i) {
      arr.add(buttonActionName(static_cast<ButtonAction>(i)));
    }
    sendJson(request, 200, doc);
  });

  server_.on(AsyncURIMatcher::exact("/api/sequence"), HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    sequencer.toJson(doc.to<JsonObject>());
    sendJson(request, 200, doc);
  });

  // Curve sequences: the saved-slot list, and the one currently loaded.
  server_.on(AsyncURIMatcher::exact("/api/curves"), HTTP_GET,
             [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    curves.slotsJson(doc.to<JsonObject>());
    sendJson(request, 200, doc);
  });

  server_.on(AsyncURIMatcher::exact("/api/curve"), HTTP_GET,
             [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    curves.toJson(root);
    // Surfacing the reason a sequence will not play here means the UI can warn
    // while it is being drawn, rather than only when Play is pressed.
    String why;
    root["playable"] = curves.checkFeasible(why);
    root["why"] = why;
    sendJson(request, 200, doc);
  });

  server_.on(AsyncURIMatcher::exact("/api/tmc"), HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    JsonArray arr = doc["drivers"].to<JsonArray>();
    for (uint8_t i = 0; i < fw::AXIS_COUNT; ++i) {
      motion_ctl.axis(i).driverStatusJson(arr.add<JsonObject>());
    }
    doc["bus"] = motion_ctl.tmcBusStarted() ? "up" : "not started";
    sendJson(request, 200, doc);
  });

  // Magnet check for the AS5600s. Doing this from the UI beats guessing why
  // an axis reads a constant angle.
  server_.on(AsyncURIMatcher::exact("/api/sensors"), HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    JsonArray arr = doc["sensors"].to<JsonArray>();
    for (uint8_t i = 0; i < fw::AXIS_COUNT; ++i) {
      const AxisConfig &c = settings.axes[i];
      if (c.feedback != FeedbackType::AS5600) continue;
      motion_ctl.lockBus();
      const AS5600Status s = readAS5600Status(motion_ctl.muxBus(),
                                              settings.muxAddress, c.muxChannel);
      motion_ctl.unlockBus();
      JsonObject o = arr.add<JsonObject>();
      o["axis"] = i;
      o["name"] = c.name;
      o["channel"] = c.muxChannel;
      o["responded"] = s.responded;
      o["magnet_detected"] = s.magnetDetected;
      o["magnet_too_weak"] = s.magnetTooWeak;
      o["magnet_too_strong"] = s.magnetTooStrong;
      o["raw"] = s.rawAngle;
      o["raw_deg"] = s.rawAngle * (360.0f / 4096.0f);
    }
    sendJson(request, 200, doc);
  });

  server_.on(AsyncURIMatcher::exact("/api/wifi/scan"), HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    JsonArray arr = doc["networks"].to<JsonArray>();
    const int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) {
      WiFi.scanNetworks(true);
      doc["scanning"] = true;
    } else if (n == WIFI_SCAN_RUNNING) {
      doc["scanning"] = true;
    } else {
      doc["scanning"] = false;
      for (int16_t i = 0; i < n; ++i) {
        JsonObject o = arr.add<JsonObject>();
        o["ssid"] = WiFi.SSID(i);
        o["rssi"] = WiFi.RSSI(i);
        o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      }
      WiFi.scanDelete();
    }
    sendJson(request, 200, doc);
  });

  // ---- simple POSTs (no body) ----
  struct SimpleRoute {
    const char *uri;
    CmdType type;
  };
  static const SimpleRoute simpleRoutes[] = {
      {"/api/stop", CmdType::STOP_ALL},
      {"/api/home", CmdType::HOME_ALL},
      {"/api/estop", CmdType::ESTOP},
      {"/api/estop/clear", CmdType::CLEAR_ESTOP},
      {"/api/sequence/play", CmdType::SEQ_PLAY},
      {"/api/sequence/pause", CmdType::SEQ_PAUSE},
      {"/api/sequence/stop", CmdType::SEQ_STOP},
      {"/api/sequence/restart", CmdType::SEQ_RESTART},
      {"/api/sequence/capture", CmdType::SEQ_CAPTURE},
      {"/api/sequence/save", CmdType::SEQ_SAVE},
      {"/api/curve/play", CmdType::CURVE_PLAY},
      {"/api/curve/pause", CmdType::CURVE_PAUSE},
      {"/api/curve/stop", CmdType::CURVE_STOP},
      {"/api/curve/new", CmdType::CURVE_NEW},
      {"/api/tmc/apply", CmdType::TMC_APPLY},
      {"/api/config/reset", CmdType::FACTORY_RESET},
      {"/api/reboot", CmdType::REBOOT},
  };
  for (const auto &route : simpleRoutes) {
    const CmdType type = route.type;
    server_.on(AsyncURIMatcher::exact(route.uri), HTTP_POST,
               [this, type](AsyncWebServerRequest *request) {
      WebCommand cmd;
      cmd.type = type;
      if (queueCommand(cmd)) {
        sendOk(request);
      } else {
        sendError(request, 503, "command queue full");
      }
    });
  }

  // ---- JSON body endpoints ----
  auto *moveHandler = new AsyncCallbackJsonWebHandler(
      AsyncURIMatcher::exact("/api/move"), [this](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObjectConst body = json.as<JsonObjectConst>();
        WebCommand cmd;
        cmd.axis = body["axis"] | 0;
        if (cmd.axis >= fw::AXIS_COUNT) {
          sendError(request, 400, "axis out of range");
          return;
        }
        if (body["delta"].is<float>()) {
          cmd.type = CmdType::NUDGE;
          cmd.value = body["delta"].as<float>();
        } else if (body["position"].is<float>()) {
          cmd.type = CmdType::MOVE;
          cmd.value = body["position"].as<float>();
        } else {
          sendError(request, 400, "expected \"position\" or \"delta\"");
          return;
        }
        // Optional speed, as a percentage of the axis maximum.
        cmd.value2 = body["speed_pct"] | 100.0f;
        queueCommand(cmd) ? sendOk(request)
                          : sendError(request, 503, "command queue full");
      });
  moveHandler->setMethod(HTTP_POST);
  server_.addHandler(moveHandler);

  auto *jogHandler = new AsyncCallbackJsonWebHandler(
      AsyncURIMatcher::exact("/api/jog"), [this](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObjectConst body = json.as<JsonObjectConst>();
        WebCommand cmd;
        cmd.type = CmdType::JOG;
        cmd.axis = body["axis"] | 0;
        if (cmd.axis >= fw::AXIS_COUNT) {
          sendError(request, 400, "axis out of range");
          return;
        }
        const int dir = body["dir"] | 0;
        cmd.dir = static_cast<int8_t>(dir > 0 ? 1 : (dir < 0 ? -1 : 0));
        cmd.value = body["speed_pct"] | 50.0f;
        queueCommand(cmd) ? sendOk(request)
                          : sendError(request, 503, "command queue full");
      });
  jogHandler->setMethod(HTTP_POST);
  server_.addHandler(jogHandler);

  auto *homeHandler = new AsyncCallbackJsonWebHandler(
      AsyncURIMatcher::exact("/api/home/axis"), [this](AsyncWebServerRequest *request, JsonVariant &json) {
        WebCommand cmd;
        cmd.type = CmdType::HOME_AXIS;
        cmd.axis = json["axis"] | 0;
        if (cmd.axis >= fw::AXIS_COUNT) {
          sendError(request, 400, "axis out of range");
          return;
        }
        queueCommand(cmd) ? sendOk(request)
                          : sendError(request, 503, "command queue full");
      });
  homeHandler->setMethod(HTTP_POST);
  server_.addHandler(homeHandler);

  auto *zeroHandler = new AsyncCallbackJsonWebHandler(
      AsyncURIMatcher::exact("/api/zero"), [this](AsyncWebServerRequest *request, JsonVariant &json) {
        WebCommand cmd;
        cmd.type = CmdType::ZERO_AXIS;
        cmd.axis = json["axis"] | 0;
        cmd.value = json["position"] | 0.0f;
        if (cmd.axis >= fw::AXIS_COUNT) {
          sendError(request, 400, "axis out of range");
          return;
        }
        queueCommand(cmd) ? sendOk(request)
                          : sendError(request, 503, "command queue full");
      });
  zeroHandler->setMethod(HTTP_POST);
  server_.addHandler(zeroHandler);

  auto *enableHandler = new AsyncCallbackJsonWebHandler(
      AsyncURIMatcher::exact("/api/enable"), [this](AsyncWebServerRequest *request, JsonVariant &json) {
        WebCommand cmd;
        cmd.type = CmdType::SET_ENABLE;
        cmd.flag = json["on"] | true;
        queueCommand(cmd) ? sendOk(request)
                          : sendError(request, 503, "command queue full");
      });
  enableHandler->setMethod(HTTP_POST);
  server_.addHandler(enableHandler);

  auto *gotoHandler = new AsyncCallbackJsonWebHandler(
      AsyncURIMatcher::exact("/api/sequence/goto"),
      [this](AsyncWebServerRequest *request, JsonVariant &json) {
        WebCommand cmd;
        cmd.type = CmdType::SEQ_GOTO;
        cmd.axis = json["index"] | 0;
        queueCommand(cmd) ? sendOk(request)
                          : sendError(request, 503, "command queue full");
      });
  gotoHandler->setMethod(HTTP_POST);
  server_.addHandler(gotoHandler);

  // Fires any of the bindable actions by name -- the web UI's buttons and the
  // physical buttons therefore go through exactly the same code path.
  auto *actionHandler = new AsyncCallbackJsonWebHandler(
      AsyncURIMatcher::exact("/api/action"), [this](AsyncWebServerRequest *request, JsonVariant &json) {
        const ButtonAction action = parseAction(json["action"]);
        if (action == ButtonAction::NONE) {
          sendError(request, 400, "unknown action");
          return;
        }
        WebCommand cmd;
        cmd.type = CmdType::RUN_ACTION;
        cmd.value = static_cast<float>(static_cast<uint8_t>(action));
        cmd.axis = json["axis"] | 0;
        queueCommand(cmd) ? sendOk(request)
                          : sendError(request, 503, "command queue full");
      });
  actionHandler->setMethod(HTTP_POST);
  server_.addHandler(actionHandler);

  auto *configHandler = new AsyncCallbackJsonWebHandler(
      AsyncURIMatcher::exact("/api/config"), [this](AsyncWebServerRequest *request, JsonVariant &json) {
        if (pendingSettingsValid_) {
          sendError(request, 409, "a config change is still being applied");
          return;
        }
        // Validate against a copy so a rejected field never lands on the live
        // settings, and so the error can name the offending value.
        Settings draft = settings;
        String error;
        if (!draft.applyJson(json.as<JsonObjectConst>(), error)) {
          sendError(request, 400, error);
          return;
        }
        pendingSettings_ = draft;
        pendingSettingsValid_ = true;
        WebCommand cmd;
        cmd.type = CmdType::APPLY_CONFIG;
        cmd.flag = true;  // persist
        if (!queueCommand(cmd)) {
          pendingSettingsValid_ = false;
          sendError(request, 503, "command queue full");
          return;
        }
        JsonDocument doc;
        doc["ok"] = true;
        // Pin and encoder changes only take effect through begin(), so tell
        // the UI to offer a reboot rather than letting the user think a dead
        // knob means they got the wiring wrong.
        doc["reboot_recommended"] = true;
        sendJson(request, 200, doc);
      });
  configHandler->setMethod(HTTP_POST);
  server_.addHandler(configHandler);

  auto *sequenceHandler = new AsyncCallbackJsonWebHandler(
      AsyncURIMatcher::exact("/api/sequence"), [this](AsyncWebServerRequest *request, JsonVariant &json) {
        if (pendingSequenceValid_) {
          sendError(request, 409, "a sequence change is still being applied");
          return;
        }
        if (!json["keyframes"].is<JsonArrayConst>()) {
          sendError(request, 400, "expected a keyframes array");
          return;
        }
        if (json["keyframes"].as<JsonArrayConst>().size() > fw::MAX_KEYFRAMES) {
          sendError(request, 400,
                    String("too many keyframes (max ") + fw::MAX_KEYFRAMES + ")");
          return;
        }
        pendingSequenceJson_.clear();
        serializeJson(json, pendingSequenceJson_);
        pendingSequenceValid_ = true;
        WebCommand cmd;
        cmd.type = CmdType::SEQ_APPLY;
        if (!queueCommand(cmd)) {
          pendingSequenceValid_ = false;
          sendError(request, 503, "command queue full");
          return;
        }
        sendOk(request);
      });
  sequenceHandler->setMethod(HTTP_POST);
  server_.addHandler(sequenceHandler);

  // ---- curve sequence slots ----
  auto *curveSelect = new AsyncCallbackJsonWebHandler(
      AsyncURIMatcher::exact("/api/curve/select"),
      [this](AsyncWebServerRequest *request, JsonVariant &json) {
        WebCommand cmd;
        cmd.type = CmdType::CURVE_SELECT;
        cmd.axis = json["slot"] | 0;
        if (cmd.axis >= fw::MAX_CURVE_SEQUENCES) {
          sendError(request, 400, "slot out of range");
          return;
        }
        queueCommand(cmd) ? sendOk(request)
                          : sendError(request, 503, "command queue full");
      });
  curveSelect->setMethod(HTTP_POST);
  server_.addHandler(curveSelect);

  auto *curveSave = new AsyncCallbackJsonWebHandler(
      AsyncURIMatcher::exact("/api/curve/save"),
      [this](AsyncWebServerRequest *request, JsonVariant &json) {
        WebCommand cmd;
        cmd.type = CmdType::CURVE_SAVE;
        cmd.axis = json["slot"] | 0;
        if (cmd.axis >= fw::MAX_CURVE_SEQUENCES) {
          sendError(request, 400, "slot out of range");
          return;
        }
        strlcpy(pendingCurveName_, json["name"] | "", sizeof(pendingCurveName_));
        queueCommand(cmd) ? sendOk(request)
                          : sendError(request, 503, "command queue full");
      });
  curveSave->setMethod(HTTP_POST);
  server_.addHandler(curveSave);

  auto *curveDelete = new AsyncCallbackJsonWebHandler(
      AsyncURIMatcher::exact("/api/curve/delete"),
      [this](AsyncWebServerRequest *request, JsonVariant &json) {
        WebCommand cmd;
        cmd.type = CmdType::CURVE_DELETE;
        cmd.axis = json["slot"] | 0;
        if (cmd.axis >= fw::MAX_CURVE_SEQUENCES) {
          sendError(request, 400, "slot out of range");
          return;
        }
        queueCommand(cmd) ? sendOk(request)
                          : sendError(request, 503, "command queue full");
      });
  curveDelete->setMethod(HTTP_POST);
  server_.addHandler(curveDelete);

  auto *curveGoto = new AsyncCallbackJsonWebHandler(
      AsyncURIMatcher::exact("/api/curve/goto"),
      [this](AsyncWebServerRequest *request, JsonVariant &json) {
        WebCommand cmd;
        cmd.type = CmdType::CURVE_GOTO;
        cmd.value = json["t"] | 0.0f;
        queueCommand(cmd) ? sendOk(request)
                          : sendError(request, 503, "command queue full");
      });
  curveGoto->setMethod(HTTP_POST);
  server_.addHandler(curveGoto);

  // The edited sequence itself. Validated here on the web task so a bad curve
  // is rejected with a reason, then staged for loop() to adopt.
  auto *curveHandler = new AsyncCallbackJsonWebHandler(
      AsyncURIMatcher::exact("/api/curve"),
      [this](AsyncWebServerRequest *request, JsonVariant &json) {
        if (pendingCurveValid_) {
          sendError(request, 409, "a curve change is still being applied");
          return;
        }
        String error;
        CurveSequence draft;
        if (!draft.fromJson(json.as<JsonObjectConst>(), error)) {
          sendError(request, 400, error);
          return;
        }
        pendingCurveJson_.clear();
        serializeJson(json, pendingCurveJson_);
        pendingCurveValid_ = true;
        WebCommand cmd;
        cmd.type = CmdType::CURVE_APPLY;
        cmd.flag = json["save"] | false;
        if (!queueCommand(cmd)) {
          pendingCurveValid_ = false;
          sendError(request, 503, "command queue full");
          return;
        }
        sendOk(request);
      });
  curveHandler->setMethod(HTTP_POST);
  server_.addHandler(curveHandler);

  server_.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
  });
}

void WebUI::handleCommand(const WebCommand &cmd) {
  String error;
  switch (cmd.type) {
    case CmdType::JOG: {
      Axis &ax = motion_ctl.axis(cmd.axis);
      if (motion_ctl.estopped()) break;
      if (cmd.dir == 0) {
        ax.stop();
      } else {
        ax.jog(cmd.dir, ax.cfg().maxSpeed * constrain(cmd.value, 1.0f, 100.0f) / 100.0f);
      }
      break;
    }
    case CmdType::MOVE: {
      Axis &ax = motion_ctl.axis(cmd.axis);
      if (motion_ctl.estopped()) break;
      const float speed =
          ax.cfg().maxSpeed * constrain(cmd.value2, 1.0f, 100.0f) / 100.0f;
      if (!ax.moveTo(cmd.value, speed, error)) Serial.printf("[web] %s\n", error.c_str());
      break;
    }
    case CmdType::NUDGE: {
      Axis &ax = motion_ctl.axis(cmd.axis);
      if (motion_ctl.estopped()) break;
      if (!ax.nudge(cmd.value, error)) Serial.printf("[web] %s\n", error.c_str());
      break;
    }
    case CmdType::STOP_AXIS:
      motion_ctl.axis(cmd.axis).stop();
      break;
    case CmdType::STOP_ALL:
      sequencer.pause();
      curves.pause();
      motion_ctl.stopAll();
      break;
    case CmdType::HOME_AXIS:
      sequencer.stop();
      curves.stop();
      motion_ctl.axis(cmd.axis).startHoming();
      break;
    case CmdType::HOME_ALL:
      sequencer.stop();
      curves.stop();
      motion_ctl.homeAll();
      break;
    case CmdType::ESTOP:
      sequencer.stop();
      curves.stop();
      motion_ctl.estop();
      break;
    case CmdType::CLEAR_ESTOP:
      motion_ctl.clearEstop();
      break;
    case CmdType::SET_ENABLE:
      motion_ctl.setDriversEnabled(cmd.flag);
      break;
    case CmdType::ZERO_AXIS:
      motion_ctl.axis(cmd.axis).setPositionUnits(cmd.value);
      break;
    case CmdType::SEQ_PLAY:
      curves.stop();
      if (!sequencer.play(error)) Serial.printf("[web] %s\n", error.c_str());
      break;
    case CmdType::SEQ_PAUSE:
      sequencer.pause();
      break;
    case CmdType::SEQ_STOP:
      sequencer.stop();
      break;
    case CmdType::SEQ_RESTART:
      sequencer.restart();
      break;
    case CmdType::SEQ_GOTO:
      if (!sequencer.gotoKeyframe(cmd.axis, error)) {
        Serial.printf("[web] %s\n", error.c_str());
      }
      break;
    case CmdType::SEQ_CAPTURE:
      if (sequencer.addFromCurrent(error)) {
        sequencer.save();
      } else {
        Serial.printf("[web] %s\n", error.c_str());
      }
      break;
    case CmdType::SEQ_SAVE:
      sequencer.save();
      break;
    case CmdType::SEQ_APPLY: {
      if (!pendingSequenceValid_) break;
      JsonDocument doc;
      if (deserializeJson(doc, pendingSequenceJson_) == DeserializationError::Ok) {
        if (!sequencer.fromJson(doc.as<JsonObjectConst>(), error)) {
          Serial.printf("[web] %s\n", error.c_str());
        } else {
          sequencer.save();
        }
      }
      pendingSequenceJson_.clear();
      pendingSequenceValid_ = false;
      break;
    }
    // The two move types are mutually exclusive by construction: both drive
    // the same steppers, so starting one always stops the other rather than
    // letting them fight over the axes mid-take.
    case CmdType::CURVE_PLAY:
      sequencer.stop();
      if (!curves.play(error)) Serial.printf("[web] %s\n", error.c_str());
      break;
    case CmdType::CURVE_PAUSE:
      curves.pause();
      break;
    case CmdType::CURVE_STOP:
      curves.stop();
      break;
    case CmdType::CURVE_GOTO:
      sequencer.stop();
      if (!curves.gotoTime(cmd.value, error)) Serial.printf("[web] %s\n", error.c_str());
      break;
    case CmdType::CURVE_SELECT:
      if (!curves.loadSlot(cmd.axis, error)) Serial.printf("[web] %s\n", error.c_str());
      break;
    case CmdType::CURVE_SAVE:
      if (!curves.saveSlot(cmd.axis, pendingCurveName_, error)) {
        Serial.printf("[web] %s\n", error.c_str());
      }
      pendingCurveName_[0] = '\0';
      break;
    case CmdType::CURVE_DELETE:
      if (!curves.deleteSlot(cmd.axis, error)) Serial.printf("[web] %s\n", error.c_str());
      break;
    case CmdType::CURVE_NEW:
      curves.stop();
      curves.active().clear();
      break;
    case CmdType::CURVE_APPLY: {
      if (!pendingCurveValid_) break;
      JsonDocument doc;
      if (deserializeJson(doc, pendingCurveJson_) == DeserializationError::Ok) {
        curves.stop();
        if (!curves.active().fromJson(doc.as<JsonObjectConst>(), error)) {
          Serial.printf("[web] %s\n", error.c_str());
        } else if (cmd.flag && curves.activeSlot() >= 0) {
          curves.saveSlot(curves.activeSlot(), nullptr, error);
        }
      }
      pendingCurveJson_.clear();
      pendingCurveValid_ = false;
      break;
    }
    case CmdType::RUN_ACTION:
      inputs.runAction(static_cast<ButtonAction>(static_cast<uint8_t>(cmd.value)),
                       cmd.axis);
      break;
    case CmdType::APPLY_CONFIG: {
      if (!pendingSettingsValid_) break;
      settings = pendingSettings_;
      pendingSettingsValid_ = false;
      if (cmd.flag) settings.save();
      // Live-apply what can be applied without a reboot: driver currents,
      // microstepping and the ramp parameters derived from them.
      for (uint8_t i = 0; i < fw::AXIS_COUNT; ++i) {
        motion_ctl.axis(i).applyDriverConfig();
      }
      Serial.println("[web] config applied");
      break;
    }
    case CmdType::TMC_APPLY:
      for (uint8_t i = 0; i < fw::AXIS_COUNT; ++i) {
        motion_ctl.axis(i).applyDriverConfig();
      }
      break;
    case CmdType::FACTORY_RESET:
      motion_ctl.estop();
      LittleFS.remove(Settings::path());
      LittleFS.remove(Sequencer::path());
      Serial.println("[web] factory reset -- rebooting");
      delay(200);
      ESP.restart();
      break;
    case CmdType::REBOOT:
      motion_ctl.estop();
      Serial.println("[web] reboot requested");
      delay(200);
      ESP.restart();
      break;
    default:
      break;
  }
}

void WebUI::buildStatus(JsonObject out) {
  out["fw"] = fw::VERSION;
  out["uptime_s"] = millis() / 1000;
  out["free_heap"] = ESP.getFreeHeap();
  out["free_psram"] = ESP.getFreePsram();
  out["network"] = networkSummary_;
  out["wifi_mode"] = WiFi.getMode() == WIFI_AP ? "ap"
                     : WiFi.getMode() == WIFI_STA ? "sta"
                                                  : "ap+sta";
  out["rssi"] = WiFi.RSSI();
  motion_ctl.telemetryJson(out);
  sequencer.telemetryJson(out["sequencer"].to<JsonObject>());
  curves.telemetryJson(out["curve"].to<JsonObject>());
  inputs.telemetryJson(out["inputs"].to<JsonObject>());
  JsonObject b = out["ble"].to<JsonObject>();
  b["enabled"] = bleRecorder.isEnabled();
  b["connected"] = bleRecorder.isConnected();
  b["recording"] = bleRecorder.isRecording();
}

void WebUI::pushTelemetry() {
  if (ws_.count() == 0) return;
  JsonDocument doc;
  buildStatus(doc.to<JsonObject>());
  String payload;
  serializeJson(doc, payload);
  ws_.textAll(payload);
}

void WebUI::handleWsMessage(const char *payload, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, len) != DeserializationError::Ok) return;
  const char *cmdName = doc["cmd"];
  if (cmdName == nullptr) return;

  WebCommand cmd;
  cmd.axis = doc["axis"] | 0;
  if (cmd.axis >= fw::AXIS_COUNT) cmd.axis = 0;

  // The socket carries only the latency-sensitive commands -- press-and-hold
  // jogging, and the stops that end it. Everything else goes over REST.
  if (strcmp(cmdName, "jog") == 0) {
    cmd.type = CmdType::JOG;
    const int dir = doc["dir"] | 0;
    cmd.dir = static_cast<int8_t>(dir > 0 ? 1 : (dir < 0 ? -1 : 0));
    cmd.value = doc["speed_pct"] | 50.0f;
  } else if (strcmp(cmdName, "stop") == 0) {
    cmd.type = CmdType::STOP_AXIS;
  } else if (strcmp(cmdName, "stop_all") == 0) {
    cmd.type = CmdType::STOP_ALL;
  } else if (strcmp(cmdName, "estop") == 0) {
    cmd.type = CmdType::ESTOP;
  } else {
    return;
  }
  queueCommand(cmd);
}

void WebUI::onWsEvent(AsyncWebSocket *, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("[web] ws client %u connected\n", client->id());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("[web] ws client %u disconnected\n", client->id());
      break;
    case WS_EVT_DATA: {
      AwsFrameInfo *info = static_cast<AwsFrameInfo *>(arg);
      if (info->final && info->index == 0 && info->len == len &&
          info->opcode == WS_TEXT) {
        handleWsMessage(reinterpret_cast<const char *>(data), len);
      }
      break;
    }
    default:
      break;
  }
}

void WebUI::update(uint32_t nowMs) {
  WebCommand cmd;
  while (queue_ != nullptr && xQueueReceive(queue_, &cmd, 0) == pdTRUE) {
    handleCommand(cmd);
  }

  ws_.cleanupClients();
  if (nowMs - lastTelemetryMs_ >= ui::TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs_ = nowMs;
    pushTelemetry();
  }
}
