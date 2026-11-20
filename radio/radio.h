#include <stdio.h>

struct RadioPacket {
  	int n = CELL_COUNT;
    int[] arr = BatteryVoltages.hv_cell_voltages[CONFIG_HV_CELL_COUNT];
    // float max_cell_voltage = max of // 4 bytes
    // float min_cell_voltage = min of BatteryVoltages.cell_voltages[CELL_COUNT]
    float max_thermistor_temp = max of BatteryVoltages.temps[THERMISTOR_COUNT] // 4 bytes
    float min_thermistor_temp = min of BatteryVoltages.temps[THERMISTOR_COUNT] // 4 bytes
    Int motorStatsError = MotorStats.error_message() // 4 bytes
	Float throttle = MotorTemps.throttle // 4 bytes
	float RPM = MotorStats.RPM // 4 bytes
	float motor_controller_temperature = MotorTemps.motor_controller_temperature // 4 bytes
	Float motor_controller_battery_voltage = MotorStats.motor_controller_battery_voltage // 4 bytes
	Float motor_current = MotorStats.motor_current // 4 bytes
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

void findMinMax(int BatteryVoltages.cell_voltages[CELL_COUNT], int n, int *max, int *min) {
    *max = 
}
