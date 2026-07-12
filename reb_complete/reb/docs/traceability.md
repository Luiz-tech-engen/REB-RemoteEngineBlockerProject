# REB Requirements Traceability Matrix

This document maps every functional (FR) and non-functional (NFR)
requirement to its source implementation and to the automated test(s)
that verify it in `tests/test_reb.c`.

**FR** = Functional Requirement · **NFR** = Non-Functional Requirement

| Test ID | Req ID | Title | Type | Implementation | Verified by (`test_reb.c`) |
|---|---|---|---|---|---|
| TEST-001 | FR-001 | State-driven remote engine immobilization | Functional | `fsm.c` | `test_001_idle_no_event`, `test_002_theft_confirmed_remote` |
| TEST-002 | FR-002 | Validated theft confirmation prior to immobilization | Functional | `fsm.c`, `sensor_fusion.c` | `test_002_theft_confirmed_remote`, `test_008_sensor_fusion_detect` |
| TEST-003 | FR-003 | Vehicle-condition-based state transition control | Functional | `fsm.c` | `test_012_parked_timer_reset_on_motion`, `test_017_signal_fault_inhibit` |
| TEST-004 | FR-004 | Local manual activation via vehicle interface | Functional | `panel_auth.c` | `test_004_panel_activation`, `test_005_panel_lockout` |
| TEST-005 | FR-005 | Remote activation via 4G (TCU/Backend) | Functional | `security_manager.c`, `can_codec.c` | `test_002_theft_confirmed_remote`, `test_003_invalid_cmd_rejected`, `test_sc01_full_remote_block` |
| TEST-006 | FR-006 | Remote activation via SMS fallback | Functional | `security_manager.c`, `can_codec.c` | `test_022_sms_fallback` |
| TEST-007 | FR-007 | Automatic activation via sensor fusion | Functional | `sensor_fusion.c` | `test_008_sensor_fusion_detect` |
| TEST-008 | FR-008 | Pre-notification and reversion window | Functional | `reversal_window.c` | `test_013_reversal_window_abort`, `test_014_reversal_window_60s_expire`, `test_015_reversal_rejected_after_actuation` |
| TEST-009 | FR-009 | Minimum fuel safety floor during in-motion derating | Functional | `actuator_iface.c` | `test_009_fuel_safety_floor`, `test_010_fuel_floor_zero_violations`, `test_023_no_derating_in_blocked` |
| TEST-010 | FR-010 | Transition from in-motion blocking to parked inhibit | Functional | `starter_control.c`, `fsm.c` | `test_011_blocked_after_120s_stop`, `test_sc03_block_while_moving` |
| TEST-011 | FR-011 | Definitive BLOCKED after 120 s safe-stop dwell | Functional | `starter_control.c`, `fsm.c` | `test_011_blocked_after_120s_stop`, `test_012_parked_timer_reset_on_motion`, `test_sc02_dwell_interrupt_with_motion_resume` |
| TEST-012 | FR-012 | 60 s warning and reversal window | Functional | `reversal_window.c` | `test_013_reversal_window_abort`, `test_014_reversal_window_60s_expire` |
| TEST-013 | FR-013 | Visual and audible alerts for imminent blocking | Functional | `alert_manager.c` | `test_021_alert_manager` |
| TEST-014 | NFR-SEC-001 | Anti-replay and integrity protection | Security | `security_manager.c` | `test_006_nonce_replay`, `test_007_timestamp_expired` |
| TEST-015 | NFR-SAF-001 | Prohibition of abrupt engine block in motion | Safety | `powertrain_validation.c` | `test_016_starter_only_when_stopped`, `test_sc03_block_while_moving` |
| TEST-016 | NFR-SAF-002 | Fail-safe under signal loss | Safety | `powertrain_validation.c` | `test_017_signal_fault_inhibit` |
| TEST-017 | NFR-REL-001 | Post-reset state recovery and persistence | Reliability | `nvm.c` | `test_018_nvm_persistence`, `test_019_nvm_restore_on_init` |
| TEST-018 | NFR-INFO-001 | Secure diagnostic logging and event tracing | Information | `event_log.c` | `test_020_event_log`, `test_info001_log_persists_across_power_cycle`, `test_info001_evt_derate_active_fired`, `test_info001_evt_cmd_received_fired`, `test_info001_evt_block_reject_speed_fired` |
| TEST-019 | NFR-SW-001 | MISRA C coding standard compliance | SW Eng. | `Makefile` (`misra` target) | Static analysis — not a runtime test |


