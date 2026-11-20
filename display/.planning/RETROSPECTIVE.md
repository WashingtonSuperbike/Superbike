# Project Retrospective

*A living document updated after each milestone. Lessons feed forward into future planning.*

---

## Milestone: v1.2 — SD Card Support

**Shipped:** 2026-04-16
**Phases:** 2 (02, 03) | **Plans:** 4 | **Commits:** 42

### What Was Built

- **SD card SPI driver** — SdFat + CH422G expander CS (pin 4), 500ms FreeRTOS poll task, sd_icon white/red mount state
- **PCF85063A RTC driver** — ported from Waveshare reference, Wire.begin() stripped, `YYYY-MM-DD_HHMMSS.csv` formatter; compile-time `__DATE__`/`__TIME__` fallback for battery-less power-on epoch
- **FreeRTOS CSV logger** — 42-column header (8 base + 24 cell + 10 thermistor), 20 Hz writes via `vTaskDelay(50ms)`, 10s flush, clean card-removal and re-insert handling
- **Code quality pass** — `std::atomic<bool>` for `sd_started`, `CONFIG_HV_CELL_COUNT` constant throughout, spinlock on simulationTask cell writes, `assert()` guard in `sd_get_spi_mutex()`

### What Worked

- **Mutex-guarded SdFat pattern** — wrapping every SdFat call in `xSemaphoreTake/Give` was clean from the start; zero lock-contention issues across SD, RTC, and LCD sharing the bus
- **State-machine edge detection** (`prev_sd_started` / `sd_now`) for card insert/remove was simple and reliable; no spurious transitions in hardware testing
- **Gap-closure as a plan** — making the RTC epoch fix its own plan (03-02) with a `gap_closure: true` marker kept the gap small and focused rather than retrofitting into 03-01
- **Code review caught real bugs** — `CR-01` (`volatile bool` → `std::atomic<bool>`) and `IN-03` (missing spinlock in simulationTask) were real concurrency hazards caught before CAN integration

### What Was Inefficient

- **Column count confusion** — PLAN listed "34 columns" but implementation correctly has 42 (8 base + 24 cell + 10 thermistor). The planning arithmetic error caused unnecessary comment drift that needed a fix pass (WR-02)
- **RTC epoch bug discovered only in hardware UAT** — the `year == 2000` power-on issue was predictable from the PCF85063A datasheet (no battery backup). Could have been planned from the start instead of discovered as a gap
- **`display/` path prefix mismatch** — SUMMARY.md files stored paths with `display/` prefix, but executor ran from inside `display/`; caused review file-scope lookup failures that needed manual correction

### Patterns Established

- **Shared I2C bus modules** — modules must never call `Wire.begin()`; the board init owns the bus. Documented in MEMORY.md.
- **`CONFIG_*` constants over magic numbers** — `CONFIG_HV_CELL_COUNT` pattern should extend to any array size referenced in multiple places
- **FreeRTOS task stack sizing** — logger_task at 4096 words proved sufficient; use as baseline for future background tasks
- **Mutex timeout as a dial** — 50ms was too tight for SPI under load; 200ms correct. Log timeouts explicitly in code comments

### Key Lessons

1. **Check hardware datasheets for power-on defaults before planning** — RTC epoch, I2C address, register state on cold boot. The PCF85063A's 2000-01-01 default was documented; planning should have included a fallback from the start
2. **Relative paths in SUMMARY.md must match executor CWD** — either always use repo-root-relative paths or always strip the project directory prefix. Pick one and enforce it
3. **Code review is cheapest when scoped to the phase** — running it immediately after execution (not at milestone close) catches issues while context is fresh and before downstream phases build on them
4. **`std::atomic` not `volatile` for cross-task flags** — `volatile` gives no memory ordering guarantees on ARM Cortex-M; `std::atomic<bool>` is the correct tool for `sd_started` and similar shared flags

### Cost Observations

- Model mix: ~100% sonnet (executor, verifier, reviewer, fixer all ran on sonnet)
- Sessions: 4-5 across the milestone
- Notable: Single-plan wave execution was fast; the worktree path had an EEXIST collision on the `.claude/worktrees` directory and fell back to sequential cleanly

---

## Milestone: v1.3 — CAN Bus Integration

**Shipped:** 2026-04-20
**Phases:** 1 (04) | **Plans:** 3 | **Commits:** 6

### What Was Built

- **Dashboard Health UI:** Added `can_icon` with 3-way color mapping: Green (receiving), Yellow (no data/timeout), Red (error-passive/bus-off).
- **TWAI Health Model:** Implemented `std::atomic<CanStatus>` in `DashboardState` with four states: `NO_DATA`, `RECEIVING`, `ERR_PASSIVE`, `BUS_OFF`.
- **Auto-Recovery Machine:**
    - **BUS_OFF:** Automatically calls `twai_initiate_recovery()` and `twai_start()` upon alert.
    - **ERR_PASSIVE:** Triggers full driver reinstall sequence (stop/uninstall/install/start) to clear hardware error counters and resume listening.
- **No-Data Watchdog:** 3-second silence timeout in `waveshare_twai_receive()` transitions the UI from `RECEIVING` to `NO_DATA`.
- **Task Stability:** Bumped `twai_recv` stack to 6144 bytes to provide headroom for the deeper call stacks required by the ESP-IDF driver recovery sequences.

### What Worked

- **Atomic State Transitions:** Using `std::atomic<CanStatus>` with a `uint8_t` underlying type was efficient and safe for cross-core status updates (Core 0 CAN -> Core 1 UI).
- **Module-level Statics for Config:** Promoting the TWAI config structs to module-level statics in `CAN_Receive.cpp` made the re-installation logic for `ERR_PASSIVE` recovery much cleaner, as it didn't require passing parameters back from `main.cpp`.
- **Dirty-checking for UI Icons:** Using a `prev_can_status` sentinel in `dashboard_refresh()` prevented redundant LVGL calls, keeping the UI task efficient even with 3-way color logic.

### What Was Inefficient

- **Stack Size Underestimation:** The initial 4096-byte stack was tight for the recovery sequences. Discovered during the final implementation phase (Plan 03), requiring a mid-execution stack bump. Baselines for recovery-intensive tasks should start at 6K+.
- **Execution Interruption:** The `gsd-executor` was interrupted before completing the final stability task (stack bump) and documentation updates, requiring a manual fix pass. Ensuring plans are atomic and small helps mitigate the impact of such interruptions.

### Patterns Established

- **TWAI Alert-Driven Recovery:** Using the alert mask (`TWAI_ALERT_BUS_OFF`, `TWAI_ALERT_ERR_PASS`) rather than polling status registers provides immediate response to fault states.
- **Watchdog-in-Receiver Pattern:** Implementing the no-data watchdog directly inside the receive task (`waveshare_twai_receive`) is more efficient than a separate task for simple timeout-based UI state changes.

### Key Lessons

1. **Recovery Sequences Add Stack Depth:** ESP-IDF driver lifecycle calls (`uninstall`/`install`) have significant stack depth. Always provide extra headroom (e.g., 6144+) for tasks that manage hardware driver lifecycles.
2. **Alert Masks are Primary for Health:** The TWAI alert system is more robust for fault detection than polling status registers, especially for transient conditions like `BUS_OFF`.
3. **Plan for Interruptions:** When executing multi-task plans, verify each task's completion individually if the executor reports an interruption.

---

## Milestone: v1.6 — Data Smoothing

**Shipped:** 2026-05-05
**Phases:** 3 (09-11) | **Plans:** 9 | **Commits:** 4

### What Was Built

- **EMAFilter utility class** — O(1), 4-byte state per signal, `alpha = 1 - exp(-dt/tau)`, `reset()` snap-to init to avoid boot ramps
- **Anti-flicker display smoothing** — τ=0.1s applied to RPM, current, voltages, gyro; eliminates adjacent-integer flicker with imperceptible 0.3s settle
- **Raw temperature display** — battery, motor, MC temps switched to raw values; eliminated 2–6s visual lag from original 2.0s tau filters
- **Temperature arc threshold colors** — arc indicator dynamically colors green/yellow/red based on per-sensor warn/crit thresholds

### What Worked

- **Post-validation retuning** — The validate-phase workflow surfaced the mismatch between stated requirements (grouped time constants) and actual UX (sluggish needle, laggy temps). Fixing it immediately kept the milestone honest rather than shipping a known degraded experience.
- **Raw temperatures** — removing filtering from slow-moving physical quantities was the right call; thermal inertia is the "filter" — no code needed.
- **Single uniform tau** — replacing four different time constant groups with one anti-flicker policy (τ=0.1s) simplified the mental model significantly.

### What Was Inefficient

- **Original time constant spec was untested** — The v1.6 requirements defined grouped tau values (0.25s–5.0s) without analyzing the 30 Hz update rate. If EMA settling time math had been worked through at planning time, the 2.0s temp tau and 0.5s RPM tau would never have been specced. 
- **No phase artifact directories** — Phases 09-11 were implemented without formal GSD PLAN/SUMMARY files. This made the validate-phase workflow harder (State C detection) and left no structured record of execution decisions.

### Patterns Established

- **EMA time constant = 63% rise time, not settle time** — The 95% settle is 3× tau. Always communicate settle time, not tau, when discussing "how long does smoothing take."
- **Anti-flicker tau sweet spot** — τ=0.1s at 30 Hz (alpha≈0.28) reduces noise amplitude to ~40% of raw while remaining visually imperceptible. Use as default for any future display signal that flickers between adjacent values.
- **Physical inertia as the filter** — Temperatures, pressures, and other physically slow-changing signals don't need EMA. Raw is better — no lag, no complexity.

### Key Lessons

1. **Spec smoothing in terms of settle time, not tau** — Saying "0.5s smoothing" is ambiguous; "95% settle in 1.5s" is what the rider experiences. Use the right unit at requirements time.
2. **Validate time constants against update rate** — tau × UPDATE_RATE_HZ determines alpha. An alpha of 0.065 (tau=0.5s at 30 Hz) is nearly a 15-sample average — far more aggressive than "half a second" sounds.
3. **Run validation promptly after implementation** — The filter tuning issue was caught at milestone completion; earlier validation would have caught it before the requirements archived incorrect time constants.

### Cost Observations

- Sessions: 2 (implementation + validation/fix)
- Notable: validate-phase workflow was the correct entry point for user feedback — surfaced a real UX problem and provided a structured path to resolution

---

## Cross-Milestone Trends

### Process Evolution

| Milestone | Phases | Plans | Key Change |
|-----------|--------|-------|------------|
| v1.1 | 1 | 1 | Baseline — single plan, no review pipeline |
| v1.2 | 2 | 4 | Added code-review → fix → verify pipeline; gap closure as explicit plan type |
| v1.3 | 1 | 3 | Full recovery state machine; alert-driven health tracking; stack-depth awareness |
| v1.5 | 4 | 10 | Error/warning framework; modal + carousel UI patterns for safety-critical alerts |
| v1.6 | 3 | 9 | EMA anti-flicker utility; validate-phase as UX feedback loop; spec in settle time not tau |

### Top Lessons (Verified Across Milestones)

1. **Hardware UAT is the ground truth** — simulation passes mean nothing until the board confirms behavior; keep hardware close during final phases
2. **Small, focused plans are faster to execute and verify** — 03-02 (1 task, 1 file) was done in minutes; larger plans accumulate more review findings
3. **Stack headroom is cheap; stack overflows are expensive** — When in doubt for driver-intensive background tasks, 6K-8K is a safer baseline than 4K.
