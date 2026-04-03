# Robust TWAI (CAN Bus) on the ESP32-S3

TWAI (Two-Wire Automotive Interface) is the ESP32's built-in CAN bus peripheral. This tutorial covers correct initialization, alert handling, and the full bus-off recovery sequence — the most commonly misimplemented part.

---

## 1. Driver Lifecycle

The TWAI driver has a strict state machine. Operations are only valid in specific states.

```
[Uninstalled]
     │  twai_driver_install()
     ▼
[Stopped]
     │  twai_start()
     ▼
[Running] ◄──────────────────────────────────┐
     │  (128+ consecutive bus errors)         │
     ▼                                        │
[Bus-Off]                                     │
     │  twai_initiate_recovery()              │
     ▼                                        │
[Recovering]                                  │
     │  (hardware: 128 bus-free cycles)       │
     ▼                                        │
[Stopped]                                     │
     │  twai_start() ────────────────────────┘
     ▼
[Running]
```

**Key rule:** Recovery does not automatically return the driver to `Running`. After `TWAI_ALERT_BUS_RECOVERED` fires, you must call `twai_start()` yourself.

---

## 2. Initialization

```cpp
#include "driver/twai.h"

bool twai_init(gpio_num_t tx_pin, gpio_num_t rx_pin) {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(tx_pin, rx_pin, TWAI_MODE_NORMAL);
    twai_timing_config_t  t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t  f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) return false;
    if (twai_start() != ESP_OK) return false;

    uint32_t alerts =
        TWAI_ALERT_TX_IDLE      |
        TWAI_ALERT_TX_SUCCESS   |
        TWAI_ALERT_TX_FAILED    |
        TWAI_ALERT_ERR_PASS     |
        TWAI_ALERT_BUS_ERROR    |
        TWAI_ALERT_BUS_OFF      |
        TWAI_ALERT_BUS_RECOVERED;   // ← required for recovery completion

    if (twai_reconfigure_alerts(alerts, NULL) != ESP_OK) return false;

    return true;
}
```

### Mode selection

| Mode | Use case |
|------|----------|
| `TWAI_MODE_NORMAL` | Standard operation with ACK |
| `TWAI_MODE_NO_ACK` | Single-node testing — transmits without requiring ACK from another node |
| `TWAI_MODE_LISTEN_ONLY` | Passive monitoring, never transmits |

Use `TWAI_MODE_NO_ACK` only during development when no other CAN node is present. In production, `TWAI_MODE_NORMAL` is required so that missing ACKs correctly signal bus problems.

### Alert configuration persists across recovery

Alert masks are preserved through `twai_initiate_recovery()` and `twai_start()`. You only need to call `twai_reconfigure_alerts()` once after install.

---

## 3. Alert-Driven Loop Pattern

Poll alerts on a fixed interval using `twai_read_alerts()`. The timeout determines how long the call blocks waiting for any alert.

```cpp
void twai_poll() {
    uint32_t alerts;
    twai_read_alerts(&alerts, pdMS_TO_TICKS(1000)); // blocks up to 1s

    twai_status_info_t status;
    twai_get_status_info(&status);

    // --- Recovery path (handle before any TX logic) ---
    if (alerts & TWAI_ALERT_BUS_OFF) {
        // Enters TWAI_STATE_RECOVERING asynchronously
        twai_initiate_recovery();
        return;
    }
    if (alerts & TWAI_ALERT_BUS_RECOVERED) {
        // Driver is now in TWAI_STATE_STOPPED — must restart manually
        twai_start();
        return;
    }

    // --- Error diagnostics ---
    if (alerts & TWAI_ALERT_ERR_PASS) {
        // TEC or REC exceeded 127 — controller is now error-passive
        // Still functional, but will not dominate the bus on errors
    }
    if (alerts & TWAI_ALERT_TX_FAILED) {
        // Message could not be sent (e.g., arbitration lost repeatedly, bus-off)
    }
    if (alerts & TWAI_ALERT_TX_SUCCESS) {
        // Last queued message was acknowledged and sent
    }

    // --- Transmit (only when Running) ---
    if (status.state == TWAI_STATE_RUNNING) {
        // queue messages here
    }
}
```

---

## 4. Bus-Off Recovery — In Detail

Bus-off is triggered by the CAN hardware when the Transmit Error Counter (TEC) reaches 256. At that point the controller disconnects from the bus entirely.

### What `twai_initiate_recovery()` does

- Checks the driver is in `TWAI_STATE_BUS_OFF` (returns `ESP_ERR_INVALID_STATE` otherwise — safe to call redundantly)
- Resets the TX queue and clears pending messages
- Exits hardware reset mode, transitioning to `TWAI_STATE_RECOVERING`
- Returns immediately — **non-blocking**

### What happens next (in hardware/ISR)

The CAN controller counts 128 occurrences of the bus-free signal (11 consecutive recessive bits). When complete, the ISR transitions the driver to `TWAI_STATE_STOPPED` and fires `TWAI_ALERT_BUS_RECOVERED`.

### What the application must do

```cpp
if (alerts & TWAI_ALERT_BUS_RECOVERED) {
    twai_start(); // mandatory — driver will not resume on its own
}
```

Omitting this leaves the driver in `TWAI_STATE_STOPPED` indefinitely. No error is raised; transmissions simply silently fail.

### Common mistakes

| Mistake | Consequence |
|---------|-------------|
| Omitting `TWAI_ALERT_BUS_RECOVERED` from alert mask | Recovery completes silently; `twai_start()` is never called |
| Not calling `twai_start()` after `TWAI_ALERT_BUS_RECOVERED` | Driver stays `STOPPED`; all subsequent transmit calls fail |
| Calling `twai_initiate_recovery()` from `TWAI_STATE_RECOVERING` | Returns `ESP_ERR_INVALID_STATE` — harmless, but indicates a logic error |
| Assuming recovery is instant | Recovery requires 128 bus-free cycles at the configured baud rate (e.g., ~0.5 ms at 250 kbps per cycle — minimum ~64 ms total) |

---

## 5. Transmitting Messages

```cpp
twai_message_t msg = {};
msg.identifier       = 0x0F6;
msg.data_length_code = 8;
for (int i = 0; i < msg.data_length_code; i++) msg.data[i] = i;

esp_err_t result = twai_transmit(&msg, pdMS_TO_TICKS(1000));
if (result != ESP_OK) {
    // ESP_ERR_TIMEOUT   — TX queue full for 1000ms
    // ESP_ERR_INVALID_STATE — driver not in Running state
    // ESP_FAIL          — message lost (bus-off, etc.)
}
```

`twai_transmit()` enqueues the message into the TX FIFO and returns. Actual transmission is handled by the driver. Use `TWAI_ALERT_TX_SUCCESS` / `TWAI_ALERT_TX_FAILED` to confirm delivery.

### Extended vs. standard frames

```cpp
msg.extd = 0; // 0 = standard 11-bit ID, 1 = extended 29-bit ID
msg.rtr  = 0; // 0 = data frame, 1 = remote frame
```

The default zero-initialization covers both — include explicitly for clarity.

---

## 6. Error Counter Interpretation

```cpp
twai_status_info_t s;
twai_get_status_info(&s);

// s.tx_error_counter — TEC (0–255; bus-off triggers at 256)
// s.rx_error_counter — REC
// s.bus_error_count  — total bit/stuff/CRC/form/ACK errors seen
// s.tx_failed_count  — messages that could not be transmitted
// s.msgs_to_tx       — messages currently queued in TX buffer
// s.msgs_to_rx       — messages waiting in RX buffer
```

| TEC value | State |
|-----------|-------|
| 0–95 | Error-active (normal) |
| 96–127 | Warning threshold crossed (`TWAI_ALERT_ABOVE_ERR_WARN`) |
| 128–255 | Error-passive (`TWAI_ALERT_ERR_PASS`) |
| 256+ | Bus-off (`TWAI_ALERT_BUS_OFF`) |

---

## 7. Teardown

```cpp
twai_stop();
twai_driver_uninstall();
```

Always uninstall before reinstalling the driver (e.g., to change baud rate). Installing over an existing driver returns `ESP_ERR_INVALID_STATE`.

---

## 8. Quick Reference

```
twai_driver_install()     — allocates driver, state: Stopped
twai_start()              — begins bus operation, state: Running
twai_transmit()           — enqueues a message (Running state required)
twai_read_alerts()        — blocks until alert or timeout
twai_get_status_info()    — reads current error counters and state
twai_reconfigure_alerts() — change alert mask at runtime
twai_initiate_recovery()  — begins bus-off recovery (async), state: Recovering
                            → hardware fires TWAI_ALERT_BUS_RECOVERED when done
                            → application must then call twai_start()
twai_stop()               — halts bus operation, state: Stopped
twai_driver_uninstall()   — frees driver resources, state: Uninstalled
```
