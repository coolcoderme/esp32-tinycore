#include "sdboot/layout.h"
#include "sdboot/endian.h"

int sdboot_layout(const sdboot_layout_in_t *in, sdboot_layout_out_t *out)
{
    uint32_t base, top, ksz, dsz, isz, min_free;
    uint32_t initrd_pa, initrd_end, dtb_pa, dtb_room;
    uint32_t used, kernel_end;

    if (!in || !out)
        return SDBOOT_LAYOUT_ERR_ARGS;

    base = in->psram_base ? in->psram_base : SDBOOT_PSRAM_BASE;
    ksz = in->kernel_size;
    dsz = in->dtb_size;
    isz = in->initrd_size;
    min_free = in->min_free ? in->min_free : SDBOOT_MIN_FREE_RAM;

    if (in->psram_size < 0x01000000u || ksz == 0)
        return SDBOOT_LAYOUT_ERR_ARGS;

    top = base + in->psram_size;
    if (top < base)
        return SDBOOT_LAYOUT_ERR_NOSPC;

    dtb_room = dsz < SDBOOT_DTB_RESERVE ? SDBOOT_DTB_RESERVE : sdboot_align_up(dsz, SDBOOT_DTB_ALIGN);

    initrd_end = top;
    if (isz) {
        initrd_pa = sdboot_align_down(initrd_end - isz, SDBOOT_INITRD_ALIGN);
        if (initrd_pa < base)
            return SDBOOT_LAYOUT_ERR_NOSPC;
        initrd_end = initrd_pa + isz;
    } else {
        initrd_pa = top;
        initrd_end = top;
    }

    dtb_pa = sdboot_align_down((isz ? initrd_pa : top) - dtb_room, SDBOOT_DTB_ALIGN);
    if (dtb_pa < base)
        return SDBOOT_LAYOUT_ERR_NOSPC;

    kernel_end = base + ksz;
    if (kernel_end > dtb_pa)
        return SDBOOT_LAYOUT_ERR_NOSPC;

    used = ksz + dsz + isz;
    if (used > in->psram_size)
        return SDBOOT_LAYOUT_ERR_NOSPC;
    if (in->psram_size - used < min_free)
        return SDBOOT_LAYOUT_ERR_NOSPC;

    out->kernel_pa = base;
    out->dtb_pa = dtb_pa;
    out->initrd_pa = initrd_pa;
    out->initrd_end = initrd_end;
    out->used = used;
    out->free_ram = in->psram_size - used;
    return SDBOOT_LAYOUT_OK;
}
