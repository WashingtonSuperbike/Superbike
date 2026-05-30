# External Integrations

**Analysis Date:** 2025-01-24

## APIs & External Services

**Hardware Protocols:**
- **CAN Bus (TWAI):** 250kbps bitrate. Used for real-time telemetry from bike subsystems.
  - SDK/Client: `driver/twai.h` (ESP-IDF) and `ESP32-TWAI-CAN`.
  - Auth: N/A (Physical bus).

## Data Storage

**Databases:**
- None (Local state only).

**File Storage:**
- **SD Card:** SPI-based storage for data logging.
  - Client: `SdFat` library.
  - Hardware: SPI bus (SCK: 12, MISO: 13, MOSI: 11) with CH422G Expander-controlled CS.

**Caching:**
- **LVGL Buffers:** Double-buffered SRAM for smooth UI rendering.

## Authentication & Identity

**Auth Provider:**
- None (Embedded firmware).

## Monitoring & Observability

**Error Tracking:**
- Unrecognized CAN IDs are logged to Serial.
- TWAI Error state monitoring in `display/src/CAN/CAN_Receive.cpp`.

**Logs:**
- Serial (UART/USB CDC) at 115200 baud.
- SD Card CSV logging (implemented in `display/src/SDCard/sd_card.cpp`).

## CI/CD & Deployment

**Hosting:**
- Local hardware (ESP32-S3).

**CI Pipeline:**
- Not detected (likely manual PlatformIO builds).

## Environment Configuration

**Required env vars:**
- `ARDUINO_USB_CDC_ON_BOOT=1`: Enables serial logging via USB.
- `BOARD_HAS_PSRAM`: Required for display buffer management.

**Secrets location:**
- No cloud secrets; hardware-specific configurations in `include/Config.h`.

## Webhooks & Callbacks

**Incoming:**
- **CAN Messages:** Decoded in `include/CANDecoder.h`. Handles Motor Stats, BMS status, Charger data, etc.

**Outgoing:**
- **CAN Transmit:** Handled by mainboard/twai_transmit modules to control hardware (e.g., requesting cell voltages).

## Hardware-Specific Integrations

**Display:**
- **RGB Interface:** 5-inch 800x480 LCD (ST7701 driver) via `ESP32_Display_Panel`.
- **Touch:** I2C-based touch controller (GT911 or similar).

**Sensors:**
- **IMU (MPU6050):** I2C interface for roll/tilt angle detection.
- **RTC (BM8563):** I2C interface for system clock synchronization.

---

*Integration audit: 2025-01-24*
