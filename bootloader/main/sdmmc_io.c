#include "sdmmc_io.h"

#include <stdlib.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_err.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

static const char *TAG = "sdmmc";

esp_err_t sdboot_sdmmc_init(const sdboot_board_t *board, sdmmc_card_t **card_out)
{
    esp_err_t err;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    sdmmc_card_t *card;
    int width;

    if (!board || !card_out)
        return ESP_ERR_INVALID_ARG;

    host.slot = board->slot;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    if (board->ldo_chan > 0) {
        sd_pwr_ctrl_ldo_config_t ldo_config = {
            .ldo_chan_id = board->ldo_chan,
        };
        sd_pwr_ctrl_handle_t pwr = NULL;
        err = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LDO%d init failed: %s", board->ldo_chan, esp_err_to_name(err));
            return err;
        }
        host.pwr_ctrl_handle = pwr;
        ESP_LOGI(TAG, "SD I/O powered by on-chip LDO%d", board->ldo_chan);
    }

    slot.clk = board->clk;
    slot.cmd = board->cmd;
    slot.d0 = board->d0;
    slot.d1 = board->d1;
    slot.d2 = board->d2;
    slot.d3 = board->d3;
    slot.width = board->width;
    if (board->internal_pullup)
        slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    err = sdmmc_host_init();
    if (err != ESP_OK)
        return err;

    width = board->width;
    card = calloc(1, sizeof(*card));
    if (!card)
        return ESP_ERR_NO_MEM;

    {
        int slots[2];
        int s;
        int last_err = ESP_FAIL;

        slots[0] = board->slot;
        slots[1] = board->slot == 0 ? 1 : 0;
        err = ESP_FAIL;
        for (s = 0; s < 2; s++) {
            host.slot = slots[s];
            slot.width = width;
            err = sdmmc_host_init_slot(host.slot, &slot);
            if (err != ESP_OK) {
                last_err = err;
                continue;
            }
            err = sdmmc_card_init(&host, card);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "SDMMC slot %d width %d OK", host.slot, slot.width);
                break;
            }
            last_err = err;
            if (width == 4) {
                ESP_LOGW(TAG, "slot %d 4-bit failed (%s), retry 1-bit",
                         host.slot, esp_err_to_name(err));
                slot.width = 1;
                err = sdmmc_host_init_slot(host.slot, &slot);
                if (err == ESP_OK)
                    err = sdmmc_card_init(&host, card);
                if (err == ESP_OK)
                    break;
                last_err = err;
                slot.width = width;
            }
        }
        err = (err == ESP_OK) ? ESP_OK : last_err;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sdmmc_card_init: %s", esp_err_to_name(err));
        free(card);
        return err;
    }

    sdmmc_card_print_info(stdout, card);
    *card_out = card;
    return ESP_OK;
}

int sdboot_sdmmc_read(void *ctx, uint32_t lba, void *buf)
{
    sdmmc_card_t *card = (sdmmc_card_t *)ctx;
    esp_err_t err = sdmmc_read_sectors(card, buf, lba, 1);
    return err == ESP_OK ? 0 : -1;
}
