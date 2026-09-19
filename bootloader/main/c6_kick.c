#include "c6_kick.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "c6";

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
