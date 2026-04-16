# Roadmap: Superbike Display Module

## Milestones

- [x] **v1.0 Build & Flash** - Phases 1 (hardware bringup complete)
- [x] **v1.1 Speedometer Dial** - Phase 1 (lv_meter speedometer complete)
- **v1.2 SD Card Support** - Phases 2-3 (in progress)

---

<details>
<summary>v1.1 Speedometer Dial (Phase 1) - COMPLETE</summary>

## v1.1 Speedometer Dial

### Phase 1: Speedometer Meter
**Goal**: `lv_meter` is the dashboard's central element — speed needle, current arcs, and digital readout all working in simulation
**Depends on**: Nothing (hardware already proven in v1.0)
**Requirements**: UI-01, UI-02, UI-03
**Plans:** 1 plan

Plans:
- [x] 01-01-PLAN.md — Replace RPM arc/power bar/speed label with lv_meter speedometer dial

**Success Criteria** (what must be TRUE):
  1. `lv_meter` renders at 400×400 px in the left-center region of the 800×480 screen
  2. Inner ring shows 0–140 mph scale with major ticks every 10 mph (labeled) and minor ticks every 5 mph; needle tracks speed from `rpm_to_mph()`
  3. Outer ring shows motor current — blue arc for positive (motoring), green arc for negative (regen), both hidden when zero
  4. Digital speed label is a child of the meter, aligned bottom-center, showing integer mph
  5. RPM arc, power bar, and large standalone speed label are removed
  6. All other widgets (temp arcs, gyro, battery info, stopwatch, icons) are unchanged and still functional in simulation

</details>

---

## v1.2 SD Card Support

**Milestone Goal:** SD card mounts at boot, status icon reflects mount state in real time, and a FreeRTOS logging task records full-telemetry CSV rows to the card at 20 Hz throughout each ride session.

## Phases

- [x] **Phase 2: SD + RTC Hardware Bringup** - Initialize SD card over SPI via CH422G expander, initialize PCF85063A RTC on shared I2C, wire sd_icon color to mount state, verify mount/unmount detection at runtime
- [x] **Phase 3: Data Model + Logging Task** - Extend DashboardBatteryVoltages with hv_cell_voltages[24], implement FreeRTOS CSV logging task that creates a datetime-named file, writes a header, logs all telemetry at 20 Hz with 10 s flush, and handles card removal gracefully (completed 2026-04-16)

## Phase Details

### Phase 2: SD + RTC Hardware Bringup
**Goal**: SD card and RTC are both initialized at boot within the existing display init sequence; sd_icon reflects live mount state and the RTC provides a datetime string for log filenames
**Depends on**: Phase 1
**Requirements**: SD-01, SD-02, SD-03, RTC-01, RTC-02, STATUS-01, STATUS-02
**Plans:** 2/2 plans executed

Plans:
- [x] 02-01-PLAN.md — SD card SPI driver with CH422G expander CS, SdFat mount, FreeRTOS poll task
- [x] 02-02-PLAN.md — PCF85063A RTC driver, datetime filename formatter, sd_icon color fix, hardware verification

**Success Criteria** (what must be TRUE):
  1. With a card inserted at boot, SD mounts successfully via SPI (MOSI=11, CLK=12, MISO=13, CS via CH422G pin 4) and the sd_icon turns white
  2. With no card at boot, the sd_icon is red; inserting a card at runtime causes the icon to turn white without a reboot
  3. Removing a card at runtime causes the sd_icon to revert to red within the next poll cycle
  4. RTC is readable at boot and produces a well-formed filename string in `YYYY-MM-DD_HHMMSS.csv` format (logged to Serial)
  5. SD and LCD share the CH422G expander and I2C bus without interfering with each other — display continues animating normally after SD/RTC init
**UI hint**: yes

### Phase 3: Data Model + Logging Task
**Goal**: A background FreeRTOS task creates a datetime-named CSV at boot, writes a full-telemetry header and 20 Hz rows from DashboardState, flushes every 10 s, and stops cleanly if the card is removed
**Depends on**: Phase 2
**Requirements**: MODEL-01, LOG-01, LOG-02, LOG-03, LOG-04, LOG-05, LOG-06
**Success Criteria** (what must be TRUE):
  1. `DashboardBatteryVoltages` has a `float hv_cell_voltages[24]` array that is populated by the simulation task (and the CAN receive path when added later)
  2. A new CSV file named from the RTC datetime is created at boot when the card is mounted; the first line is a header row with all column names matching LOG-03 (elapsed_ms, speed_mph, motor_rpm, motor_current_A, motor_temp_C, mc_temp_C, bms_voltage_V, aux_voltage_V, cell_01_V … cell_24_V, thermistor_01_C … thermistor_10_C)
  3. Rows are appended at 20 Hz (50 ms interval) with values read from DashboardState; after 10 seconds the file is flushed and remains readable on a PC as valid CSV
  4. Pulling the card while logging causes the task to stop writing without crashing the firmware; re-inserting the card causes logging to resume in a new file
**Plans:** 2/2 plans complete

Plans:
- [x] 03-01-PLAN.md — Data model extension, SD accessors with SPI mutex, Logger task with RTC-named CSV files
- [x] 03-02-PLAN.md — Gap closure: RTC compile-time fallback for battery-less PCF85063A

## Progress

**Execution Order:**
Phases execute in numeric order: 2 -> 3

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Speedometer Meter | v1.1 | 1/1 | Complete | 2026-04-03 |
| 2. SD + RTC Hardware Bringup | v1.2 | 2/2 | Complete | 2026-04-15 |
| 3. Data Model + Logging Task | v1.2 | 2/2 | Complete   | 2026-04-16 |
