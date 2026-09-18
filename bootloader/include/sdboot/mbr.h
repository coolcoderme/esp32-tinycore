#ifndef SDBOOT_MBR_H
#define SDBOOT_MBR_H

#include "sdboot/disk.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDBOOT_PART_FAT12     0x01
#define SDBOOT_PART_FAT16     0x04
#define SDBOOT_PART_FAT16B    0x06
#define SDBOOT_PART_FAT32     0x0B
#define SDBOOT_PART_FAT32_LBA 0x0C
#define SDBOOT_PART_FAT16_LBA 0x0E
#define SDBOOT_PART_GPT_PROTECTIVE 0xEE

typedef struct {
    uint8_t  status;
    uint8_t  type;
    uint32_t start_lba;
    uint32_t sectors;
} sdboot_part_t;

typedef struct {
    int          count;
    sdboot_part_t parts[4];
} sdboot_mbr_t;

int sdboot_mbr_parse(const uint8_t sector[SDBOOT_SECTOR_SIZE], sdboot_mbr_t *out);

/* True if this partition type is a FAT volume we can mount. */
int sdboot_part_is_fat(uint8_t type);

#ifdef __cplusplus
}
#endif

#endif
