#ifndef SDBOOT_LAYOUT_H
#define SDBOOT_LAYOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDBOOT_PSRAM_BASE      0x48000000u
#define SDBOOT_PSRAM_WINDOW    0x04000000u /* 64 MB virtual window */
#define SDBOOT_INITRD_ALIGN    0x00200000u /* 2 MB */
#define SDBOOT_DTB_ALIGN       64u
#define SDBOOT_DTB_RESERVE     0x10000u    /* 64 KB */
#define SDBOOT_MIN_FREE_RAM    0x00C00000u /* 12 MB */

enum {
    SDBOOT_LAYOUT_OK        = 0,
    SDBOOT_LAYOUT_ERR_ARGS  = -1,
    SDBOOT_LAYOUT_ERR_NOSPC = -2,
};

typedef struct {
    uint32_t psram_base;
    uint32_t psram_size;
    uint32_t kernel_size;
    uint32_t dtb_size;
    uint32_t initrd_size;
    uint32_t min_free;
} sdboot_layout_in_t;

typedef struct {
    uint32_t kernel_pa;
    uint32_t dtb_pa;
    uint32_t initrd_pa;
    uint32_t initrd_end;
    uint32_t used;
    uint32_t free_ram;
} sdboot_layout_out_t;

int sdboot_layout(const sdboot_layout_in_t *in, sdboot_layout_out_t *out);

#ifdef __cplusplus
}
#endif

#endif
