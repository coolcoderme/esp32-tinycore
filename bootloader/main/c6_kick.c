#include "c6_kick.h"

#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "c6";

/* Function EV C6 SDIO on the GPIO matrix (linux-loader leaves this
 * mux live across the jump; there is no pinctrl-esp32p4 yet). */
#define C6_SDIO_CLK 18
#define C6_SDIO_CMD 19
#define C6_SDIO_D0  14
#define C6_SDIO_D1  15
#define C6_SDIO_D2  16
#define C6_SDIO_D3  17
#define C6_SDIO_SLOT 1

void sdboot_c6_kick(const sdboot_board_t *board)
{
    int gpio;
    int run_level;
    int rst_level;

    if (!board || board->c6_reset_gpio <= 0) {
        ESP_LOGI(TAG, "no C6 reset GPIO — skip Wi-Fi coprocessor kick");
        return;
    }

    gpio = board->c6_reset_gpio;
    run_level = board->c6_reset_active_high ? 1 : 0;
    rst_level = run_level ? 0 : 1;

    gpio_reset_pin(gpio);
    gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(gpio, rst_level);
    esp_rom_delay_us(1000);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(gpio, run_level);
    ESP_LOGI(TAG, "C6 EN GPIO%d released (active-%s) — ESP-Hosted SDIO CLK=18 CMD=19 D0-3=14-17",
             gpio, board->c6_reset_active_high ? "high" : "low");
    vTaskDelay(pdMS_TO_TICKS(100));
}

void sdboot_c6_sdio_mux(void)
{
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_err_t err;

    slot.clk = C6_SDIO_CLK;
    slot.cmd = C6_SDIO_CMD;
    slot.d0 = C6_SDIO_D0;
    slot.d1 = C6_SDIO_D1;
    slot.d2 = C6_SDIO_D2;
    slot.d3 = C6_SDIO_D3;
    slot.width = 4;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    /* Host must already be up (sdboot_sdmmc_init). */
    err = sdmmc_host_init_slot(C6_SDIO_SLOT, &slot);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "C6 SDIO slot %d mux failed: %s (kernel may still probe)",
                 C6_SDIO_SLOT, esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "C6 SDIO slot %d muxed CLK=%d CMD=%d D0-3=%d-%d",
             C6_SDIO_SLOT, C6_SDIO_CLK, C6_SDIO_CMD, C6_SDIO_D0, C6_SDIO_D3);
}
