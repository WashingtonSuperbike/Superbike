#pragma once

/**
 * Initialize the PCF85063A RTC on the shared I2C bus (SDA=8, SCL=9, addr 0x51).
 * MUST be called AFTER board->begin() — does NOT call Wire.begin().
 */
void rtc_init();

/**
 * Read current datetime from RTC and format as a filename string.
 * Output format: "YYYY-MM-DD_HHMMSS.csv" (e.g. "2026-04-12_093045.csv")
 * No validation is performed on the datetime (per D-02).
 *
 * @param buf     Output buffer (must be at least 25 bytes)
 * @param buflen  Size of output buffer
 */
void rtc_get_filename(char *buf, size_t buflen);
