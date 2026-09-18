#pragma once

#include "sdmmc_cmd.h"
#include "board.h"

esp_err_t sdboot_sdmmc_init(const sdboot_board_t *board, sdmmc_card_t **card_out);
int sdboot_sdmmc_read(void *ctx, uint32_t lba, void *buf);
