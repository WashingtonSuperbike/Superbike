/**
 * rtc.cpp - PCF85063A RTC driver
 *
 * Ported from Waveshare waveshare_pcf85063a.cpp with modifications:
 *   - Wire.begin() call removed — bus is already initialized by board->begin()
 *   - Only rtc_init() and rtc_get_filename() are exported (public API)
 *   - Alarm, Set, Reset functions are excluded (not needed in Phase 2)
 *   - datetime_to_str() excluded — its format is wrong for filenames
 *   - I2C helpers and BCD conversion functions are file-scoped static
 */

#include "driver/i2c.h"
#include <Arduino.h>
#include "rtc.h"

#define RTC_I2C_PORT        I2C_NUM_0
#define RTC_I2C_TIMEOUT_MS  pdMS_TO_TICKS(50)

// ============================================================================
// Type definitions
// ============================================================================

#define UBYTE   uint8_t
#define UWORD   uint16_t

typedef struct {
    UWORD year;
    UBYTE month;
    UBYTE day;
    UBYTE dotw;
    UBYTE hour;
    UBYTE min;
    UBYTE sec;
} datetime_t;

// ============================================================================
// Register defines
// ============================================================================

#define PCF85063A_ADDRESS   (0x51)
#define YEAR_OFFSET         (2000)
#define RTC_CTRL_1_ADDR     (0x00)
#define RTC_CTRL_1_DEFAULT  (0x00)
#define RTC_CTRL_1_CAP_SEL  (0x01)
#define RTC_SECOND_ADDR     (0x04)

// ============================================================================
// Static I2C helpers
// ============================================================================

static esp_err_t DEV_I2C_Write_Byte(uint8_t addr, uint8_t reg, uint8_t Value) {
    uint8_t buf[2] = {reg, Value};
    return i2c_master_write_to_device(RTC_I2C_PORT, addr, buf, sizeof(buf), RTC_I2C_TIMEOUT_MS);
}

static esp_err_t DEV_I2C_Write_nByte(uint8_t addr, uint8_t *pData, uint32_t Len) {
    return i2c_master_write_to_device(RTC_I2C_PORT, addr, pData, Len, RTC_I2C_TIMEOUT_MS);
}

static esp_err_t DEV_I2C_Read_Byte(uint8_t addr, uint8_t reg, uint8_t *data) {
    return i2c_master_write_read_device(RTC_I2C_PORT, addr, &reg, 1, data, 1, RTC_I2C_TIMEOUT_MS);
}

static esp_err_t DEV_I2C_Read_nByte(uint8_t addr, uint8_t reg, uint8_t *pData, uint32_t Len) {
    return i2c_master_write_read_device(RTC_I2C_PORT, addr, &reg, 1, pData, Len, RTC_I2C_TIMEOUT_MS);
}

// ============================================================================
// Static BCD helpers
// ============================================================================

static uint8_t decToBcd(UBYTE val) {
    return (uint8_t)((val / 10 * 16) + (val % 10));
}

static int bcdToDec(uint8_t val) {
    return (int)((val / 16 * 10) + (val % 16));
}

// ============================================================================
// PCF85063A read function
// ============================================================================

static esp_err_t PCF85063A_Read_now(datetime_t *time) {
    uint8_t bufss[7] = {0};
    esp_err_t err = DEV_I2C_Read_nByte(PCF85063A_ADDRESS, RTC_SECOND_ADDR, bufss, 7);
    if (err != ESP_OK) return err;
    time->sec   = bcdToDec(bufss[0] & 0x7F);
    time->min   = bcdToDec(bufss[1] & 0x7F);
    time->hour  = bcdToDec(bufss[2] & 0x3F);
    time->day   = bcdToDec(bufss[3] & 0x3F);
    time->dotw  = bcdToDec(bufss[4] & 0x07);
    time->month = bcdToDec(bufss[5] & 0x1F);
    time->year  = bcdToDec(bufss[6]) + YEAR_OFFSET;
    return ESP_OK;
}

// ============================================================================
// Public API
// ============================================================================

void rtc_init() {
    uint8_t value = RTC_CTRL_1_DEFAULT | RTC_CTRL_1_CAP_SEL;
    DEV_I2C_Write_Byte(PCF85063A_ADDRESS, RTC_CTRL_1_ADDR, value);
    Serial.println("RTC PCF85063A initialized (no Wire.begin)");
}

void rtc_get_filename(char *buf, size_t buflen) {
    datetime_t t = {};
    esp_err_t err = PCF85063A_Read_now(&t);
    if (err != ESP_OK) {
        snprintf(buf, buflen, "RTC_ERR_%d.csv", (int)err);
        Serial.printf("RTC read failed: %d\n", (int)err);
        return;
    }
    snprintf(buf, buflen, "%04d-%02d-%02d_%02d%02d%02d.csv",
             (int)t.year, (int)t.month, (int)t.day,
             (int)t.hour, (int)t.min, (int)t.sec);
}
