#include "sdboot/detect.h"
#include "sdboot/endian.h"
#include "sdboot/mbr.h"

#include <string.h>

enum sdboot_media sdboot_detect_media(const uint8_t sector0[512],
                                      const uint8_t sector16[512])
{
    sdboot_mbr_t mbr;

    if (sector16 && memcmp(sector16 + 1, "CD001", 5) == 0)
        return SDBOOT_MEDIA_ISO9660;

    if (!sector0)
        return SDBOOT_MEDIA_UNKNOWN;

    if (sdboot_mbr_parse(sector0, &mbr) == 0 && mbr.count > 0) {
        int i, only_protective = 1;
        for (i = 0; i < mbr.count; i++) {
            if (mbr.parts[i].type != SDBOOT_PART_GPT_PROTECTIVE)
                only_protective = 0;
        }
        if (only_protective)
            return SDBOOT_MEDIA_GPT;
        return SDBOOT_MEDIA_MBR;
    }

    /* Superfloppy: FAT BPB in sector 0. */
    if (sector0[510] == 0x55 && sector0[511] == 0xAA) {
        if (memcmp(sector0 + 0x52, "FAT32", 5) == 0 ||
            memcmp(sector0 + 0x36, "FAT16", 5) == 0 ||
            memcmp(sector0 + 0x36, "FAT12", 5) == 0 ||
            memcmp(sector0 + 0x36, "FAT", 3) == 0)
            return SDBOOT_MEDIA_SUPERFLOPPY;
    }
    return SDBOOT_MEDIA_UNKNOWN;
}

enum sdboot_kernel sdboot_detect_kernel(const void *buf, size_t len)
{
    const uint8_t *b = (const uint8_t *)buf;

    if (!b || len < 64)
        return SDBOOT_KERN_UNKNOWN;

    /* x86 bzImage setup header "HdrS" at 0x202. */
    if (len >= 0x206 && memcmp(b + 0x202, "HdrS", 4) == 0)
        return SDBOOT_KERN_X86;

    /* Classic boot signature plus a 16-bit real-mode stub. */
    if (len >= 512 && b[510] == 0x55 && b[511] == 0xAA &&
        len >= 0x206 && memcmp(b + 0x202, "HdrS", 4) == 0)
        return SDBOOT_KERN_X86;

    /* RISC-V Image magic is the u64 "RISCV\0\0\0" at byte 48. */
    if (memcmp(b + 48, "RISCV", 5) == 0)
        return SDBOOT_KERN_RISCV;

    return SDBOOT_KERN_UNKNOWN;
}

uint32_t sdboot_riscv_image_size(const void *buf, size_t len)
{
    const uint8_t *b = (const uint8_t *)buf;
    uint32_t lo;
    uint32_t hi;

    if (sdboot_detect_kernel(buf, len) != SDBOOT_KERN_RISCV)
        return 0;
    if (len < 24)
        return 0;
    lo = sdboot_le32(b + 16);
    hi = sdboot_le32(b + 20);
    if (hi != 0)
        return 0;
    return lo;
}
