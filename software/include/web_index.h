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
/* Greyscale chrome, dark red for anything that acts. Status colours stay
   distinct from the accent so a fault never reads as furniture. */
:root{--bg:#121212;--panel:#1a1a1a;--panel2:#232323;--panel3:#2c2c2c;
--line:#343434;--line2:#454545;--fg:#e4e4e4;--dim:#9a9a9a;--dim2:#6f6f6f;
--accent:#8e2226;--accent2:#a92b30;--accent-fg:#f6ecec;
--ok:#84a184;--warn:#b8934a;--bad:#cf3b41;
--mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{position:sticky;top:0;z-index:10;background:var(--panel);border-bottom:1px solid var(--line);
padding:8px 12px;display:flex;gap:8px;align-items:center;flex-wrap:wrap}
header h1{font-size:15px;margin:0 8px 0 0;font-weight:600;letter-spacing:.2px}
#conn{font-size:11px;color:var(--dim);font-family:var(--mono)}
#estop{margin-left:auto;background:var(--bad);border-color:var(--bad);color:#fff;font-weight:700;
letter-spacing:.5px}
#estop:hover{background:#e04a50;border-color:#e04a50}
nav{display:flex;gap:2px;background:var(--panel);border-bottom:1px solid var(--line);
padding:0 8px;overflow-x:auto}
nav button{background:none;border:none;border-bottom:2px solid transparent;color:var(--dim);
padding:9px 12px;cursor:pointer;font-size:13px;white-space:nowrap}
nav button:hover{color:var(--fg)}
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
button:hover{border-color:var(--line2);background:var(--panel3)}
button:disabled{opacity:.4;cursor:not-allowed}
button.primary{background:var(--accent);border-color:var(--accent);color:var(--accent-fg);font-weight:600}
button.primary:hover{background:var(--accent2);border-color:var(--accent2)}
button.jog{min-width:52px;font-size:17px;font-family:var(--mono)}
button.seg{border-radius:0;border-right-width:0}
button.seg:first-of-type{border-radius:6px 0 0 6px}
button.seg:last-of-type{border-radius:0 6px 6px 0;border-right-width:1px}
button.seg.on{background:var(--accent);border-color:var(--accent);color:var(--accent-fg);font-weight:600}
input[type=number],input[type=text],input[type=password]{width:100%}
input:focus,select:focus{outline:none;border-color:var(--accent2)}
input[type=range]{padding:0;background:none;border:none;accent-color:var(--accent2)}
input[type=checkbox]{accent-color:var(--accent2)}
label{font-size:12px;color:var(--dim);display:block;margin-bottom:2px}
.f{margin-bottom:8px}
table{width:100%;border-collapse:collapse;font-size:13px}
th{text-align:left;color:var(--dim);font-weight:600;font-size:11px;text-transform:uppercase;
padding:4px 6px;border-bottom:1px solid var(--line)}
td{padding:3px 6px;border-bottom:1px solid var(--line)}
tr.now td{background:#241416}
td input{padding:4px 6px;width:100%;min-width:64px}
.num{font-family:var(--mono);font-variant-numeric:tabular-nums}
.pos{font-family:var(--mono);font-size:22px;font-variant-numeric:tabular-nums}
.tag{font-size:10px;padding:1px 6px;border-radius:99px;border:1px solid var(--line2);color:var(--dim);
text-transform:uppercase;letter-spacing:.4px}
.tag.ok{color:var(--ok);border-color:var(--ok)}
.tag.warn{color:var(--warn);border-color:var(--warn)}
.tag.bad{color:var(--bad);border-color:var(--bad)}
.bar{height:6px;background:var(--panel3);border-radius:99px;margin:8px 0;position:relative;overflow:hidden}
.bar i{position:absolute;top:0;bottom:0;background:var(--accent2);border-radius:99px}
.bar u{position:absolute;top:-2px;width:2px;height:10px;background:var(--warn)}
#toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:var(--panel3);
border:1px solid var(--line2);border-radius:8px;padding:9px 14px;max-width:90vw;opacity:0;
transition:opacity .2s;pointer-events:none;z-index:50;font-size:13px}
#toast.on{opacity:1}
#toast.bad{border-color:var(--bad);color:var(--bad)}
.hint{font-size:12px;color:var(--dim);margin:6px 0 0}
.banner{background:#2a2118;border:1px solid var(--warn);color:#d8b877;padding:8px 10px;
border-radius:6px;margin-bottom:12px;font-size:13px;display:none}
.banner.on{display:block}
.banner.stop{background:#2a1416;border-color:var(--bad);color:#eda1a4}

/* ---- sequencer graphs ---- */
.gwrap{margin:0 0 4px}
.ghead{display:flex;gap:8px;align-items:baseline;font-size:12px;color:var(--dim);padding:0 2px}
.ghead b{color:var(--fg);font-size:13px}
.gval{margin-left:auto;font-family:var(--mono);font-variant-numeric:tabular-nums;color:var(--dim)}
.gscroll{overflow-x:auto}
svg.graph{display:block;touch-action:none}
.gbg{fill:var(--panel2);stroke:var(--line)}
.ggrid{stroke:#2b2b2b;stroke-width:1}
.gkf{stroke:#303030;stroke-width:1;stroke-dasharray:2 3}
.gtick{fill:var(--dim2);font-size:10px;font-family:var(--mono)}
.gcurve{fill:none;stroke:var(--accent2);stroke-width:2;stroke-linejoin:round;stroke-linecap:round}
.glive{stroke:var(--dim2);stroke-width:1;stroke-dasharray:4 4}
.gpt{fill:var(--panel);stroke:var(--accent2);stroke-width:2;cursor:grab}
.gpt:hover{fill:var(--accent2)}
.gpt.cur{fill:var(--accent);stroke:var(--fg)}

/* ---- curve editor ---- */
.gpt.sel{fill:var(--fg);stroke:var(--fg)}
.ghandle{stroke:var(--dim);stroke-width:1}
.ggrip{fill:var(--panel);stroke:var(--dim);stroke-width:1.5;cursor:grab}
.ggrip:hover{fill:var(--fg);stroke:var(--fg)}
.gplay{stroke:var(--fg);stroke-width:1;opacity:.7}
.gcurve.over{stroke:var(--bad)}
.gempty{fill:var(--dim2);font-size:11px}
.slotbar{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.slotbar select{min-width:190px}
.dirty{font-size:11px;font-family:var(--mono);color:var(--warn)}
.dirty.clean{color:var(--dim2)}
.warnbox{background:#2a1416;border:1px solid var(--bad);color:#eda1a4;padding:7px 10px;
border-radius:6px;margin:8px 0 0;font-size:12px;display:none}
.warnbox.on{display:block}
</style></head><body>

<header>
  <h1>Camera Slider</h1>
  <span id="conn">connecting…</span>
  <button id="estop">E-STOP</button>
</header>

<nav>
  <button data-tab="control" class="on">Control</button>
  <button data-tab="curves">Sequences</button>
  <button data-tab="sequence">Keyframes</button>
  <button data-tab="axes">Axes</button>
  <button data-tab="controls">Controls</button>
  <button data-tab="system">System</button>
</nav>

<main>
  <div id="estopBanner" class="banner stop">E-stop latched — drivers de-energised.
    <button onclick="clearEstop()">Clear e-stop</button></div>
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
    <p id="axisHidden" class="hint"></p>
  </section>

  <section id="curves">
    <div class="card">
      <div class="slotbar">
        <label style="margin:0">Sequence</label>
        <select id="slotSel" onchange="selectSlot(+this.value)"></select>
        <button onclick="newCurve()">New</button>
        <button class="primary" onclick="saveCurve()">Save</button>
        <button onclick="saveCurveAs()">Save as…</button>
        <button onclick="renameCurve()">Rename</button>
        <button style="border-color:var(--bad);color:var(--bad)" onclick="deleteCurve()">Delete</button>
        <span id="curveDirty" class="dirty clean"></span>
      </div>
      <div class="row" style="margin-top:10px">
        <button class="primary" onclick="post('/api/curve/play')">Play</button>
        <button onclick="post('/api/curve/pause')">Pause</button>
        <button onclick="post('/api/curve/stop')">Stop</button>
        <span id="curveState" class="tag">idle</span>
        <span class="num" id="curveClock" style="color:var(--dim)">0.00 / 0.00 s</span>
        <span style="margin-left:auto" class="row">
          <button onclick="addKeyHere()">+ Point</button>
          <button onclick="delKey()">− Point</button>
          <button onclick="resetHandles()">Reset curve</button>
        </span>
      </div>
      <input type="range" id="curveScrub" min="0" max="1000" value="0"
        style="width:100%;margin-top:10px" onchange="scrubTo(this.value)">
      <div id="curveWarn" class="warnbox"></div>
      <p class="hint">A sequence gives every axis its own independent track on one
        shared clock — unlike a keyframe, which is a single pose that all axes
        reach together. Double-click a chart to add a point; click one to select
        it, then drag it, drag its handles, or press Delete.</p>
    </div>
    <div id="curveGraphs"></div>
    <p class="hint">Handles set how the axis enters and leaves a point. Flat is a
      dead stop, so a longer flat handle holds still for longer and bends the
      curve harder; aim a handle straight at the next point and that leg runs
      linear. Shorten a flat handle and the curve straightens back out.</p>
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
      <h2>Timeline</h2>
      <div class="row">
        <span style="color:var(--dim);font-size:12px">Values</span>
        <span><button class="seg on" id="modeAbs" onclick="setSeqMode('abs')">Absolute</button
          ><button class="seg" id="modeRel" onclick="setSeqMode('rel')">Relative</button></span>
        <span id="seqModeHint" class="hint" style="margin:0"></span>
      </div>
      <div id="graphs" style="margin-top:10px"></div>
      <p class="hint">Drag a point up or down to change that axis's position, or
        left and right to change how long the leg into it takes. Each axis is
        independent — the shared time axis is the only thing they have in common,
        which is exactly what makes a compound move look deliberate.</p>
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
    <p class="hint">Clearing <b>Enabled</b> releases the axis's step generator and
      hides it from the Control dashboard and the sequencer timeline. Its
      keyframe values are kept, so re-enabling it restores the move intact.</p>
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
function esc(s){return String(s).replace(/[&<>"]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]));}

// A latched e-stop that will not clear is worse than one that never latched, so
// this reports the firmware's own view rather than trusting the 200.
async function clearEstop(){
  try{
    await api("/api/estop/clear","POST");
    // The POST only queues the command; the main loop applies it. Give it a
    // couple of loop passes before asking whether it actually took.
    await new Promise(r=>setTimeout(r,250));
    const s=await api("/api/status");
    ST=s; renderStatus();
    toast(s.estop?"e-stop still latched — check the physical button":"e-stop cleared",!!s.estop);
  }catch(e){toast(e.message,true);}
}

/* ---------- tabs ---------- */
$$("nav button").forEach(b=>b.onclick=()=>{
  $$("nav button").forEach(x=>x.classList.toggle("on",x===b));
  $$("section").forEach(s=>s.classList.toggle("on",s.id===b.dataset.tab));
  if(b.dataset.tab==="system"){loadTmc();loadSensors();}
  // The graphs are laid out in real pixels, so they can only be sized once the
  // section they live in is actually on screen.
  if(b.dataset.tab==="sequence")renderGraphs();
  if(b.dataset.tab==="curves")renderCurveGraphs();
});

/* ---------- websocket telemetry ---------- */
function connect(){
  ws=new WebSocket("ws://"+location.host+"/ws");
  ws.onopen=()=>$("#conn").textContent="live";
  ws.onclose=()=>{$("#conn").textContent="reconnecting…";setTimeout(connect,1500);};
  ws.onmessage=e=>{try{ST=JSON.parse(e.data);renderStatus();}catch(err){}};
}
function wsSend(o){if(ws&&ws.readyState===1)ws.send(JSON.stringify(o));}

/* ---------- axis visibility ----------
   A disabled axis has no step generator in firmware, so showing it a dashboard
   card would only offer buttons that quietly do nothing. It stays visible on
   the Axes tab, which is the one place you can turn it back on. */
function liveAxes(){return CFG?CFG.axes.map((a,i)=>i).filter(i=>CFG.axes[i].enabled):[];}

/* ---------- control tab ---------- */
function buildAxisCards(){
  const live=liveAxes();
  $("#axisCards").innerHTML=live.map(i=>{
   const a=CFG.axes[i];
   return `
   <div class="card" data-axis="${i}">
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
   </div>`;}).join("")||
   `<p class="hint">No axes are enabled. Turn one on under <b>Axes</b>.</p>`;
  const off=CFG?CFG.axes.length-live.length:0;
  $("#axisHidden").textContent=off?
    off+" axis"+(off>1?"es are":" is")+" disabled and hidden. Enable under Axes.":"";
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
function nudge(i,sign){post("/api/move",{axis:i,delta:sign});}
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
  if($("#sequence").classList.contains("on"))updateGraphs();
  if($("#curves").classList.contains("on"))updateCurveGraphs();
  renderSysStatus(); renderInputLive();
}

/* ---------- generic field rendering ---------- */
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
        <span class="tag">${fx(a.steps_per_unit,2)} steps/${a.units}</span>
        <span class="tag ${a.enabled?"ok":""}">${a.enabled?"enabled":"disabled"}</span></h2>
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

/* ---------- sequence tab ----------
   Keyframes are stored absolutely, because that is what the firmware moves to.
   "Relative" is an editing mode, not a second storage format: a relative edit
   moves the keyframe *and carries every later one with it*, so the legs after
   the one you touched keep exactly the shape you gave them. */
let SEQ_MODE="abs";

function setSeqMode(m){
  SEQ_MODE=m;
  $("#modeAbs").classList.toggle("on",m==="abs");
  $("#modeRel").classList.toggle("on",m==="rel");
  renderSeq();
}
function seqModeHint(){
  $("#seqModeHint").textContent=SEQ_MODE==="abs"
    ? "Each value is where the axis should be. Editing moves only that keyframe."
    : "Each value is the change from the keyframe before. Editing carries every later keyframe with it. Row 1 is the start pose, so it stays absolute.";
}
// What the user sees in the cell for axis `a` at keyframe `i`.
function shownVal(i,a){
  if(SEQ_MODE==="abs"||i===0)return SEQ.keyframes[i].pos[a];
  return SEQ.keyframes[i].pos[a]-SEQ.keyframes[i-1].pos[a];
}
// The absolute target the user is asking for, given what they typed.
function absFromShown(i,a,v){
  if(SEQ_MODE==="abs"||i===0)return v;
  return SEQ.keyframes[i-1].pos[a]+v;
}
function axLimits(a){
  const c=CFG.axes[a];
  return c.soft_limits&&c.max_limit>c.min_limit?[c.min_limit,c.max_limit]:null;
}
// Applies a new absolute position, respecting the current editing mode and the
// axis's soft limits. In relative mode the whole tail moves by the same delta,
// so the clamp has to hold for every keyframe it will touch -- one keyframe
// already sitting on a limit pins the entire tail. Returns false when the move
// was shortened, so a typed edit can say why it did not land where asked.
function setPos(i,a,want){
  const lim=axLimits(a), kfs=SEQ.keyframes;
  const asked=want-kfs[i].pos[a];
  let d=asked;
  const tail=SEQ_MODE==="rel";
  if(lim){
    const to=tail?kfs.length-1:i;
    let dMax=Infinity,dMin=-Infinity;
    for(let j=i;j<=to;j++){
      dMax=Math.min(dMax,lim[1]-kfs[j].pos[a]);
      dMin=Math.max(dMin,lim[0]-kfs[j].pos[a]);
    }
    d=Math.max(dMin,Math.min(dMax,d));
  }
  if(tail){for(let j=i;j<kfs.length;j++)kfs[j].pos[a]+=d;}
  else kfs[i].pos[a]+=d;
  return Math.abs(asked-d)<1e-6;
}

async function loadSeq(){
  SEQ=await api("/api/sequence");
  $("#seqLoop").checked=!!SEQ.loop; $("#seqEase").checked=!!SEQ.ease;
  renderSeq();
}
function renderSeq(){
  seqModeHint();
  const live=liveAxes();
  const cols=live.length+4;
  const head=`<tr><th>#</th>${live.map(a=>
    `<th>${esc(CFG.axes[a].name)} (${SEQ_MODE==="rel"?"Δ":""}${CFG.axes[a].units})</th>`).join("")
    }<th>Travel s</th><th>Hold s</th><th></th></tr>`;
  const rows=SEQ.keyframes.map((k,i)=>`<tr data-row="${i}">
    <td class="num">${i+1}</td>
    ${live.map(a=>`<td><input type="number" step="0.1" value="${fx(shownVal(i,a))}"
       data-kf="${i}" data-col="${a}"></td>`).join("")}
    <td><input type="number" step="0.1" min="0.05" value="${fx(k.duration_s,2)}"
       data-kf="${i}" data-col="d" ${i===0?"disabled title='The first keyframe is the start pose — nothing travels into it'":""}></td>
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
    `<tr><td colspan="${cols}">
      <button onclick="post('/api/sequence/capture').then(loadSeq)">+ Capture current pose</button></td></tr>`;
  $("#seqTable").querySelectorAll("[data-kf]").forEach(el=>{
    el.onchange=()=>{const i=+el.dataset.kf,c=el.dataset.col,v=parseFloat(el.value);
      if(isNaN(v)){renderSeq();return;}
      if(c==="d")SEQ.keyframes[i].duration_s=Math.max(0.05,v);
      else if(c==="h")SEQ.keyframes[i].hold_s=Math.max(0,v);
      else if(!setPos(i,+c,absFromShown(i,+c,v))){
        // Better to say so than to leave a number the user did not type sitting
        // in the box looking like it was accepted.
        const n=esc(CFG.axes[+c].name);
        toast(SEQ_MODE==="rel"
          ? n+": soft limits stopped the rest of the sequence from following"
          : n+": clamped to its soft limits",true);
      }
      renderSeq();};
  });
  renderGraphs();
}
function grabKf(i){
  if(!ST||!ST.axes)return;
  // Only the axes that are actually running have a meaningful position; an
  // axis with no step generator reports 0, which would silently rewrite a
  // keyframe value that is still perfectly good.
  liveAxes().forEach(a=>{if(ST.axes[a])SEQ.keyframes[i].pos[a]=ST.axes[a].pos;});
  renderSeq();
}
function delKf(i){SEQ.keyframes.splice(i,1);renderSeq();}
function moveKf(i,d){const j=i+d;if(j<0||j>=SEQ.keyframes.length)return;
  const t=SEQ.keyframes[i];SEQ.keyframes[i]=SEQ.keyframes[j];SEQ.keyframes[j]=t;renderSeq();}
async function saveSeq(){await post("/api/sequence",{keyframes:SEQ.keyframes});await loadSeq();}
async function saveSeqOpts(){
  await post("/api/config",{sequencer:{loop:$("#seqLoop").checked,ease:$("#seqEase").checked}});
  $("#rebootBanner").classList.remove("on"); // sequencer options apply live
  renderGraphs();                            // the curve shape follows `ease`
}

/* ---------- sequencer graphs ----------
   One position-vs-time plot per enabled axis, all sharing a single time axis.
   The curve is drawn the way the firmware actually moves: a flat run through
   each hold, and a quintic ease between keyframes when easing is on. */
const G={H:132,L:52,R:14,T:12,B:22,SEG:14};
const easeQuintic=t=>t*t*t*(t*(t*6-15)+10);
let DRAG=null;   // {ai,i,total,moved} — freezes the time scale while dragging

// Arrival and departure time of every keyframe, in seconds from the start.
function seqTimes(){
  const a=[],d=[]; let t=0;
  SEQ.keyframes.forEach((k,i)=>{
    if(i>0)t+=Math.max(0.05,k.duration_s||0);
    a[i]=t;
    t+=Math.max(0,k.hold_s||0);
    d[i]=t;
  });
  return {arrive:a,depart:d,total:t};
}
// Deliberately derived from the keyframes alone, never from the live position:
// a scale that breathed with telemetry would make the whole curve crawl while
// the rig moved, which is useless for editing.
function yRange(a){
  const lim=axLimits(a);
  if(lim)return lim;
  const vals=SEQ.keyframes.map(k=>k.pos[a]);
  let lo=Math.min.apply(null,vals), hi=Math.max.apply(null,vals);
  if(!isFinite(lo)||!isFinite(hi)){lo=-1;hi=1;}
  if(hi-lo<1e-6){lo-=1;hi+=1;}
  const pad=(hi-lo)*0.1;
  return [lo-pad,hi+pad];
}
function graphW(){
  const host=$("#graphs");
  return Math.max(320,(host&&host.clientWidth)||640);
}
function scales(a,total,W){
  const [lo,hi]=yRange(a);
  const x0=G.L, x1=W-G.R, y0=G.T, y1=G.H-G.B;
  const span=total>0?total:1;
  return {
    lo,hi,x0,x1,y0,y1,
    X:t=>x0+(x1-x0)*(t/span),
    Y:v=>y1-(y1-y0)*((v-lo)/((hi-lo)||1)),
    invX:px=>(px-x0)/((x1-x0)||1)*span,
    invY:py=>lo+(y1-py)/((y1-y0)||1)*(hi-lo),
  };
}
function axisPath(a,T,S){
  const n=SEQ.keyframes.length;
  if(!n)return "";
  const ease=$("#seqEase").checked;
  const p=i=>SEQ.keyframes[i].pos[a];
  let d="M"+S.X(T.arrive[0]).toFixed(1)+","+S.Y(p(0)).toFixed(1);
  d+="L"+S.X(T.depart[0]).toFixed(1)+","+S.Y(p(0)).toFixed(1);
  for(let i=1;i<n;i++){
    const v0=p(i-1),v1=p(i),t0=T.depart[i-1],t1=T.arrive[i];
    if(ease&&v0!==v1){
      for(let s=1;s<=G.SEG;s++){
        const u=s/G.SEG;
        d+="L"+S.X(t0+(t1-t0)*u).toFixed(1)+","+S.Y(v0+(v1-v0)*easeQuintic(u)).toFixed(1);
      }
    }else{
      d+="L"+S.X(t1).toFixed(1)+","+S.Y(v1).toFixed(1);
    }
    d+="L"+S.X(T.depart[i]).toFixed(1)+","+S.Y(v1).toFixed(1);
  }
  return d;
}
function renderGraphs(){
  const host=$("#graphs");
  if(!host||!CFG)return;
  const live=liveAxes();
  if(!live.length){host.innerHTML='<p class="hint">No axes are enabled.</p>';return;}
  if(!SEQ.keyframes.length){
    host.innerHTML='<p class="hint">No keyframes yet — capture a pose to start the timeline.</p>';
    return;
  }
  const T=seqTimes(), total=DRAG?DRAG.total:T.total, W=graphW();
  host.innerHTML=live.map(a=>{
    const c=CFG.axes[a], S=scales(a,total,W);
    const kfLines=SEQ.keyframes.map((k,i)=>
      `<line class="gkf" id="gk${a}_${i}" x1="0" y1="${S.y0}" x2="0" y2="${S.y1}"/>`).join("");
    const pts=SEQ.keyframes.map((k,i)=>
      `<circle class="gpt" id="gc${a}_${i}" data-ax="${a}" data-kf="${i}" r="5" cx="0" cy="0"/>`).join("");
    return `<div class="gwrap">
      <div class="ghead"><b>${esc(c.name)}</b>
        <span>${c.kind} · ${c.units}</span>
        <span class="gval" id="gv${a}"></span></div>
      <div class="gscroll"><svg class="graph" data-ax="${a}" width="${W}" height="${G.H}"
        viewBox="0 0 ${W} ${G.H}">
        <rect class="gbg" x="${S.x0}" y="${S.y0}" width="${S.x1-S.x0}" height="${S.y1-S.y0}"/>
        <line class="ggrid" x1="${S.x0}" x2="${S.x1}" y1="${(S.y0+S.y1)/2}" y2="${(S.y0+S.y1)/2}"/>
        ${kfLines}
        <text class="gtick" id="gyh${a}" x="${S.x0-6}" y="${S.y0+9}" text-anchor="end"></text>
        <text class="gtick" id="gym${a}" x="${S.x0-6}" y="${(S.y0+S.y1)/2+4}" text-anchor="end"></text>
        <text class="gtick" id="gyl${a}" x="${S.x0-6}" y="${S.y1}" text-anchor="end"></text>
        <text class="gtick" x="${S.x0}" y="${G.H-6}">0s</text>
        <text class="gtick" id="gxt${a}" x="${S.x1}" y="${G.H-6}" text-anchor="end"></text>
        <line class="glive" id="glv${a}" x1="${S.x0}" x2="${S.x1}" y1="0" y2="0"/>
        <path class="gcurve" id="gpath${a}" d=""/>
        ${pts}
      </svg></div></div>`;
  }).join("");
  host.querySelectorAll("circle.gpt").forEach(attachDrag);
  updateGraphs();
}
// Redraws the moving parts in place. Never rebuilds the DOM, because a rebuild
// mid-drag would destroy the element holding the pointer capture.
function updateGraphs(){
  const host=$("#graphs");
  if(!host||!CFG||!SEQ.keyframes.length)return;
  const T=seqTimes(), total=DRAG?DRAG.total:T.total, W=graphW();
  const curIdx=(ST&&ST.sequencer&&ST.sequencer.state!=="idle")?ST.sequencer.index:-1;
  liveAxes().forEach(a=>{
    const path=document.getElementById("gpath"+a);
    if(!path)return;
    const S=scales(a,total,W);
    path.setAttribute("d",axisPath(a,T,S));
    const set=(id,at,v)=>{const e=document.getElementById(id);if(e)e.setAttribute(at,v);};
    const txt=(id,v)=>{const e=document.getElementById(id);if(e)e.textContent=v;};
    txt("gyh"+a,fx(S.hi,1)); txt("gym"+a,fx((S.hi+S.lo)/2,1)); txt("gyl"+a,fx(S.lo,1));
    txt("gxt"+a,fx(T.total,2)+"s");
    if(ST&&ST.axes&&ST.axes[a]){
      const y=Math.max(S.y0,Math.min(S.y1,S.Y(ST.axes[a].pos)));
      set("glv"+a,"y1",y); set("glv"+a,"y2",y);
    }
    SEQ.keyframes.forEach((k,i)=>{
      const x=S.X(T.arrive[i]), y=S.Y(k.pos[a]);
      set("gk"+a+"_"+i,"x1",x); set("gk"+a+"_"+i,"x2",x);
      const c=document.getElementById("gc"+a+"_"+i);
      if(c){c.setAttribute("cx",x);c.setAttribute("cy",y);
        c.classList.toggle("cur",i===curIdx);}
    });
    if(!DRAG||DRAG.ai!==a)txt("gv"+a,"");
  });
  // The table's numbers and the graph are two views of the same array.
  if(DRAG)syncRowInputs(DRAG.i);
}
function syncRowInputs(i){
  const row=$('#seqTable tr[data-row="'+i+'"]');
  if(!row)return;
  row.querySelectorAll("input[data-kf]").forEach(el=>{
    const c=el.dataset.col;
    if(c==="d")el.value=fx(SEQ.keyframes[i].duration_s,2);
    else if(c==="h")el.value=fx(SEQ.keyframes[i].hold_s,2);
    else el.value=fx(shownVal(i,+c));
  });
}
function attachDrag(circle){
  circle.addEventListener("pointerdown",ev=>{
    ev.preventDefault();
    const svg=circle.ownerSVGElement;
    const a=+circle.dataset.ax, i=+circle.dataset.kf;
    const T=seqTimes();
    DRAG={ai:a,i,total:T.total};
    try{circle.setPointerCapture(ev.pointerId);}catch(err){}
    circle.style.cursor="grabbing";

    const move=e=>{
      const r=svg.getBoundingClientRect();
      const k=r.width?svg.width.baseVal.value/r.width:1;   // survives page zoom
      const px=(e.clientX-r.left)*k, py=(e.clientY-r.top)*k;
      const S=scales(a,DRAG.total,svg.width.baseVal.value);
      setPos(i,a,S.invY(py));
      // Horizontal drag retimes the leg *into* this keyframe. The first
      // keyframe has no incoming leg, so it stays pinned at t=0.
      if(i>0){
        const t=seqTimes();
        SEQ.keyframes[i].duration_s=Math.max(0.05,S.invX(px)-t.depart[i-1]);
      }
      const v=$("#gv"+a);
      if(v)v.textContent=fx(shownVal(i,a))+" "+CFG.axes[a].units+
        (i>0?"  ·  "+fx(SEQ.keyframes[i].duration_s,2)+"s":"");
      updateGraphs();
    };
    const up=e=>{
      circle.style.cursor="";
      try{circle.releasePointerCapture(ev.pointerId);}catch(err){}
      circle.removeEventListener("pointermove",move);
      circle.removeEventListener("pointerup",up);
      circle.removeEventListener("pointercancel",up);
      DRAG=null;
      renderSeq();   // re-lays the table, then re-renders the graphs at the new time scale
    };
    circle.addEventListener("pointermove",move);
    circle.addEventListener("pointerup",up);
    circle.addEventListener("pointercancel",up);
  });
}
let resizeTimer=null;
window.addEventListener("resize",()=>{
  clearTimeout(resizeTimer);
  resizeTimer=setTimeout(()=>{
    if($("#sequence").classList.contains("on"))renderGraphs();
    if($("#curves").classList.contains("on"))renderCurveGraphs();
  },150);
});

/* ---------- curve sequences ----------
   A sequence is one independent track per axis on a shared clock, so unlike a
   keyframe (a pose every axis reaches together) the channels here need not
   share key times or even key counts. Between two keys the axis follows a
   cubic Bezier whose handles are stored normalised to that leg -- x as a
   fraction of its duration, y as a fraction of its displacement -- which is
   the same parameterisation the firmware evaluates, so what is drawn here is
   what the rig runs. */
let CUR=null, SLOTS=[], curSel=null, curDirty=false, curPushTimer=null, curDrag=null;

const clamp=(v,lo,hi)=>v<lo?lo:(v>hi?hi:v);
function chan(a){return CUR&&CUR.channels?CUR.channels[a]:null;}
function curveDuration(){
  let d=0;
  if(CUR&&CUR.channels)CUR.channels.forEach(c=>{if(c.keys.length)d=Math.max(d,c.keys[c.keys.length-1].t);});
  return d;
}
function cBez(p0,p1,p2,p3,u){const m=1-u;
  return m*m*m*p0+3*m*m*u*p1+3*m*u*u*p2+u*u*u*p3;}
// Control points of one leg in real (seconds, units) space.
function cLeg(k0,k1){
  const dt=k1.t-k0.t, dv=k1.v-k0.v;
  return {t0:k0.t,t1:k1.t,v0:k0.v,v1:k1.v,dt,dv,
    cx1:k0.t+clamp(k0.out_x,0,1)*dt, cy1:k0.v+k0.out_y*dv,
    cx2:k1.t-clamp(k1.in_x,0,1)*dt,  cy2:k1.v-k1.in_y*dv};
}
// Bezier parameter is not wall time: solve Bx(u)=t. Bx is monotonic because
// the handle x fractions are clamped into [0,1], so Newton with a bisection
// guard always lands -- the same trick browsers use for cubic-bezier().
function cUAtTime(l,t){
  if(l.dt<=1e-4)return 1;
  let u=(t-l.t0)/l.dt, lo=0, hi=1;
  for(let i=0;i<12;i++){
    const x=cBez(l.t0,l.cx1,l.cx2,l.t1,u), err=x-t;
    if(Math.abs(err)<1e-5)break;
    if(err>0)hi=u;else lo=u;
    const m=1-u;
    const dx=3*m*m*(l.cx1-l.t0)+6*m*u*(l.cx2-l.cx1)+3*u*u*(l.t1-l.cx2);
    u=Math.abs(dx)>1e-6?u-err/dx:(lo+hi)/2;
    if(u<lo||u>hi)u=(lo+hi)/2;
  }
  return clamp(u,0,1);
}
function cValueAt(a,t){
  const c=chan(a); if(!c||!c.keys.length)return 0;
  const k=c.keys;
  if(k.length===1||t<=k[0].t)return k[0].v;
  if(t>=k[k.length-1].t)return k[k.length-1].v;
  let i=0; while(i+1<k.length&&k[i+1].t<=t)i++;
  const l=cLeg(k[i],k[i+1]);
  return cBez(l.v0,l.cy1,l.cy2,l.v1,cUAtTime(l,t));
}
// Peak demand, sampled the same way the firmware samples it.
function cPeaks(a){
  const c=chan(a); let ps=0, pa=0;
  if(!c)return {speed:0,accel:0};
  for(let i=0;i+1<c.keys.length;i++){
    const l=cLeg(c.keys[i],c.keys[i+1]);
    if(l.dt<=1e-4)continue;
    const n=48, dt=l.dt/n;
    let pv=cValueAt(a,l.t0), pspd=0;
    for(let s=1;s<=n;s++){
      const v=cValueAt(a,l.t0+dt*s), spd=Math.abs(v-pv)/dt;
      if(spd>ps)ps=spd;
      if(s>1){const ac=Math.abs(spd-pspd)/dt; if(ac>pa)pa=ac;}
      pspd=spd; pv=v;
    }
  }
  return {speed:ps,accel:pa};
}
function curveProblems(){
  const out=[];
  if(!CUR||!CFG)return out;
  CFG.axes.forEach((c,a)=>{
    const ch=chan(a); if(!ch||ch.keys.length<2)return;
    if(!c.enabled){out.push({a,msg:c.name+" has points but the axis is disabled"});return;}
    const p=cPeaks(a);
    if(p.speed>c.max_speed*1.02)
      out.push({a,msg:c.name+" needs "+fx(p.speed,1)+" "+c.units+"/s but tops out at "+fx(c.max_speed,1)});
    else if(p.accel>c.accel*1.05)
      out.push({a,msg:c.name+" needs "+fx(p.accel,0)+" "+c.units+"/s² but is limited to "+fx(c.accel,0)});
  });
  return out;
}

/* ---- slots ---- */
async function loadCurves(){
  try{
    SLOTS=(await api("/api/curves")).slots||[];
    CUR=await api("/api/curve");
    curSel=null; markCurveClean();
    renderCurveSlots(); renderCurveGraphs();
  }catch(e){toast(e.message,true);}
}
function renderCurveSlots(){
  const active=CUR?CUR.active_slot:-1;
  $("#slotSel").innerHTML=SLOTS.map(s=>
    `<option value="${s.slot}" ${s.slot===active?"selected":""}>${
      s.slot+1}. ${s.empty?"— empty —":esc(s.name)}</option>`).join("");
}
function markCurveDirty(){curDirty=true;
  $("#curveDirty").textContent="unsaved"; $("#curveDirty").className="dirty";}
function markCurveClean(){curDirty=false;
  $("#curveDirty").textContent=CUR&&CUR.active_slot>=0?"saved":"not saved to a slot";
  $("#curveDirty").className="dirty clean";}

// Every edit is pushed to the firmware (debounced) so Play and scrub always
// run what is on screen; the Save button is what makes it survive a reboot.
function pushCurve(){
  clearTimeout(curPushTimer);
  curPushTimer=setTimeout(async()=>{
    try{await api("/api/curve","POST",curveBody(false));}
    catch(e){toast(e.message,true);}
  },350);
}
function curveBody(save){
  return {name:CUR.name,save:!!save,
    channels:(CUR.channels||[]).map(c=>({axis:c.axis,keys:c.keys}))};
}
async function selectSlot(slot){
  if(curDirty&&!confirm("Discard unsaved changes to this sequence?")){renderCurveSlots();return;}
  try{await api("/api/curve/select","POST",{slot});await loadCurves();toast("loaded");}
  catch(e){toast(e.message,true);}
}
function firstEmptySlot(){const s=SLOTS.find(x=>x.empty);return s?s.slot:-1;}
async function saveCurve(){
  let slot=CUR.active_slot;
  if(slot<0){slot=firstEmptySlot();
    if(slot<0){toast("all 16 slots are full — delete one first",true);return;}}
  await doSaveCurve(slot,CUR.name);
}
async function saveCurveAs(){
  const slot=firstEmptySlot();
  if(slot<0){toast("all 16 slots are full — delete one first",true);return;}
  const name=prompt("Name for this sequence:",CUR.name||("Sequence "+(slot+1)));
  if(name===null)return;
  await doSaveCurve(slot,name);
}
async function doSaveCurve(slot,name){
  try{
    CUR.name=name||CUR.name;
    await api("/api/curve","POST",curveBody(false));
    await api("/api/curve/save","POST",{slot,name:CUR.name});
    await new Promise(r=>setTimeout(r,200));   // let loop() write the file
    await loadCurves();
    toast("saved to slot "+(slot+1));
  }catch(e){toast(e.message,true);}
}
async function renameCurve(){
  const name=prompt("Rename this sequence:",CUR.name);
  if(name===null||!name.trim())return;
  if(CUR.active_slot<0){CUR.name=name.trim();markCurveDirty();pushCurve();return;}
  await doSaveCurve(CUR.active_slot,name.trim());
}
async function newCurve(){
  if(curDirty&&!confirm("Discard unsaved changes to this sequence?"))return;
  try{await api("/api/curve/new","POST");await new Promise(r=>setTimeout(r,150));
    await loadCurves();}catch(e){toast(e.message,true);}
}
async function deleteCurve(){
  const slot=CUR.active_slot;
  if(slot<0){toast("this sequence is not saved to a slot",true);return;}
  if(!confirm("Delete \""+CUR.name+"\" from slot "+(slot+1)+"?"))return;
  try{await api("/api/curve/delete","POST",{slot});
    await new Promise(r=>setTimeout(r,150));await loadCurves();}
  catch(e){toast(e.message,true);}
}
function scrubTo(v){
  const d=curveDuration();
  post("/api/curve/goto",{t:d*(+v/1000)});
}

/* ---- editing ---- */
function keyLimits(a,i){
  // Keys stay strictly ordered: the firmware walks them in order and the
  // Bezier solve needs a positive span on every leg.
  const k=chan(a).keys;
  return [i>0?k[i-1].t+0.05:0, i+1<k.length?k[i+1].t-0.05:Math.max(k[i].t+30,60)];
}
function addKeyHere(){
  if(!CUR){return;}
  const a=curSel?curSel.a:(liveAxes()[0]);
  if(a===undefined){toast("no axes are enabled",true);return;}
  const c=chan(a), d=curveDuration();
  // With a point selected, drop the new one midway to its neighbour; with
  // nothing selected, extend the track by a couple of seconds.
  let t;
  if(curSel&&curSel.i+1<c.keys.length)t=(c.keys[curSel.i].t+c.keys[curSel.i+1].t)/2;
  else if(c.keys.length)t=c.keys[c.keys.length-1].t+Math.max(2,d*0.25);
  else t=0;
  insertKey(a,t,null);
}
// Inserting on an existing leg samples the curve, then matches the new point's
// handles to the local tangent -- so adding a point does not change the move
// the rig will make, which matters when you are refining a take, not authoring
// a new one.
function insertKey(a,t,vOverride){
  const c=chan(a);
  if(!c)return;
  if(c.keys.length>=(CUR.max_keys||16)){
    toast(CFG.axes[a].name+" is at its "+(CUR.max_keys||16)+"-point limit",true);
    return;
  }
  t=Math.max(0,t);
  const onCurve=c.keys.length>=2&&t>c.keys[0].t&&t<c.keys[c.keys.length-1].t;
  let v=vOverride;
  if(v===null||v===undefined)v=onCurve?cValueAt(a,t):(c.keys.length?c.keys[c.keys.length-1].v:0);
  const lim=axLimits(a); if(lim)v=clamp(v,lim[0],lim[1]);
  const k={t,v,out_x:0.33,out_y:0,in_x:0.33,in_y:0};
  if(onCurve){
    // Local slope, in units per second, from a short central difference.
    const h=Math.min(0.05,Math.max(1e-3,curveDuration()*0.002));
    const slope=(cValueAt(a,t+h)-cValueAt(a,t-h))/(2*h);
    let idx=0; while(idx<c.keys.length&&c.keys[idx].t<t)idx++;
    const prev=c.keys[idx-1], next=c.keys[idx];
    if(next){const dt=next.t-t, dv=next.v-v;
      k.out_y=Math.abs(dv)>1e-6?clamp(slope*0.33*dt/dv,-2,3):0;}
    if(prev){const dt=t-prev.t, dv=v-prev.v;
      k.in_y=Math.abs(dv)>1e-6?clamp(slope*0.33*dt/dv,-2,3):0;}
  }
  c.keys.push(k);
  c.keys.sort((x,y)=>x.t-y.t);
  curSel={a,i:c.keys.indexOf(k)};
  markCurveDirty(); pushCurve(); renderCurveGraphs();
}
function delKey(){
  if(!curSel){toast("select a point first",true);return;}
  const c=chan(curSel.a);
  c.keys.splice(curSel.i,1);
  curSel=null;
  markCurveDirty(); pushCurve(); renderCurveGraphs();
}
function resetHandles(){
  if(!CUR)return;
  const axes=curSel?[curSel.a]:liveAxes();
  axes.forEach(a=>chan(a).keys.forEach(k=>{
    k.out_x=0.33;k.out_y=0;k.in_x=0.33;k.in_y=0;}));
  markCurveDirty(); pushCurve(); renderCurveGraphs();
  toast(curSel?"curve reset for "+CFG.axes[curSel.a].name:"all curves reset");
}

/* ---- drawing ---- */
function cScales(a,total,W){
  const lim=axLimits(a);
  let lo,hi;
  if(lim){[lo,hi]=lim;}
  else{
    const vals=(chan(a)?chan(a).keys.map(k=>k.v):[]);
    lo=vals.length?Math.min.apply(null,vals):-1;
    hi=vals.length?Math.max.apply(null,vals):1;
    if(hi-lo<1e-6){lo-=1;hi+=1;}
    const pad=(hi-lo)*0.15; lo-=pad; hi+=pad;
  }
  const x0=G.L,x1=W-G.R,y0=G.T,y1=G.H-G.B, span=total>0?total:1;
  return {lo,hi,x0,x1,y0,y1,span,
    X:t=>x0+(x1-x0)*(t/span), Y:v=>y1-(y1-y0)*((v-lo)/((hi-lo)||1)),
    invX:px=>clamp((px-x0)/((x1-x0)||1)*span,0,span*4),
    invY:py=>lo+(y1-py)/((y1-y0)||1)*(hi-lo)};
}
function cPath(a,S){
  const c=chan(a); if(!c||c.keys.length<2)return "";
  let d="M"+S.X(c.keys[0].t).toFixed(1)+","+S.Y(c.keys[0].v).toFixed(1);
  for(let i=0;i+1<c.keys.length;i++){
    const l=cLeg(c.keys[i],c.keys[i+1]);
    // Sampling the Bezier directly in (t,v) needs no cubic solve, and 18
    // segments is well below the point where a leg looks faceted.
    for(let s=1;s<=18;s++){const u=s/18;
      d+="L"+S.X(cBez(l.t0,l.cx1,l.cx2,l.t1,u)).toFixed(1)+","+
             S.Y(cBez(l.v0,l.cy1,l.cy2,l.v1,u)).toFixed(1);}
  }
  return d;
}
// Each chart sits inside its own card, so the drawable width is the card's
// content box, not the container's. Measured from a real .gscroll rather than
// derived from padding, which would silently drift if the CSS changed.
let curveWpx=0;
function curveW(){
  const g=$("#curveGraphs .gscroll");
  if(g&&g.clientWidth>40)curveWpx=g.clientWidth;
  const h=$("#curveGraphs");
  return Math.max(320,curveWpx||(h&&h.clientWidth)||640);
}
function renderCurveGraphs(remeasured){
  const host=$("#curveGraphs");
  if(!host||!CFG||!CUR)return;
  const live=liveAxes();
  if(!live.length){host.innerHTML='<div class="card"><p class="hint">No axes are enabled.</p></div>';return;}
  const total=Math.max(curveDuration(),1), W=curveW();
  host.innerHTML=live.map(a=>{
    const c=CFG.axes[a], ch=chan(a)||{keys:[]}, S=cScales(a,total,W);
    const pts=ch.keys.map((k,i)=>
      `<circle class="gpt" id="ck${a}_${i}" data-ax="${a}" data-key="${i}" r="5" cx="0" cy="0"/>`).join("");
    return `<div class="card" style="padding:10px 12px">
      <div class="ghead"><b>${esc(c.name)}</b>
        <span>${c.kind} · ${c.units}</span>
        <span class="gval" id="cv${a}">${ch.keys.length} point${ch.keys.length===1?"":"s"}</span></div>
      <div class="gscroll"><svg class="graph" data-cax="${a}" width="${W}" height="${G.H}"
        viewBox="0 0 ${W} ${G.H}">
        <rect class="gbg" id="cbg${a}" x="${S.x0}" y="${S.y0}"
          width="${S.x1-S.x0}" height="${S.y1-S.y0}"/>
        <line class="ggrid" x1="${S.x0}" x2="${S.x1}"
          y1="${(S.y0+S.y1)/2}" y2="${(S.y0+S.y1)/2}"/>
        <text class="gtick" id="cyh${a}" x="${S.x0-6}" y="${S.y0+9}" text-anchor="end"></text>
        <text class="gtick" id="cym${a}" x="${S.x0-6}" y="${(S.y0+S.y1)/2+4}" text-anchor="end"></text>
        <text class="gtick" id="cyl${a}" x="${S.x0-6}" y="${S.y1}" text-anchor="end"></text>
        <text class="gtick" x="${S.x0}" y="${G.H-6}">0s</text>
        <text class="gtick" id="cxt${a}" x="${S.x1}" y="${G.H-6}" text-anchor="end"></text>
        ${ch.keys.length?"":`<text class="gempty" x="${(S.x0+S.x1)/2}" y="${(S.y0+S.y1)/2}"
          text-anchor="middle">double-click to add the first point</text>`}
        <line class="glive" id="clv${a}" x1="${S.x0}" x2="${S.x1}" y1="0" y2="0"/>
        <path class="gcurve" id="cpath${a}" d=""/>
        <g id="chandles${a}"></g>
        ${pts}
        <line class="gplay" id="cph${a}" y1="${S.y0}" y2="${S.y1}" x1="-9" x2="-9"/>
      </svg></div></div>`;
  }).join("");
  // First render on a fresh tab has no .gscroll to measure, so it can be off
  // by the card's padding. Measure the real one and redraw once if so.
  if(!remeasured){
    const g=host.querySelector(".gscroll");
    if(g&&Math.abs(g.clientWidth-W)>2&&g.clientWidth>40){
      curveWpx=g.clientWidth;
      renderCurveGraphs(true);
      return;
    }
  }
  live.forEach(a=>{
    const svg=host.querySelector('svg[data-cax="'+a+'"]');
    svg.addEventListener("dblclick",ev=>{
      const S=cScales(a,Math.max(curveDuration(),1),svg.width.baseVal.value);
      const r=svg.getBoundingClientRect(), k=r.width?svg.width.baseVal.value/r.width:1;
      const px=(ev.clientX-r.left)*k, py=(ev.clientY-r.top)*k;
      if(px<S.x0-8)return;
      const t=S.invX(px);
      const ch=chan(a);
      const onCurve=ch.keys.length>=2&&t>ch.keys[0].t&&t<ch.keys[ch.keys.length-1].t;
      insertKey(a,t,onCurve?null:S.invY(py));
    });
    svg.querySelectorAll("circle.gpt").forEach(attachKeyDrag);
  });
  updateCurveGraphs();
}
function updateCurveGraphs(){
  if(!CUR||!CFG||!$("#curveGraphs"))return;
  const total=Math.max(curveDuration(),1), W=curveW();
  const probs=curveProblems(), bad=new Set(probs.map(p=>p.a));
  const set=(id,at,v)=>{const e=document.getElementById(id);if(e)e.setAttribute(at,v);};
  const txt=(id,v)=>{const e=document.getElementById(id);if(e)e.textContent=v;};
  const playT=(ST&&ST.curve)?ST.curve.t:0;
  const playing=(ST&&ST.curve&&ST.curve.state!=="idle");

  liveAxes().forEach(a=>{
    const path=document.getElementById("cpath"+a); if(!path)return;
    const S=cScales(a,total,W), ch=chan(a)||{keys:[]};
    path.setAttribute("d",cPath(a,S));
    path.classList.toggle("over",bad.has(a));
    txt("cyh"+a,fx(S.hi,1)); txt("cym"+a,fx((S.hi+S.lo)/2,1)); txt("cyl"+a,fx(S.lo,1));
    txt("cxt"+a,fx(total,2)+"s");
    if(ST&&ST.axes&&ST.axes[a]){
      const y=clamp(S.Y(ST.axes[a].pos),S.y0,S.y1);
      set("clv"+a,"y1",y); set("clv"+a,"y2",y);
    }
    const ph=S.X(clamp(playT,0,total));
    set("cph"+a,"x1",playing?ph:-9); set("cph"+a,"x2",playing?ph:-9);
    ch.keys.forEach((k,i)=>{
      const c=document.getElementById("ck"+a+"_"+i);
      if(c){c.setAttribute("cx",S.X(k.t));c.setAttribute("cy",S.Y(k.v));
        c.classList.toggle("sel",!!curSel&&curSel.a===a&&curSel.i===i);}
    });
    const g=document.getElementById("chandles"+a);
    if(g)g.innerHTML=(curSel&&curSel.a===a)?handleMarkup(a,curSel.i,S):"";
  });
  if(curSel){
    const k=chan(curSel.a).keys[curSel.i];
    if(k)txt("cv"+curSel.a,"t "+fx(k.t,2)+"s · "+fx(k.v)+" "+CFG.axes[curSel.a].units);
  }
  const w=$("#curveWarn");
  w.classList.toggle("on",probs.length>0);
  w.innerHTML=probs.map(p=>esc(p.msg)).join("<br>")+
    (probs.length?"<br><span style='opacity:.8'>Lengthen the leg, flatten the handles, or raise the axis limit under Axes.</span>":"");
  txt("curveClock",fx(playT,2)+" / "+fx(curveDuration(),2)+" s");
  const se=$("#curveState");
  if(se&&ST&&ST.curve){se.textContent=ST.curve.state;
    se.className="tag"+(ST.curve.state==="playing"?" ok":ST.curve.state==="paused"?" warn":"");}
  const sc=$("#curveScrub");
  if(sc&&!sc.matches(":active")&&curveDuration()>0)
    sc.value=Math.round(clamp(playT/curveDuration(),0,1)*1000);
}
// The two handles of the selected key, drawn only for that key so the charts
// stay readable when a track has a dozen points.
function handleMarkup(a,i,S){
  const ch=chan(a), k=ch.keys[i];
  if(!k)return "";
  let out="";
  const grip=(id,x,y)=>`<line class="ghandle" x1="${S.X(k.t)}" y1="${S.Y(k.v)}" x2="${x}" y2="${y}"/>
    <rect class="ggrip" id="${id}" data-ax="${a}" data-key="${i}" data-grip="${id.slice(0,3)}"
      x="${x-4}" y="${y-4}" width="8" height="8" rx="1.5"/>`;
  const next=ch.keys[i+1], prev=ch.keys[i-1];
  if(next){const dt=next.t-k.t, dv=next.v-k.v;
    out+=grip("outg"+a+"_"+i,S.X(k.t+clamp(k.out_x,0,1)*dt),S.Y(k.v+k.out_y*dv));}
  if(prev){const dt=k.t-prev.t, dv=k.v-prev.v;
    out+=grip("ing"+a+"_"+i,S.X(k.t-clamp(k.in_x,0,1)*dt),S.Y(k.v-k.in_y*dv));}
  return out;
}
function attachKeyDrag(circle){
  circle.addEventListener("pointerdown",ev=>{
    ev.preventDefault(); ev.stopPropagation();
    const svg=circle.ownerSVGElement;
    const a=+circle.dataset.ax, i=+circle.dataset.key;
    curSel={a,i};
    try{circle.setPointerCapture(ev.pointerId);}catch(e){}
    const total=Math.max(curveDuration(),1);
    let moved=false;
    const move=e=>{
      moved=true;
      const r=svg.getBoundingClientRect(), s=r.width?svg.width.baseVal.value/r.width:1;
      const S=cScales(a,total,svg.width.baseVal.value);
      const k=chan(a).keys[i];
      const [tMin,tMax]=keyLimits(a,i);
      k.t=clamp(S.invX((e.clientX-r.left)*s),tMin,tMax);
      const lim=axLimits(a);
      let v=S.invY((e.clientY-r.top)*s);
      k.v=lim?clamp(v,lim[0],lim[1]):v;
      markCurveDirty(); updateCurveGraphs();
    };
    const up=()=>{
      circle.removeEventListener("pointermove",move);
      circle.removeEventListener("pointerup",up);
      circle.removeEventListener("pointercancel",up);
      if(moved){pushCurve();renderCurveGraphs();}else{updateCurveGraphs();}
    };
    circle.addEventListener("pointermove",move);
    circle.addEventListener("pointerup",up);
    circle.addEventListener("pointercancel",up);
  });
  // Right-click removes a point without needing the keyboard, which is the
  // only way to do it on a tablet on set.
  circle.addEventListener("contextmenu",ev=>{
    ev.preventDefault();
    curSel={a:+circle.dataset.ax,i:+circle.dataset.key};
    delKey();
  });
}
// Handle grips are re-created on every update(), so their drag is bound once
// here by delegation rather than per element.
document.addEventListener("pointerdown",ev=>{
  const grip=ev.target.closest?ev.target.closest("rect.ggrip"):null;
  if(!grip)return;
  ev.preventDefault(); ev.stopPropagation();
  const svg=grip.ownerSVGElement;
  const a=+grip.dataset.ax, i=+grip.dataset.key, which=grip.dataset.grip;
  const total=Math.max(curveDuration(),1);
  const move=e=>{
    const r=svg.getBoundingClientRect(), s=r.width?svg.width.baseVal.value/r.width:1;
    const S=cScales(a,total,svg.width.baseVal.value);
    const ch=chan(a), k=ch.keys[i];
    const tp=S.invX((e.clientX-r.left)*s), vp=S.invY((e.clientY-r.top)*s);
    if(which==="out"){
      const n=ch.keys[i+1]; if(!n)return;
      const dt=n.t-k.t, dv=n.v-k.v;
      k.out_x=clamp(dt>1e-6?(tp-k.t)/dt:0.33,0,1);
      k.out_y=Math.abs(dv)>1e-6?clamp((vp-k.v)/dv,-2,3):0;
    }else{
      const p=ch.keys[i-1]; if(!p)return;
      const dt=k.t-p.t, dv=k.v-p.v;
      k.in_x=clamp(dt>1e-6?(k.t-tp)/dt:0.33,0,1);
      k.in_y=Math.abs(dv)>1e-6?clamp((k.v-vp)/dv,-2,3):0;
    }
    markCurveDirty(); updateCurveGraphs();
  };
  const up=()=>{
    window.removeEventListener("pointermove",move);
    window.removeEventListener("pointerup",up);
    pushCurve();
  };
  window.addEventListener("pointermove",move);
  window.addEventListener("pointerup",up);
});

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
  const onCurves=$("#curves").classList.contains("on");
  if(e.code==="Space"){e.preventDefault();
    post(onCurves?"/api/curve/play":"/api/sequence/play");}
  if(e.code==="Escape"){post("/api/estop");}
  if(onCurves&&curSel&&(e.code==="Delete"||e.code==="Backspace")){
    e.preventDefault(); delKey();
  }
});
(async()=>{
  try{ACTIONS=await api("/api/actions");}catch(e){ACTIONS=["none"];}
  await loadConfig();
  await loadSeq();
  await loadCurves();
  connect();
})();
</script></body></html>
)HTMLDOC";
