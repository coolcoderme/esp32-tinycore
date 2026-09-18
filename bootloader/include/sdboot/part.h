#ifndef SDBOOT_PART_H
#define SDBOOT_PART_H

#include "sdboot/detect.h"
#include "sdboot/disk.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    SDBOOT_PART_OK      = 0,
    SDBOOT_PART_ERR_IO  = -1,
    SDBOOT_PART_ERR_NONE = -2,
    SDBOOT_PART_ERR_ISO = -3,
};

/*
 * Find the first FAT volume the linux-loader can mount.
 *
 * Superfloppy: LBA 0. MBR: first FAT12/16/32 type. GPT (protective MBR
 * 0xEE or a GPT header at LBA 1): first EFI System / Basic Data partition.
 * ISO9660 without a FAT partition returns SDBOOT_PART_ERR_ISO.
 */
int sdboot_find_fat_lba(sdboot_read_fn read, void *ctx, uint32_t *lba_out,
                        enum sdboot_media *media_out);

#ifdef __cplusplus
}
#endif

#endif
