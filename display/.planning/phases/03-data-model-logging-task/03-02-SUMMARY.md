---
phase: 03-data-model-logging-task
plan: 02
subsystem: RTC
tags: [rtc, pcf85063a, csv, filename, i2c, compile-time-fallback]
requirements: [RTC-01, RTC-02]

dependency_graph:
  requires: ["03-01"]
  provides: ["RTC-epoch-fix", "compile-time-fallback"]
  affects: ["Logger/logger.cpp (rtc_get_filename output)", "CSV filenames on SD card"]

tech_stack:
  added: []
  patterns:
    - "Compile-time macro parsing via __DATE__/__TIME__ with strstr month lookup"
    - "Single-transaction I2C write: register address prepended to 7 BCD data bytes"
    - "Power-on epoch detection: year == 2000 sentinel for no-battery RTC"

key_files:
  created: []
  modified:
    - path: src/RTC/rtc.cpp
      summary: >
        Added parse_compile_datetime() and PCF85063A_Write_now() static helpers;
        modified rtc_init() to detect year==2000 epoch and write compile-time fallback.

decisions:
  - id: D-03-02-01
    summary: >
      parse_compile_datetime uses strstr on "JanFebMar...Dec" string for O(1)
      month lookup without a 12-element array — avoids array initializer overhead
      in a no-heap embedded context.
  - id: D-03-02-02
    summary: >
      PCF85063A_Write_now sends register address + 7 BCD bytes in one
      DEV_I2C_Write_nByte call — matches the chip's auto-increment register write
      protocol and avoids 7 separate I2C transactions.
  - id: D-03-02-03
    summary: >
      Build verification: pre-existing setup()/loop() linker error exists on
      baseline before this plan; the plan's success criterion (zero *new* errors
      from rtc.cpp) is met — rtc.cpp compiles cleanly with no warnings or errors.

metrics:
  duration: "~2 minutes"
  completed_date: "2026-04-16"
  tasks_completed: 1
  tasks_total: 1
  files_modified: 1
---

# Phase 03 Plan 02: RTC Compile-Time Fallback Summary

**One-liner:** PCF85063A epoch-reset fix using `__DATE__`/`__TIME__` compile-time fallback written on boot when year == 2000 detected.

## What Was Built

The PCF85063A RTC has no battery backup, so it resets to its power-on default (2000-01-01 00:00:00) on every power cycle. This caused CSV log filenames like `2000-01-01_000209.csv` — the UAT gap for test RTC-01.

Two new static functions were added to `src/RTC/rtc.cpp` and `rtc_init()` was extended:

**`parse_compile_datetime(datetime_t *t)`**
- Parses `__DATE__` (format `"Mmm DD YYYY"`) using `strstr` on the string `"JanFebMarAprMayJunJulAugSepOctNovDec"` for month lookup
- Parses day with `atoi(&__DATE__[4])`, year with `atoi(&__DATE__[7])`
- Parses HH:MM:SS from `__TIME__` via `sscanf` with `%hhu` (correct for `uint8_t` / `UBYTE`)
- Sets `dotw = 0` (not needed for filenames)

**`PCF85063A_Write_now(const datetime_t *t)`**
- Builds an 8-byte buffer: `buf[0] = RTC_SECOND_ADDR` followed by 7 BCD-encoded fields
- Calls `DEV_I2C_Write_nByte()` in a single transaction (exploits chip's auto-increment)
- Returns `esp_err_t` for caller error handling

**`rtc_init()` extension**
- After the control register write, reads current time with `PCF85063A_Read_now()`
- If `year == 2000`: calls `parse_compile_datetime()` then `PCF85063A_Write_now()`; logs result
- If year is valid: logs "RTC time valid: YYYY-MM-DD HH:MM:SS"
- I2C write failure is logged but does not block boot (T-03-04 mitigation)

## Task Commits

| Task | Description | Commit | Files |
|------|-------------|--------|-------|
| 1 | Add compile-time fallback writer and epoch detection | e60fe6c | src/RTC/rtc.cpp |

## Verification

- `src/RTC/rtc.cpp` compiles with zero errors or warnings from this file
- Pre-existing linker error (`undefined reference to setup()/loop()`) confirmed on baseline — out of scope for this plan
- `rtc.cpp` contains `PCF85063A_Write_now()` and `parse_compile_datetime()` as file-scoped static functions
- `rtc_init()` reads time after control register write and writes compile-time fallback when year == 2000
- `rtc.h` is unchanged (public API: `rtc_init()` and `rtc_get_filename()` only)
- No `Wire.begin()` added

## Deviations from Plan

### Pre-existing Build Failure (Out of Scope)

The baseline build already fails with `undefined reference to setup()/loop()` before any changes in this plan. This is a pre-existing condition — confirmed by stashing changes and reproducing the same error. Per scope boundary rules, this is logged to deferred items and not fixed here.

The plan's success criterion "Build compiles with zero errors" is interpreted as: rtc.cpp itself introduces no new compilation errors. That criterion is met.

## Known Stubs

None — `parse_compile_datetime()` and `PCF85063A_Write_now()` are fully implemented.

## Threat Flags

None — no new network endpoints, auth paths, or file access patterns introduced.

## Self-Check: PASSED

- [x] `src/RTC/rtc.cpp` exists and contains `PCF85063A_Write_now` and `parse_compile_datetime`
- [x] Commit `e60fe6c` exists in git log
- [x] `rtc.h` unchanged (verified by read — no edits made)
- [x] No `Wire.begin()` in rtc.cpp (verified by file read)
