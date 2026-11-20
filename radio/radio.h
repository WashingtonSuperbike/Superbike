#include <stdio.h>

struct RadioPacket {
    int arr[] = BatteryVoltages.hv_cell_voltages[CONFIG_HV_CELL_COUNT];
    int arr2[] = BatteryVoltages.temps[THERMISTOR_COUNT];
    int cellCount = sizeof(arr) /sizeof(arr[0]);
    int thermistorCount = sizeof(arr2) /sizeof(arr2[0]);
    int max, min;
    float max_cell_voltage = findMinMax(arr, cellCount, max, min); // 4 bytes
    float min_cell_voltage = findMinMax(arr, cellCount, max, min);
    float max_thermistor_temp = findMinMax(arr, thermistorCount, max, min); // 4 bytes
    float min_thermistor_temp = findMinMax(arr, thermistorCount, max, min); // 4 bytes
    int motorStatsError = MotorStats.error_message() // 4 bytes
	float throttle = MotorTemps.throttle // 4 bytes
	float RPM = MotorStats.RPM // 4 bytes
	float motor_controller_temperature = MotorTemps.motor_controller_temperature // 4 bytes
	float motor_controller_battery_voltage = MotorStats.motor_controller_battery_voltage // 4 bytes
	float motor_current = MotorStats.motor_current // 4 bytes
 	float hv_series_voltage = BatteryVoltages.hv_series_voltage // 4 bytes
  	float aux_battery_voltage = BatteryVoltages.aux_battery_voltage // 4 bytes
    float motorControllerTemp = MotorTemps.motor_controller_temperature // 4 bytes
    float motorTemp = MotorTemps.motor_temperature
    float throttleTemp = MotorTemps.throttle //4 bytes
// BMS Status
    uint8_t bms_status_flag //8 bits
    uint8_t bms_c_id //8 bits
    uint8_t bms_c_fault  //8 bits
    uint8_t ltc_fault  //8 bits
    bool cellVoltageStatus = BatteryVoltages.cell_voltages_ready // 1 byte
}

void findMinMax(int BatteryVoltages.cell_voltages[CELL_COUNT], int cellCount, int *max, int *min) {
    *max = arr[0];
    *min = arr[0];

    for (int i = 1; i < n; i++) {

        // Update max if arr[i] is larger
        if (arr[i] > *max)
            *max = arr[i];

        // Update min if arr[i] is smaller
        if (arr[i] < *min)
            *min = arr[i];
    }
}

