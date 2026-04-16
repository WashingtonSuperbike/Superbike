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

static SdFs   sd;
static Board *s_board = nullptr;
static bool   s_sd_ever_began = false;
static FsFile s_log_file;

void sd_init(Board *board)
{
    s_board = board;

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

    // Card claims to be mounted — probe it with a CID read to confirm
    // it's physically present. errorCode() stays 0 even after card removal until
    // an actual I/O is attempted, so we must probe rather than just check the code.
    // Only probe the existing mount state if sd.begin() has succeeded at least
    // once. Without this guard, sd.card() may return a non-null internal pointer
    // on a default-constructed SdFs object even before any successful begin(),
    // which could skip the sd.begin() block and silently fail to mount.
    if (s_sd_ever_began && sd.card() && sd.card()->errorCode() == 0) {
        cid_t cid;
        if (sd.card()->readCID(&cid)) {
            return true;  // card still responding
        }

        // Card removed — deassert CS and report immediately without trying to re-mount.
        // Next poll cycle will try to re-mount automatically.
        expander->digitalWrite(SD_EXPANDER_CS_PIN, HIGH);
        return false;
    }

    // Assert expander CS (active low) before starting transaction
    expander->digitalWrite(SD_EXPANDER_CS_PIN, LOW);

    // SD_DUMMY_GPIO is passed to SdFat as its GPIO csPin, but actual CS is
    // driven by the expander above. SHARED_SPI tells SdFat not to take
    // exclusive ownership of the bus.
    SdSpiConfig cfg(SD_DUMMY_GPIO, SHARED_SPI, SD_SCK_MHZ(25), &SPI);
    bool ok = sd.begin(cfg);

    if (ok) {
        s_sd_ever_began = true;
    } else {
        // Deassert CS on failure so the bus is released cleanly
        expander->digitalWrite(SD_EXPANDER_CS_PIN, HIGH);
    }

    return ok;
}

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

bool sd_open_log_file(const char *filename) {
    if (s_log_file.isOpen()) {
        s_log_file.close();
    }
    return s_log_file.open(filename, O_WRONLY | O_CREAT | O_TRUNC);
}

bool sd_write_log_line(const char *buf, size_t len) {
    if (!s_log_file.isOpen()) return false;
    return (size_t)s_log_file.write(buf, len) == len;
}

bool sd_sync_log_file() {
    if (!s_log_file.isOpen()) return false;
    return s_log_file.sync();
}

void sd_close_log_file() {
    if (s_log_file.isOpen()) {
        s_log_file.sync();
        s_log_file.close();
    }
}
