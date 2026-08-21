#include "WebConfig.h"

#include <ArduinoJson.h>
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "Settings.h"
#include "config.h"

namespace webconfig {
namespace {

WebServer server(webcfg::HTTP_PORT);
bool (*busyCheck)() = nullptr;

bool isBusy() { return busyCheck != nullptr && busyCheck(); }

const char *typeName(settings::Type t) {
  return t == settings::Type::BOOL ? "bool" : "number";
}

// Number of decimals to render per type, so integer settings don't show up
// in the form as "32000.00".
uint8_t decimalsFor(settings::Type t) {
  return t == settings::Type::F32 ? 3 : 0;
}

const char kIndexHtml[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Camera Slider Config</title>
<style>
:root{color-scheme:dark}
*{box-sizing:border-box}
body{margin:0;padding:16px;background:#14161a;color:#e8eaed;
 font:15px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
h1{font-size:19px;margin:0 0 4px}
.sub{color:#9aa0a6;font-size:13px;margin-bottom:18px}
fieldset{border:1px solid #2c3038;border-radius:8px;margin:0 0 14px;padding:12px 14px}
legend{color:#8ab4f8;font-weight:600;padding:0 6px;font-size:14px}
.row{display:flex;align-items:center;gap:10px;padding:5px 0;flex-wrap:wrap}
.row label{flex:1 1 190px;min-width:150px}
.unit{color:#9aa0a6;font-size:12px;min-width:52px}
input[type=number]{width:120px;background:#0d0f12;color:#e8eaed;
 border:1px solid #3c4149;border-radius:5px;padding:6px 8px;font-size:14px}
input[type=number]:focus{outline:2px solid #8ab4f8;outline-offset:-1px}
input[type=checkbox]{width:19px;height:19px;accent-color:#8ab4f8}
.rb{color:#fdd663;font-size:11px;border:1px solid #5c4a12;background:#2a2412;
 border-radius:4px;padding:1px 5px}
.bar{position:sticky;bottom:0;background:#14161a;border-top:1px solid #2c3038;
 padding:12px 0;display:flex;gap:10px;flex-wrap:wrap;align-items:center}
button{background:#8ab4f8;color:#14161a;border:0;border-radius:6px;
 padding:9px 16px;font-size:14px;font-weight:600;cursor:pointer}
button.sec{background:#2c3038;color:#e8eaed}
button.danger{background:#3a1f1f;color:#f28b82}
button:disabled{opacity:.5;cursor:not-allowed}
#msg{font-size:13px}
.ok{color:#81c995}.err{color:#f28b82}.warn{color:#fdd663}
.note{color:#9aa0a6;font-size:12px;border-left:2px solid #3c4149;
 padding-left:10px;margin:14px 0}
</style></head><body>
<h1>Camera Slider</h1>
<div class="sub">Runtime configuration &middot; saved to flash</div>
<div id="form"></div>
<div class="note">Pin assignments, I2C addresses and TMC address straps are
compile-time values &mdash; they describe how the board is physically wired,
so changing them here could only make firmware disagree with reality.
Edit <code>config.h</code> and reflash to change wiring.</div>
<div class="bar">
 <button id="save">Save</button>
 <button id="reload" class="sec">Reload</button>
 <button id="reboot" class="sec">Reboot</button>
 <button id="reset" class="danger">Reset defaults</button>
 <span id="msg"></span>
</div>
<script>
let schema=[];
const el=id=>document.getElementById(id);
const msg=(t,c)=>{el('msg').textContent=t;el('msg').className=c||''};

function render(items){
 schema=items;
 const groups={};
 items.forEach(s=>{(groups[s.group]=groups[s.group]||[]).push(s)});
 let h='';
 for(const g in groups){
  h+='<fieldset><legend>'+g+'</legend>';
  groups[g].forEach(s=>{
   h+='<div class="row"><label for="'+s.key+'">'+s.label+'</label>';
   if(s.type==='bool'){
    h+='<input type="checkbox" id="'+s.key+'"'+(s.value?' checked':'')+'>';
    h+='<span class="unit"></span>';
   }else{
    h+='<input type="number" id="'+s.key+'" value="'+s.value+
       '" min="'+s.min+'" max="'+s.max+'" step="any">';
    h+='<span class="unit">'+(s.unit||'')+'</span>';
   }
   if(s.reboot)h+='<span class="rb">reboot</span>';
   h+='</div>';
  });
  h+='</fieldset>';
 }
 el('form').innerHTML=h;
}

async function load(){
 try{
  const r=await fetch('/api/config');
  const d=await r.json();
  render(d.settings);
  msg('Loaded '+d.settings.length+' settings','ok');
 }catch(e){msg('Load failed: '+e,'err')}
}

async function save(){
 const body={};
 let needsReboot=false;
 schema.forEach(s=>{
  const i=el(s.key);
  const v=s.type==='bool'?(i.checked?1:0):parseFloat(i.value);
  if(!isNaN(v)){body[s.key]=v;if(s.reboot&&v!==s.value)needsReboot=true}
 });
 el('save').disabled=true;
 try{
  const r=await fetch('/api/config',{method:'POST',
   headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  if(r.status===409){msg('Rejected: a shot is running','warn')}
  else if(!r.ok){msg('Save failed: HTTP '+r.status,'err')}
  else{
   const d=await r.json();
   msg('Saved '+d.applied+' settings'+(needsReboot?' — reboot to apply some':''),
       needsReboot?'warn':'ok');
   await load();
  }
 }catch(e){msg('Save failed: '+e,'err')}
 el('save').disabled=false;
}

el('save').onclick=save;
el('reload').onclick=load;
el('reboot').onclick=async()=>{
 if(!confirm('Reboot the controller? All axes will re-home.'))return;
 msg('Rebooting…','warn');
 try{await fetch('/api/reboot',{method:'POST'})}catch(e){}
};
el('reset').onclick=async()=>{
 if(!confirm('Erase all saved settings and restore compiled defaults?'))return;
 try{
  const r=await fetch('/api/reset',{method:'POST'});
  msg(r.ok?'Defaults restored — reboot to apply':'Reset failed',r.ok?'warn':'err');
 }catch(e){msg('Reset failed: '+e,'err')}
};
load();
</script></body></html>)HTML";

void handleRoot() { server.send_P(200, "text/html", kIndexHtml); }

void handleGetConfig() {
  JsonDocument doc;
  doc["shotActive"] = isBusy();
  JsonArray arr = doc["settings"].to<JsonArray>();

  for (uint16_t i = 0; i < settings::kSettingsCount; i++) {
    const settings::Desc &d = settings::kSettings[i];
    JsonObject o = arr.add<JsonObject>();
    o["key"] = d.key;
    o["group"] = d.group;
    o["label"] = d.label;
    o["unit"] = d.unit;
    o["type"] = typeName(d.type);
    o["min"] = d.min;
    o["max"] = d.max;
    o["reboot"] = d.needsReboot;

    const float v = settings::getValue(d);
    if (decimalsFor(d.type) == 0) {
      o["value"] = static_cast<long>(lroundf(v));
    } else {
      o["value"] = v;
    }
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handlePostConfig() {
  // Changing speeds, limits or step ratios underneath a running shot would
  // corrupt the move already in flight — its waypoints were computed against
  // the old values.
  if (isBusy()) {
    server.send(409, "application/json",
                "{\"error\":\"a shot is running; stop it before changing settings\"}");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"malformed JSON body\"}");
    return;
  }

  uint16_t applied = 0;
  uint16_t rejected = 0;
  for (JsonPair kv : doc.as<JsonObject>()) {
    const settings::Desc *d = settings::find(kv.key().c_str());
    if (d == nullptr || !kv.value().is<float>()) {
      rejected++;
      continue;
    }
    if (settings::setValue(*d, kv.value().as<float>())) {
      applied++;
    } else {
      rejected++;
    }
  }

  settings::recomputeDerived();
  settings::saveAll();

  JsonDocument resp;
  resp["applied"] = applied;
  resp["rejected"] = rejected;
  String out;
  serializeJson(resp, out);
  server.send(200, "application/json", out);
}

void handleReset() {
  if (isBusy()) {
    server.send(409, "application/json", "{\"error\":\"a shot is running\"}");
    return;
  }
  settings::resetToDefaults();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleReboot() {
  server.send(200, "application/json", "{\"ok\":true}");
  server.client().flush();
  delay(150);  // let the response actually leave before the reset
  ESP.restart();
}

}  // namespace

void begin(bool (*isBusyFn)()) {
  busyCheck = isBusyFn;

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(webcfg::AP_SSID, webcfg::AP_PASSWORD)) {
    Serial.println("ERROR: WiFi AP failed to start — web config unavailable");
    return;
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.begin();

  Serial.printf("Web config: http://%s  (SSID '%s')\n",
                WiFi.softAPIP().toString().c_str(), webcfg::AP_SSID);
}

void update() { server.handleClient(); }

}  // namespace webconfig
