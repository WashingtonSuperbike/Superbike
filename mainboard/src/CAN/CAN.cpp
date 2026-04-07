/**
    Manages the CAN bus interface. Receives messages and updates relevant variables in bike_context.
    If any errors are detected in the CAN nodes, prints the relevant error to syslog.
*/
#include "CAN.h"
#include <Arduino.h>
#include "driver/gpio.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Global message buffer for CAN reception
static twai_message_t message;

/**
 * Ensure CAN bus is initialized and configured properly
 */
 void initCAN()
{
    // Initialize configuration structures using macro initializers
    // TODO: Maybe adjust and utilize specific timing and filter configurations
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    //Set up TWAI driver (CAN for ESP32-S3)
    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        printf("Failed to install TWAI driver\n");
        return;
    }
    if (twai_start() != ESP_OK) {
        printf("Failed to start TWAI driver\n");
        return;
    }
}

void decipherEVCCStats(twai_message_t msg, ChargeControllerStats *evcc_stats)
{
    // Check that msg has at least 5 data bytes
    if (msg.data_length_code < 5) {
        printf("Invalid EVCC_STATS message length: %d\n", msg.data_length_code);
        return;
    }
    evcc_stats->en = (msg.data[0]);
    evcc_stats->charge_voltage = ((msg.data[2] << 8) | msg.data[1]) / 10.0;
    evcc_stats->charge_current = (3200 - ((msg.data[4] << 8) | msg.data[3])) / 10.0;
}

void decipherChargerStats(twai_message_t msg, ChargerStats *charger_stats)
{    
    // Check that msg has at least 7 data bytes
    if (msg.data_length_code < 7) {
        printf("Invalid CHARGER_STATS message length: %d\n", msg.data_length_code);
        return;
    }
    charger_stats->status_flag = msg.data[0];
    charger_stats->charge_flag = msg.data[1];
    charger_stats->output_voltage = ((msg.data[3] << 8) | msg.data[2]) / 10.0;
    charger_stats->output_current = (3200 - ((msg.data[5] << 8) | msg.data[4])) / 10.0;
    charger_stats->charger_temp = msg.data[6] - 40;
}

void decodeMotorStats(twai_message_t msg, MotorStats *motor_stats)
{
    // Check that msg has at least 8 data bytes
    if (msg.data_length_code < 8) {
        printf("Invalid MOTOR_STATS message length: %d\n", msg.data_length_code);
        return;
    }
    motor_stats->RPM = (float)((msg.data[1] << 8) | msg.data[0]);
    motor_stats->motor_current = ((msg.data[3] << 8) | msg.data[2]) / 10.0;
    motor_stats->motor_controller_battery_voltage = ((msg.data[5] << 8) | msg.data[4]) / 10.0;
    motor_stats->error_message = ((msg.data[7] << 8) | msg.data[6]);
}

void decodeMotorTemps(twai_message_t msg, MotorTemps *motor_temps)
{
    // Check that msg has at least 5 data bytes
    if (msg.data_length_code < 5) {
        printf("Invalid MOTOR_TEMPS message length: %d\n", msg.data_length_code);
        return;
    }
    motor_temps->throttle = msg.data[0] / 255.0;
    motor_temps->motor_controller_temperature = msg.data[1] - 40;
    motor_temps->motor_temperature = msg.data[2] - 30;
    motor_temps->controller_status = msg.data[4];
}

void decipherBMSStatus(twai_message_t msg, BMSStatus *bms_status)
{
    // Check that msg has at least 5 data bytes
    if (msg.data_length_code < 5) {
        printf("Invalid BMS_STATUS message length: %d\n", msg.data_length_code);
        return;
    }
    bms_status->bms_status_flag = (float)(msg.data[0]);
    bms_status->bms_c_id = msg.data[1];
    bms_status->bms_c_fault = msg.data[2];
    bms_status->ltc_fault = msg.data[3];
    bms_status->ltc_count = msg.data[4];
}

// sums the voltage of each cell in main accumulator
static void calculateSeriesVoltage(BatteryVoltages *battery_voltages)
{
    float partialSeriesVoltage = 0;
    int current_cell;
    for (current_cell = 0; current_cell < CONFIG_HV_CELL_COUNT; current_cell++)
    {
        partialSeriesVoltage += battery_voltages->hv_cell_voltages[current_cell];
    }
    battery_voltages->hv_series_voltage = partialSeriesVoltage;
}


void decipherCellsVoltage(twai_message_t msg, BatteryVoltages *battery_voltages)
{
    // Check that msg has at least 16 data bytes (4 cells x 4 bytes each)
    if (msg.data_length_code < 16) {
        printf("Invalid CELL_VOLTAGES message length: %d\n", msg.data_length_code);
        return;
    }
    // TODO: THE FOLLOWING DATATYPE NEEDS TO BE CHANGED
    uint32_t msgID = msg.identifier;
    int totalOffset = 0; // totalOffset equals the index of array cellVoltages
    int cellOffset = (((msgID >> 8) & 0xF) - 0x9);
    int ltcOffset = (msgID & 0x1);
    totalOffset = (cellOffset * 4) + (ltcOffset * 12);
    int cellIndex;

    static bool hv_cell_detected[CONFIG_HV_CELL_COUNT];

    for (cellIndex = 0; cellIndex < 4; cellIndex++)
    {
        uint16_t *buf = (uint16_t *)msg.data;
        battery_voltages->hv_cell_voltages[cellIndex + totalOffset] = ((float)buf[cellIndex]) / 10000;
        hv_cell_detected[cellIndex + totalOffset] = true;
    }

    calculateSeriesVoltage(battery_voltages);

    if (!battery_voltages->hv_cell_voltages_ready)
    {
        for (int i = 0; i < CONFIG_HV_CELL_COUNT; i++)
        {
            if (!hv_cell_detected[i])
            {
                return;
            }
        }
        battery_voltages->hv_cell_voltages_ready = true;
    }
}

void decipherThermistors(twai_message_t msg, ThermistorTemps *thermistor_temps)
{
    // Check that msg has at least 7 data bytes
    if (msg.data_length_code < 16) {
        printf("Invalid DD_BMSC_TH_STATUS_IND message length: %d\n", msg.data_length_code);
        return;
    }
    byte ltcID = msg.data[0];
    /* review datasheet for these 2 bytes *
    thermistorE = msg.data[1];
    thermistorPresent = msg.data[2];
     *                                    */
    byte *currentThermistor = &msg.data[3];
    int thermistor;
    for (thermistor = 0; thermistor < 5; thermistor++)
    {
        thermistor_temps->temps[thermistor + 5 * ltcID] = currentThermistor[thermistor];
        thermistor_temps->temps_valid[thermistor + 5 + ltcID] = true;
    }
}


// used to print the contents of a CAN msg
void printMessage(twai_message_t msg)
{
    for (int i = 0; i < msg.data_length_code; i++)
    {
        Serial.print(msg.data[i]);
        Serial.print(":");
    }
    Serial.println();
}

// unused currently but should be implemented into the current firmware
void printBMSStatus(BMSStatus bms_status)
{
    switch ((int)bms_status.bms_status_flag)
    {
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
    switch (bms_status.bms_c_fault)
    {
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
    if (bms_status.ltc_fault == 1)
    {
        Serial.printf("LTC fault detected\n");
    }
    Serial.printf("%d LTCs detected\n");
}

void requestCellVoltages()
{
    static int next_can_id = BMSC1_LTC1_REQUEST_CELLS;
    twai_message_t cellVoltageRxMsg = {};
    cellVoltageRxMsg.identifier = next_can_id;
    cellVoltageRxMsg.extd = 1; // Extended 29-bit ID
    cellVoltageRxMsg.rtr = 1;  // Remote transmission request
    cellVoltageRxMsg.data_length_code = 0; // no data payload

    if (twai_transmit(&cellVoltageRxMsg, pdMS_TO_TICKS(1000)) != ESP_OK) {
        printf("Failed to request cell voltages\n");
    }

    // Toggle between requesting from LTC1 and LTC2
    next_can_id = next_can_id == BMSC1_LTC1_REQUEST_CELLS ?
        BMSC1_LTC2_REQUEST_CELLS :
        BMSC1_LTC1_REQUEST_CELLS;
}

/**
 * Checks the CAN bus for any buffered messages and decodes one into the bike context.
 * 
 * @param canData struct containing pointer to bike context
 *  */ 
static void checkCAN(superbike::CANTaskData canData)
{
    superbike::Context *context = canData.bike_context;

    if (twai_receive(&message, pdMS_TO_TICKS(0)) != ESP_OK) {
        // No message received
        return;
    }

    // Check what kind of message we received and decode accordingly
    switch (message.identifier)
    {
    case MOTOR_STATS_MSG:
        decodeMotorStats(message, &(context->motor_stats));
        break;
    case MOTOR_TEMPS_MSG:
        decodeMotorTemps(message, &(context->motor_temps));
        break;
    case DD_BMS_STATUS_IND:
        decipherBMSStatus(message, &(context->bms_status));
        // printBMSStatus(context->bms_status);
        break;
    case EVCC_STATS:
        decipherEVCCStats(message, &(context->charge_controller_stats));
        break;
    case CHARGER_STATS:
        decipherChargerStats(message, &(context->charger_stats));
    case BMSC1_LTC1_CELLS_04:
        decipherCellsVoltage(message, &(context->battery_voltages));
        break;
    case BMSC1_LTC1_CELLS_58:
        decipherCellsVoltage(message, &(context->battery_voltages));
        break;
    case BMSC1_LTC1_CELLS_912:
        decipherCellsVoltage(message, &(context->battery_voltages));
        break;
    case BMSC1_LTC2_CELLS_04:
        decipherCellsVoltage(message, &(context->battery_voltages));
        break;
    case BMSC1_LTC2_CELLS_58:
        decipherCellsVoltage(message, &(context->battery_voltages));
        break;
    case BMSC1_LTC2_CELLS_912:
        decipherCellsVoltage(message, &(context->battery_voltages));
        break;
    case DD_BMSC_TH_STATUS_IND:
        decipherThermistors(message, &(context->thermistor_temps));
        break;
    default:
        // Unrecognized message ID
        printf("Received unrecognized CAN message ID: 0x%lX\n", message.identifier);
        break;

    }
}

/**
 * A FreeRTOS task that continuously checks the CAN bus for new messages
 * 
 * @param canData pointer to the CAN task data structure
 */
void canTask(void *canData)
{
    TickType_t last_request = xTaskGetTickCount();
    int requests = 0;
    while (1)
    {
        /* NOTE: CAN breaks if we try sending messages with 0 other nodes on the bus.
        / Therefore, change CAN_NODES in Main.h to make sure things dont break. */
        if (CAN_NODES != 0)
        {
            checkCAN(*(superbike::CANTaskData *)canData);
            /* Ask for other half of cell voltages from BMS every 2 seconds, 
            test timings later to improve boot performance */
            if (xTaskGetTickCount() > (last_request + 2000))
            {
                requestCellVoltages();
                last_request = xTaskGetTickCount();
            }
        }
        // delay 20ms
        vTaskDelay((20 * configTICK_RATE_HZ) / 1000);
    }
}
