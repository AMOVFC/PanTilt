#pragma once
// The entire web UI, embedded in flash as one self-contained page: no CDN, no
// filesystem upload step, nothing to get out of sync with the firmware. It is
// served straight from this array (see WebUI.cpp) rather than copied into a
// String, so it costs flash but no heap.
//
// The forms are generated from field tables that mirror Settings.h, and the
// button-action dropdown is populated from /api/actions, so the UI cannot
// drift away from what the firmware actually accepts.

static const char INDEX_HTML[] = R"HTMLDOC(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Camera Slider</title>
<style>
:root{--bg:#0e1116;--panel:#161b22;--panel2:#1c2430;--line:#2a3441;--fg:#e6edf3;--dim:#8b98a5;
--accent:#4c9aff;--ok:#3fb950;--warn:#d29922;--bad:#f85149;--mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{position:sticky;top:0;z-index:10;background:var(--panel);border-bottom:1px solid var(--line);
padding:8px 12px;display:flex;gap:8px;align-items:center;flex-wrap:wrap}
header h1{font-size:15px;margin:0 8px 0 0;font-weight:600;letter-spacing:.2px}
#conn{font-size:11px;color:var(--dim);font-family:var(--mono)}
#estop{margin-left:auto;background:var(--bad);border-color:var(--bad);color:#fff;font-weight:700}
nav{display:flex;gap:2px;background:var(--panel);border-bottom:1px solid var(--line);
padding:0 8px;overflow-x:auto}
nav button{background:none;border:none;border-bottom:2px solid transparent;color:var(--dim);
padding:9px 12px;cursor:pointer;font-size:13px;white-space:nowrap}
nav button.on{color:var(--fg);border-bottom-color:var(--accent)}
main{padding:12px;max-width:1100px;margin:0 auto}
section{display:none}section.on{display:block}
.card{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:12px;margin-bottom:12px}
.card h2{font-size:13px;margin:0 0 10px;color:var(--dim);text-transform:uppercase;letter-spacing:.6px;font-weight:600}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:12px}
button,select,input{font:inherit;color:var(--fg);background:var(--panel2);border:1px solid var(--line);
border-radius:6px;padding:6px 10px}
button{cursor:pointer;user-select:none;-webkit-user-select:none;touch-action:manipulation}
button:hover{border-color:var(--accent)}
button:disabled{opacity:.4;cursor:not-allowed}
button.primary{background:var(--accent);border-color:var(--accent);color:#04121f;font-weight:600}
button.jog{min-width:52px;font-size:17px;font-family:var(--mono)}
input[type=number],input[type=text],input[type=password]{width:100%}
input[type=range]{padding:0;background:none;border:none}
label{font-size:12px;color:var(--dim);display:block;margin-bottom:2px}
.f{margin-bottom:8px}
table{width:100%;border-collapse:collapse;font-size:13px}
th{text-align:left;color:var(--dim);font-weight:600;font-size:11px;text-transform:uppercase;
padding:4px 6px;border-bottom:1px solid var(--line)}
td{padding:3px 6px;border-bottom:1px solid var(--line)}
td input{padding:4px 6px;width:100%;min-width:64px}
.num{font-family:var(--mono);font-variant-numeric:tabular-nums}
.pos{font-family:var(--mono);font-size:22px;font-variant-numeric:tabular-nums}
.tag{font-size:10px;padding:1px 6px;border-radius:99px;border:1px solid var(--line);color:var(--dim);
text-transform:uppercase;letter-spacing:.4px}
.tag.ok{color:var(--ok);border-color:var(--ok)}
.tag.warn{color:var(--warn);border-color:var(--warn)}
.tag.bad{color:var(--bad);border-color:var(--bad)}
.bar{height:6px;background:var(--panel2);border-radius:99px;margin:8px 0;position:relative;overflow:hidden}
.bar i{position:absolute;top:0;bottom:0;background:var(--accent);border-radius:99px}
.bar u{position:absolute;top:-2px;width:2px;height:10px;background:var(--warn)}
#toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:var(--panel2);
border:1px solid var(--line);border-radius:8px;padding:9px 14px;max-width:90vw;opacity:0;
transition:opacity .2s;pointer-events:none;z-index:50;font-size:13px}
#toast.on{opacity:1}
#toast.bad{border-color:var(--bad);color:var(--bad)}
.hint{font-size:12px;color:var(--dim);margin:6px 0 0}
.banner{background:#3a2a10;border:1px solid var(--warn);color:#f0c674;padding:8px 10px;
border-radius:6px;margin-bottom:12px;font-size:13px;display:none}
.banner.on{display:block}
</style></head><body>

<header>
  <h1>Camera Slider</h1>
  <span id="conn">connecting…</span>
  <button id="estop">E-STOP</button>
</header>

<nav>
  <button data-tab="control" class="on">Control</button>
  <button data-tab="sequence">Sequence</button>
  <button data-tab="axes">Axes</button>
  <button data-tab="controls">Controls</button>
  <button data-tab="system">System</button>
</nav>

<main>
  <div id="estopBanner" class="banner">E-stop latched — drivers de-energised.
    <button onclick="post('/api/estop/clear')">Clear e-stop</button></div>
  <div id="rebootBanner" class="banner">Config saved. Pin, encoder and button
    changes only take effect after a restart.
    <button onclick="post('/api/reboot')">Reboot now</button></div>

  <section id="control" class="on">
    <div class="card">
      <div class="row">
        <button onclick="post('/api/home')">Home all</button>
        <button onclick="post('/api/stop')">Stop all</button>
        <button id="enableBtn" onclick="toggleEnable()">Drivers</button>
        <button onclick="act('rec_toggle')">Rec toggle</button>
        <button onclick="act('rec_resync')">Rec resync</button>
        <span style="margin-left:auto" class="row">
          <label style="margin:0">Jog speed <b id="jogPctLabel" class="num">50</b>%</label>
          <input type="range" id="jogPct" min="1" max="100" value="50" style="width:120px">
        </span>
      </div>
    </div>
    <div id="axisCards" class="grid"></div>
  </section>

  <section id="sequence">
    <div class="card">
      <div class="row">
        <button class="primary" onclick="post('/api/sequence/play')">Play</button>
        <button onclick="post('/api/sequence/pause')">Pause</button>
        <button onclick="post('/api/sequence/stop')">Stop</button>
        <button onclick="post('/api/sequence/restart')">Restart</button>
        <button onclick="post('/api/sequence/capture').then(loadSeq)">Capture pose</button>
        <span id="seqState" class="tag">idle</span>
        <label class="row" style="margin:0 0 0 auto"><input type="checkbox" id="seqLoop"
          onchange="saveSeqOpts()" style="width:auto"> Loop</label>
        <label class="row" style="margin:0"><input type="checkbox" id="seqEase"
          onchange="saveSeqOpts()" style="width:auto"> Ease in/out</label>
      </div>
      <p class="hint">Durations are the travel time into each keyframe. If an axis
        cannot make it in that time, the whole leg is slowed so the axes stay in
        sync — nothing is ever left behind.</p>
    </div>
    <div class="card">
      <h2>Keyframes</h2>
      <div style="overflow-x:auto"><table id="seqTable"></table></div>
      <div class="row" style="margin-top:10px">
        <button class="primary" onclick="saveSeq()">Save sequence</button>
        <button onclick="loadSeq()">Revert</button>
        <button onclick="exportJson('/api/sequence','sequence.json')">Export</button>
        <button onclick="importJson('/api/sequence',loadSeq)">Import</button>
      </div>
    </div>
  </section>

  <section id="axes"><div id="axisForms"></div>
    <div class="row"><button class="primary" onclick="saveAxes()">Save axes</button>
      <button onclick="loadConfig()">Revert</button>
      <button onclick="post('/api/tmc/apply')">Re-push driver settings</button></div>
  </section>

  <section id="controls">
    <div class="card"><h2>Encoders</h2><div id="encForms" class="grid"></div>
      <p class="hint">Velocity mode turns the knob into a speed dial (back to the
        detent it started on = stop). Position mode nudges the target one step per
        click. Encoders are decoded in interrupts, not by the pulse-counter
        peripheral, so they do not compete with the step generators.</p></div>
    <div class="card"><h2>Buttons</h2><div id="btnForms" class="grid"></div></div>
    <div class="row"><button class="primary" onclick="saveControls()">Save controls</button>
      <button onclick="loadConfig()">Revert</button></div>
    <div class="card"><h2>Live input test</h2>
      <p class="hint">Turn a knob or press a button and watch these update — the
        quickest way to confirm wiring before you trust a binding.</p>
      <div id="inputLive" class="grid"></div></div>
  </section>

  <section id="system">
    <div class="card"><h2>Status</h2><div id="sysStatus" class="grid"></div></div>
    <div class="card"><h2>Stepper drivers (TMC2209 UART)</h2>
      <div class="row"><button onclick="loadTmc()">Refresh</button></div>
      <div style="overflow-x:auto"><table id="tmcTable"></table></div>
      <p class="hint">A driver that does not answer is usually a module that is not
        fully seated, or MS1/MS2 straps that disagree with the address configured
        for that axis.</p></div>
    <div class="card"><h2>Angle sensors (AS5600)</h2>
      <div class="row"><button onclick="loadSensors()">Check magnets</button></div>
      <div style="overflow-x:auto"><table id="sensorTable"></table></div></div>
    <div class="card"><h2>Network</h2><div id="wifiForm" class="grid"></div>
      <div class="row" style="margin-top:8px"><button class="primary" onclick="saveWifi()">Save network</button>
        <button onclick="scanWifi()">Scan</button></div>
      <div id="scanResults" class="hint"></div></div>
    <div class="card"><h2>Hardware pins</h2><div id="hwForm" class="grid"></div>
      <div class="row" style="margin-top:8px"><button class="primary" onclick="saveHw()">Save hardware</button></div>
      <p class="hint">GPIO 22–25 do not exist on this part, and 26–37 are the flash
        and PSRAM buses; the firmware rejects them.</p></div>
    <div class="card"><h2>Config file</h2>
      <div class="row"><button onclick="exportJson('/api/config','config.json')">Export config</button>
        <button onclick="importJson('/api/config',loadConfig)">Import config</button>
        <button onclick="post('/api/reboot')">Reboot</button>
        <button style="border-color:var(--bad);color:var(--bad)" onclick="factoryReset()">Factory reset</button></div></div>
  </section>
</main>

<div id="toast"></div>

<script>
"use strict";
let CFG=null, ST=null, ACTIONS=[], SEQ={keyframes:[]}, ws=null;
const $=s=>document.querySelector(s), $$=s=>[...document.querySelectorAll(s)];
const fx=(v,n)=>Number(v).toFixed(n===undefined?2:n);

function toast(msg,bad){const t=$("#toast");t.textContent=msg;t.className="on"+(bad?" bad":"");
  clearTimeout(t._h);t._h=setTimeout(()=>t.className="",bad?4500:2000);}

async function api(path,method,body){
  const opt={method:method||"GET",headers:{}};
  if(body!==undefined){opt.headers["Content-Type"]="application/json";opt.body=JSON.stringify(body);}
  const r=await fetch(path,opt);
  let j=null; try{j=await r.json();}catch(e){}
  if(!r.ok){throw new Error((j&&j.error)||("HTTP "+r.status));}
  return j;
}
async function post(path,body){
  try{const j=await api(path,"POST",body);
    if(j&&j.reboot_recommended)$("#rebootBanner").classList.add("on");
    toast("ok");return j;}
  catch(e){toast(e.message,true);throw e;}
}
function act(name,axis){return post("/api/action",{action:name,axis:axis||0});}

/* ---------- tabs ---------- */
$$("nav button").forEach(b=>b.onclick=()=>{
  $$("nav button").forEach(x=>x.classList.toggle("on",x===b));
  $$("section").forEach(s=>s.classList.toggle("on",s.id===b.dataset.tab));
  if(b.dataset.tab==="system"){loadTmc();loadSensors();}
});

/* ---------- websocket telemetry ---------- */
function connect(){
  ws=new WebSocket("ws://"+location.host+"/ws");
  ws.onopen=()=>$("#conn").textContent="live";
  ws.onclose=()=>{$("#conn").textContent="reconnecting…";setTimeout(connect,1500);};
  ws.onmessage=e=>{try{ST=JSON.parse(e.data);renderStatus();}catch(err){}};
}
function wsSend(o){if(ws&&ws.readyState===1)ws.send(JSON.stringify(o));}

/* ---------- control tab ---------- */
function buildAxisCards(){
  $("#axisCards").innerHTML=CFG.axes.map((a,i)=>`
   <div class="card" data-axis="${i}" ${a.enabled?"":'style="opacity:.45"'}>
    <div class="row"><b>${esc(a.name)}</b><span class="tag" id="st${i}">—</span>
      <span class="tag" id="hm${i}">—</span></div>
    <div class="pos"><span id="p${i}">0.00</span>
      <span style="font-size:12px;color:var(--dim)">${a.units}</span></div>
    <div style="font-size:12px;color:var(--dim)">target <span class="num" id="t${i}">0.00</span>
      · <span class="num" id="v${i}">0.00</span> ${a.units}/s</div>
    <div class="bar"><i id="b${i}" style="left:0;width:0"></i><u id="u${i}" style="left:0"></u></div>
    <div class="row">
      <button class="jog" data-jog="${i}" data-dir="-1">&#9668;</button>
      <button class="jog" data-jog="${i}" data-dir="1">&#9658;</button>
      <button onclick="post('/api/home/axis',{axis:${i}})">Home</button>
      <button onclick="post('/api/zero',{axis:${i},position:0})">Zero</button>
    </div>
    <div class="row" style="margin-top:6px">
      <input type="number" step="0.1" id="g${i}" placeholder="go to (${a.units})" style="flex:1">
      <button onclick="goTo(${i})">Go</button>
      <button onclick="nudge(${i},-1)">−</button><button onclick="nudge(${i},1)">+</button>
    </div>
   </div>`).join("");
  $$("[data-jog]").forEach(btn=>{
    const axis=+btn.dataset.jog, dir=+btn.dataset.dir;
    const start=e=>{e.preventDefault();wsSend({cmd:"jog",axis,dir,speed_pct:+$("#jogPct").value});};
    const stop=e=>{e.preventDefault();wsSend({cmd:"stop",axis});};
    btn.addEventListener("pointerdown",start);
    ["pointerup","pointerleave","pointercancel"].forEach(ev=>btn.addEventListener(ev,stop));
  });
}
function goTo(i){const v=parseFloat($("#g"+i).value);if(isNaN(v))return;
  post("/api/move",{axis:i,position:v,speed_pct:+$("#jogPct").value});}
function nudge(i,sign){const step=CFG.axes[i].kind==="rotary"?1:1;
  post("/api/move",{axis:i,delta:sign*step});}
function toggleEnable(){post("/api/enable",{on:!(ST&&ST.drivers_enabled)});}

function renderStatus(){
  if(!ST||!CFG)return;
  $("#estopBanner").classList.toggle("on",!!ST.estop);
  const eb=$("#enableBtn");
  if(eb){eb.textContent=ST.drivers_enabled?"Drivers on":"Drivers off";
    eb.style.borderColor=ST.drivers_enabled?"var(--ok)":"var(--line)";}
  (ST.axes||[]).forEach((a,i)=>{
    const p=$("#p"+i); if(!p)return;
    p.textContent=fx(a.pos);
    $("#t"+i).textContent=fx(a.target);
    $("#v"+i).textContent=fx(a.speed,1);
    const st=$("#st"+i);
    st.textContent=!a.available?"n/a":a.running?"moving":"idle";
    st.className="tag"+(a.running?" ok":"");
    const hm=$("#hm"+i);
    hm.textContent=a.limit_min?"MIN":a.limit_max?"MAX":a.homing;
    hm.className="tag"+((a.limit_min||a.limit_max)?" bad":a.homed?" ok":" warn");
    const span=(a.max-a.min)||1, pct=v=>Math.max(0,Math.min(100,(v-a.min)/span*100));
    $("#b"+i).style.width=pct(a.pos)+"%";
    $("#u"+i).style.left=pct(a.target)+"%";
  });
  const s=ST.sequencer||{};
  const se=$("#seqState");
  if(se){se.textContent=`${s.state} ${s.count?(s.index+1)+"/"+s.count:""}`;
    se.className="tag"+(s.state==="moving"?" ok":s.state==="paused"?" warn":"");}
  renderSysStatus(); renderInputLive();
}

/* ---------- generic field rendering ---------- */
function esc(s){return String(s).replace(/[&<>"]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]));}
function field(path,def,val){
  const id="f_"+path.replace(/[^a-z0-9]/gi,"_");
  if(def.t==="bool")
    return `<div class="f"><label>${def.l}</label><input type="checkbox" id="${id}"
      data-path="${path}" data-t="bool" style="width:auto" ${val?"checked":""}></div>`;
  if(def.t==="select"){
    const opts=(typeof def.o==="function"?def.o():def.o)
      .map(o=>`<option value="${esc(o)}" ${o==val?"selected":""}>${esc(o)}</option>`).join("");
    return `<div class="f"><label>${def.l}</label><select id="${id}" data-path="${path}"
      data-t="str">${opts}</select></div>`;}
  const type=def.t==="num"?"number":(def.t==="pass"?"password":"text");
  const step=def.s!==undefined?` step="${def.s}"`:"";
  return `<div class="f"><label>${def.l}</label><input type="${type}"${step} id="${id}"
    data-path="${path}" data-t="${def.t==="num"?"num":"str"}" value="${esc(val==null?"":val)}"></div>`;
}
function collect(scope){
  const out={};
  scope.querySelectorAll("[data-path]").forEach(el=>{
    const v=el.dataset.t==="bool"?el.checked:el.dataset.t==="num"?parseFloat(el.value):el.value;
    if(el.dataset.t==="num"&&isNaN(v))return;
    out[el.dataset.path]=v;
  });
  return out;
}
function nest(flat){ // "0.max_speed" -> [{max_speed:…}]
  const arr=[];
  for(const k in flat){const [i,key]=k.split(".");arr[+i]=arr[+i]||{};arr[+i][key]=flat[k];}
  return arr;
}

/* ---------- axes tab ---------- */
const AXIS_FIELDS=[
  {k:"name",l:"Name",t:"str"},{k:"enabled",l:"Enabled",t:"bool"},
  {k:"kind",l:"Type",t:"select",o:["linear","rotary"]},
  {k:"step_pin",l:"STEP GPIO",t:"num",s:1},{k:"dir_pin",l:"DIR GPIO",t:"num",s:1},
  {k:"invert_dir",l:"Invert direction",t:"bool"},
  {k:"motor_steps_per_rev",l:"Motor steps/rev",t:"num",s:1},
  {k:"microsteps",l:"Microsteps",t:"select",o:["1","2","4","8","16","32","64","128","256"]},
  {k:"belt_pitch_mm",l:"Belt pitch (mm, linear)",t:"num",s:0.1},
  {k:"pulley_teeth",l:"Pulley teeth (linear)",t:"num",s:1},
  {k:"gear_ratio",l:"Gear ratio (rotary)",t:"num",s:0.01},
  {k:"max_speed",l:"Max speed (units/s)",t:"num",s:1},
  {k:"accel",l:"Acceleration (units/s²)",t:"num",s:1},
  {k:"min_limit",l:"Soft limit min",t:"num",s:0.1},
  {k:"max_limit",l:"Soft limit max",t:"num",s:0.1},
  {k:"soft_limits",l:"Enforce soft limits",t:"bool"},
  {k:"homing",l:"Homing",t:"select",o:["none","limit_min","limit_max","sensor"]},
  {k:"limit_min_pin",l:"Limit min GPIO",t:"num",s:1},
  {k:"limit_max_pin",l:"Limit max GPIO",t:"num",s:1},
  {k:"limit_active_low",l:"Limits active low",t:"bool"},
  {k:"homing_speed",l:"Homing speed (units/s)",t:"num",s:1},
  {k:"homing_backoff",l:"Homing back-off (units)",t:"num",s:0.1},
  {k:"feedback",l:"Feedback sensor",t:"select",o:["none","as5600"]},
  {k:"mux_channel",l:"Mux channel",t:"num",s:1},
  {k:"zero_offset_deg",l:"Sensor zero offset (deg)",t:"num",s:0.1},
  {k:"drift_check_ms",l:"Drift check (ms, 0=off)",t:"num",s:100},
  {k:"drift_threshold_deg",l:"Drift threshold (deg)",t:"num",s:0.1},
  {k:"tmc_enabled",l:"TMC2209 over UART",t:"bool"},
  {k:"tmc_address",l:"UART address (MS1/MS2)",t:"num",s:1},
  {k:"run_current_ma",l:"Run current (mA)",t:"num",s:50},
  {k:"hold_current_pct",l:"Hold current (%)",t:"num",s:5},
  {k:"stealthchop",l:"StealthChop (quiet)",t:"bool"},
];
function buildAxes(){
  $("#axisForms").innerHTML=CFG.axes.map((a,i)=>`
    <div class="card" data-axisform="${i}">
      <h2>Axis ${i} — ${esc(a.name)}
        <span class="tag">${fx(a.steps_per_unit,2)} steps/${a.units}</span></h2>
      <div class="grid">${AXIS_FIELDS.map(f=>field(i+"."+f.k,f,a[f.k])).join("")}</div>
    </div>`).join("");
}
async function saveAxes(){
  const axes=nest(collect($("#axisForms")));
  // Microsteps is a <select>, so it arrives as a string; the API only merges
  // it when it is a JSON number.
  axes.forEach(a=>{if(a.microsteps!==undefined)a.microsteps=+a.microsteps;});
  await post("/api/config",{axes});
  await loadConfig();
}

/* ---------- controls tab ---------- */
const ENC_FIELDS=[
  {k:"name",l:"Name",t:"str"},{k:"enabled",l:"Enabled",t:"bool"},
  {k:"pin_a",l:"A GPIO",t:"num",s:1},{k:"pin_b",l:"B GPIO",t:"num",s:1},
  {k:"mode",l:"Mode",t:"select",o:["off","velocity","position"]},
  {k:"axis",l:"Controls axis",t:"select",o:()=>CFG.axes.map((a,i)=>String(i))},
  {k:"invert",l:"Invert",t:"bool"},
  {k:"counts_per_detent",l:"Counts per click",t:"num",s:1},
  {k:"units_per_detent",l:"Units per click (position mode)",t:"num",s:0.1},
  {k:"detents_for_max_speed",l:"Clicks to full speed (velocity mode)",t:"num",s:1},
];
const BTN_FIELDS=[
  {k:"name",l:"Name",t:"str"},{k:"enabled",l:"Enabled",t:"bool"},
  {k:"pin",l:"GPIO",t:"num",s:1},{k:"active_low",l:"Active low",t:"bool"},
  {k:"short_press",l:"Short press",t:"select",o:()=>ACTIONS},
  {k:"long_press",l:"Long press",t:"select",o:()=>ACTIONS},
  {k:"axis_arg",l:"Target axis (for axis actions)",t:"select",o:()=>CFG.axes.map((a,i)=>String(i))},
];
function buildControls(){
  $("#encForms").innerHTML=CFG.encoders.map((e,i)=>`
    <div class="card"><h2>Encoder ${i}</h2>
      ${ENC_FIELDS.map(f=>field("e"+i+"."+f.k,f,e[f.k])).join("")}</div>`).join("");
  $("#btnForms").innerHTML=CFG.buttons.map((b,i)=>`
    <div class="card"><h2>Button ${i}</h2>
      ${BTN_FIELDS.map(f=>field("b"+i+"."+f.k,f,b[f.k])).join("")}</div>`).join("");
}
async function saveControls(){
  const enc=[],btn=[];
  $("#encForms").querySelectorAll("[data-path]").forEach(el=>{
    const m=el.dataset.path.match(/^e(\d+)\.(.+)$/); if(!m)return;
    enc[+m[1]]=enc[+m[1]]||{};
    enc[+m[1]][m[2]]=el.dataset.t==="bool"?el.checked:
      el.dataset.t==="num"?parseFloat(el.value):el.value;
  });
  $("#btnForms").querySelectorAll("[data-path]").forEach(el=>{
    const m=el.dataset.path.match(/^b(\d+)\.(.+)$/); if(!m)return;
    btn[+m[1]]=btn[+m[1]]||{};
    btn[+m[1]][m[2]]=el.dataset.t==="bool"?el.checked:
      el.dataset.t==="num"?parseFloat(el.value):el.value;
  });
  // axis / axis_arg come out of <select> as strings; the API wants numbers.
  enc.forEach(e=>{if(e.axis!==undefined)e.axis=+e.axis;});
  btn.forEach(b=>{if(b.axis_arg!==undefined)b.axis_arg=+b.axis_arg;});
  await post("/api/config",{encoders:enc,buttons:btn});
  await loadConfig();
}
function renderInputLive(){
  if(!ST||!ST.inputs)return;
  const e=ST.inputs.encoders||[],b=ST.inputs.buttons||[];
  $("#inputLive").innerHTML=
    e.map(x=>`<div class="card"><b>${esc(x.name)}</b>
      <div class="pos">${x.count}</div>
      <span class="tag ${x.attached?"ok":"bad"}">${x.attached?"attached":"not attached"}</span></div>`).join("")+
    b.map(x=>`<div class="card"><b>${esc(x.name)}</b>
      <div><span class="tag ${x.pressed?"ok":""}">${x.pressed?"pressed":"released"}</span></div></div>`).join("");
}

/* ---------- sequence tab ---------- */
async function loadSeq(){
  SEQ=await api("/api/sequence");
  $("#seqLoop").checked=!!SEQ.loop; $("#seqEase").checked=!!SEQ.ease;
  renderSeq();
}
function renderSeq(){
  const units=CFG?CFG.axes.map(a=>a.units):[];
  const head=`<tr><th>#</th>${(CFG?CFG.axes:[]).map((a,i)=>
    `<th>${esc(a.name)} (${units[i]})</th>`).join("")}<th>Travel s</th><th>Hold s</th><th></th></tr>`;
  const rows=SEQ.keyframes.map((k,i)=>`<tr>
    <td class="num">${i+1}</td>
    ${k.pos.map((p,a)=>`<td><input type="number" step="0.1" value="${fx(p)}"
       data-kf="${i}" data-col="${a}"></td>`).join("")}
    <td><input type="number" step="0.1" min="0.05" value="${fx(k.duration_s,2)}"
       data-kf="${i}" data-col="d"></td>
    <td><input type="number" step="0.1" min="0" value="${fx(k.hold_s,2)}"
       data-kf="${i}" data-col="h"></td>
    <td class="row" style="flex-wrap:nowrap">
      <button title="Go to this pose" onclick="post('/api/sequence/goto',{index:${i}})">&#9654;</button>
      <button title="Overwrite with the current pose" onclick="grabKf(${i})">&#9679;</button>
      <button title="Move up" onclick="moveKf(${i},-1)">&#9650;</button>
      <button title="Move down" onclick="moveKf(${i},1)">&#9660;</button>
      <button title="Delete" onclick="delKf(${i})">&times;</button>
    </td></tr>`).join("");
  $("#seqTable").innerHTML=head+rows+
    `<tr><td colspan="${(CFG?CFG.axes.length:0)+4}">
      <button onclick="post('/api/sequence/capture').then(loadSeq)">+ Capture current pose</button></td></tr>`;
  $("#seqTable").querySelectorAll("[data-kf]").forEach(el=>{
    el.onchange=()=>{const i=+el.dataset.kf,c=el.dataset.col,v=parseFloat(el.value);
      if(isNaN(v))return;
      if(c==="d")SEQ.keyframes[i].duration_s=v;
      else if(c==="h")SEQ.keyframes[i].hold_s=v;
      else SEQ.keyframes[i].pos[+c]=v;};
  });
}
function grabKf(i){if(!ST)return;SEQ.keyframes[i].pos=ST.axes.map(a=>a.pos);renderSeq();}
function delKf(i){SEQ.keyframes.splice(i,1);renderSeq();}
function moveKf(i,d){const j=i+d;if(j<0||j>=SEQ.keyframes.length)return;
  const t=SEQ.keyframes[i];SEQ.keyframes[i]=SEQ.keyframes[j];SEQ.keyframes[j]=t;renderSeq();}
async function saveSeq(){await post("/api/sequence",{keyframes:SEQ.keyframes});await loadSeq();}
async function saveSeqOpts(){
  await post("/api/config",{sequencer:{loop:$("#seqLoop").checked,ease:$("#seqEase").checked}});
  $("#rebootBanner").classList.remove("on"); // sequencer options apply live
}

/* ---------- system tab ---------- */
function renderSysStatus(){
  if(!ST)return;
  const items=[["Firmware",ST.fw],["Uptime",Math.floor(ST.uptime_s/60)+"m "+(ST.uptime_s%60)+"s"],
    ["Network",ST.network+" ("+ST.wifi_mode+")"],["RSSI",ST.rssi+" dBm"],
    ["Free heap",Math.round(ST.free_heap/1024)+" kB"],
    ["Free PSRAM",Math.round(ST.free_psram/1024)+" kB"],
    ["Drivers",ST.drivers_enabled?"enabled":"disabled"],
    ["E-stop",ST.estop?"LATCHED":"clear"],
    ["BLE",ST.ble.enabled?(ST.ble.connected?"connected":"advertising"):"off"],
    ["Recording",ST.ble.recording?"believed ON":"believed off"]];
  $("#sysStatus").innerHTML=items.map(([k,v])=>
    `<div><label>${k}</label><div class="num">${esc(v)}</div></div>`).join("");
}
async function loadTmc(){
  try{const j=await api("/api/tmc");
    $("#tmcTable").innerHTML=
      `<tr><th>Axis</th><th>Addr</th><th>State</th><th>µsteps</th><th>Current</th>
       <th>Mode</th><th>Flags</th></tr>`+
      j.drivers.map(d=>`<tr><td>${esc(d.name)}</td><td class="num">${d.address}</td>
        <td><span class="tag ${d.present?"ok":"bad"}">${d.present?"ok":"no reply"}</span></td>
        <td class="num">${d.microsteps??"—"}</td><td class="num">${d.rms_current_ma??"—"}</td>
        <td>${d.present?(d.stealthchop?"stealth":"spread"):"—"}</td>
        <td>${flags(d)}</td></tr>`).join("");
  }catch(e){toast(e.message,true);}
}
function flags(d){
  const f=[];
  if(d.overtemp)f.push('<span class="tag bad">over temp</span>');
  else if(d.overtemp_warn)f.push('<span class="tag warn">temp warn</span>');
  if(d.open_load_a||d.open_load_b)f.push('<span class="tag warn">open load</span>');
  if(d.short_a||d.short_b)f.push('<span class="tag bad">short</span>');
  if(d.note)f.push('<span class="tag">'+esc(d.note)+"</span>");
  return f.join(" ")||'<span class="tag ok">clear</span>';
}
async function loadSensors(){
  try{const j=await api("/api/sensors");
    $("#sensorTable").innerHTML=
      `<tr><th>Axis</th><th>Ch</th><th>Reply</th><th>Magnet</th><th>Raw</th><th>Angle</th></tr>`+
      (j.sensors.length?j.sensors.map(s=>`<tr><td>${esc(s.name)}</td>
        <td class="num">${s.channel}</td>
        <td><span class="tag ${s.responded?"ok":"bad"}">${s.responded?"yes":"silent"}</span></td>
        <td>${!s.responded?"—":s.magnet_too_weak?'<span class="tag warn">too far</span>':
          s.magnet_too_strong?'<span class="tag warn">too close</span>':
          s.magnet_detected?'<span class="tag ok">good</span>':'<span class="tag bad">none</span>'}</td>
        <td class="num">${s.raw}</td><td class="num">${fx(s.raw_deg,1)}°</td></tr>`).join("")
        :`<tr><td colspan="6">No axis is configured with an AS5600.</td></tr>`);
  }catch(e){toast(e.message,true);}
}
const WIFI_FIELDS=[
  {k:"sta_enabled",l:"Join an existing network",t:"bool"},
  {k:"ssid",l:"Network SSID",t:"str"},
  {k:"pass",l:"Network password (blank = keep current)",t:"pass"},
  {k:"ap_fallback",l:"Fall back to own hotspot",t:"bool"},
  {k:"ap_ssid",l:"Hotspot name",t:"str"},
  {k:"ap_pass",l:"Hotspot password (8+ chars, or blank for open)",t:"str"},
  {k:"hostname",l:"Hostname (…​.local)",t:"str"},
];
const HW_FIELDS=[
  {k:"driver_enable_pin",l:"Driver EN GPIO",t:"num",s:1},
  {k:"driver_enable_active_low",l:"EN active low",t:"bool"},
  {k:"tmc_tx_pin",l:"TMC UART TX GPIO",t:"num",s:1},
  {k:"tmc_rx_pin",l:"TMC UART RX GPIO",t:"num",s:1},
  {k:"tmc_baud",l:"TMC UART baud",t:"num",s:1},
  {k:"tmc_rsense",l:"Sense resistor (ohm)",t:"num",s:0.01},
  {k:"mux_sda_pin",l:"Mux SDA GPIO",t:"num",s:1},
  {k:"mux_scl_pin",l:"Mux SCL GPIO",t:"num",s:1},
  {k:"mux_address",l:"TCA9548A address",t:"num",s:1},
  {k:"oled_sda_pin",l:"OLED SDA GPIO",t:"num",s:1},
  {k:"oled_scl_pin",l:"OLED SCL GPIO",t:"num",s:1},
  {k:"oled_address",l:"OLED address",t:"num",s:1},
  {k:"display_enabled",l:"OLED enabled",t:"bool"},
  {k:"display_refresh_ms",l:"OLED refresh (ms)",t:"num",s:10},
];
function buildSystemForms(){
  $("#wifiForm").innerHTML=WIFI_FIELDS.map(f=>field("w."+f.k,f,CFG.wifi[f.k])).join("");
  $("#hwForm").innerHTML=HW_FIELDS.map(f=>field("h."+f.k,f,CFG.hardware[f.k])).join("");
}
function pick(scope,prefix){
  const out={};
  scope.querySelectorAll("[data-path]").forEach(el=>{
    const m=el.dataset.path.match(new RegExp("^"+prefix+"\\.(.+)$")); if(!m)return;
    const v=el.dataset.t==="bool"?el.checked:el.dataset.t==="num"?parseFloat(el.value):el.value;
    if(el.dataset.t==="num"&&isNaN(v))return;
    out[m[1]]=v;
  });
  return out;
}
async function saveWifi(){await post("/api/config",{wifi:pick($("#wifiForm"),"w")});await loadConfig();}
async function saveHw(){await post("/api/config",{hardware:pick($("#hwForm"),"h")});await loadConfig();}
async function scanWifi(){
  $("#scanResults").textContent="scanning…";
  const poll=async()=>{const j=await api("/api/wifi/scan");
    if(j.scanning){setTimeout(poll,1200);return;}
    $("#scanResults").innerHTML=j.networks.map(n=>
      `<button onclick="document.getElementById('f_w_ssid').value=${JSON.stringify(n.ssid)}">
        ${esc(n.ssid)} (${n.rssi})</button>`).join(" ")||"nothing found";};
  poll();
}
function factoryReset(){
  if(!confirm("Erase config and sequence, then reboot to factory defaults?"))return;
  post("/api/config/reset");
}
async function exportJson(path,name){
  const j=await api(path);
  const a=document.createElement("a");
  a.href=URL.createObjectURL(new Blob([JSON.stringify(j,null,2)],{type:"application/json"}));
  a.download=name;a.click();URL.revokeObjectURL(a.href);
}
function importJson(path,after){
  const inp=document.createElement("input");inp.type="file";inp.accept=".json";
  inp.onchange=async()=>{const f=inp.files[0];if(!f)return;
    try{await post(path,JSON.parse(await f.text()));if(after)await after();}
    catch(e){toast(e.message,true);}};
  inp.click();
}

/* ---------- boot ---------- */
async function loadConfig(){
  CFG=await api("/api/config");
  buildAxisCards();buildAxes();buildControls();buildSystemForms();
  renderSeq();
}
$("#estop").onclick=()=>post("/api/estop");
$("#jogPct").oninput=e=>$("#jogPctLabel").textContent=e.target.value;
document.addEventListener("keydown",e=>{
  if(e.target.tagName==="INPUT"||e.target.tagName==="SELECT")return;
  if(e.code==="Space"){e.preventDefault();post("/api/sequence/play");}
  if(e.code==="Escape"){post("/api/estop");}
});
(async()=>{
  try{ACTIONS=await api("/api/actions");}catch(e){ACTIONS=["none"];}
  await loadConfig();
  await loadSeq();
  connect();
})();
</script></body></html>
)HTMLDOC";
