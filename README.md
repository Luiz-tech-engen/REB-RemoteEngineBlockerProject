# REB — Remote Engine Blocker

> A deterministic embedded C firmware implementing a vehicle anti-theft engine blocking system, with a browser-based simulation cockpit and Python bridge layer.


---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [FSM State Machine](#fsm-state-machine)
- [Directory Structure](#directory-structure)
- [Requirements](#requirements)
- [Building](#building)
- [Running the Tests](#running-the-tests)
- [Simulation UI](#simulation-ui)
- [Security Model](#security-model)
- [Requirements Traceability](#requirements-traceability)

---

## Overview

REB is a deterministic finite state machine (FSM) designed to run at a fixed 10 ms cycle rate on an automotive-grade MCU. It monitors vehicle sensors and remote commands to progressively block a stolen vehicle — first by derating the fuel pump output, then by inhibiting the starter — without endangering the occupants.

**Key capabilities:**

- **Progressive fuel derating** from 100% down to a 10% safety floor at 0.75%/s
- **Starter inhibit** only after the vehicle has been stationary for 120 consecutive seconds
- **Three activation paths:** automatic sensor fusion, remote 4G/SMS command, local physical panel
- **Anti-replay protection** on all remote commands via nonce + timestamp + HMAC signature verification
- **NVM persistence** — FSM state and event log survive power cycles
- **Sensor fusion** — weighted composite of glass-break sensor (60%) and accelerometer peak (40%) with 2-second debounce
- **Browser-based simulation cockpit** for HIL-style testing without hardware

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        reb_fsm_step()  (10 ms)                  │
│                                                                 │
│  reb_inputs_t ──► sensor_fusion ──► FSM core ──► reb_outputs_t  │
│                   security_mgr  ──►        ──►  actuator_iface  │
│                   panel_auth    ──►        ──►  alert_manager   │
│                   powertrain_validation    ──►  starter_control  │
│                   can_rx_watchdog          ──►  event_log       │
│                                            ──►  nvm (persist)   │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────┐        ┌──────────────────────┐
│  libreb_bridge  │◄──────►│  reb_backend.py       │
│  (shared lib)   │  ctypes│  (Python bridge)      │
└─────────────────┘        └──────────┬───────────┘
                                      │ HTTP/JSON
                           ┌──────────▼───────────┐
                           │  app.py (HTTP server) │
                           └──────────┬───────────┘
                                      │
                           ┌──────────▼───────────┐
                           │  cockpit.js (React)   │
                           │  Browser simulation   │
                           └──────────────────────┘
```

The firmware core (`reb/`) is decoupled from the simulation layer (`ui/`). The C library is compiled to a shared object (`libreb_bridge.so`) and loaded by the Python backend via `ctypes`. The browser UI communicates with the Python server over a JSON HTTP API.

---

## FSM State Machine

```
                     sensor fusion trigger
          ┌──────────────────────────────────────┐
          │                                      ▼
     ┌────┴────┐   theft detected        ┌──────────────────┐
     │  IDLE   │────────────────────────►│ THEFT_CONFIRMED  │
     └─────────┘                         └────────┬─────────┘
          ▲                                       │ remote / auto / manual
          │  authenticated unlock                 ▼
     ┌────┴────┐                         ┌──────────────────┐
     │ BLOCKED │◄────────────────────────│    BLOCKING      │
     └─────────┘  vehicle stopped 120s   │                  │
                                         │ PREALERT (60s)   │
                                         │   → DERATING     │
                                         │   → PARKED       │
                                         └──────────────────┘
```

| State | Description |
|-------|-------------|
| `IDLE` | System armed; monitoring sensors and CAN bus |
| `THEFT_CONFIRMED` | Theft event detected; awaiting block authorisation |
| `BLOCKING` | Progressive fuel derating; three sub-phases (PREALERT → DERATING → PARKED) |
| `BLOCKED` | Definitive block; starter inhibited; unlockable via authenticated remote or panel command |

**Activation sources:**

| Source | Trigger |
|--------|---------|
| `SOURCE_AUTO` | Sensor fusion composite score ≥ 0.7 for ≥ 2 s |
| `SOURCE_REMOTE` | Authenticated CAN/4G command (nonce + timestamp + HMAC) |
| `SOURCE_PANEL` | Local panel PIN authenticated within 3 attempts |

---

## Directory Structure

```
reb_complete/
├── reb/                          # Firmware core
│   ├── include/reb/
│   │   ├── reb_types.h           # Types, enums, and structs
│   │   └── reb_params.h          # Compile-time constants
│   ├── src/
│   │   ├── reb_core/
│   │   │   ├── fsm.c / fsm.h     # Main state machine
│   │   │   ├── sensor_fusion.c   # Glass-break + accelerometer fusion
│   │   │   ├── security_manager.c# Anti-replay nonce/timestamp/HMAC
│   │   │   ├── panel_auth.c      # PIN authentication + lockout
│   │   │   ├── powertrain_validation.c # Signal validity checks
│   │   │   ├── actuator_iface.c  # Fuel derating commands
│   │   │   ├── starter_control.c # Starter inhibit logic
│   │   │   ├── alert_manager.c   # Horn / hazard outputs
│   │   │   ├── reversal_window.c # Cancellation window timer
│   │   │   ├── event_log.c       # Circular runtime event log
│   │   │   └── nvm.c             # CRC32-verified NVM persistence
│   │   └── can/
│   │       ├── can_codec.c       # CAN frame encode/decode
│   │       ├── can_frame.c       # Frame validation
│   │       ├── can_ids.c         # Message descriptor table
│   │       ├── can_monitor.c     # Bus health supervision
│   │       ├── can_defs.h        # CAN IDs, DLCs, and frame struct definitions 
│   │       ├── can_rx.c          # RX watchdog
│   │       ├── can_rx_dev.c      # RX frame dispatcher
│   │       ├── can_tx.c          # TX frame builder
│   │       └── can_v3_adapter.c  # CAN ↔ reb_inputs_t adapter
│   ├── reb_main.c / reb_main.h   # Public integration API
│   ├── Makefile
│   ├── docs/
│   │   └── traceability.md       # Requirements traceability matrix
│   └── tests/
│       └── test_reb.c            # 31-test suite (unit + integration)
│
└── ui/                           # Simulation layer
    ├── app.py                    # Threaded HTTP server (stdlib only)
    ├── reb_backend.py            # ctypes bridge to libreb_bridge.so
    ├── backend_bridge.c          # C bridge — flat struct API over reb_ctx_t
    ├── templates/
    │   ├── cockpit.html          # Simulation cockpit
    │   └── index.html            # Technical interface
    └── static/
        ├── cockpit.js            # React cockpit (no build step)
        ├── app.js                # Technical interface JS
        └── style.css
```

---

## Requirements

### Firmware (C core)

| Tool | Minimum version |
|------|----------------|
| `gcc` | 9.0 |
| `make` | 3.81 |
| `ar` | binutils (any modern) |
| `cppcheck` | 2.x *(optional, for static analysis)* |

### Simulation UI

| Tool | Minimum version |
|------|----------------|
| Python | 3.10 |
| gcc | 9.0 *(to compile `libreb_bridge.so`)* |
| Modern browser | Chrome 110+ / Firefox 110+ / Safari 16+ |

No Python packages beyond the standard library are required.

---

## Building

### 1. Compile and test the firmware core

```bash
cd reb_complete/reb

# Build static library + run test suite
make

# Build static library only
make lib

# Run tests only (requires prior build)
make test


# Clean build artifacts
make clean
```

Expected output on success:
```
--- test_001_idle_no_event ---
  PASS  out.current_state == STATE_IDLE
  ...
All tests passed
```

### 2. Compile the simulation shared library

```bash
cd reb_complete

gcc -std=c99 -O2 -fPIC -shared \
  -Wall -Wextra -Wno-unused-parameter \
  -I./reb -I./reb/include \
  -o ui/libreb_bridge.so \
  reb/src/reb_core/event_log.c \
  reb/src/reb_core/nvm.c \
  reb/src/reb_core/security_manager.c \
  reb/src/reb_core/panel_auth.c \
  reb/src/reb_core/sensor_fusion.c \
  reb/src/reb_core/powertrain_validation.c \
  reb/src/reb_core/actuator_iface.c \
  reb/src/reb_core/starter_control.c \
  reb/src/reb_core/alert_manager.c \
  reb/src/reb_core/reversal_window.c \
  reb/src/can/can_rx.c \
  reb/src/can/can_frame.c \
  reb/src/can/can_ids.c \
  reb/src/can/can_codec.c \
  reb/src/can/can_rx_dev.c \
  reb/src/can/can_tx.c \
  reb/src/can/can_monitor.c \
  reb/src/can/can_v3_adapter.c \
  reb/src/reb_core/fsm.c \
  reb/reb_main.c \
  ui/backend_bridge.c \
  -lm
```

### 3. Start the simulation server

```bash
cd reb_complete/ui
python3 app.py
```

Open your browser at **http://localhost:8000**.

---

## Running the Tests

```bash
cd reb_complete/reb
make test
```

The test binary links all core source files directly — no shared libraries required.

**Test suite:**

| Test | What it covers |
|------|---------------|
| `test_001_idle_no_event` | IDLE state with no inputs |
| `test_002_theft_confirmed_remote` | Remote command triggers THEFT_CONFIRMED |
| `test_003_invalid_cmd_rejected` | Commands with bad signature are rejected |
| `test_004_panel_activation` | Local panel PIN activates block |
| `test_005_panel_lockout` | Three wrong attempts trigger 300s lockout |
| `test_006_nonce_replay` | Replayed nonce is rejected |
| `test_007_timestamp_expired` | Expired timestamp is rejected |
| `test_008_sensor_fusion_detect` | Fusion score above threshold triggers theft |
| `test_009_fuel_safety_floor` | Derating never goes below 10% |
| `test_010_fuel_floor_zero_violations` | Derating floor holds under prolonged activation |
| `test_011_blocked_after_120s_stop` | BLOCKED after 120s parked dwell |
| `test_012_parked_timer_reset_on_motion` | Motion resets parked timer |
| `test_013_reversal_window_abort` | Authenticated cancel aborts blocking |
| `test_014_reversal_window_60s_expire` | Window expires and block proceeds |
| `test_015_reversal_rejected_after_actuation` | Cancel rejected after actuation issued |
| `test_016_starter_only_when_stopped` | Starter inhibit only when speed = 0 |
| `test_017_signal_fault_inhibit` | Speed signal fault inhibits block |
| `test_018_nvm_persistence` | State written to NVM on block |
| `test_019_nvm_restore_on_init` | State restored from NVM on power-up |
| `test_020_event_log` | Events recorded in circular log |
| `test_021_alert_manager` | Horn and hazard outputs during BLOCKING |
| `test_022_sms_fallback` | SMS channel used when IP unavailable |
| `test_023_no_derating_in_blocked` | No derating applied in BLOCKED state |
| `test_024_periodic_transmission_flags` | CAN TX period flags set correctly |
| `test_info001_log_persists_across_power_cycle` | Event log survives power cycle |
| `test_info001_evt_derate_active_fired` | DERATE_ACTIVE event recorded |
| `test_info001_evt_cmd_received_fired` | CMD_RECEIVED event recorded |
| `test_info001_evt_block_reject_speed_fired` | BLOCK_REJ_SPEED event recorded |
| `test_sc01_full_remote_block` | Integration: IDLE → BLOCKED via remote command |
| `test_sc02_dwell_interrupt_with_motion_resume` | Integration: parked timer reset then completes |
| `test_sc03_block_while_moving` | Integration: block command while vehicle in motion |

---

## Simulation UI

The browser cockpit provides a HIL-style simulation environment:

| Feature | Description |
|---------|-------------|
| **Animated cockpit** | Perspective road scene, analogue gauges (speed + RPM), ignition switch |
| **Smartphone panel** | Simulated mobile app — theft alert with countdown, remote block/unblock flow |
| **Infotainment display** | PIN entry panel, signal status LEDs, diagnostic console |
| **Turbo mode** | Accelerates simulation at ×1 / ×10 / ×100 / ×1000 real time |
| **Windshield crack** | Click to simulate glass-break sensor trigger |
| **Diagnostic export** | ASC-format CAN log export with DBC signal decoding |
| **Post Reset test** | One-click post-reset state recovery automated test |

The UI runs entirely in vanilla React loaded from CDN — no build step, no `npm`.

---

## Security Model

| Property | Implementation |
|----------|---------------|
| **Anti-replay** | Every remote command carries a monotonic nonce and a timestamp. Commands with a replayed nonce or a timestamp outside the 30-second acceptance window are rejected (`security_manager.c`). |
| **Command integrity** | The `cmd_sig_ok` input signals that an external HMAC/signature over the command payload has been verified by the TCU before forwarding to the REB. |
| **Panel lockout** | Three consecutive wrong PIN attempts trigger a 300-second lockout enforced in firmware (`panel_auth.c`). |
| **Constant-time comparison** | Panel PIN comparison uses `const_time_eq_u32` (`panel_auth.c:29`) to prevent timing side-channels. |
| **NVM integrity** | Persisted state is protected by a CRC32 checksum. Corrupted NVM causes a clean `IDLE` start rather than an undefined restore (`nvm.c`). |

---

## Requirements Traceability


Traceability matrix: [`reb_complete/reb/docs/traceability.md`](reb_complete/reb/docs/traceability.md)


