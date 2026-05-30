#include "CAN_Receive.h"
#include "CANDecoder.h"
#include "MainboardProtocol.h"

// ---------------------------------------------------------------------------
// Spinlock protecting BatteryVoltages.cell_voltages writes/reads.
// Extern declaration in DashboardUI.h; used by Logging/logging.cpp for reads.
portMUX_TYPE g_cell_voltages_mux = portMUX_INITIALIZER_UNLOCKED;

// Module-level TWAI config — stored so ERR_PASS recovery can reinstall the driver
// without re-passing parameters from main.cpp. Assigned in waveshare_twai_init().
static twai_general_config_t s_g_config;
static twai_timing_config_t  s_t_config;
static twai_filter_config_t  s_f_config;

// ---------------------------------------------------------------------------
// Dispatcher — called once per received message
// ---------------------------------------------------------------------------

static void handle_rx_message(twai_message_t &message, DashboardState *state)
{
    // Mainboard status is a point-to-point display message (the mainboard never
    // decodes its own status), so it's handled here rather than in the shared
    // CANDecoder dispatcher. Codes only — update_error_state() maps them to tips.
    if (message.identifier == MAINBOARD_STATUS_IND) {
        if (message.data_length_code >= MB_STATUS_DLC) {
            state->mb_hv_state.store(message.data[MB_STATUS_OFF_HV_STATE]);
            state->mb_fault_reason.store(message.data[MB_STATUS_OFF_FAULT]);
            state->mb_precharge_pct.store(message.data[MB_STATUS_OFF_PRECHARGE]);
            state->mb_last_rx_ms = millis();   // mainboard liveness watchdog
        }
        return;
    }

    bool recognized = CANDecoder::dispatch(
        message,
        &state->motor,
        &state->temps,
        &state->bms,
        &state->evcc,
        &state->charger,
        &state->thermistors,
        &state->battery,
        &g_cell_voltages_mux,
        const_cast<uint32_t*>(&state->motor_last_rx_ms),
        const_cast<uint32_t*>(&state->bms_last_rx_ms),
        const_cast<uint32_t*>(&state->charger_last_rx_ms),
        const_cast<uint32_t*>(&state->evcc_last_rx_ms)
    );

    if (!recognized) {
        // Unrecognized — log and continue
        Serial.printf("Received unrecognized CAN message ID: 0x%lX\n", message.identifier);
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool waveshare_twai_init()
{
    s_g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, (gpio_num_t)RX_PIN, TWAI_MODE_LISTEN_ONLY);
    s_t_config = TWAI_TIMING_CONFIG_250KBITS();
    s_f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&s_g_config, &s_t_config, &s_f_config) != ESP_OK) {
        Serial.println("Failed to install driver");
        return false;
    }
    Serial.println("Driver installed");

    if (twai_start() != ESP_OK) {
        Serial.println("Failed to start driver");
        return false;
    }
    Serial.println("Driver started");

    uint32_t alerts_to_enable = TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS
                              | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL
                              | TWAI_ALERT_BUS_OFF;   // D-05: required for CANR-01
    if (twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK) {
        Serial.println("CAN Alerts reconfigured");
    } else {
        Serial.println("Failed to reconfigure alerts");
        return false;
    }

    return true;
}

void waveshare_twai_receive(DashboardState *state)
{
    // No-data watchdog: track timestamp of last successfully decoded frame (D-03, CANU-04)
    static uint32_t last_rx_ms = 0;

    uint32_t alerts_triggered;
    twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(POLLING_RATE_MS));
    twai_status_info_t twaistatus;
    twai_get_status_info(&twaistatus);

    // -- BUS_OFF recovery (CANR-01, D-06) --
    if (alerts_triggered & TWAI_ALERT_BUS_OFF) {
        Serial.println("Alert: TWAI bus-off. Initiating recovery.");
        state->can_status.store(CanStatus::BUS_OFF);           // CANR-03: reflect fault before recovery
        if (twai_initiate_recovery() == ESP_OK) {
            if (twai_start() == ESP_OK) {
                Serial.println("TWAI bus-off recovery complete.");
                state->can_status.store(CanStatus::BOOT);   // D-09: BOOT after successful restart
            } else {
                Serial.println("TWAI twai_start() failed after bus-off recovery.");
            }
        } else {
            Serial.println("TWAI twai_initiate_recovery() failed.");
        }
    }

    // -- ERR_PASS recovery: full driver reinstall (CANR-02, D-07) --
    if (alerts_triggered & TWAI_ALERT_ERR_PASS) {
        Serial.println("Alert: TWAI error-passive. Reinstalling driver.");
        state->can_status.store(CanStatus::ERR_PASSIVE);       // CANR-03: reflect fault before recovery
        twai_stop();
        twai_driver_uninstall();
        if (twai_driver_install(&s_g_config, &s_t_config, &s_f_config) == ESP_OK) {
            if (twai_start() == ESP_OK) {
                Serial.println("TWAI driver reinstalled after ERR_PASS.");
                state->can_status.store(CanStatus::BOOT);   // D-09: BOOT after successful restart
                // Re-enable alerts — they are cleared by uninstall/reinstall cycle
                uint32_t alerts_to_enable = TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS
                                          | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL
                                          | TWAI_ALERT_BUS_OFF;
                if (twai_reconfigure_alerts(alerts_to_enable, NULL) != ESP_OK) {
                    Serial.println("TWAI failed to reconfigure alerts after reinstall.");
                }
            } else {
                Serial.println("TWAI twai_start() failed after reinstall.");
            }
        } else {
            Serial.println("TWAI twai_driver_install() failed during ERR_PASS recovery.");
        }
    }

    // -- BUS_ERROR: log only (no recovery action; mirrors original behavior) --
    if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
        Serial.println("Alert: A (Bit, Stuff, CRC, Form, ACK) error has occurred on the bus.");
        Serial.printf("Bus error count: %d\n", twaistatus.bus_error_count);
    }

    // -- RX_QUEUE_FULL: log only --
    if (alerts_triggered & TWAI_ALERT_RX_QUEUE_FULL) {
        Serial.println("Alert: The RX queue is full causing a received frame to be lost.");
        Serial.printf("RX buffered: %d\t", twaistatus.msgs_to_rx);
        Serial.printf("RX missed: %d\t",   twaistatus.rx_missed_count);
        Serial.printf("RX overrun %d\n",   twaistatus.rx_overrun_count);
    }

    // -- RX_DATA: decode frames, update watchdog and RECEIVING state (CANU-03, D-03) --
    if (alerts_triggered & TWAI_ALERT_RX_DATA) {
        twai_message_t message;
        while (twai_receive(&message, 0) == ESP_OK) {
            handle_rx_message(message, state);
            last_rx_ms = millis();                              // reset watchdog on each decoded frame
            state->can_status.store(CanStatus::RECEIVING);     // CANU-03: transition to RECEIVING
        }
    }

    // -- No-data watchdog: 1 s silence demotes RECEIVING → TIMEOUT (CANU-04, D-03) --
    // Only demote from RECEIVING — do not mask an active ERR_PASSIVE or BUS_OFF status.
    if (state->can_status.load() == CanStatus::RECEIVING &&
        last_rx_ms != 0 &&
        (millis() - last_rx_ms) > 1000U) {
        state->can_status.store(CanStatus::TIMEOUT);
    }

    // Process structured error logic after all messages are handled (v1.5)
    update_error_state(state);
}
