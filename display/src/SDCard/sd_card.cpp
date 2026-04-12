#include "sd_card.h"
#include <SPI.h>
#include <SdFat.h>
#include "DashboardUI/DashboardUI.h"

// SD card SPI pin definitions
#define SD_CS       4   // CH422G expander logical pin 4 (I2C-controlled)
#define SD_CLK      12
#define SD_MISO     13
#define SD_MOSI     11

// SD_DUMMY_CS: a safe, unconnected ESP32 GPIO used as SdFat's csPin argument.
// GPIO 4 is unassigned in all board source files (LCD uses GPIOs 0-3 only for
// DATA11-13/DE; CAN uses 15-16; I2C uses 8-9; RTC interrupt uses 6).
// SdFat requires a real GPIO for its csPin — GPIO 4 will be briefly toggled
// but is unconnected, causing no interference. Do NOT use SS (maps to GPIO 10
// = LCD DATA4 on this board).
#define SD_DUMMY_CS 4

static SdFs   sd;
static Board *s_board = nullptr;

void sd_init(Board *board)
{
    s_board = board;

    // Configure CH422G expander pin 4 (SD_CS) as output, deasserted high
    auto expander = board->getIO_Expander()->getBase();
    expander->pinMode(SD_CS, OUTPUT);
    expander->digitalWrite(SD_CS, HIGH);

    // Configure dummy CS GPIO and hold it high so it doesn't glitch the bus
    pinMode(SD_DUMMY_CS, OUTPUT);
    digitalWrite(SD_DUMMY_CS, HIGH);

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

    // Fast-path: card already mounted and healthy — skip re-init
    if (sd.card() && sd.card()->errorCode() == 0) {
        return true;
    }

    // Assert expander CS (active low) before starting transaction
    expander->digitalWrite(SD_CS, LOW);

    // SD_DUMMY_CS is passed to SdFat as its GPIO csPin, but actual CS is
    // driven by the expander above. SHARED_SPI tells SdFat not to take
    // exclusive ownership of the bus.
    SdSpiConfig cfg(SD_DUMMY_CS, SHARED_SPI, SD_SCK_MHZ(25), &SPI);
    bool ok = sd.begin(cfg);

    if (!ok) {
        // Deassert CS on failure so the bus is released cleanly
        expander->digitalWrite(SD_CS, HIGH);
    }

    Serial.printf("SD mount: %s\n", ok ? "OK" : "FAIL");
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
