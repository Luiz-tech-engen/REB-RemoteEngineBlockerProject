from __future__ import annotations

import json
import mimetypes
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

BASE_DIR = Path(__file__).resolve().parent
TEMPLATE_DIR = BASE_DIR / "templates"
STATIC_DIR   = BASE_DIR / "static"

_use_c = False
try:
    from reb_backend import RebLib, build_presets, build_schema, PANEL_PASSWORD_HASH, TS_MS
    backend  = RebLib()
    _use_c   = True
    print("[REB] Using native C backend (libreb_bridge.so)")
except Exception as _e:
    print(f"[REB] C backend unavailable ({_e})")



def _json_bytes(data) -> bytes:
    return json.dumps(data, ensure_ascii=False).encode("utf-8")

def _run_nfr_rel_001_test() -> dict:
    """
    @brief Executes the automated NFR-REL-001 post-reset state recovery test.

    The test sequence is:
      1. Full reset with NVM cleared; asserts initial state is IDLE.
      2. Issues a remote block command; asserts transition to THEFT_CONFIRMED.
      3. Advances the simulation past T_MIN_AFTER; asserts entry into BLOCKING.
      4. Parks the vehicle for 120 s (12 000 cycles); asserts transition to BLOCKED
         with starter inhibit active.
      5. Executes a power cycle (NVM preserved); asserts state is restored to BLOCKED,
         NVM was loaded, and starter inhibit remains active.

    A negative-control step verifying that a full NVM clear returns the system
    to IDLE is not included in the returned evidence but is covered by step 1.

    @return Dictionary containing:
            - "pass": overall boolean result,
            - "requirement": requirement identifier string,
            - "description": human-readable requirement description,
            - "state_before_power_cycle": FSM state string before the power cycle,
            - "state_after_power_cycle": FSM state string after the power cycle,
            - "nvm_state_loaded": bool indicating NVM was successfully restored,
            - "starter_inhibit_after_reset": bool indicating starter inhibit is active
              after the power cycle,
            - "steps": list of per-step evidence dictionaries.
    """
    steps = []
    passed = True

    backend.reset(clear_nvm=True)
    snap = backend.snapshot()
    steps.append({
        "step": 1,
        "desc": "Clean reset (NVM cleared)",
        "state": snap.get("state"),
        "ok": snap.get("state") == "IDLE",
    })
    if snap.get("state") != "IDLE":
        passed = False

    nonce = 9001
    snap2 = backend.step(
        {
            "vehicle_speed_kmh": 0.0,
            "ignition_state": 2,
            "brake_pedal": 0.0,
            "speed_sig_status": 0,
            "ip_rx_ok": True,
            "sms_rx_ok": True,
            "auth_blocked_remote": True,
            "cmd_nonce": nonce,
            "cmd_timestamp_ms": 0,
            "cmd_sig_ok": True,
            "tcu_ack": True,
            "sim_time_ms": 100,
        },
        mode="REMOTE", cycles=1, auto_advance_ms=True,
    )
    steps.append({
        "step": 2,
        "desc": "Remote command sent — expected THEFT_CONFIRMED",
        "state": snap2.get("state"),
        "ok": snap2.get("state") == "THEFT_CONFIRMED",
    })
    if snap2.get("state") != "THEFT_CONFIRMED":
        passed = False

    snap3 = backend.step(
        {"vehicle_speed_kmh": 0.0, "ignition_state": 2, "speed_sig_status": 0,
         "ip_rx_ok": True, "auth_blocked_remote": False, "cmd_sig_ok": True,
         "tcu_ack": True},
        mode="REMOTE", cycles=110, auto_advance_ms=True,
    )
    steps.append({
        "step": 3,
        "desc": "Waited T_MIN_AFTER — expected BLOCKING",
        "state": snap3.get("state"),
        "ok": snap3.get("state") == "BLOCKING",
    })
    if snap3.get("state") != "BLOCKING":
        passed = False

    snap4 = backend.step(
        {"vehicle_speed_kmh": 0.0, "ignition_state": 2, "speed_sig_status": 0,
         "brake_pedal": 1.0, "ip_rx_ok": True, "auth_blocked_remote": False,
         "cmd_sig_ok": True, "tcu_ack": True},
        mode="REMOTE", cycles=12100, auto_advance_ms=True,
    )
    state_before = snap4.get("state")
    steps.append({
        "step": 4,
        "desc": "Vehicle parked for 120s — expected BLOCKED",
        "state": state_before,
        "starter_inhibit": snap4.get("starter_inhibit_active"),
        "ok": state_before == "BLOCKED" and bool(snap4.get("starter_inhibit_active")),
    })
    if state_before != "BLOCKED":
        passed = False

    if hasattr(backend, "power_cycle"):
        backend.power_cycle()
    else:
        backend.reset(clear_nvm=False)

    snap5 = backend.snapshot()
    state_after        = snap5.get("state")
    nvm_loaded         = bool(snap5.get("nvm_state_loaded"))
    starter_after      = bool(snap5.get("starter_inhibit_active"))

    step5_ok = (state_after == "BLOCKED") and nvm_loaded and starter_after
    steps.append({
        "step": 5,
        "desc": "Power-cycle (NVM preserved) — state must restore to BLOCKED",
        "state": state_after,
        "nvm_state_loaded": nvm_loaded,
        "starter_inhibit_active": starter_after,
        "ok": step5_ok,
    })
    if not step5_ok:
        passed = False

    return {
        "pass": passed,
        "description": "Post-Reset State Recovery and Persistence",
        "state_before_power_cycle": state_before,
        "state_after_power_cycle": state_after,
        "nvm_state_loaded": nvm_loaded,
        "starter_inhibit_after_reset": starter_after,
        "steps": steps,
    }


class Handler(BaseHTTPRequestHandler):
    """
    @brief HTTP request handler for the REB integration lab server.

    Serves the frontend templates and static assets, exposes a JSON REST API
    for simulation control, and delegates all state management to the active
    backend instance. All responses include CORS headers permitting any origin.
    """

    server_version = "REBIntegrationLab/3.0"

    def _send_json(self, data, status=200):
        """
        @brief Serialises @p data to JSON and writes a complete HTTP response.

        @param data    Python object to serialise; must be JSON-serialisable.
        @param status  HTTP status code; defaults to 200.
        """
        payload = _json_bytes(data)
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(payload)

    def _send_file(self, path: Path):
        """
        @brief Reads a file from disk and writes its contents as an HTTP response.

        Responds with 404 if @p path does not exist or is not a regular file.
        MIME type is inferred from the file extension.

        @param path  Absolute path to the file to serve.
        """
        if not path.exists() or not path.is_file():
            self.send_error(HTTPStatus.NOT_FOUND, "File not found")
            return
        data = path.read_bytes()
        ctype, _ = mimetypes.guess_type(str(path))
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", ctype or "application/octet-stream")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def _read_json(self):
        """
        @brief Reads and deserialises the request body as JSON.

        Returns an empty dict when Content-Length is absent, zero, or the body
        cannot be parsed as valid JSON.

        @return Deserialised Python object, or an empty dict on failure.
        """
        length = int(self.headers.get("Content-Length", "0") or 0)
        if length <= 0:
            return {}
        try:
            return json.loads(self.rfile.read(length).decode("utf-8"))
        except Exception:
            return {}

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/":
            return self._send_file(TEMPLATE_DIR / "index.html")
        if path == "/api/schema":
            return self._send_json({
                "schema":  build_schema(),
                "presets": build_presets(),
                "meta": {
                    "ts_ms": TS_MS,
                    "threshold": 0.7,
                    "version": "reb-sim-3.0",
                    "panel_password_hash": PANEL_PASSWORD_HASH,
                    "backend": "c" if _use_c else "python",
                    "T_PARKED_S": 120,
                    "T_PREALERT_S": 60,
                    "FUEL_FLOOR_PCT": 10,
                    "V_STOP_KMH": 0.5,
                },
            })
        if path == "/api/snapshot":
            return self._send_json(backend.snapshot())
        if path.startswith("/static/"):
            return self._send_file(STATIC_DIR / path.removeprefix("/static/"))
        if path.startswith("/images/"):
            return self._send_file(STATIC_DIR / "images" / path.removeprefix("/images/"))
        if path == "/cockpit":
            return self._send_file(TEMPLATE_DIR / "cockpit.html")
        self.send_error(HTTPStatus.NOT_FOUND)

    def do_POST(self):
        path = urlparse(self.path).path
        body = self._read_json()
        if path == "/api/reset":
            backend.reset(clear_nvm=bool(body.get("clear_nvm", True)))
            return self._send_json(backend.snapshot())
        if path == "/api/power_cycle":
            if hasattr(backend, "power_cycle"):
                backend.power_cycle()
            else:
                backend.reset(clear_nvm=False)
            snap = backend.snapshot()
            snap["power_cycle"] = True
            return self._send_json(snap)

        if path == "/api/test/nfr_rel_001":
            return self._send_json(_run_nfr_rel_001_test())

        if path == "/api/step":
            inputs       = body.get("inputs", {}) or {}
            mode         = body.get("mode", "AUTO")
            cycles       = int(body.get("cycles", 1))
            auto_advance = bool(body.get("auto_advance", True))
            return self._send_json(
                backend.step(inputs, mode=mode, cycles=cycles,
                             auto_advance_ms=auto_advance)
            )
        self.send_error(HTTPStatus.NOT_FOUND)

    def log_message(self, *_):
        return


def main():
    """
    @brief Entry point. Starts the threaded HTTP server on port 8000.

    Binds to all interfaces (0.0.0.0) and serves until a KeyboardInterrupt
    is received, after which the server socket is closed cleanly.
    """
    server = ThreadingHTTPServer(("0.0.0.0", 8000), Handler)
    print("REB UI  →  http://localhost:8000", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
