from __future__ import annotations

import ctypes as C
import subprocess
from pathlib import Path
from typing import Any, Dict, List

ROOT = Path(__file__).resolve().parent.parent
REB_DIR = ROOT / "reb"
LIB_PATH = ROOT / "ui" / "libreb_bridge.so"
TS_MS = 10
PANEL_PASSWORD_HASH = 0x7C78C98F

STATE_NAMES = {0: "IDLE", 1: "THEFT_CONFIRMED", 2: "BLOCKING", 3: "BLOCKED"}
BLOCK_PHASE_NAMES = {0: "PREALERT", 1: "DERATING", 2: "PARKED"}
MODE_NAMES = {"AUTO": "AUTO", "REMOTE": "REMOTE", "MANUAL": "MANUAL"}


def _sources() -> List[Path]:
    return [
        REB_DIR / "reb_main.c",
        REB_DIR / "src" / "reb_core" / "event_log.c",
        REB_DIR / "src" / "reb_core" / "nvm.c",
        REB_DIR / "src" / "reb_core" / "security_manager.c",
        REB_DIR / "src" / "reb_core" / "panel_auth.c",
        REB_DIR / "src" / "reb_core" / "sensor_fusion.c",
        REB_DIR / "src" / "reb_core" / "powertrain_validation.c",
        REB_DIR / "src" / "reb_core" / "actuator_iface.c",
        REB_DIR / "src" / "reb_core" / "starter_control.c",
        REB_DIR / "src" / "reb_core" / "alert_manager.c",
        REB_DIR / "src" / "reb_core" / "reversal_window.c",
        REB_DIR / "src" / "can" / "can_rx.c",
        REB_DIR / "src" / "reb_core" / "fsm.c",
        ROOT / "ui" / "backend_bridge.c",
        REB_DIR / "include" / "reb" / "reb_params.h",
        REB_DIR / "include" / "reb" / "reb_types.h",
        REB_DIR / "reb_main.h",
    ]


def _needs_rebuild(target: Path, sources: List[Path]) -> bool:
    """
    @brief Determines whether the shared library target requires recompilation.

    Returns True if the target does not exist or if any source file that
    exists on disk has a modification time newer than the target.

    @param target   Path to the compiled shared library.
    @param sources  List of source and header paths to check against the target.
    @return         True if a rebuild is required, False otherwise.
    """
    if not target.exists():
        return True
    t = target.stat().st_mtime
    return any(src.exists() and src.stat().st_mtime > t for src in sources)


def build_shared_library() -> Path:
    """
    @brief Compiles the REB C sources into a position-independent shared library.

    Skips compilation when the library is already up to date relative to all
    source files. Raises RuntimeError on non-zero compiler exit status,
    including compiler stdout and stderr in the exception message.

    @return Path to the compiled shared library.
    @throws RuntimeError if the compiler returns a non-zero exit code.
    """
    sources = _sources()
    if not _needs_rebuild(LIB_PATH, sources):
        return LIB_PATH
    cmd = [
        "gcc",
        "-std=c99",
        "-O2",
        "-fPIC",
        "-shared",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-I./reb",
        "-I./reb/include",
        "-I./ui",
        "-o",
        str(LIB_PATH),
        *[str(src.relative_to(ROOT)) for src in sources],
        "-lm",
    ]
    proc = subprocess.run(cmd, cwd=str(ROOT), capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(
            "Failed to compile the native library.\n"
            f"STDOUT:\n{proc.stdout}\n\nSTDERR:\n{proc.stderr}"
        )
    return LIB_PATH


class BridgeInputs(C.Structure):
    """
    @brief ctypes mirror of the C @c reb_bridge_inputs_t structure.

    Field order and types must remain identical to the C declaration to
    ensure correct memory layout when passing pointers across the FFI boundary.
    """
    _fields_ = [
        ("vehicle_speed_kmh", C.c_float),
        ("engine_rpm", C.c_uint16),
        ("ignition_state", C.c_uint8),
        ("brake_pedal", C.c_float),
        ("speed_sig_status", C.c_int32),
        ("accel_peak", C.c_float),
        ("glass_break_flag", C.c_float),
        ("ip_rx_ok", C.c_bool),
        ("sms_rx_ok", C.c_bool),
        ("auth_blocked_remote", C.c_bool),
        ("auth_block_automatic", C.c_uint8),
        ("remote_unblock_remote", C.c_bool),
        ("cmd_nonce", C.c_uint16),
        ("cmd_timestamp_ms", C.c_uint32),
        ("cmd_sig_ok", C.c_bool),
        ("tcu_ack", C.c_bool),
        ("cancel_request", C.c_bool),
        ("auth_manual_out", C.c_bool),
        ("password_attempt", C.c_uint32),
        ("sim_time_ms", C.c_uint32),
    ]


class BridgeOutputs(C.Structure):
    """
    @brief ctypes mirror of the C @c reb_bridge_outputs_t structure.

    Field order and types must remain identical to the C declaration to
    ensure correct memory layout when passing pointers across the FFI boundary.
    """
    _fields_ = [
        ("derate_pct", C.c_uint8),
        ("starter_ok", C.c_bool),
        ("alert_visual", C.c_bool),
        ("alert_sonic", C.c_bool),
        ("hmi_alert_code", C.c_int32),
        ("notify_theft", C.c_bool),
        ("notify_blocked", C.c_bool),
        ("gps_send", C.c_bool),
        ("reb_status_tx_due", C.c_bool),
        ("derate_cmd_tx_due", C.c_bool),
        ("current_state", C.c_int32),
        ("sensor_score", C.c_float),
        ("pre_block_alert", C.c_bool),
        ("reversal_timer_s", C.c_uint32),
        ("starter_inhibit_active", C.c_bool),
        ("fuel_derating_active", C.c_bool),
        ("nvm_state_loaded", C.c_bool),
        ("blocked_flag", C.c_bool),
        ("parked", C.c_uint8),
        ("derating_active", C.c_bool),
        ("source_trigger_out", C.c_uint8),
        ("channel_rx_ok", C.c_bool),
        ("rx_fail", C.c_bool),
        ("rx_channel_id", C.c_int32),
        ("panel_password_ok", C.c_bool),
        ("panel_locked_out", C.c_bool),
        ("panel_attempt_count", C.c_uint8),
        ("block_phase", C.c_int32),
        ("powertrain_valid", C.c_bool),
        ("speed_valid", C.c_bool),
        ("ign_valid", C.c_bool),
        ("brake_valid", C.c_bool),
        ("pt_fault_code", C.c_uint16),
    ]


class LogEntry(C.Structure):
    """
    @brief ctypes mirror of the C @c reb_bridge_log_entry_t structure.

    Used as the element type of the array buffer passed to
    @c reb_bridge_get_logs.
    """
    _fields_ = [
        ("ts_ms", C.c_uint32),
        ("kind", C.c_uint8),
        ("state_from", C.c_uint8),
        ("state_to", C.c_uint8),
        ("source", C.c_uint8),
        ("auth_fail", C.c_uint8),
    ]


class RebLib:
    """
    @brief Python wrapper around the REB native shared library.

    Manages the library lifecycle (build, load, destroy), binds all FFI
    function signatures, and exposes a high-level interface for stepping
    the simulation, reading state snapshots, and querying event logs.
    """

    def __init__(self) -> None:
        build_shared_library()
        self.lib = C.CDLL(str(LIB_PATH))
        self._bind()
        self.handle = self.lib.reb_bridge_create()
        if not self.handle:
            raise RuntimeError("Failed to create the handle for the native backend")
        self.last_inputs: Dict[str, Any] = {}
        self.reset(clear_nvm=True)

    def _bind(self) -> None:
        """
        @brief Declares argtypes and restypes for every exported bridge symbol.

        Must be called once after the shared library is loaded and before any
        bridge function is invoked. Incorrect type bindings result in undefined
        behaviour across the FFI boundary.
        """
        self.lib.reb_bridge_create.argtypes = []
        self.lib.reb_bridge_create.restype = C.c_void_p
        self.lib.reb_bridge_destroy.argtypes = [C.c_void_p]
        self.lib.reb_bridge_destroy.restype = None
        self.lib.reb_bridge_reset.argtypes = [C.c_void_p, C.c_bool]
        self.lib.reb_bridge_reset.restype = None
        self.lib.reb_bridge_step.argtypes = [C.c_void_p, C.POINTER(BridgeInputs), C.POINTER(BridgeOutputs)]
        self.lib.reb_bridge_step.restype = None
        self.lib.reb_bridge_snapshot.argtypes = [C.c_void_p, C.POINTER(BridgeOutputs)]
        self.lib.reb_bridge_snapshot.restype = None
        self.lib.reb_bridge_state.argtypes = [C.c_void_p]
        self.lib.reb_bridge_state.restype = C.c_int32
        self.lib.reb_bridge_sim_time_ms.argtypes = [C.c_void_p]
        self.lib.reb_bridge_sim_time_ms.restype = C.c_uint32
        self.lib.reb_bridge_last_nonce.argtypes = [C.c_void_p]
        self.lib.reb_bridge_last_nonce.restype = C.c_uint32
        self.lib.reb_bridge_panel_wrong_cnt.argtypes = [C.c_void_p]
        self.lib.reb_bridge_panel_wrong_cnt.restype = C.c_uint8
        self.lib.reb_bridge_panel_lockout.argtypes = [C.c_void_p]
        self.lib.reb_bridge_panel_lockout.restype = C.c_bool
        self.lib.reb_bridge_derate_ramp.argtypes = [C.c_void_p]
        self.lib.reb_bridge_derate_ramp.restype = C.c_float
        self.lib.reb_bridge_log_count.argtypes = [C.c_void_p]
        self.lib.reb_bridge_log_count.restype = C.c_uint16
        self.lib.reb_bridge_get_logs.argtypes = [C.c_void_p, C.POINTER(LogEntry), C.c_uint16]
        self.lib.reb_bridge_get_logs.restype = C.c_uint16

    def __del__(self) -> None:
        handle = getattr(self, "handle", None)
        lib = getattr(self, "lib", None)
        if handle and lib:
            try:
                lib.reb_bridge_destroy(handle)
            except Exception:
                pass

    def reset(self, clear_nvm: bool = True) -> None:
        """
        @brief Resets the bridge instance and seeds it with a single neutral step.

        Resets the native context, clears cached inputs, and zeroes the parked
        timer counter. A single step with all-safe inputs is executed so that
        the output snapshot is valid immediately after reset.

        @param clear_nvm  When True, NVM state is invalidated before re-init.
        """
        self.lib.reb_bridge_reset(self.handle, bool(clear_nvm))
        self.last_inputs = {}
        self._parked_timer_cycles = 0
        safe = {
            "vehicle_speed_kmh": 0.0,
            "engine_rpm": 0,
            "ignition_state": 0,
            "brake_pedal": 0.0,
            "speed_sig_status": 0,
            "accel_peak": 0.0,
            "glass_break_flag": 0.0,
            "ip_rx_ok": True,
            "sms_rx_ok": True,
            "auth_blocked_remote": False,
            "auth_block_automatic": 0,
            "remote_unblock_remote": False,
            "cmd_nonce": 1,
            "cmd_timestamp_ms": 0,
            "cmd_sig_ok": True,
            "tcu_ack": True,
            "cancel_request": False,
            "auth_manual_out": False,
            "password_attempt": PANEL_PASSWORD_HASH,
            "sim_time_ms": 0,
        }
        self.step(safe, mode="AUTO", cycles=1, auto_advance_ms=False)

    def power_cycle(self) -> None:
        """
        @brief Simulates a power cycle without erasing NVM state.

        Resets the native context with @c clear_nvm=False so that previously
        persisted state is restored on the subsequent step. Executes one
        neutral step to populate the output snapshot from the restored NVM state.
        """
        self.lib.reb_bridge_reset(self.handle, False)
        self.last_inputs = {}
        self._parked_timer_cycles = 0
        safe = {
            "vehicle_speed_kmh": 0.0,
            "engine_rpm": 0,
            "ignition_state": 0,
            "brake_pedal": 0.0,
            "speed_sig_status": 0,
            "accel_peak": 0.0,
            "glass_break_flag": 0.0,
            "ip_rx_ok": True,
            "sms_rx_ok": True,
            "auth_blocked_remote": False,
            "auth_block_automatic": 0,
            "remote_unblock_remote": False,
            "cmd_nonce": 1,
            "cmd_timestamp_ms": 0,
            "cmd_sig_ok": True,
            "tcu_ack": True,
            "cancel_request": False,
            "auth_manual_out": False,
            "password_attempt": PANEL_PASSWORD_HASH,
            "sim_time_ms": 0,
        }
        self.step(safe, mode="AUTO", cycles=1, auto_advance_ms=False)

    def _build_inputs(self, payload: Dict[str, Any], sim_time_ms: int) -> BridgeInputs:
        """
        @brief Constructs a @c BridgeInputs structure from a payload dictionary.

        Merges @p payload over a set of safe defaults. Fields not present in
        @p payload receive default values. @c sim_time_ms and
        @c cmd_timestamp_ms are normalised from @p sim_time_ms when absent.

        @param payload      Dictionary of input field overrides.
        @param sim_time_ms  Current simulation time used to fill time fields
                            not explicitly set in @p payload.
        @return             Fully populated @c BridgeInputs instance.
        """
        defaults = {
            "vehicle_speed_kmh": 0.0,
            "engine_rpm": 0,
            "ignition_state": 0,
            "brake_pedal": 0.0,
            "speed_sig_status": 0,
            "accel_peak": 0.0,
            "glass_break_flag": 0.0,
            "ip_rx_ok": True,
            "sms_rx_ok": True,
            "auth_blocked_remote": False,
            "auth_block_automatic": 0,
            "remote_unblock_remote": False,
            "cmd_nonce": 1,
            "cmd_timestamp_ms": sim_time_ms,
            "cmd_sig_ok": True,
            "tcu_ack": True,
            "cancel_request": False,
            "auth_manual_out": False,
            "password_attempt": PANEL_PASSWORD_HASH,
            "sim_time_ms": sim_time_ms,
        }
        defaults.update(payload or {})
        defaults["sim_time_ms"] = int(defaults.get("sim_time_ms", sim_time_ms))
        defaults["cmd_timestamp_ms"] = int(defaults.get("cmd_timestamp_ms", defaults["sim_time_ms"]))
        data = BridgeInputs()
        for name, _ in data._fields_:
            setattr(data, name, defaults[name])
        return data

    def step(self, payload: Dict[str, Any], mode: str = "AUTO", cycles: int = 1, auto_advance_ms: bool = True) -> Dict[str, Any]:
        """
        @brief Advances the REB state machine for a given number of cycles.

        Each cycle constructs an input structure from @p payload, calls the
        native step function, and updates the parked-phase cycle counter used
        for UI timer display. When @p auto_advance_ms is True, @c sim_time_ms
        is incremented by @c TS_MS between consecutive cycles.

        @p cycles is clamped to the range [1, 15000].

        @param payload         Input field values for the step.
        @param mode            Operating mode string; defaults to "AUTO".
                               Invalid values are silently coerced to "AUTO".
        @param cycles          Number of simulation steps to execute.
        @param auto_advance_ms When True, simulation time advances by @c TS_MS
                               per cycle.
        @return                Output snapshot dictionary after the final cycle.
        """
        mode = (mode or "AUTO").upper()
        if mode not in MODE_NAMES:
            mode = "AUTO"
        cycles = max(1, min(int(cycles), 15000))
        local = dict(payload or {})
        out = BridgeOutputs()
        for _ in range(cycles):
            sim_time = int(local.get("sim_time_ms", self.lib.reb_bridge_sim_time_ms(self.handle)))
            local["sim_time_ms"] = sim_time
            local["cmd_timestamp_ms"] = int(local.get("cmd_timestamp_ms", sim_time))
            in_struct = self._build_inputs(local, sim_time)
            self.lib.reb_bridge_step(self.handle, C.byref(in_struct), C.byref(out))

            if BLOCK_PHASE_NAMES.get(int(out.block_phase)) == "PARKED" and \
                STATE_NAMES.get(int(out.current_state)) == "BLOCKING":
                self._parked_timer_cycles = getattr(self, "_parked_timer_cycles", 0) + 1
            elif BLOCK_PHASE_NAMES.get(int(out.block_phase)) == "DERATING" and \
            STATE_NAMES.get(int(out.current_state)) == "BLOCKING":
                self._parked_timer_cycles = 0
            elif STATE_NAMES.get(int(out.current_state)) in ("IDLE", "THEFT_CONFIRMED", "BLOCKED"):
                if STATE_NAMES.get(int(out.current_state)) != "BLOCKED":
                    self._parked_timer_cycles = 0

            if auto_advance_ms:
                local["sim_time_ms"] = sim_time + TS_MS
        self.last_inputs = dict(local)
        return self.snapshot(out=out, mode=mode, inputs=local)

    def _read_logs(self, limit: int = 30) -> List[Dict[str, Any]]:
        """
        @brief Retrieves the most recent event log entries from the native library.

        Allocates a fixed-size buffer of up to @p limit entries, calls
        @c reb_bridge_get_logs, and converts each record to a dictionary.
        State indices are resolved to their string names via @c STATE_NAMES.

        @param limit  Maximum number of log entries to retrieve.
        @return       List of log entry dictionaries, ordered oldest to newest.
        """
        count = int(self.lib.reb_bridge_log_count(self.handle))
        if count <= 0:
            return []
        take = min(limit, count)
        buf = (LogEntry * take)()
        written = int(self.lib.reb_bridge_get_logs(self.handle, buf, take))
        items: List[Dict[str, Any]] = []
        for i in range(written):
            rec = buf[i]
            items.append({
                "ts_ms": int(rec.ts_ms),
                "kind": int(rec.kind),
                "state_from": STATE_NAMES.get(int(rec.state_from), str(int(rec.state_from))),
                "state_to": STATE_NAMES.get(int(rec.state_to), str(int(rec.state_to))),
                "source": int(rec.source),
                "auth_fail": int(rec.auth_fail),
            })
        return items

    def _anti_replay_reason(self, inputs: Dict[str, Any]) -> str:
        """
        @brief Evaluates the anti-replay conditions for a command input set.

        Checks, in order: signature validity, nonce monotonicity, and timestamp
        freshness within a ±30 000 ms window relative to the current simulation
        time. Returns the first failing condition or "AUTH_OK" if all pass.

        @param inputs  Dictionary of input fields, expected to contain
                       @c cmd_sig_ok, @c cmd_nonce, and @c cmd_timestamp_ms.
        @return        One of "AUTH_SIG_INVALID", "AUTH_NONCE_REPLAY",
                       "AUTH_TS_EXPIRED", or "AUTH_OK".
        """
        if not bool(inputs.get("cmd_sig_ok", True)):
            return "AUTH_SIG_INVALID"
        nonce = int(inputs.get("cmd_nonce", 0) or 0)
        last_nonce = int(self.lib.reb_bridge_last_nonce(self.handle))
        if nonce <= last_nonce:
            return "AUTH_NONCE_REPLAY"
        ts_ms = int(inputs.get("cmd_timestamp_ms", self.lib.reb_bridge_sim_time_ms(self.handle)) or 0)
        if abs(int(self.lib.reb_bridge_sim_time_ms(self.handle)) - ts_ms) > 30000:
            return "AUTH_TS_EXPIRED"
        return "AUTH_OK"

    def snapshot(self, out: BridgeOutputs | None = None, mode: str = "AUTO", inputs: Dict[str, Any] | None = None) -> Dict[str, Any]:
        """
        @brief Builds a serialisable snapshot dictionary from the current output state.

        When @p out is None, a fresh @c BridgeOutputs is populated via
        @c reb_bridge_snapshot. Derived fields such as @c effective_speed_kmh,
        @c parked_timer_s, and @c parked_timer_pct are computed from raw output
        values and the cached parked-cycle counter. Anti-replay status and the
        event log are appended unconditionally.

        @param out     Pre-populated @c BridgeOutputs, or None to fetch a fresh snapshot.
        @param mode    Mode string included verbatim in the result dictionary.
        @param inputs  Input dictionary used for anti-replay evaluation;
                       falls back to @c last_inputs when None.
        @return        Dictionary containing all observable simulation outputs.
        """
        if out is None:
            out = BridgeOutputs()
            self.lib.reb_bridge_snapshot(self.handle, C.byref(out))
        inputs = inputs or self.last_inputs
        sim_time_ms = int(self.lib.reb_bridge_sim_time_ms(self.handle))
        current_speed = float(inputs.get("vehicle_speed_kmh", 0.0) or 0.0)
        derate_pct = int(out.derate_pct)
        effective_speed = round(current_speed * derate_pct / 100.0, 2)
        return {
            "state": STATE_NAMES.get(int(out.current_state), str(int(out.current_state))),
            "mode": MODE_NAMES.get(mode, mode),
            "sim_time_ms": sim_time_ms,
            "derate_ramp": round(float(self.lib.reb_bridge_derate_ramp(self.handle)), 2),
            "current_speed_kmh": current_speed,
            "effective_speed_kmh": effective_speed,
            "derate_pct": derate_pct,
            "starter_ok": bool(out.starter_ok),
            "alert_visual": bool(out.alert_visual),
            "alert_sonic": bool(out.alert_sonic),
            "hmi_alert_code": int(out.hmi_alert_code),
            "notify_theft": bool(out.notify_theft),
            "notify_blocked": bool(out.notify_blocked),
            "gps_send": bool(out.gps_send),
            "reb_status_tx_due": bool(out.reb_status_tx_due),
            "derate_cmd_tx_due": bool(out.derate_cmd_tx_due),
            "pre_block_alert": bool(out.pre_block_alert),
            "reversal_timer_s": int(out.reversal_timer_s),
            "starter_inhibit_active": bool(out.starter_inhibit_active),
            "fuel_derating_active": bool(out.fuel_derating_active),
            "nvm_state_loaded": bool(out.nvm_state_loaded),
            "blocked_flag": bool(out.blocked_flag),
            "parked": int(out.parked),
            "parked_timer_s": round(float(getattr(self, "_parked_timer_cycles", 0)) * TS_MS / 1000.0, 1),
            "parked_timer_pct": round(float(getattr(self, "_parked_timer_cycles", 0)) / 12000.0 * 100.0, 1),
            "derating_active": bool(out.derating_active),
            "source_trigger_out": int(out.source_trigger_out),
            "channel_rx_ok": bool(out.channel_rx_ok),
            "rx_fail": bool(out.rx_fail),
            "rx_channel_id": int(out.rx_channel_id),
            "panel_password_ok": bool(out.panel_password_ok),
            "panel_locked_out": bool(out.panel_locked_out),
            "panel_attempt_count": int(out.panel_attempt_count),
            "block_phase": BLOCK_PHASE_NAMES.get(int(out.block_phase), str(int(out.block_phase))),
            "powertrain_valid": bool(out.powertrain_valid),
            "speed_valid": bool(out.speed_valid),
            "ign_valid": bool(out.ign_valid),
            "brake_valid": bool(out.brake_valid),
            "pt_fault_code": int(out.pt_fault_code),
            "sensor_score": round(float(out.sensor_score), 4),
            "anti_replay": {
                "ok": self._anti_replay_reason(inputs) == "AUTH_OK",
                "reason": self._anti_replay_reason(inputs),
                "last_nonce": int(self.lib.reb_bridge_last_nonce(self.handle)),
            },
            "logs": self._read_logs(),
        }


def build_schema() -> List[Dict[str, Any]]:
    """
    @brief Returns the UI field schema describing all configurable simulation inputs.

    Each entry defines a logical group and a list of field descriptors with name,
    label, type, default value, and, where applicable, an enumerated options list.
    The schema is consumed by the front-end to render the input form dynamically.

    @return List of group dictionaries, each containing a "group" key and a
            "fields" list of field descriptor dictionaries.
    """
    return [
        {
            "group": "Powertrain",
            "fields": [
                    {"name": "ignition_state", "label": "Ignition state", "type": "select", "default": 0, "options": [
                    {"value": 0, "label": "OFF"},
                    {"value": 1, "label": "ACC"},
                    {"value": 2, "label": "ON"},
                    {"value": 3, "label": "START"},
                ]},
                    {"name": "speed_sig_status", "label": "Speed signal status", "type": "select", "default": 0, "options": [
                    {"value": 0, "label": "SIG_VALID"},
                    {"value": 1, "label": "SIG_MISSING"},
                    {"value": 2, "label": "SIG_TIMEOUT"},
                ]},
            ],
        },
        {
            "group": "Sensors",
            "fields": [
                {"name": "accel_peak", "label": "Accel peak", "type": "number", "step": 0.1, "default": 0.0},
                {"name": "glass_break_flag", "label": "Glass break flag", "type": "number", "step": 0.1, "default": 0.0},
            ],
        },
        {
            "group": "Communications",
            "fields": [
                {"name": "ip_rx_ok", "label": "IP RX OK", "type": "bool", "default": True},
                {"name": "sms_rx_ok", "label": "SMS RX OK", "type": "bool", "default": True},
                {"name": "tcu_ack", "label": "TCU ACK", "type": "bool", "default": True},
            ],
        },
        {
            "group": "Remote / Panel commands",
            "fields": [
                {"name": "auth_blocked_remote", "label": "Remote block", "type": "bool", "default": False},
                {"name": "remote_unblock_remote", "label": "Remote unblock", "type": "bool", "default": False},
                {"name": "auth_block_automatic", "label": "Automatic block auth", "type": "select", "default": 0, "options": [
                    {"value": 0, "label": "PENDING"},
                    {"value": 1, "label": "YES"},
                    {"value": 2, "label": "NO"},
                ]},
                {"name": "auth_manual_out", "label": "Manual auth/unblock", "type": "bool", "default": False},
                {"name": "cancel_request", "label": "Cancel request", "type": "bool", "default": False},
                {"name": "password_attempt", "label": "Password attempt (hash)", "type": "number", "step": 1, "default": PANEL_PASSWORD_HASH},
            ],
        },
        {
            "group": "Anti-replay",
            "fields": [
                {"name": "cmd_nonce", "label": "Command nonce", "type": "number", "step": 1, "default": 1},
                {"name": "cmd_timestamp_ms", "label": "Command timestamp (ms)", "type": "number", "step": 1, "default": 0},
                {"name": "cmd_sig_ok", "label": "Signature OK", "type": "bool", "default": True},
            ],
        },
        {
            "group": "Timing",
            "fields": [
                {"name": "sim_time_ms", "label": "Simulation time (ms)", "type": "number", "step": 1, "default": 0},
            ],
        },
    ]


def build_presets() -> Dict[str, Dict[str, Any]]:
    """
    @brief Returns a set of named input presets for common simulation scenarios.

    Each preset provides a complete set of input field values suitable for
    passing directly to @c RebLib.step. The three defined presets correspond
    to automatic theft detection (AUTO), remote command blocking (REMOTE),
    and manual panel authentication (MANUAL).

    @return Dictionary mapping preset name strings to input field dictionaries.
    """
    return {
        "AUTO": {
            "vehicle_speed_kmh": 45.0,
            "engine_rpm": 1800,
            "ignition_state": 2,
            "brake_pedal": 0.0,
            "speed_sig_status": 0,
            "accel_peak": 8.0,
            "glass_break_flag": 1.0,
            "ip_rx_ok": True,
            "sms_rx_ok": True,
            "tcu_ack": True,
            "auth_blocked_remote": False,
            "remote_unblock_remote": False,
            "auth_manual_out": False,
            "cancel_request": False,
            "cmd_nonce": 1,
            "cmd_timestamp_ms": 0,
            "cmd_sig_ok": True,
            "sim_time_ms": 0,
        },
        "REMOTE": {
            "vehicle_speed_kmh": 20.0,
            "engine_rpm": 1200,
            "ignition_state": 2,
            "brake_pedal": 0.0,
            "speed_sig_status": 0,
            "ip_rx_ok": True,
            "sms_rx_ok": True,
            "tcu_ack": True,
            "auth_blocked_remote": True,
            "remote_unblock_remote": False,
            "auth_manual_out": False,
            "cancel_request": False,
            "cmd_nonce": 2,
            "cmd_timestamp_ms": 0,
            "cmd_sig_ok": True,
            "sim_time_ms": 0,
        },
        "MANUAL": {
            "vehicle_speed_kmh": 0.0,
            "engine_rpm": 0,
            "ignition_state": 0,
            "brake_pedal": 0.0,
            "speed_sig_status": 0,
            "ip_rx_ok": True,
            "sms_rx_ok": True,
            "tcu_ack": True,
            "auth_blocked_remote": False,
            "remote_unblock_remote": False,
            "auth_manual_out": True,
            "cancel_request": False,
            "password_attempt": PANEL_PASSWORD_HASH,
            "cmd_nonce": 3,
            "cmd_timestamp_ms": 0,
            "cmd_sig_ok": True,
            "sim_time_ms": 0,
        },
    }
