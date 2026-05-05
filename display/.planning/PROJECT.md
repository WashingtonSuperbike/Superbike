# Superbike Display Module

## What This Is

A dedicated PlatformIO project targeting a Waveshare ESP32-S3 5" LCD (800×480, no touch) that replaces the old TFT-based display on the electric motorcycle. The display runs LVGL v8, shows a live speedometer with current arcs, temperature gauges, and error/warning indicators, records full-telemetry CSV logs to an SD card at 20 Hz, and receives live data from the main board over CAN bus.

## Core Value

A self-contained display module that compiles, runs on hardware, and presents live telemetry clearly — the rider's window into the motorcycle's state.

## Current Milestone: v1.7 Screen Separation

**Goal:** Validate the charging and drive screen implementations (including debug overrides), then split the monolithic DashboardUI.cpp into per-screen source files.

**Target features:**
- Validate EVCC/charger watchdog detection and screen transition animation
- Validate `DEBUG_CHARGING_SCREEN_ONLY` and `DEBUG_SPEEDOMETER_SCREEN_ONLY` force the correct screen regardless of live data
- Extract drive screen into `DriveScreen.h/.cpp` (build, refresh, EMA filters, dirty-check state)
- Extract charging screen into `ChargingScreen.h/.cpp` (build, refresh, charging widget handles)
- `DashboardUI.cpp` becomes thin coordinator; shared infra (error mux, color macros, `create_temp_arc`) accessible to both screens

## Current State (after v1.6)

- **v1.6 shipped 2026-05-05** — Data smoothing with anti-flicker EMA (τ=0.1s) on speed/voltages/current/gyro; temperatures raw
- **v1.5 shipped 2026-04-20** — Full error/warning framework: BMS + MC severity icons, warning carousel, critical error modal
- **v1.3 shipped 2026-04-20** — CAN health UI and TWAI auto-recovery (BUS_OFF + ERR_PASSIVE)
- **v1.2 shipped 2026-04-16** — SdFat SD card + PCF85063A RTC; 42-column 20 Hz CSV logging
- Dashboard shows: speedometer needle, current arcs, 3 temperature arcs (BATT/MOTOR/MC), gyro, battery voltage/%, error/warning overlays
- EMAFilter utility class available for future display signals
- SD logs remain raw (unfiltered) for engineering analysis
- Display refresh: 30 Hz (`UPDATE_RATE_HZ = 30.0f`)

## Requirements

### Validated

- ✓ Project compiles without errors using ESP32_Display_Panel and LVGL v8 — v1.0
- ✓ Binary flashes and boots on the Waveshare ESP32-S3-Touch-LCD-5 — v1.0
- ✓ lv_meter speedometer with speed needle, current arcs, digital readout — v1.1
- ✓ SD card mounts at boot via SPI/CH422G; sd_icon reflects mount state — v1.2
- ✓ PCF85063A RTC initialized; datetime used for log filenames — v1.2
- ✓ FreeRTOS CSV logging task: 20 Hz, 42 columns, 10s flush, card-removal safe — v1.2
- ✓ hv_cell_voltages[24] in DashboardBatteryVoltages, logged per row — v1.2
- ✓ CAN health status indicator (BOOT/RX/TIMEOUT/FAULT) — v1.3
- ✓ TWAI bus-off and error-passive auto-recovery — v1.3
- ✓ BMS and MC error/warning severity icons in bottom strip — v1.5
- ✓ Warning carousel for non-critical errors — v1.5
- ✓ Critical error modal overlay for CRIT severity — v1.5
- ✓ Generic EMAFilter class (O(1), 4 bytes state, snap-to init) — v1.6
- ✓ Anti-flicker smoothing on speed/voltages/current/gyro (τ=0.1s) — v1.6
- ✓ Temperature arcs raw (no filter lag); arc color tracks thresholds — v1.6

### Active

- [ ] Charging screen switch validated on hardware (EVCC/charger detection, transition animation) — v1.7
- [ ] DEBUG_CHARGING_SCREEN_ONLY and DEBUG_SPEEDOMETER_SCREEN_ONLY verified to lock to correct screen — v1.7
- [ ] Drive screen extracted into DriveScreen.h/.cpp — v1.7
- [ ] Charging screen extracted into ChargingScreen.h/.cpp — v1.7
- [ ] DashboardUI.cpp reduced to thin coordinator; shared infra accessible to both screens — v1.7

### Out of Scope

- Gyro/IMU integration — Tabled by user; do not suggest until requested
- Watchdog-triggered TWAI driver restart — De-scoped (v1.4)
- CAN message rate counter / FPS display — De-scoped (v1.4)
- UI layout redesign — evaluating on hardware first; revisit after more riding data
- Touch input — hardware has no touch panel
- PC log viewer / parser tool — raw CSV works fine with standard spreadsheet tools
- Log rotation / max file size — not needed for short sessions; revisit if card fills
- Mobile app — not applicable

## Context

- Old display used Adafruit GFX / ILI9341 TFT (320×240) running inside the main firmware (`OLD_FIRMWARE/Display.ino`, `hidden/NewUI.cpp`)
- New display is a standalone PlatformIO project (`display/`) on a dedicated ESP32-S3 board
- LVGL v8 vendored locally in `display/lib/lvgl/`; `lv_conf.h` lives in `display/lib/`
- `ESP32_Display_Panel@^1.1.1` and `ESP32_IO_Expander@^1.1.0` pulled via lib_deps
- SD, RTC, and LCD share CH422G expander and I2C bus (SDA=8, SCL=9) without conflicts
- Logger, SDCard, and RTC are standalone modules under `src/`; no Wire.begin() in display code (board->begin() owns bus)
- EMAFilter lives in `src/Utils/`; time constant tuning will move to `DriveScreen.cpp` after refactor
- Drive and charging screens share `DashboardState` (read-only in refresh calls) and `g_error_list_mux` spinlock
- Display refresh rate is 30 Hz; EMA alpha values are computed from this rate at construction time

## Constraints

- **Hardware**: Waveshare ESP32-S3-Touch-LCD-5 (ESP32-S3, RGB LCD, 800×480, PSRAM)
- **Framework**: Arduino + FreeRTOS via PlatformIO `espressif32` platform
- **LVGL**: v8 (API must not use v9 calls)
- **No touch**: Board has no touch controller — touch init paths must be guarded
- **C++ standard**: `-std=gnu++17` applied to all translation units

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Dedicated display ESP32-S3 board | Offload rendering from main MCU; cleaner separation of concerns | ✓ Working well — no resource contention |
| LVGL v8 vendored locally | Avoid registry version conflicts; pin to known-good version | ✓ Stable; no header conflicts |
| Simulation-first approach | Validate display hardware before CAN integration | ✓ Proved hardware before any CAN work |
| SdFat over Arduino SD library | Direct FsFile access, mutex-compatible, better performance | ✓ Mutex-guarded I/O works correctly |
| CH422G for SD CS control | Shares expander already used for LCD; saves GPIO | ✓ No conflicts in practice |
| Compile-time RTC fallback | PCF85063A has no battery — use __DATE__/__TIME__ on power-on epoch | ✓ Prevents 2000-01-01 filenames |
| CONFIG_HV_CELL_COUNT constant | Avoids hardcoded 24 in logger; tracks pack size change automatically | ✓ Applied to snapshot buffer |
| Anti-flicker EMA τ=0.1s uniform | Eliminates adjacent-value display flicker; 95% settle ≈ 0.3s (imperceptible) | ✓ Replaces original grouped time constants (0.25–5.0s) which caused needle sluggishness |
| Temperatures raw (no EMA) | Physical thermal inertia prevents inter-frame flicker; filter added 2–6s display lag | ✓ Immediate temp readings, no perceived lag |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-05-05 — Milestone v1.7 Screen Separation started*
