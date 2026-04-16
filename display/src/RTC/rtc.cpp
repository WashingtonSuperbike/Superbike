/**
 * rtc.cpp - PCF85063A RTC driver
 *
 * Ported from Waveshare waveshare_pcf85063a.cpp with modifications:
 *   - Wire.begin() call removed — bus is already initialized by board->begin()
 *   - Only rtc_init() and rtc_get_filename() are exported (public API)
 *   - Alarm, Set, Reset functions are excluded (not needed in Phase 2)
 *   - datetime_to_str() excluded — its format is wrong for filenames
 *   - I2C helpers and BCD conversion functions are file-scoped static
 *   - parse_compile_datetime() and PCF85063A_Write_now() added in Phase 3
 *     to handle the no-battery epoch-reset bug: on power-on the chip defaults
 *     to 2000-01-01; rtc_init() detects this and writes compile-time fallback.
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
// Compile-time fallback helpers
// ============================================================================

/**
 * parse_compile_datetime() - Parse __DATE__ and __TIME__ macros into datetime_t.
 *
 * __DATE__ format: "Mmm DD YYYY" (e.g. "Apr 16 2026")
 * __TIME__ format: "HH:MM:SS"   (e.g. "14:30:05")
 *
 * Result is a best-effort approximation; dotw is set to 0 (not needed for
 * filenames). Used only when the RTC reports year == 2000 (power-on default).
 */
static void parse_compile_datetime(datetime_t *t) {
    // Month lookup: position 0=Jan, 1=Feb, ..., 11=Dec
    static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon_str[4] = {0};
    // __DATE__[0..2] is the 3-letter month abbreviation
    mon_str[0] = __DATE__[0];
    mon_str[1] = __DATE__[1];
    mon_str[2] = __DATE__[2];
    const char *p = strstr(months, mon_str);
    t->month = (p != nullptr) ? (UBYTE)((p - months) / 3 + 1) : 1;

    t->day  = (UBYTE)atoi(&__DATE__[4]);
    t->year = (UWORD)atoi(&__DATE__[7]);

    sscanf(__TIME__, "%hhu:%hhu:%hhu", &t->hour, &t->min, &t->sec);
    t->dotw = 0;
}

/**
 * PCF85063A_Write_now() - Write a datetime_t to the PCF85063A registers.
 *
 * Writes 7 BCD-encoded time registers starting at RTC_SECOND_ADDR (0x04)
 * in a single I2C transaction (register address + 7 data bytes = 8 bytes).
 * Returns the esp_err_t from the I2C write.
 */
static esp_err_t PCF85063A_Write_now(const datetime_t *t) {
    uint8_t buf[8];
    buf[0] = RTC_SECOND_ADDR;                              // register address
    buf[1] = decToBcd(t->sec);                             // 0x04 seconds
    buf[2] = decToBcd(t->min);                             // 0x05 minutes
    buf[3] = decToBcd(t->hour);                            // 0x06 hours
    buf[4] = decToBcd(t->day);                             // 0x07 day
    buf[5] = decToBcd(t->dotw);                            // 0x08 weekday
    buf[6] = decToBcd(t->month);                           // 0x09 month
    buf[7] = decToBcd((uint8_t)(t->year - YEAR_OFFSET));   // 0x0A year (0..99)
    return DEV_I2C_Write_nByte(PCF85063A_ADDRESS, buf, 8);
}

// ============================================================================
// Public API
// ============================================================================

void rtc_init() {
    uint8_t value = RTC_CTRL_1_DEFAULT | RTC_CTRL_1_CAP_SEL;
    DEV_I2C_Write_Byte(PCF85063A_ADDRESS, RTC_CTRL_1_ADDR, value);
    Serial.println("RTC PCF85063A initialized (no Wire.begin)");

    // Detect uninitialized RTC: PCF85063A has no battery backup, so it
    // resets to its power-on epoch (2000-01-01 00:00:00) on every boot.
    // When year == 2000 we write compile-time datetime as a fallback so that
    // CSV filenames reflect an approximate real date instead of 2000-01-01.
    datetime_t now = {};
    esp_err_t err = PCF85063A_Read_now(&now);
    if (err == ESP_OK && now.year == 2000) {
        datetime_t compile_time = {};
        parse_compile_datetime(&compile_time);
        esp_err_t werr = PCF85063A_Write_now(&compile_time);
        if (werr == ESP_OK) {
            Serial.printf("RTC was uninitialized -- set to compile time: %04d-%02d-%02d %02d:%02d:%02d\n",
                           compile_time.year, compile_time.month, compile_time.day,
                           compile_time.hour, compile_time.min, compile_time.sec);
        } else {
            Serial.printf("RTC fallback write failed: %d\n", (int)werr);
        }
    } else if (err == ESP_OK) {
        Serial.printf("RTC time valid: %04d-%02d-%02d %02d:%02d:%02d\n",
                       now.year, now.month, now.day,
                       now.hour, now.min, now.sec);
    }
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
