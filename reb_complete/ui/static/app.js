
let schema       = [];
let presets      = {};
let meta         = {};
let currentInputs = {};
let lastToken    = null;
let selectedCycles = 1;
let turboDelay   = 0;
let turboRunning = false;
let speedInicial = 0;

const $  = id => document.getElementById(id);
const esc = s => String(s).replace(/[&<>"']/g, m =>
  ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[m]));
const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
const boolTxt = v => v ? "ON" : "OFF";

/**
 * @constant {Object} EVT_NAMES
 * @description Maps numeric event kind codes to their symbolic names as defined in requirements.
 */
const EVT_NAMES = {
  1:"STATE_TRANSITION", 2:"AUTH_FAIL", 3:"AUTH_OK", 4:"PANEL_LOCKOUT",
  5:"SENSOR_THEFT", 6:"DERATE_ACTIVE", 7:"STARTER_INHIBIT", 8:"UNBLOCK",
  9:"REVERSAL_ABORT", 10:"REVERSAL_EXPIRE", 11:"NVM_WRITE", 12:"NVM_RESTORE",
  13:"SIGNAL_FAULT", 14:"CMD_RECEIVED", 15:"SPEED_SAFE_STOP",
  16:"BLOCK_REJECT_SIGNAL", 17:"BLOCK_REJECT_SPEED", 18:"RX_SUPERVISION_FAIL",
};

/**
 * @constant {number} V_STOP_KMH - Speed threshold below which the vehicle is considered stopped (km/h).
 * @constant {number} FUEL_FLOOR - Minimum fuel derating floor percentage.
 * @constant {number} T_PARKED_S - Dwell timer duration in seconds before transitioning to BLOCKED.
 * @constant {number} T_REVERSAL_S - Reversal window duration in seconds (THEFT_CONFIRMED state).
 * @constant {number} DERATE_RATE - Fuel derating decrement rate in percent per second.
 * @constant {number} TS_S - Simulation time step in seconds per cycle.
 * @constant {number} V_MAX_FLOOR_KMH - Maximum sustainable speed at the fuel floor (physical estimate).
 * @constant {number} DECEL_FLOOR_RATE - Speed decrement per cycle when operating at fuel floor (km/h/cycle).
 */
const V_STOP_KMH    = 0.5;
const FUEL_FLOOR    = 10;
const T_PARKED_S    = 120;
const T_REVERSAL_S  = 60;
const DERATE_RATE   = 0.75;
const TS_S          = 0.010;
const V_MAX_FLOOR_KMH = 30;
const DECEL_FLOOR_RATE = 0.005;

/** @type {string} Tracks the FSM state from the previous render cycle. */
let lastState = "IDLE";

/**
 * @function updateFsm
 * @description Highlights the active FSM node in the UI and updates state-related badges and sub-timers.
 * @param {Object} out - Simulator output object returned by the backend step API.
 */
function updateFsm(out) {
  const st = out.state || "IDLE";
  const STATES = ["IDLE","THEFT_CONFIRMED","BLOCKING","BLOCKED"];
  STATES.forEach(s => {
    const el = $(`fsm-${s}`);
    if (!el) return;
    el.className = "fsm-node";
    if (s === st) {
      el.classList.add("active");
      if (s !== "IDLE") el.classList.add(`active-${s}`);
    }
  });

  $("simTimeBadge").textContent = `t = ${out.sim_time_ms || 0} ms`;

  const srcMap = {0:"PANEL",1:"REMOTE",2:"AUTO"};
  $("sourceBadge").textContent = srcMap[out.source_trigger_out] || "—";
  $("phaseBadge").textContent  = out.block_phase || "—";

  const rvs = out.reversal_timer_s;
  $("subtimerTC").textContent = (st === "THEFT_CONFIRMED" && rvs > 0)
    ? ` ${rvs}s remaining` : "";

  const pt = out.parked_timer_s || 0;
  $("subtimerBK").textContent = (st === "BLOCKING")
    ? (out.block_phase === "PARKED" ? `Dwell: ${pt.toFixed(0)}s / 120s`
                                    : ` Derating`) : "";

  const sb = $("stateBadge");
  sb.textContent = st;
  sb.className   = `state-badge ${st}`;
}

/**
 * @function updateAlerts
 * @description Updates alert tiles in the UI according to current output flags (FR-013).
 * @param {Object} out - Simulator output object.
 */
function updateAlerts(out) {
  setTile("tileVisual",   "valVisual",   out.alert_visual,           "ON", "OFF");
  setTile("tileSonic",    "valSonic",    out.alert_sonic,            "ON", "OFF");
  setTile("tilePreBlock", "valPreBlock", out.pre_block_alert,        "ON","OFF", true);
  setTile("tileTheft",    "valTheft",    out.notify_theft,           "ON","OFF", true);
  setTile("tileBlocked",  "valBlocked",  out.notify_blocked,         "ON","OFF");
  setTile("tileStarter",  "valStarter",  out.starter_inhibit_active, "INHIBITED","FREE");
}

/**
 * @function setTile
 * @description Applies active or inactive styling and label to an alert tile element.
 * @param {string} id - Element ID of the tile container.
 * @param {string} valId - Element ID of the value label within the tile.
 * @param {boolean} active - Whether the alert condition is active.
 * @param {string} onTxt - Label text when active.
 * @param {string} offTxt - Label text when inactive.
 * @param {boolean} [warn] - If true, applies warning styling instead of standard active styling.
 */
function setTile(id, valId, active, onTxt, offTxt, warn) {
  const el = $(id), vl = $(valId);
  if (!el || !vl) return;
  el.className = "alert-tile" + (active ? (warn ? " on-warn" : " on") : "");
  vl.textContent = active ? onTxt : offTxt;
}

/**
 * @function updateSpeedMetrics
 * @description Computes and renders effective vehicle speed, derate level, reversal timer,
 *              dwell timer, starter status, channel status, and powertrain validity.
 *
 * Effective speed coupling rules per requirements:
 *  - Derating is only applied in BLOCKING state when speed exceeds V_STOP_KMH.
 *  - When effective speed reaches V_STOP_KMH or below, the dwell phase begins (PARKED).
 *  - In BLOCKED state, effective speed is forced to zero; starter inhibit takes over.
 *
 * @param {Object} out - Simulator output object.
 */
function updateSpeedMetrics(out) {
  const inputSpd  = parseFloat(currentInputs.vehicle_speed_kmh || 0);
  const derate    = parseInt(out.derate_pct || 100, 10);
  const st        = out.state || "IDLE";

  let effSpd;
  if (st === "BLOCKING" && inputSpd > V_STOP_KMH) {
    effSpd = inputSpd * (derate / 100);
  } else if (st === "BLOCKED") {
    effSpd = 0;
  } else {
    effSpd = inputSpd;
  }

  const MAX_SPD = Math.max(120, speedInicial + 10);
  setGauge($("lblSpeedReal"), $("barSpeedReal"), inputSpd, MAX_SPD, `${inputSpd.toFixed(1)} km/h`, 100);
  setGauge($("lblSpeedEff"),  $("barSpeedEff"),  effSpd,   MAX_SPD, `${effSpd.toFixed(1)} km/h`,  100);
  setGauge($("lblDerate"),    $("barDerate"),     derate,   100,     `${derate}%`,                  100);

  const vstopNote = $("noteVstop");
  if (vstopNote) vstopNote.classList.toggle("hidden", !(st === "BLOCKING" && effSpd <= V_STOP_KMH));

  const floorNote = $("noteFloor");
  if (floorNote) {
    const floorActive = st === "BLOCKING" && inputSpd > V_STOP_KMH && derate <= FUEL_FLOOR;
    floorNote.classList.toggle("hidden", !floorActive);
  }

  const rvs = out.reversal_timer_s || 0;
  const rvBlock = $("blockReversal");
  if (rvBlock) {
    rvBlock.style.display = (st === "THEFT_CONFIRMED") ? "" : "none";
    if (st === "THEFT_CONFIRMED") {
      setGauge($("lblReversal"), $("barReversal"), rvs, T_REVERSAL_S, `${rvs}s`, T_REVERSAL_S);
    }
  }

  const pt = out.parked_timer_s || 0;
  const ptPct = out.parked_timer_pct || 0;
  const dwBlock = $("blockDwell");
  if (dwBlock) {
    dwBlock.style.display = (st === "BLOCKING") ? "" : "none";
    if (st === "BLOCKING") {
      setGauge($("lblDwell"), $("barDwell"), pt, T_PARKED_S, `${pt.toFixed(0)}s / 120s`, T_PARKED_S);
    }
  }

  setPill($("starterBadge"), out.starter_inhibit_active ? "danger" : "ok",
          out.starter_inhibit_active ? "INHIBITED " : "RELEASED");

  const ch = out.channel_rx_ok;
  const chId = out.rx_channel_id || 0;
  const chTxt = ch ? (chId === 0 ? "4G OK" : chId === 1 ? "SMS (fallback)" : "—") : "OFFLINE";
  setPill($("channelBadge"), ch ? "ok" : "danger", chTxt);

  setPill($("pwtBadge"), out.powertrain_valid ? "ok" : "danger",
          out.powertrain_valid ? "VALID" : "INVALID");
}

/**
 * @function setGauge
 * @description Sets the width of a progress bar element and the text of a label element.
 * @param {HTMLElement} lblEl - Label element to update.
 * @param {HTMLElement} barEl - Progress bar element to update.
 * @param {number} value - Current value.
 * @param {number} max - Maximum value used to compute fill percentage.
 * @param {string} text - Display text for the label.
 */
function setGauge(lblEl, barEl, value, max, text) {
  if (barEl) barEl.style.width = `${clamp(value / max * 100, 0, 100)}%`;
  if (lblEl) lblEl.textContent = text;
}

/**
 * @function setPill
 * @description Applies a CSS class and text to a status pill element.
 * @param {HTMLElement} el - Target pill element.
 * @param {string} cls - CSS modifier class (e.g., "ok", "danger").
 * @param {string} txt - Display text.
 */
function setPill(el, cls, txt) {
  if (!el) return;
  el.className = `status-pill ${cls}`;
  el.textContent = txt;
}

/**
 * @function updateAntiReplay
 * @description Renders anti-replay validation results including acceptance status,
 *              rejection reason, last accepted nonce, and active receive channel.
 * @param {Object} out - Simulator output object containing an `anti_replay` field.
 */
function updateAntiReplay(out) {
  const ar  = out.anti_replay || {};
  const ok  = !!ar.ok;
  const el  = $("arStatus");
  el.textContent = ok ? "ACCEPTED" : "REJECTED";
  el.className   = `ar-status ${ok ? "ok" : "bad"}`;
  $("arReason").textContent  = ar.reason  || "—";
  $("arNonce").textContent   = ar.last_nonce != null ? ar.last_nonce : "—";
  const chId = out.rx_channel_id || 0;
  $("arChannel").textContent = chId === 0 ? "IP/4G" : chId === 1 ? "SMS" : "NONE";
}

/**
 * @function updatePanel
 * @description Renders the physical panel authentication attempt counter, lockout state,
 *              and per-attempt dot indicators.
 * @param {Object} out - Simulator output object.
 */
function updatePanel(out) {
  const cnt    = out.panel_attempt_count || 0;
  const locked = !!out.panel_locked_out;
  $("panelCnt").textContent = `${cnt} / 3`;
  $("panelLock").classList.toggle("hidden", !locked);
  for (let i = 0; i < 3; i++) {
    const d = $(`dot${i}`);
    if (!d) continue;
    d.classList.toggle("used",   i < cnt && !locked);
    d.classList.toggle("locked", locked && i < cnt);
  }
}

/**
 * @function updateButtons
 * @description Enables or disables action buttons based on the current FSM state,
 *              channel availability, and panel lockout condition.
 *              Also controls the speed input editability during BLOCKED and DERATING phases.
 * @param {Object} out - Simulator output object.
 */
function updateButtons(out) {
  const st     = out.state || "IDLE";
  const chOk   = !!out.channel_rx_ok;
  const locked = !!out.panel_locked_out;
  const lockIndicator = $("speedInicialLock");
  const lockSpeed = (st === "BLOCKED") ||
                    (st === "BLOCKING" && (out.block_phase === "DERATING"));
  const spd = document.getElementById("inputSpeedInicial");
  if (lockIndicator) lockIndicator.style.display = lockSpeed ? "inline" : "none";

  setBtnState($("btnRemoteBlock"),   st === "IDLE" && chOk,             " Remote Block",   "Remote Block");
  setBtnState($("btnSmsBlock"),      st === "IDLE",                     "SMS Block",       "SMS Block");
  setBtnState($("btnRemoteUnblock"), st !== "IDLE" && chOk,            "Remote Unblock",  "Remote Unblock");
  setBtnState($("btnPanelUnlock"),   st !== "IDLE" && !locked,         "Panel Unlock",    locked ? "Panel locked" : "Panel Unlock");
  setBtnState($("btnReplayAtk"),     true,                              "Replay Attack",   "");
  setBtnState($("btnFreshToken"),    true,                              "Fresh Token",     "");


  

  if (spd) {
    spd.disabled = lockSpeed;
    spd.classList.toggle("locked", lockSpeed);
  }

}

/**
 * @function setBtnState
 * @description Applies enabled or disabled state and corresponding label to a button element.
 * @param {HTMLElement} btn - Target button element.
 * @param {boolean} enabled - Whether the button should be interactive.
 * @param {string} onLabel - Label text when enabled.
 * @param {string} offLabel - Label text when disabled.
 * @param {boolean} [isSim] - If true, applies simulation-specific CSS classes.
 */
function setBtnState(btn, enabled, onLabel, offLabel, isSim) {
  if (!btn) return;
  btn.disabled = !enabled;
  btn.textContent = enabled ? onLabel : offLabel;
  if (isSim) {
    btn.classList.toggle("sim",    true);
    btn.classList.toggle("locked", !enabled);
  } else {
    btn.classList.toggle("ok",     enabled);
    btn.classList.toggle("locked", !enabled);
  }
}

/**
 * @constant {Set<string>} DANGER_KV - Output keys rendered with danger styling when truthy.
 * @constant {Set<string>} WARN_KV   - Output keys rendered with warning styling when truthy.
 * @constant {Set<string>} OK_KV     - Output keys rendered with success styling when truthy.
 */
const DANGER_KV = new Set(["starter_inhibit_active","blocked_flag","rx_fail","panel_locked_out"]);
const WARN_KV   = new Set(["fuel_derating_active","derating_active","pre_block_alert","notify_theft"]);
const OK_KV     = new Set(["starter_ok","channel_rx_ok","powertrain_valid"]);

/**
 * @function renderOutputs
 * @description Builds and injects the key-value output table into the DOM,
 *              applying conditional CSS classes based on output semantics.
 * @param {Object} out - Simulator output object.
 */
function renderOutputs(out) {
  const rows = [
    ["state",                out.state],
    ["source_trigger_out",   ({0:"PANEL",1:"REMOTE",2:"AUTO"}[out.source_trigger_out]) || out.source_trigger_out],
    ["derate_pct",           `${out.derate_pct}%`],
    ["starter_ok",           out.starter_ok],
    ["notify_theft",         out.notify_theft],
    ["notify_blocked",       out.notify_blocked],
    ["reversal_timer_s",     `${out.reversal_timer_s || 0}s`],
    ["parked_timer_s",       `${(out.parked_timer_s || 0).toFixed(1)}s`],
    ["alert_visual",         out.alert_visual],
    ["alert_sonic",          out.alert_sonic],
    ["gps_send",             out.gps_send],
    ["channel_rx_ok",        out.channel_rx_ok],
    ["rx_fail",              out.rx_fail],
    ["rx_channel_id",        out.rx_channel_id],
    ["panel_locked_out",     out.panel_locked_out],
    ["panel_attempt_count",  out.panel_attempt_count],
    ["powertrain_valid",     out.powertrain_valid],
    ["sensor_score",         out.sensor_score],
    ["nvm_state_loaded",     out.nvm_state_loaded],
  ];
  $("outputs").innerHTML = rows.map(([k,v]) => {
    let cls = "";
    if (DANGER_KV.has(k) && v) cls = "danger";
    else if (WARN_KV.has(k) && v) cls = "warn";
    else if (OK_KV.has(k)   && v) cls = "ok";
    return `<div class="kv-row ${cls}"><span>${esc(k)}</span><strong>${esc(v)}</strong></div>`;
  }).join("");
}


/**
 * @function renderLogs
 * @description Renders the event log list in reverse chronological order with per-event CSS classification.
 *              Also updates the NVM badge and NFR-INFO-001 compliance panel.
 * @param {Array<Object>} logs - Array of event log entries from the simulator output.
 */
function renderLogs(logs) {
  const badge = $("logCountBadge");
  if (badge) badge.textContent = `${logs ? logs.length : 0} events`;
  if (!logs || !logs.length) {
    $("logs").innerHTML = `<div class="muted" style="padding:8px">No events</div>`;
    return;
  }
  const kinds = new Set(logs.map(l => Number(l.kind)));
  
  const nvmBadge = $("nvmLoadedBadge");
  if (nvmBadge) nvmBadge.style.display = kinds.has(12) ? "" : "none";
  $("logs").innerHTML = [...logs].reverse().map(item => {
    const code = Number(item.kind);
    const name = EVT_NAMES[code] || `EVT_0x${code.toString(16).toUpperCase()}`;
    let cls = "log-entry";
    if (code===1)  cls+=" ev-state";
    if (code===2||code===4) cls+=" ev-auth";
    if (code===6)  cls+=" ev-derate";
    if (code===14) cls+=" ev-cmd";
    if (code===17) cls+=" ev-reject";
    if (code===13) cls+=" ev-fault";
    if (code===11||code===12) cls+=" ev-nvm";
    return `<div class="${cls}">
      <div class="log-head">
        <strong>${esc(name)}</strong>
        <span class="ts">${esc(item.ts_ms)} ms</span>
        ${item.state_from?`<span>${esc(item.state_from)} → ${esc(item.state_to)}</span>`:""}
      </div></div>`;
  }).join("");
}



/**
 * @function renderAll
 * @description Orchestrates a full UI refresh after each simulation step.
 *              Captures the initial speed on BLOCKING entry, applies derate-driven
 *              speed updates during the DERATING phase, resets ephemeral inputs on
 *              transition back to IDLE, and delegates to all sub-render functions.
 * @param {Object} out - Simulator output object.
 */
function renderAll(out) {
  window._lastOut = out;
  if (out.state === "BLOCKING" && lastState !== "BLOCKING") {
    speedInicial = parseFloat(currentInputs.vehicle_speed_kmh || 0);
  }



  if (out.state === "BLOCKING") {
    const derateRamp = parseFloat(out.derate_ramp || 100);
    if (out.block_phase === "DERATING") {
      if (derateRamp > FUEL_FLOOR) {
        let vNext = speedInicial * (derateRamp / 100);
        currentInputs.vehicle_speed_kmh = vNext <= V_STOP_KMH ? 0 : parseFloat(vNext.toFixed(4));
      } else if (speedInicial > 0) {
        const vAtual = parseFloat(currentInputs.vehicle_speed_kmh || 0);
        const vNext = Math.max(0, vAtual - DECEL_FLOOR_RATE);
        currentInputs.vehicle_speed_kmh = parseFloat(vNext.toFixed(4));
      }
    }


  }
  if (out.state === "IDLE" && lastState !== "IDLE") {
    currentInputs.accel_peak            = 0.0;
    currentInputs.glass_break_flag      = 0.0;
    currentInputs.auth_blocked_remote   = false;
    currentInputs.remote_unblock_remote = false;
    currentInputs.auth_block_automatic  = 0;
    currentInputs.auth_manual_out       = false;
    currentInputs.cancel_request        = false;
  }

  lastState = out.state || "IDLE";

  updateFsm(out);
  updateAlerts(out);
  updateSpeedMetrics(out);
  updateAntiReplay(out);
  updatePanel(out);
  updateButtons(out);
  renderOutputs(out);
  renderLogs(out.logs || []);
  $("modeHint").textContent = `Mode: ${out.mode || "AUTO"}`;
}

/**
 * @function buildInputs
 * @description Rebuilds the dynamic input form from the schema definition,
 *              grouping fields by their schema group and re-binding all input events.
 */
function buildInputs() {
  $("inputs").innerHTML = "";
  schema.forEach(grp => {
    const card = document.createElement("div");
    card.className = "group";
    card.innerHTML = `<h3>${grp.group}</h3>`;
    grp.fields.forEach(f => card.appendChild(renderField(f)));
    $("inputs").appendChild(card);
  });
  bindInputEvents();
}

/**
 * @function renderField
 * @description Dispatches field rendering to the appropriate widget factory based on field type.
 * @param {Object} f - Schema field descriptor containing name, type, and options.
 * @returns {HTMLElement} The constructed field widget element.
 */
function renderField(f) {
  const val = currentInputs[f.name] ?? f.default ?? 0;
  if (f.name === "ignition_state") return mkIgnition(f, val);
  if (f.type === "bool")   return mkBool(f, val);
  if (f.type === "select") return mkSelect(f, val);
  return mkNumber(f, val);
}

/**
 * @function mkBool
 * @description Creates a boolean toggle field widget using a range input (0–1).
 * @param {Object} f - Field descriptor.
 * @param {*} val - Current field value.
 * @returns {HTMLElement} Field widget element.
 */
function mkBool(f, val) {
  const w = document.createElement("div"); w.className = "field";
  w.innerHTML = `<label>${f.label}<span class="pill" id="${f.name}_pill">${boolTxt(!!val)}</span></label>
    <input type="range" min="0" max="1" step="1" value="${val?1:0}" data-name="${f.name}" data-type="bool">`;
  return w;
}

/**
 * @function mkNumber
 * @description Creates a numeric input field widget.
 * @param {Object} f - Field descriptor, including optional step attribute.
 * @param {*} val - Current field value.
 * @returns {HTMLElement} Field widget element.
 */
function mkNumber(f, val) {
  const w = document.createElement("div"); w.className = "field";
  w.innerHTML = `<label>${f.label}</label>
    <input type="number" step="${f.step??1}" value="${val}" data-name="${f.name}" data-type="number">`;
  return w;
}

/**
 * @function mkSelect
 * @description Creates a dropdown select field widget from the field's options array.
 * @param {Object} f - Field descriptor containing an `options` array of {value, label} pairs.
 * @param {*} val - Current field value.
 * @returns {HTMLElement} Field widget element.
 */
function mkSelect(f, val) {
  const w = document.createElement("div"); w.className = "field";
  const opts = (f.options||[]).map(o =>
    `<option value="${o.value}" ${Number(o.value)===Number(val)?"selected":""}>${o.label}</option>`).join("");
  w.innerHTML = `<label>${f.label}</label><select data-name="${f.name}" data-type="select">${opts}</select>`;
  return w;
}

/**
 * @function mkIgnition
 * @description Creates a segmented button widget for the ignition state field.
 * @param {Object} f - Field descriptor containing an `options` array.
 * @param {*} val - Current field value.
 * @returns {HTMLElement} Field widget element.
 */
function mkIgnition(f, val) {
  const w = document.createElement("div"); w.className = "field";
  w.innerHTML = `<label>${f.label}</label>
    <div class="ignition-seg seg" data-name="${f.name}">
      ${(f.options||[]).map(o =>
        `<button type="button" class="seg-btn ${Number(o.value)===Number(val)?"active":""}" data-value="${o.value}">${o.label}</button>`
      ).join("")}
    </div>`;
  return w;
}

/**
 * @function bindInputEvents
 * @description Attaches change event listeners to all dynamically rendered input elements,
 *              synchronising their values into `currentInputs`.
 */
function bindInputEvents() {
  $("inputs").querySelectorAll("input[data-type='bool']").forEach(el =>
    el.addEventListener("input", () => {
      currentInputs[el.dataset.name] = el.value === "1";
      const p = document.getElementById(`${el.dataset.name}_pill`);
      if (p) p.textContent = boolTxt(currentInputs[el.dataset.name]);
    })
  );
  $("inputs").querySelectorAll("input[data-type='number']").forEach(el =>
    el.addEventListener("change", () => { currentInputs[el.dataset.name] = Number(el.value); })
  );
  $("inputs").querySelectorAll("select[data-type='select']").forEach(el =>
    el.addEventListener("change", () => { currentInputs[el.dataset.name] = Number(el.value); })
  );
  $("inputs").querySelectorAll(".ignition-seg .seg-btn").forEach(btn =>
    btn.addEventListener("click", () => {
      const host = btn.closest(".ignition-seg");
      currentInputs[host.dataset.name] = Number(btn.dataset.value);
      host.querySelectorAll(".seg-btn").forEach(b => b.classList.toggle("active", b === btn));
    })
  );
 
  ["speed_sig_status","tcu_ack","ip_rx_ok","sms_rx_ok","cmd_sig_ok"].forEach(function(fieldName) {
    const el = document.querySelector(`[data-name="${fieldName}"]`);
    if (el) el.addEventListener("change", function() { _onValidatorChange(); });
    if (el) el.addEventListener("input",  function() { _onValidatorChange(); });
  });
  const VALIDATOR_KEYS = new Set(["speed_sig_status","tcu_ack","ip_rx_ok","sms_rx_ok","cmd_sig_ok"]);
  function _onValidatorChange() {
    try { localStorage.setItem("reb_inputs", JSON.stringify(currentInputs)); } catch(e) {}
    doStep().catch(() => {});
  }
}

/**
 * @function syncDOM
 * @description Reads all current DOM input values into `currentInputs`,
 *              ensuring the state is consistent before an API call.
 */
function syncDOM() {
  $("inputs").querySelectorAll("input[data-type='number'],select[data-type='select']")
    .forEach(el => { currentInputs[el.dataset.name] = Number(el.value); });
  $("inputs").querySelectorAll("input[data-type='bool']")
    .forEach(el => { currentInputs[el.dataset.name] = el.value === "1"; });
  $("inputs").querySelectorAll(".ignition-seg .seg-btn.active")
    .forEach(btn => { currentInputs[btn.closest(".ignition-seg").dataset.name] = Number(btn.dataset.value); });
}

/**
 * @function freshToken
 * @description Generates a fresh authentication token by incrementing the nonce and
 *              setting the appropriate block or unblock command flags in `currentInputs`.
 *              Publishes an ephemeral command pulse to localStorage for cross-tab synchronisation.
 * @param {boolean} [unblock=false] - If true, generates an unblock token; otherwise generates a block token.
 */
function freshToken(unblock = false) {
  const nonce = Number(currentInputs.cmd_nonce || 0) + 1;
  const sigOk = (currentInputs.cmd_sig_ok !== undefined) ? !!currentInputs.cmd_sig_ok : true;
  lastToken = { cmd_nonce: nonce, cmd_timestamp_ms: Number(currentInputs.sim_time_ms || 0), cmd_sig_ok: sigOk };
  Object.assign(currentInputs, lastToken, {
    auth_blocked_remote:   !unblock,
    remote_unblock_remote: unblock,
  });
  const _pulse = unblock ? {remote_unblock_remote:true} : {auth_blocked_remote:true};
  try { localStorage.setItem("reb_cmd_pulse", JSON.stringify(Object.assign(_pulse,{ts:Date.now()}))); } catch(e) {}
}

/**
 * @function replayToken
 * @description Replays the last issued token to simulate a replay attack.
 *              If no token has been issued, generates a fresh one instead.
 */
function replayToken() {
  if (!lastToken) { freshToken(); return; }
  Object.assign(currentInputs, lastToken, {
     auth_blocked_remote: true, remote_unblock_remote: false,
  });
}

/**
 * @function apiStep
 * @description Sends current inputs to the backend `/api/step` endpoint and returns the output.
 *              Updates `sim_time_ms` and `cmd_nonce` from the response, and persists
 *              `currentInputs` to localStorage for cross-tab synchronisation.
 * @param {Object} [extra={}] - Additional input overrides merged into the request body.
 * @param {number|null} [cycles=null] - Number of simulation cycles; defaults to `selectedCycles`.
 * @returns {Promise<Object>} Simulator output object.
 */
async function apiStep(extra = {}, cycles = null) {
  const c = cycles ?? selectedCycles;
  const body = { inputs: { ...currentInputs, ...extra }, mode: "AUTO", cycles: c };
  const res  = await fetch("/api/step", {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify(body),
  });
  const out = await res.json();
  if (Number.isFinite(out.sim_time_ms)) currentInputs.sim_time_ms = out.sim_time_ms + 10;
  if (out.anti_replay?.last_nonce != null) {
    currentInputs.cmd_nonce        = Number(out.anti_replay.last_nonce) + 1;
    currentInputs.cmd_timestamp_ms = currentInputs.sim_time_ms;
  }
  try { localStorage.setItem("reb_inputs", JSON.stringify(currentInputs)); } catch(e) {}
  return out;
}

/**
 * @function apiPowerCycle
 * @description Issues a power-cycle request to the backend and returns the resulting output.
 * @returns {Promise<Object>} Simulator output object after power-cycle.
 */
async function apiPowerCycle() {
  const res = await fetch("/api/power_cycle", {
    method:"POST", headers:{"Content-Type":"application/json"}, body:"{}"
  });
  return await res.json();
}

/**
 * @function apiTestNfrRel001
 * @description Executes the NFR-REL-001 automated test via the backend test endpoint.
 * @returns {Promise<Object>} Test result object containing pass/fail status and step details.
 */
async function apiTestNfrRel001() {
  const res = await fetch("/api/test/nfr_rel_001", {
    method:"POST", headers:{"Content-Type":"application/json"}, body:"{}"
  });
  return await res.json();
}

/**
 * @function renderNfrRel001Result
 * @description Renders the NFR-REL-001 test result into the designated result container,
 *              including per-step pass/fail indicators and NVM restoration details.
 * @param {Object} r - Test result object from `apiTestNfrRel001`.
 */
function renderNfrRel001Result(r) {
  const el = $("nfrRel001Result");
  if (!el) return;
  el.style.display = "";
  const ok = r.pass === true;
  const color = ok ? "#22c55e" : "#ef4444";
  const stepsHtml = (r.steps||[]).map(s =>
    `<div class="kv-row ${s.ok?"ok":"danger"}" style="font-size:0.85em">
      <span><strong>Step ${esc(s.step)}</strong> — ${esc(s.desc)}</span>
      <strong>${s.ok?"PASS":"FAIL"} (${esc(s.state||"?")})</strong>
    </div>`
  ).join("");
  el.innerHTML = `
    <div style="padding:12px;border-radius:8px;border:2px solid ${color};background:${color}18">
      <div style="font-size:1.1em;font-weight:700;margin-bottom:8px;color:${color}">
        Post-Reset Recovery Test: ${ok ? "PASSED" : "FAILED"}
      </div>
      <div class="kv" style="margin-bottom:8px">
        <div class="kv-row"><span>State before power-cycle</span><strong>${esc(r.state_before_power_cycle||"?")}</strong></div>
        <div class="kv-row"><span>State after power-cycle</span><strong>${esc(r.state_after_power_cycle||"?")}</strong></div>
        <div class="kv-row"><span>NVM restored</span><strong>${r.nvm_state_loaded?"true":"false"}</strong></div>
        <div class="kv-row"><span>Starter inhibited after restore</span><strong>${r.starter_inhibit_after_reset?"true":"false"}</strong></div>
      </div>
      <div class="kv">${stepsHtml}</div>
    </div>`;
}

/**
 * @function doStep
 * @description Synchronises the DOM, executes one API step, rebuilds the input form,
 *              and triggers a full UI render.
 * @param {Object} [extra={}] - Additional input overrides for this step.
 * @param {number|null} [cycles=null] - Number of cycles to advance.
 * @returns {Promise<Object>} Simulator output object.
 */
async function doStep(extra = {}, cycles = null) {
  syncDOM();
  const out = await apiStep(extra, cycles);
  buildInputs();
  renderAll(out);
  return out;
}

/**
 * @function startTurbo
 * @description Starts the continuous simulation loop, advancing by `selectedCycles` per
 *              iteration with an optional inter-step delay. Yields to the browser event
 *              loop when no delay is configured.
 */
async function startTurbo() {
  if (turboRunning) return;
  turboRunning = true;
  $("btnRun").disabled  = true;
  $("btnStop").disabled = false;

  while (turboRunning) {
    const out = await apiStep({}, selectedCycles);
    renderAll(out);
    if (turboDelay > 0) await sleep(turboDelay);
    else await new Promise(r => setTimeout(r, 0));
  }
}

/**
 * @function stopTurbo
 * @description Halts the continuous simulation loop and restores button states.
 */
function stopTurbo() {
  turboRunning = false;
  $("btnRun").disabled  = false;
  $("btnStop").disabled = true;
}

/** @function sleep @param {number} ms @returns {Promise<void>} */
const sleep = ms => new Promise(r => setTimeout(r, ms));




/**
 * @function loadSchema
 * @description Fetches the simulator schema, presets, and metadata from the backend,
 *              initialises `currentInputs` from field defaults, and restores any
 *              previously persisted inputs from localStorage.
 * @returns {Promise<void>}
 */
async function loadSchema() {
  const res  = await fetch("/api/schema");
  const data = await res.json();
  schema  = data.schema  || [];
  presets = data.presets || {};
  meta    = data.meta    || {};

  currentInputs = {};
  schema.forEach(g => g.fields.forEach(f => { currentInputs[f.name] = f.default; }));
  currentInputs.sim_time_ms      = 0;
  currentInputs.cmd_timestamp_ms = 0;

  try {
    const saved = localStorage.getItem("reb_inputs");
    if (saved) {
      const parsed = JSON.parse(saved);
      Object.keys(parsed).forEach(k => { if (k in currentInputs) currentInputs[k] = parsed[k]; });
    }
  } catch(e) {}

  document.querySelectorAll(".cycle-btn").forEach(btn =>
    btn.addEventListener("click", () => {
      selectedCycles = parseInt(btn.dataset.cycles);
      document.querySelectorAll(".cycle-btn").forEach(b => b.classList.toggle("active", b === btn));
    })
  );

  document.querySelectorAll(".turbo-btn").forEach(btn =>
    btn.addEventListener("click", () => {
      turboDelay = parseInt(btn.dataset.delay);
      document.querySelectorAll(".turbo-btn").forEach(b => b.classList.toggle("active", b === btn));
    })
  );

  buildInputs();
  renderAll({ state:"IDLE", mode:"AUTO", block_phase:"PREALERT", derate_pct:100,
    starter_ok:true, channel_rx_ok:true, powertrain_valid:true,
    anti_replay:{ok:true,reason:"AUTH_OK",last_nonce:0}, logs:[] });
}

/**
 * @function rampSpeed
 * @description Smoothly ramps vehicle speed from zero to `vTarget` in two phases:
 *              Phase 1 increments speed by 10 km/h per step until target is reached.
 *              Phase 2 runs 55 stabilisation cycles with minimal oscillation to allow
 *              the powertrain recovery counter to reach its required threshold.
 *              UI controls are disabled for the duration of the ramp.
 * @param {number} vTarget - Target speed in km/h.
 * @returns {Promise<void>}
 */
async function rampSpeed(vTarget) {
  if (vTarget <= 0) {
    currentInputs.vehicle_speed_kmh = 0;
    return;
  }

  const RAMP_BTNS = ["btnStep","btnRun","btnRemoteBlock","btnSmsBlock",
                   "btnRemoteUnblock","btnPanelUnlock","btnReplayAtk",
                   "btnFreshToken"];
  RAMP_BTNS.forEach(id => { if ($(id)) $(id).disabled = true; });
  document.getElementById("inputSpeedInicial").disabled = true;

  $("scenProgress").classList.remove("hidden");

  const INCREMENT = 10;
  const rampSteps = Math.ceil(vTarget / INCREMENT);
  currentInputs.vehicle_speed_kmh = 0;

  for (let i = 1; i <= rampSteps; i++) {
    currentInputs.vehicle_speed_kmh = parseFloat(
      Math.min(vTarget, INCREMENT * i).toFixed(2)
    );
    const out = await apiStep({}, 1);
    renderAll(out);
    const pct = Math.round(i / rampSteps * 50);
    $("scenBar").style.width = `${pct}%`;
    $("scenLabel").textContent =
      `Ramping up…  ${currentInputs.vehicle_speed_kmh.toFixed(1)} / ${vTarget} km/h`;
    await new Promise(r => setTimeout(r, 10));
  }

  $("scenLabel").textContent = "Stabilising powertrain…";
  const STABILITY_CYCLES = 55;

  for (let i = 0; i < STABILITY_CYCLES; i++) {
    const noise = (i % 2 === 0) ? 0.005 : -0.005;
    currentInputs.vehicle_speed_kmh = parseFloat((vTarget + noise).toFixed(4));
    const out = await apiStep({}, 1);
    renderAll(out);
    const pct = 50 + Math.round(i / STABILITY_CYCLES * 50);
    $("scenBar").style.width = `${pct}%`;
    $("scenLabel").textContent =
      `Stabilising powertrain… ${i + 1}/${STABILITY_CYCLES} cycles`;
    await new Promise(r => setTimeout(r, 5));
  }

  currentInputs.vehicle_speed_kmh = vTarget;

  $("scenProgress").classList.add("hidden");
  document.getElementById("inputSpeedInicial").disabled = false;
  RAMP_BTNS.forEach(id => { if ($(id)) $(id).disabled = false; });
  buildInputs();
}

/**
 * @function rampSpeedBlocking
 * @description Gradually adjusts vehicle speed toward `vTarget` during an active BLOCKING phase.
 *              Caps the target at `V_MAX_FLOOR_KMH` to respect the physical fuel floor constraint.
 *              Updates `speedInicial` so derate calculations remain anchored to the adjusted reference.
 * @param {number} vTarget - Desired target speed in km/h.
 * @returns {Promise<void>}
 */
async function rampSpeedBlocking(vTarget) {
  const cappedTarget = Math.min(vTarget, V_MAX_FLOOR_KMH);
  speedInicial = cappedTarget;
  vTarget = cappedTarget;
  const vCurrent = parseFloat(currentInputs.vehicle_speed_kmh || 0);
  if (Math.abs(vTarget - vCurrent) < 0.01) return;

  const INCREMENT = vTarget > vCurrent ? 10 : -10;
  const steps = Math.ceil(Math.abs(vTarget - vCurrent) / Math.abs(INCREMENT));

  for (let i = 1; i <= steps; i++) {
    const v = vCurrent + INCREMENT * i;
    currentInputs.vehicle_speed_kmh = parseFloat(
      (INCREMENT > 0 ? Math.min(vTarget, v) : Math.max(vTarget, v)).toFixed(2)
    );
    const out = await apiStep({}, 1);
    renderAll(out);
    await new Promise(r => setTimeout(r, 10));
  }
  currentInputs.vehicle_speed_kmh = vTarget;
}

/**
 * @function wireButtons
 * @description Binds click and change event handlers to all primary action buttons
 *              and the initial speed input, delegating to the appropriate API and
 *              ramp functions.
 */
function wireButtons() {
  $("btnStep").addEventListener("click", async () => { await doStep(); });

  $("btnRun").addEventListener("click", () => startTurbo());
  $("btnStop").addEventListener("click", () => stopTurbo());

  $("btnReset").addEventListener("click", async () => {
    stopTurbo();
    await fetch("/api/reset", {method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({clear_nvm:true})});
    speedInicial = 0;
    await loadSchema();
  });

  $("btnClearLogs").addEventListener("click", () => {
    $("logs").innerHTML = `<div class="muted" style="padding:8px">Log cleared</div>`;
  });

  $("btnPowerCycle") && $("btnPowerCycle").addEventListener("click", async () => {
    stopTurbo();
    const out = await apiPowerCycle();
    currentInputs.sim_time_ms = 0;
    currentInputs.cmd_timestamp_ms = 0;
    buildInputs();
    renderAll(out);
  });

  $("btnTestNfrRel001") && $("btnTestNfrRel001").addEventListener("click", async () => {
    const btn = $("btnTestNfrRel001");
    btn.disabled = true;
    btn.textContent = "Running…";
    try {
      const result = await apiTestNfrRel001();
      renderNfrRel001Result(result);
      const snap = await fetch("/api/snapshot").then(r => r.json());
      renderAll(snap);
    } finally {
      btn.disabled = false;
      btn.textContent = "Run Power outage Test";
    }
  });

  document.getElementById("inputSpeedInicial").addEventListener("change", e => {
    const vTarget = parseFloat(e.target.value) || 0;
    const out = window._lastOut || {};
    if (out.state === "BLOCKING" && out.block_phase === "PARKED") {
      rampSpeedBlocking(vTarget);
    } else {
      rampSpeed(vTarget);
    }
  });
}

/**
 * @description Cross-tab synchronisation listener via the `localStorage` `storage` event.
 *
 * Two channels are used:
 *  - `reb_inputs`:    Persistent input state written by the Cockpit tab; merges known keys
 *                     into `currentInputs` and rebuilds the input form DOM.
 *  - `reb_cmd_pulse`: Ephemeral command pulse from the Cockpit tab; applies command flags
 *                     immediately and clears them after 1500 ms to prevent re-submission.
 */
window.addEventListener("storage", function(e) {
  if (e.key === "reb_cmd_pulse" && e.newValue) {
    try {
      const p = JSON.parse(e.newValue);
      const _ephKeys = ["auth_blocked_remote","auth_block_automatic","remote_unblock_remote","auth_manual_out","cancel_request"];
      _ephKeys.forEach(k => { if (p[k] !== undefined) currentInputs[k] = p[k]; });
      buildInputs();
      setTimeout(() => {
        _ephKeys.forEach(k => { currentInputs[k] = (k === "auth_block_automatic") ? 0 : false; });
        buildInputs();
      }, 1500);
    } catch(ex) {}
    return;
  }
  if (e.key !== "reb_inputs" || !e.newValue) return;
  try {
    const p = JSON.parse(e.newValue);
    Object.keys(p).forEach(k => { if (k in currentInputs) currentInputs[k] = p[k]; });
    buildInputs();
  } catch(ex) {}
});

loadSchema()
  .then(wireButtons)
  .catch(err => {
    document.body.innerHTML = `<pre style="padding:20px;color:salmon">${esc(err.stack||String(err))}</pre>`;
  });
