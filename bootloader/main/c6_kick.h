#pragma once

#include "board.h"

/* Pulse the ESP32-C6 EN line so its ESP-Hosted slave firmware starts
 * before Linux probes SDIO slot 1. */
void sdboot_c6_kick(const sdboot_board_t *board);
