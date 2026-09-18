#include "handoff.h"

#include "esp_attr.h"
#include "esp_rom_sys.h"
#include "hal/cache_hal.h"
#include "esp_private/hw_stack_guard.h"

/*
 * IDF leaves PMP entries 5 and 7-10 unset. M-mode still has access; U-mode
 * would fault. Program unlocked entry 9 as NAPOT RWX over the 64 MB PSRAM
 * window 0x48000000..0x4C000000 so Linux (CONFIG_RISCV_M_MODE) can tighten
 * it later. Sequence matches why2025-linux's boot shim.
 */
static inline void IRAM_ATTR pmp_grant_psram(void)
{
    const unsigned long pmpaddr9 = 0x127FFFFFUL;
    const unsigned long cfg_byte = 0x1FUL;
    unsigned long cfg2;

    __asm__ volatile("csrw pmpaddr9, %0" :: "r"(pmpaddr9));
    __asm__ volatile("csrr %0, pmpcfg2" : "=r"(cfg2));
    cfg2 = (cfg2 & ~(0xFFUL << 8)) | (cfg_byte << 8);
    __asm__ volatile("csrw pmpcfg2, %0" :: "r"(cfg2));
}

void sdboot_cache_commit(uint32_t pa, uint32_t size)
{
    if (size)
        cache_hal_writeback_addr(pa, size);
}

void IRAM_ATTR sdboot_jump_linux(uint32_t entry, uint32_t dtb_pa)
{
    esp_hw_stack_guard_monitor_stop();

    __asm__ volatile("csrci mstatus, 0x8");
    pmp_grant_psram();
    __asm__ volatile("fence.i");

    __asm__ volatile(
        "li   a0, 0\n"
        "mv   a1, %1\n"
        "jr   %0\n"
        :
        : "r"(entry), "r"(dtb_pa)
        : "a0", "a1");
    __builtin_unreachable();
}
