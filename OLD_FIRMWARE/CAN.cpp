/**
    Manages the CAN bus interface. Receives messages and updates relevant variables in bike_context.
    If any errors are detected in the CAN nodes, prints the relevant error to syslog.
*/
#include "CAN.h"
#include "arduino_freertos.h"
#include "avr/pgmspace.h"
#include "MainboardProtocol.h"

// FlexCAN_T4 uses HEX and constrain directly; bridge both from the arduino:: namespace
using arduino::HEX;
#ifndef constrain
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#endif

#include "FlexCAN_T4.h"

/* CAN bus handle */
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> CAN_bus;

/* CAN struct for reading/writing */
CAN_message_t CAN_msg;

void initCAN() {
  CAN_bus.begin();
  CAN_bus.setBaudRate(250000);
}

void decipherEVCCStats(CAN_message_t msg, ChargeControllerStats *evcc_stats) {
  evcc_stats->en = (msg.buf[0]);
  evcc_stats->charge_voltage = ((msg.buf[2] << 8) | msg.buf[1]) / 10.0;
  evcc_stats->charge_current = (3200 - ((msg.buf[4] << 8) | msg.buf[3])) / 10.0;
}

void decipherChargerStats(CAN_message_t msg, ChargerStats *charger_stats) {
  charger_stats->status_flag = msg.buf[0];
  charger_stats->charge_flag = msg.buf[1];
  charger_stats->output_voltage = ((msg.buf[3] << 8) | msg.buf[2]) / 10.0;
  charger_stats->output_current = (3200 - ((msg.buf[5] << 8) | msg.buf[4])) / 10.0;
  charger_stats->charger_temp = msg.buf[6] - 40;
}

void decodeMotorStats(CAN_message_t msg, MotorStats *motor_stats) {
  motor_stats->RPM = (float) ((msg.buf[1] << 8) | msg.buf[0]);
  motor_stats->motor_current = ((msg.buf[3] << 8) | msg.buf[2]) / 10.0;
  motor_stats->motor_controller_battery_voltage = ((msg.buf[5] << 8) | msg.buf[4]) / 10.0;
  motor_stats->error_message = ((msg.buf[7] << 8) | msg.buf[6]);
}

void decodeMotorTemps(CAN_message_t msg, MotorTemps *motor_temps) {
  motor_temps->throttle = msg.buf[0] / 255.0;
  motor_temps->motor_controller_temperature = msg.buf[1] - 40;
  motor_temps->motor_temperature = msg.buf[2] - 30;
  motor_temps->controller_status = msg.buf[4];
}

void decipherBMSStatus(CAN_message_t msg, BMSStatus *bms_status) {
  bms_status->bms_status_flag = msg.buf[0];
  bms_status->bms_c_id = msg.buf[1];
  bms_status->bms_c_fault = msg.buf[2];
  bms_status->ltc_fault = msg.buf[3];
  bms_status->ltc_count = msg.buf[4];
}

// sums the voltage of each cell in main accumulator
static void calculateSeriesVoltage(BatteryVoltages *battery_voltages) {
  float partialSeriesVoltage = 0;
  int current_cell;
  for (current_cell = 0; current_cell < CELL_COUNT; current_cell++) {
    partialSeriesVoltage += battery_voltages->cell_voltages[current_cell];
  }
  battery_voltages->hv_series_voltage = partialSeriesVoltage;
}

void decipherCellsVoltage(CAN_message_t msg, BatteryVoltages *battery_voltages) {
  // THE FOLLOWING DATATYPE NEEDS TO BE CHANGED
  uint32_t msgID = msg.id;
  int totalOffset = 0; // totalOffset equals the index of array cellVoltages
  int cellOffset = (((msgID >> 8) & 0xF) - 0x9);
  int ltcOffset = (msgID & 0x1);
  totalOffset = (cellOffset * 4) + (ltcOffset * 12);

  if (totalOffset < 0 || totalOffset + 4 > CELL_COUNT)
    return;

  int cellIndex;

  static bool hv_cell_detected[CELL_COUNT];

  for (cellIndex = 0; cellIndex < 4; cellIndex++) {
    // Safe load: memcpy avoids the strict-aliasing/unaligned-read UB of casting
    // msg.buf to uint16_t*. Field renamed to cell_voltages per display_bringup.
    uint16_t raw;
    memcpy(&raw, &msg.buf[cellIndex * 2], sizeof(raw));
    battery_voltages->cell_voltages[cellIndex + totalOffset] = ((float)raw) / 10000;
    hv_cell_detected[cellIndex + totalOffset] = true;
  }
  
  calculateSeriesVoltage(battery_voltages);

  if (!battery_voltages->cell_voltages_ready) {
    for (int i = 0; i < CELL_COUNT; i++ ) {
      if (!hv_cell_detected[i]) {
        return;
      }
    }
    battery_voltages->cell_voltages_ready = true;
  }
}

void decipherThermistors(CAN_message_t msg, ThermistorTemps *thermistor_temps) {
  byte ltcID = msg.buf[0];
  /* review datasheet for these 2 bytes *
  thermistorE = msg.buf[1];
  thermistorPresent = msg.buf[2];
   *                                    */
  byte *currentThermistor = &msg.buf[3];
  int thermistor;
  for (thermistor = 0; thermistor < 5; thermistor++) {
    // ltcID comes straight off the CAN bus (0-255) and is untrusted. Bound the
    // destination index to the array so a malformed/unexpected frame can never
    // write past temps[]/temps_valid[] into adjacent Context fields (e.g.
    // battery_voltages, which gates HV). Out-of-range thermistor data is dropped.
    int idx = thermistor + 5 * ltcID;
    if (idx < 0 || idx >= THERMISTOR_COUNT) {
      continue;
    }
    thermistor_temps->temps[idx] = currentThermistor[thermistor];
    thermistor_temps->temps_valid[idx] = true;
  }
}

// used to print the contents of a CAN msg
void printMessage(CAN_message_t msg) {
  for (int i = 0; i < msg.len; i++) {
    Serial.print(msg.buf[i]);
    Serial.print(":");
  }
  Serial.println();
}

// checks the can bus for any new data
static void checkCAN(CANTaskData canData) {
  Context *context = canData.bike_context;
  if (CAN_bus.read(CAN_msg)) { // if we read non-zero # of bytes
    // Any frame is proof the bus is alive — feed the CAN-liveness watchdog
    // (CAN-1.1 in isHVSafe) and the recovery logic in canTask.
    context->last_can_rx_tick = xTaskGetTickCount();
    // Serial.printf("Received CAN Message with id %x, buffer[0] = %x\n", CAN_msg.id, CAN_msg.buf[0]);
    switch (CAN_msg.id) {
      case MOTOR_STATS_MSG:
        decodeMotorStats(CAN_msg, &(context->motor_stats));
        break;
      case MOTOR_TEMPS_MSG:
        decodeMotorTemps(CAN_msg, &(context->motor_temps));
        break;
      case DD_BMS_STATUS_IND:
        decipherBMSStatus(CAN_msg, &(context->bms_status));
        context->last_bms_rx_tick = xTaskGetTickCount();
        break;
      case EVCC_STATS:
        decipherEVCCStats(CAN_msg, &(context->charge_controller_stats));
        break;
      case CHARGER_STATS:
        decipherChargerStats(CAN_msg, &(context->charger_stats));
        break;
      case BMSC1_LTC1_CELLS_04:
        decipherCellsVoltage(CAN_msg,  &(context->battery_voltages));
        context->last_bms_rx_tick = xTaskGetTickCount();
        break;
      case BMSC1_LTC1_CELLS_58:
        decipherCellsVoltage(CAN_msg,  &(context->battery_voltages));
        context->last_bms_rx_tick = xTaskGetTickCount();
        break;
      case BMSC1_LTC1_CELLS_912:
        decipherCellsVoltage(CAN_msg,  &(context->battery_voltages));
        context->last_bms_rx_tick = xTaskGetTickCount();
        break;
      case BMSC1_LTC2_CELLS_04:
        decipherCellsVoltage(CAN_msg,  &(context->battery_voltages));
        context->last_bms_rx_tick = xTaskGetTickCount();
        break;
      case BMSC1_LTC2_CELLS_58:
        decipherCellsVoltage(CAN_msg,  &(context->battery_voltages));
        context->last_bms_rx_tick = xTaskGetTickCount();
        break;
      case BMSC1_LTC2_CELLS_912:
        decipherCellsVoltage(CAN_msg,  &(context->battery_voltages));
        context->last_bms_rx_tick = xTaskGetTickCount();
        break;
      case DD_BMSC_TH_STATUS_IND:
        decipherThermistors(CAN_msg, &(context->thermistor_temps));
        context->last_bms_rx_tick = xTaskGetTickCount();
        break;
    }
  }
}

// unused currently but should be implemented into the current firmware
void printBMSStatus(BMSStatus bms_status) {
  switch ((int)bms_status.bms_status_flag) {
    case 1:
      Serial.printf("at least one cell V is > High Voltage Cutoff\n");
      break;
    case 2:
      Serial.printf("at least one cell V is < Low Voltage Cutoff\n");
      break;
    case 4:
      Serial.printf("at least one cell V is > Balance Voltage Cutoff\n");
      break;
  }
  Serial.printf("The BMSC ID is %d\n", bms_status.bms_c_id);
  switch (bms_status.bms_c_fault) {
    case 1:
      Serial.printf("BMS Fault: configuration not locked\n");
      break;
    case 2:
      Serial.printf("BMS Fault: not all cells present\n");
      break;
    case 4:
      Serial.printf("BMS Fault: thermistor overtemp\n");
      break;
    case 8:
      Serial.printf("BMS Fault: not all thermistors present\n");
      break;
  }
  if (bms_status.ltc_fault == 1) {
    Serial.printf("LTC fault detected\n");
  }
  // Serial.printf("%d LTCs detected\n");
}

void requestCellVoltages() {
  static int next_can_id = BMSC1_LTC1_REQUEST_CELLS;
  static CAN_message_t msg = {};
  msg.flags.extended = 1; // set for 29-bit IDs
  msg.id = next_can_id;
  CAN_bus.write(msg);
  // Serial.println("Requested cell voltages");

  if (msg.id == BMSC1_LTC1_REQUEST_CELLS)
    next_can_id = BMSC1_LTC2_REQUEST_CELLS;
  else
    next_can_id = BMSC1_LTC1_REQUEST_CELLS;
}

// Broadcast the mainboard's own derived state for the display. Codes only —
// the display maps (hv_state, fault_reason) to rider-facing tip text. The
// rolling counter lets the display detect a frozen/dead mainboard.
static void sendMainboardStatus() {
  static uint8_t counter = 0;
  CAN_message_t msg = {};
  msg.flags.extended = 1;            // 29-bit ID
  msg.id = MAINBOARD_STATUS_IND;
  msg.len = MB_STATUS_DLC;
  msg.buf[MB_STATUS_OFF_HV_STATE]  = (uint8_t)get_hv_state();
  msg.buf[MB_STATUS_OFF_FAULT]     = get_hv_fault_reason();
  msg.buf[MB_STATUS_OFF_COUNTER]   = counter++;
  msg.buf[MB_STATUS_OFF_PRECHARGE] = get_precharge_pct();
  CAN_bus.write(msg);
}

// Reinstall the FlexCAN controller after prolonged bus silence (bus-off /
// transceiver fault). FlexCAN auto-recovers from bus-off by default; this is a
// belt-and-suspenders restore. The HV safe-state is handled separately by the
// CAN-1.1 liveness check in isHVSafe(), which trips HV_ERROR while silent.
static void recoverCAN() {
  Serial.println("CAN bus silent — reinstalling controller");
  CAN_bus.reset();
  CAN_bus.begin();
  CAN_bus.setBaudRate(250000);
}

void canTask(void *canData) {
  Context *context = ((CANTaskData *)canData)->bike_context;
  TickType_t last_request = xTaskGetTickCount();
  TickType_t last_status  = xTaskGetTickCount();
  TickType_t last_recovery = xTaskGetTickCount();
  while (1) {
    /* NOTE: CAN breaks if we try sending messages with 0 other nodes on the bus.
    / Therefore, change CAN_NODES in Main.h to make sure things dont break. */
    if (CAN_NODES != 0) {
      checkCAN(*(CANTaskData *)canData);
      /* Ask for other half of cell voltages from BMS every 2 seconds, test timings later to improve boot performance */
      if (xTaskGetTickCount() - last_request > pdMS_TO_TICKS(2000)) {
        requestCellVoltages();
        last_request = xTaskGetTickCount();
      }
      /* Broadcast mainboard status for the display at MB_STATUS_PERIOD_MS (10 Hz). */
      if (xTaskGetTickCount() - last_status > pdMS_TO_TICKS(MB_STATUS_PERIOD_MS)) {
        sendMainboardStatus();
        last_status = xTaskGetTickCount();
      }
      /* Bus-off recovery: if no frame has arrived for CAN_BUS_TIMEOUT_MS, attempt
         a controller reinstall, rate-limited to once per CAN_BUS_TIMEOUT_MS. */
      uint32_t last_rx = context->last_can_rx_tick;
      if (last_rx != 0 &&
          (xTaskGetTickCount() - last_rx) > pdMS_TO_TICKS(CAN_BUS_TIMEOUT_MS) &&
          (xTaskGetTickCount() - last_recovery) > pdMS_TO_TICKS(CAN_BUS_TIMEOUT_MS)) {
        recoverCAN();
        last_recovery = xTaskGetTickCount();
      }
    }
    // delay 20ms
    vTaskDelay((20 * configTICK_RATE_HZ) / 1000);
  }
}
