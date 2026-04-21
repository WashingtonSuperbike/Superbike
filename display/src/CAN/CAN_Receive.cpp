#include "CAN_Receive.h"

// ---------------------------------------------------------------------------
// Spinlock protecting DashboardBatteryVoltages.hv_cell_voltages writes/reads.
// Extern declaration in DashboardUI.h; used by Logging/logging.cpp for reads.
portMUX_TYPE g_cell_voltages_mux = portMUX_INITIALIZER_UNLOCKED;

// Module-level TWAI config — stored so ERR_PASS recovery can reinstall the driver
// without re-passing parameters from main.cpp. Assigned in waveshare_twai_init().
static twai_general_config_t s_g_config;
static twai_timing_config_t  s_t_config;
static twai_filter_config_t  s_f_config;

// ---------------------------------------------------------------------------
// Decode helpers — mirror mainboard/src/CAN/CAN.cpp with Dashboard types
// ---------------------------------------------------------------------------

static void decodeMotorStats(twai_message_t msg, DashboardMotorStats *motor)
{
    if (msg.data_length_code < 8) {
        Serial.printf("Invalid MOTOR_STATS message length: %d\n", msg.data_length_code);
        return;
    }
    motor->RPM                              = (float)((msg.data[1] << 8) | msg.data[0]);
    motor->motor_current                    = ((msg.data[3] << 8) | msg.data[2]) / 10.0f;
    motor->motor_controller_battery_voltage = ((msg.data[5] << 8) | msg.data[4]) / 10.0f;
    motor->error_message                    = ((msg.data[7] << 8) | msg.data[6]);
}

static void decodeMotorTemps(twai_message_t msg, DashboardMotorTemps *temps)
{
    if (msg.data_length_code < 5) {
        Serial.printf("Invalid MOTOR_TEMPS message length: %d\n", msg.data_length_code);
        return;
    }
    temps->throttle                     = msg.data[0] / 255.0f;
    temps->motor_controller_temperature = msg.data[1] - 40;
    temps->motor_temperature            = msg.data[2] - 30;
    temps->controller_status            = msg.data[4];
}

static void decipherBMSStatus(twai_message_t msg, DashboardBMSStatus *bms)
{
    if (msg.data_length_code < 5) {
        Serial.printf("Invalid BMS_STATUS message length: %d\n", msg.data_length_code);
        return;
    }
    bms->bms_status_flag = (float)(msg.data[0]);
    bms->bms_c_id    = msg.data[1];
    bms->bms_c_fault = msg.data[2];
    bms->ltc_fault   = msg.data[3];
    bms->ltc_count   = msg.data[4];
}

static void decipherCellsVoltage(twai_message_t msg, DashboardBatteryVoltages *battery)
{
    if (msg.data_length_code < 8) {
        Serial.printf("Invalid CELL_VOLTAGES message length: %d\n", msg.data_length_code);
        return;
    }

    uint32_t msgID  = msg.identifier;
    int cellOffset  = (((msgID >> 8) & 0xF) - 0x9);
    int ltcOffset   = (msgID & 0x1);
    int totalOffset = (cellOffset * 4) + (ltcOffset * 12);

    uint16_t *buf = (uint16_t *)msg.data;
    taskENTER_CRITICAL(&g_cell_voltages_mux);
    for (int i = 0; i < 4; i++) {
        if (i + totalOffset < CONFIG_HV_CELL_COUNT) {
            battery->hv_cell_voltages[i + totalOffset] = ((float)buf[i]) / 10000.0f;
        }
    }
    taskEXIT_CRITICAL(&g_cell_voltages_mux);

    float sum = 0.0f;
    for (int i = 0; i < CONFIG_HV_CELL_COUNT; i++) {
        sum += battery->hv_cell_voltages[i];
    }
    battery->hv_series_voltage = sum;
}

static void decipherThermistors(twai_message_t msg, DashboardThermistorTemps *thermistors)
{
    if (msg.data_length_code < 8) {
        Serial.printf("Invalid DD_BMSC_TH_STATUS_IND message length: %d\n", msg.data_length_code);
        return;
    }
    byte ltcID = msg.data[0];
    byte *currentThermistor = &msg.data[3];
    for (int i = 0; i < 5; i++) {
        int idx = i + 5 * ltcID;
        if (idx < DASHBOARD_THERMISTOR_COUNT) {
            thermistors->temps[idx] = currentThermistor[i];
        }
    }
}

// ---------------------------------------------------------------------------
// Dispatcher — called once per received message
// ---------------------------------------------------------------------------

static void handle_rx_message(twai_message_t &message, DashboardState *state)
{
    switch (message.identifier)
    {
    case MOTOR_STATS_MSG:
        decodeMotorStats(message, &state->motor);
        break;
    case MOTOR_TEMPS_MSG:
        decodeMotorTemps(message, &state->temps);
        break;
    case DD_BMS_STATUS_IND:
        decipherBMSStatus(message, &state->bms);
        break;
    case BMSC1_LTC1_CELLS_04:
    case BMSC1_LTC1_CELLS_58:
    case BMSC1_LTC1_CELLS_912:
    case BMSC1_LTC2_CELLS_04:
    case BMSC1_LTC2_CELLS_58:
    case BMSC1_LTC2_CELLS_912:
        decipherCellsVoltage(message, &state->battery);
        break;
    case DD_BMSC_TH_STATUS_IND:
        decipherThermistors(message, &state->thermistors);
        break;
    default:
        // Unrecognized — log and continue
        Serial.printf("Received unrecognized CAN message ID: 0x%lX\n", message.identifier);
        break;
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
