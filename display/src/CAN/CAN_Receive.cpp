#include "CAN_Receive.h"

// ---------------------------------------------------------------------------
// Spinlock protecting DashboardBatteryVoltages.hv_cell_voltages writes/reads.
// Extern declaration in DashboardUI.h; used by Logging/logging.cpp for reads.
portMUX_TYPE g_cell_voltages_mux = portMUX_INITIALIZER_UNLOCKED;

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
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, (gpio_num_t)RX_PIN, TWAI_MODE_LISTEN_ONLY);
    twai_timing_config_t  t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t  f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        Serial.println("Failed to install driver");
        return false;
    }
    Serial.println("Driver installed");

    if (twai_start() != ESP_OK) {
        Serial.println("Failed to start driver");
        return false;
    }
    Serial.println("Driver started");

    uint32_t alerts_to_enable = TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL;
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
    uint32_t alerts_triggered;
    twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(POLLING_RATE_MS));
    twai_status_info_t twaistatus;
    twai_get_status_info(&twaistatus);

    if (alerts_triggered & TWAI_ALERT_ERR_PASS) {
        Serial.println("Alert: TWAI controller has become error passive.");
    }
    if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
        Serial.println("Alert: A (Bit, Stuff, CRC, Form, ACK) error has occurred on the bus.");
        Serial.printf("Bus error count: %d\n", twaistatus.bus_error_count);
    }
    if (alerts_triggered & TWAI_ALERT_RX_QUEUE_FULL) {
        Serial.println("Alert: The RX queue is full causing a received frame to be lost.");
        Serial.printf("RX buffered: %d\t", twaistatus.msgs_to_rx);
        Serial.printf("RX missed: %d\t",   twaistatus.rx_missed_count);
        Serial.printf("RX overrun %d\n",   twaistatus.rx_overrun_count);
    }

    if (alerts_triggered & TWAI_ALERT_RX_DATA) {
        twai_message_t message;
        while (twai_receive(&message, 0) == ESP_OK) {
            handle_rx_message(message, state);
        }
    }
}
