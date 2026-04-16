#include "sd_card.h"
#include <SPI.h>
#include <SdFat.h>
#include "DashboardUI/DashboardUI.h"

// SD card SPI pin definitions
#define SD_EXPANDER_CS_PIN  4   // CH422G I/O expander logical pin 4 (I2C-controlled)
#define SD_CLK              12
#define SD_MISO             13
#define SD_MOSI             11

// SD_DUMMY_GPIO: a safe, unconnected ESP32 GPIO used as SdFat's csPin argument.
// GPIO 4 is unassigned in all board source files (LCD uses GPIOs 0-3 only for
// DATA11-13/DE; CAN uses 15-16; I2C uses 8-9; RTC interrupt uses 6).
// SdFat requires a real GPIO for its csPin — GPIO 4 will be briefly toggled
// but is unconnected, causing no interference. Do NOT use SS (maps to GPIO 10
// = LCD DATA4 on this board).
// NOTE: SD_EXPANDER_CS_PIN and SD_DUMMY_GPIO share the same numeric value (4)
// but refer to entirely different hardware resources — expander I/O vs ESP32 GPIO.
#define SD_DUMMY_GPIO       4

static SdFs              sd;
static Board            *s_board = nullptr;
static bool              s_sd_ever_began = false;
static SemaphoreHandle_t spi_mutex = nullptr;

void sd_init(Board *board)
{
    s_board = board;

    // Create SPI bus mutex before any SPI operations (D-04)
    spi_mutex = xSemaphoreCreateMutex();

    // Configure CH422G expander pin 4 (SD_EXPANDER_CS_PIN) as output, deasserted high
    auto expander = board->getIO_Expander()->getBase();
    expander->pinMode(SD_EXPANDER_CS_PIN, OUTPUT);
    expander->digitalWrite(SD_EXPANDER_CS_PIN, HIGH);

    // Configure dummy CS GPIO and hold it high so it doesn't glitch the bus
    pinMode(SD_DUMMY_GPIO, OUTPUT);
    digitalWrite(SD_DUMMY_GPIO, HIGH);

    // Disable SPI hardware CS — expander drives the real CS line
    SPI.setHwCs(false);

    // Start SPI bus: CLK=12, MISO=13, MOSI=11, no hardware CS pin
    SPI.begin(SD_CLK, SD_MISO, SD_MOSI, -1);

    Serial.println("SD SPI initialized");
}

/**
 * Attempt to mount the SD card. Returns true if healthy/mounted.
 * Handles both initial mount and re-mount after card removal.
 */
static bool do_mount()
{
    auto expander = s_board->getIO_Expander()->getBase();

    // Guard all SdFat I/O with the SPI mutex (D-04) — both sd_poll_task and
    // logger_task share the bus; concurrent access corrupts transfers.
    if (xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;  // bus busy; caller retries next poll cycle
    }

    bool result = false;

    if (s_sd_ever_began && sd.card()) {
        if (sd.card()->errorCode() != 0) {
            // SdFat already has an I/O error — a concurrent write (logger_task)
            // already proved the card is gone. Skip sd.begin() (it blocks ~2s when
            // no card is present) and return false immediately.
            // Reset so the next poll cycle attempts a clean remount via sd.begin().
            expander->digitalWrite(SD_EXPANDER_CS_PIN, HIGH);
            s_sd_ever_began = false;
            result = false;
        } else {
            // No error yet — probe with a CID read to confirm physical presence.
            // errorCode() stays 0 until an I/O is attempted, so we can't rely on it alone.
            cid_t cid;
            if (sd.card()->readCID(&cid)) {
                result = true;  // card still responding
            } else {
                // Card removed — deassert CS, reset for clean remount next cycle.
                expander->digitalWrite(SD_EXPANDER_CS_PIN, HIGH);
                s_sd_ever_began = false;
                result = false;
            }
        }
    } else {
        // Assert expander CS (active low) before starting transaction
        expander->digitalWrite(SD_EXPANDER_CS_PIN, LOW);

        // SD_DUMMY_GPIO is passed to SdFat as its GPIO csPin, but actual CS is
        // driven by the expander above. SHARED_SPI tells SdFat not to take
        // exclusive ownership of the bus.
        SdSpiConfig cfg(SD_DUMMY_GPIO, SHARED_SPI, SD_SCK_MHZ(25), &SPI);
        bool ok = sd.begin(cfg);

        if (ok) {
            s_sd_ever_began = true;
            result = true;
        } else {
            // Deassert CS on failure so the bus is released cleanly
            expander->digitalWrite(SD_EXPANDER_CS_PIN, HIGH);
            result = false;
        }
    }

    xSemaphoreGive(spi_mutex);
    return result;
}

SdFs& sd_get_fs() { return sd; }
SemaphoreHandle_t sd_get_spi_mutex() { return spi_mutex; }

void sd_poll_task(void *param)
{
    DashboardState *state = (DashboardState *)param;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(500));

        bool mounted = do_mount();

        if (mounted != state->sd_started) {
            state->sd_started = mounted;
            Serial.printf("SD state changed: %s\n", mounted ? "mounted" : "unmounted");
        }
    }
}

