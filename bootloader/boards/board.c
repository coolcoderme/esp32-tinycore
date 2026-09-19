#include "board.h"

/*
 * Dedicated SDMMC IOMUX pins on ESP32-P4 (Function EV Board, and the
 * TF slot on current Waveshare P4 kits):
 *   D0=39 D1=40 D2=41 D3=42 CLK=43 CMD=44
 * SD I/O rail is typically LDO channel 4 (LDO_VO4).
 */
static const sdboot_board_t k_board = {
#if defined(CONFIG_SDBOOT_BOARD_WAVESHARE)
    .name = "waveshare-esp32-p4",
#elif defined(CONFIG_SDBOOT_BOARD_PSRAM64)
    .name = "esp32-p4-psram64",
#else
    .name = "esp32-p4-function-ev",
#endif
#ifdef CONFIG_SDBOOT_SD_SLOT
    .slot = CONFIG_SDBOOT_SD_SLOT,
#else
    .slot = 0,
#endif
#ifdef CONFIG_SDBOOT_SD_CLK
    .clk = CONFIG_SDBOOT_SD_CLK,
    .cmd = CONFIG_SDBOOT_SD_CMD,
    .d0 = CONFIG_SDBOOT_SD_D0,
    .d1 = CONFIG_SDBOOT_SD_D1,
    .d2 = CONFIG_SDBOOT_SD_D2,
    .d3 = CONFIG_SDBOOT_SD_D3,
#else
    .clk = 43,
    .cmd = 44,
    .d0 = 39,
    .d1 = 40,
    .d2 = 41,
    .d3 = 42,
#endif
#ifdef CONFIG_SDBOOT_SD_WIDTH
    .width = CONFIG_SDBOOT_SD_WIDTH,
#else
    .width = 4,
#endif
#ifdef CONFIG_SDBOOT_SD_LDO_CHAN
    .ldo_chan = CONFIG_SDBOOT_SD_LDO_CHAN,
#else
    .ldo_chan = 4,
#endif
    .internal_pullup = 1,
#ifdef CONFIG_SDBOOT_C6_RESET_GPIO
    .c6_reset_gpio = CONFIG_SDBOOT_C6_RESET_GPIO,
#else
    .c6_reset_gpio = 54,
#endif
#ifdef CONFIG_SDBOOT_C6_RESET_ACTIVE_LOW
    .c6_reset_active_high = 0,
#else
    .c6_reset_active_high = 1,
#endif
};

const sdboot_board_t *sdboot_board(void)
{
    return &k_board;
}
