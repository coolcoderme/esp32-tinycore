#include "sdboot/mbr.h"
#include "sdboot/endian.h"

#include <string.h>

int sdboot_part_is_fat(uint8_t type)
{
    return type == SDBOOT_PART_FAT12 || type == SDBOOT_PART_FAT16 ||
           type == SDBOOT_PART_FAT16B || type == SDBOOT_PART_FAT32 ||
           type == SDBOOT_PART_FAT32_LBA || type == SDBOOT_PART_FAT16_LBA;
}

int sdboot_mbr_parse(const uint8_t sector[SDBOOT_SECTOR_SIZE], sdboot_mbr_t *out)
{
    int i;

    if (!sector || !out)
        return -1;
    memset(out, 0, sizeof(*out));

    if (sector[510] != 0x55 || sector[511] != 0xAA)
        return -1;

    for (i = 0; i < 4; i++) {
        const uint8_t *e = sector + 0x1BE + (i * 16);
        uint8_t type = e[4];
        uint32_t start = sdboot_le32(e + 8);
        uint32_t secs = sdboot_le32(e + 12);

        if (type == 0 || secs == 0)
            continue;
        out->parts[out->count].status = e[0];
        out->parts[out->count].type = type;
        out->parts[out->count].start_lba = start;
        out->parts[out->count].sectors = secs;
        out->count++;
    }
    return 0;
}
