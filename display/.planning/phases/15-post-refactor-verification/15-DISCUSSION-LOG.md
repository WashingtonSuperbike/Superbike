# Phase 15: Post-Refactor Verification - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-07
**Phase:** 15-post-refactor-verification
**Areas discussed:** Warning threshold, Smoke test scope

---

## Warning Threshold

| Option | Description | Selected |
|--------|-------------|----------|
| Zero total warnings | Aim for a completely clean build. If warnings exist, fix before the phase passes. | ✓ |
| Zero new warnings only | Only refactor-introduced warnings count. Requires capturing a pre-refactor baseline commit. | |
| Zero errors only | Warnings noted but don't block. Phase passes as long as it compiles. | |

**User's choice:** Zero total warnings
**Notes:** Simpler pass/fail criterion — no need for a pre-refactor snapshot.

---

| Option | Description | Selected |
|--------|-------------|----------|
| Fix them | Phase 15 resolves any warnings found before considering the phase complete. | ✓ |
| Report and block | Document warnings and fail the phase; defer fixing to a follow-up. | |

**User's choice:** Fix them
**Notes:** Phase is not complete until build is clean.

---

## Smoke Test Scope

| Option | Description | Selected |
|--------|-------------|----------|
| Targeted spot-check | Verify refactored paths: drive screen visual, charging screen visual, screen switching via debug defines. Skip Phase 12 edge cases. | ✓ |
| Full Phase 12 replay | Re-run every Phase 12 test case on hardware. | |
| Build only | No hardware test. Trust build passing and Phase 14 static verification. | |

**User's choice:** Targeted spot-check
**Notes:** Phase 12 validated live CAN paths and timing — that logic wasn't touched in phases 13-14.

---

| Option | Description | Selected |
|--------|-------------|----------|
| CAN timeout (natural) | Let the CAN watchdog expire with no CAN connected. Errors appear naturally within CAN_TIMEOUT_MS. | ✓ |
| Debug define or sim injection | Use a compile-time define or error simulation path to inject an error. | |
| Skip error overlay | Only test needle/arcs/charging visuals. Error overlay deferred. | |

**User's choice:** CAN timeout (natural)
**Notes:** Simple and always available — no extra tooling needed.

---

| Option | Description | Selected |
|--------|-------------|----------|
| Debug defines only | Use DEBUG_CHARGING_SCREEN_ONLY and DEBUG_SPEEDOMETER_SCREEN_ONLY. Phase 12 already validated live paths. | ✓ |
| Both debug defines and live CAN | Verify debug defines AND at least one live EVCC/charger transition. | |

**User's choice:** Debug defines only
**Notes:** Phase 12 validated live EVCC/charger switching — no duplication needed.

---

## Claude's Discretion

- Plan structure (one plan vs. two — could split build/fix from hardware smoke)
- Order of build check vs. hardware test within the phase
- Exact warning categories to address if any are found

## Deferred Ideas

None — discussion stayed within phase scope.
