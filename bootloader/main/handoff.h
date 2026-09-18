#pragma once

#include <stdint.h>

void sdboot_cache_commit(uint32_t pa, uint32_t size);
void sdboot_jump_linux(uint32_t entry, uint32_t dtb_pa);
