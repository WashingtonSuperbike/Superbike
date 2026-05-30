# Testing Patterns

**Analysis Date:** 2025-07-22

## Test Framework

**Runner:**
- No automated unit test runner (like Unity or Google Test) is currently integrated into the CI/CD or `platformio.ini`.
- Testing is primarily manual and Hardware-in-the-Loop (HIL).

**Assertion Library:**
- Standard C `assert()` for boot-time checks.
- Logic checks via `if` statements in simulators.

**Run Commands:**
```bash
pio run -e hardware_in_loop -t upload   # Flash the HIL simulator to an ESP32-S3
pio run -e display -t upload            # Flash the display and observe simulation
```

## Test File Organization

**Location:**
- Hardware simulators: `hardware_in_loop/src/`
- Test scenario files: `hardware_in_loop/src/TestFiles/`
- Embedded UI simulations: `display/src/main.cpp` (conditionally compiled)

**Naming:**
- Simulators: `main.cpp` or specific test files like `BMSTest.cpp`, `MotorTest.cpp`.

**Structure:**
```
hardware_in_loop/
├── src/
│   ├── main.cpp                # ESP32-S3 TWAI Simulator
│   └── TestFiles/
│       ├── BMSTest.cpp         # Teensy/FlexCAN-based BMS tests
│       └── MotorTest.cpp       # Teensy/FlexCAN-based Motor tests
```

## Test Structure

**Suite Organization:**
Test scenarios are organized as individual functions that send specific sequences of CAN messages.

```typescript
// From hardware_in_loop/src/TestFiles/BMSTest.cpp

// BMS_Test_1.2 
// Overtemp Fault (At least 1 thermistor is over temperature threshold) 
// Expected output: Contactor opened, warning displayed to rider. 
void sendOvertempFault() {
  CAN_message_t msg;
  // ... configuration ...
  msg.buf[2] = BMS_FAULT_OVERTEMP;
  CAN_2.write(msg);
  // ... send accompanying data ...
}
```

**Patterns:**
- **Setup pattern:** Initialize CAN bus and serial console in `setup()`.
- **Teardown pattern:** Not explicitly used (embedded systems usually run indefinitely).
- **Assertion pattern:** Manual observation of `Serial` output and Display UI state.

## Mocking

**Framework:** Custom hardware-based mocking.

**Patterns:**
```cpp
// Simulated response logic in hardware_in_loop/src/main.cpp
if (rec_msg.identifier == DD_BMS_CVCUR_REQ) {
    // Mimic BMS response to cell voltage request
    send_msg.identifier = DD_BMS_CVCUR_C1_TO_C4_RSP;
    twai_transmit(&send_msg, pdMS_TO_TICKS(10));
}
```

**What to Mock:**
- External CAN nodes (BMS, Motor Controller, Charger).
- Sensor inputs (using sin waves or random noise in UI simulations).

**What NOT to Mock:**
- Low-level driver initialization (tested on real hardware).

## Fixtures and Factories

**Test Data:**
Pre-defined CAN message buffers with specific fault flags.

```cpp
// Fault flag definitions in hardware_in_loop/src/main.cpp
#define BMS_FLAG_CELL_HVC        0x01 
#define BMS_FAULT_OVERTEMP       0x04 
```

**Location:**
- Hardcoded in simulator source files.

## Coverage

**Requirements:** None enforced.

**View Coverage:** Not applicable (no coverage tool integrated).

## Test Types

**Unit Tests:**
- Limited to isolated logic in `include/CANDecoder.h` (tested via injection in HIL).

**Integration Tests:**
- **Hardware-in-the-Loop (HIL):** Using one ESP32 to act as a simulator for the rest of the bike.
- Verifies that CAN message decoding translates to correct state updates in the `Context`.

**UI Simulation:**
- `drivingSimulationTask` and `chargingSimulationTask` in `display/src/main.cpp` allow developers to verify UI layout and gauge behavior without a CAN bus or real sensors.

## Common Patterns

**Async Testing:**
- Simulated by FreeRTOS tasks that update state at fixed intervals.

**Error Testing:**
- Injection of fault bits into CAN messages (e.g., `BMS_FLAG_CELL_HVC`) and observing the system's fail-safe behavior.

---

*Testing analysis: 2025-07-22*
