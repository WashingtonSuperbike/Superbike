/**
 * CANDecoder.h
 * 
 * Shared static inline functions for parsing CAN messages across the
 * Superbike firmware. This ensures both the mainboard and display
 * decode the raw CAN data consistently.
 */

#pragma once

#include "Types.h"
#include "Constants.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"

namespace CANDecoder {

static inline void decodeMotorStats(twai_message_t msg, MotorStats *motor_stats) {
    if (msg.data_length_code < 8) return;
    motor_stats->RPM = (float)((msg.data[1] << 8) | msg.data[0]);
    motor_stats->motor_current = ((msg.data[3] << 8) | msg.data[2]) / 10.0f;
    motor_stats->motor_controller_battery_voltage = ((msg.data[5] << 8) | msg.data[4]) / 10.0f;
    motor_stats->error_message = ((msg.data[7] << 8) | msg.data[6]);
}

static inline void decodeMotorTemps(twai_message_t msg, MotorTemps *motor_temps) {
    if (msg.data_length_code < 6) return;
    motor_temps->throttle = msg.data[0] / 255.0f;
    motor_temps->motor_controller_temperature = msg.data[1] - 40.0f;
    motor_temps->motor_temperature = msg.data[2] - 30.0f;
    motor_temps->controller_status = msg.data[4];
    motor_temps->switch_status = msg.data[5];
}

static inline void decipherBMSStatus(twai_message_t msg, BMSStatus *bms_status) {
    if (msg.data_length_code < 5) return;
    bms_status->bms_status_flag = (float)(msg.data[0]);
    bms_status->bms_c_id = msg.data[1];
    bms_status->bms_c_fault = msg.data[2];
    bms_status->ltc_fault = msg.data[3];
    bms_status->ltc_count = msg.data[4];
}

static inline void decipherEVCCStats(twai_message_t msg, ChargeControllerStats *evcc_stats) {
    if (msg.data_length_code < 5) return;
    evcc_stats->en = (msg.data[0]);
    evcc_stats->charge_voltage = ((msg.data[2] << 8) | msg.data[1]) / 10.0f;
    evcc_stats->charge_current = (3200.0f - ((msg.data[4] << 8) | msg.data[3])) / 10.0f;
}

static inline void decipherChargerStats(twai_message_t msg, ChargerStats *charger_stats) {    
    if (msg.data_length_code < 7) return;
    charger_stats->status_flag = msg.data[0];
    charger_stats->charge_flag = msg.data[1];
    charger_stats->output_voltage = ((msg.data[3] << 8) | msg.data[2]) / 10.0f;
    charger_stats->output_current = (3200.0f - ((msg.data[5] << 8) | msg.data[4])) / 10.0f;
    charger_stats->charger_temp = msg.data[6] - 40;
}

static inline void decipherThermistors(twai_message_t msg, ThermistorTemps *thermistor_temps) {
    if (msg.data_length_code < 8) return;
    uint8_t ltcID = msg.data[0];
    uint8_t *currentThermistor = &msg.data[3];
    for (int i = 0; i < 5; i++) {
        int idx = i + 5 * ltcID;
        if (idx < THERMISTOR_COUNT) {
            thermistor_temps->temps[idx] = currentThermistor[i];
            thermistor_temps->temps_valid[idx] = true;
        }
    }
}

/**
 * Shared cell voltage decoder. Handles bit-masking, offset calculation,
 * thread-safe writes (via optional mux), and readiness tracking.
 */
static inline void decipherCellsVoltage(twai_message_t msg, BatteryVoltages *battery, portMUX_TYPE *mux = nullptr) {
    if (msg.data_length_code < 8) return;

    uint32_t msgID = msg.identifier;
    int cellOffset = (((msgID >> 8) & 0xF) - 0x9);
    int ltcOffset = (msgID & 0xFF);
    int totalOffset = (cellOffset * 4) + (ltcOffset * 12);
    
    uint16_t *buf = (uint16_t *)msg.data;
    float parsed[4];
    for (int i = 0; i < 4; i++) {
        parsed[i] = ((float)buf[i]) / 10000.0f;
    }

    static bool hv_cell_detected[CELL_COUNT] = {false};

    if (mux) taskENTER_CRITICAL(mux);
    for (int i = 0; i < 4; i++) {
        int idx = i + totalOffset;
        if (idx >= 0 && idx < CELL_COUNT) {
            battery->hv_cell_voltages[idx] = parsed[i];
            hv_cell_detected[idx] = true;
        }
    }

    float sum = 0.0f;
    for (int i = 0; i < CELL_COUNT; i++) {
        sum += battery->hv_cell_voltages[i];
    }
    battery->hv_series_voltage = sum;
    if (mux) taskEXIT_CRITICAL(mux);

    if (!battery->hv_cell_voltages_ready) {
        bool all_detected = true;
        for (int i = 0; i < CELL_COUNT; i++) {
            if (!hv_cell_detected[i]) {
                all_detected = false;
                break;
            }
        }
        battery->hv_cell_voltages_ready = all_detected;
    }
}

/**
 * Unified Dispatcher
 * 
 * Maps an incoming TWAI message to the appropriate decoder.
 * Returns true if the message was recognized and decoded.
 */
 static inline bool dispatch(
     twai_message_t msg,
     MotorStats *motor = nullptr,
     MotorTemps *temps = nullptr,
     BMSStatus *bms = nullptr,
     ChargeControllerStats *evcc = nullptr,
     ChargerStats *charger = nullptr,
     ThermistorTemps *thermistors = nullptr,
     BatteryVoltages *battery = nullptr,
     portMUX_TYPE *cell_mux = nullptr,
     uint32_t *motor_last_rx = nullptr,
     uint32_t *bms_last_rx = nullptr,
     uint32_t *charger_last_rx = nullptr,
     uint32_t *evcc_last_rx = nullptr
 ) {
     uint32_t pgn = msg.identifier >> 8;
     uint32_t now = 0;
 #ifdef ARDUINO
     now = millis();
 #endif

     switch (pgn) {
         case (MOTOR_STATS_MSG >> 8):
             if (motor) decodeMotorStats(msg, motor);
             if (motor_last_rx) *motor_last_rx = now;
             return true;
         case (MOTOR_TEMPS_MSG >> 8):
             if (temps) decodeMotorTemps(msg, temps);
             if (motor_last_rx) *motor_last_rx = now;
             return true;
         case (DD_BMS_STATUS_IND >> 8):
             if (bms) decipherBMSStatus(msg, bms);
             if (bms_last_rx) *bms_last_rx = now;
             return true;
         case (EVCC_STATS >> 8):
             if (evcc) decipherEVCCStats(msg, evcc);
             if (evcc_last_rx) *evcc_last_rx = now;
             return true;
         case (CHARGER_STATS >> 8):
             if (charger) decipherChargerStats(msg, charger);
             if (charger_last_rx) *charger_last_rx = now;
             return true;
         case (DD_BMSC_TH_STATUS_IND >> 8):
             if (thermistors) decipherThermistors(msg, thermistors);
             if (bms_last_rx) *bms_last_rx = now;
             return true;
         case (BMSC1_LTC1_CELLS_04 >> 8):
         case (BMSC1_LTC1_CELLS_58 >> 8):
         case (BMSC1_LTC1_CELLS_912 >> 8):
             if (battery) decipherCellsVoltage(msg, battery, cell_mux);
             if (bms_last_rx) *bms_last_rx = now;
             return true;
         default:
             return false;
     }
 }
} // namespace CANDecoder
