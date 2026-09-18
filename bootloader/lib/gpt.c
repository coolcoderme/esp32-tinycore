#include "sdboot/gpt.h"
#include "sdboot/endian.h"

#include <string.h>

/* EFI System Partition */
static const uint8_t k_guid_esp[16] = {
    0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8, 0xd2, 0x11,
    0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b
};

/* Microsoft Basic Data */
static const uint8_t k_guid_ms_basic[16] = {
    0xa2, 0xa0, 0xd0, 0xeb, 0xe5, 0xb9, 0x33, 0x44,
    0x87, 0xc9, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7
};

int sdboot_gpt_type_is_fat(const uint8_t type_guid[16])
{
    if (!type_guid)
        return 0;
    return memcmp(type_guid, k_guid_esp, 16) == 0 ||
           memcmp(type_guid, k_guid_ms_basic, 16) == 0;
}

int sdboot_gpt_parse(sdboot_read_fn read, void *ctx, sdboot_gpt_t *out)
{
    uint8_t hdr[SDBOOT_SECTOR_SIZE];
    uint8_t sec[SDBOOT_SECTOR_SIZE];
    uint32_t entry_lba, nent, entsz, i, max_ent;
    uint64_t entry_lba64, nent_cap;
    int rc;

    if (!read || !out)
        return SDBOOT_GPT_ERR_RANGE;
    memset(out, 0, sizeof(*out));

    rc = read(ctx, 1, hdr);
    if (rc != 0)
        return SDBOOT_GPT_ERR_IO;
    if (memcmp(hdr, "EFI PART", 8) != 0)
        return SDBOOT_GPT_ERR_MAGIC;

    entry_lba64 = sdboot_le64(hdr + 72);
    nent = sdboot_le32(hdr + 80);
    entsz = sdboot_le32(hdr + 84);
    if (entry_lba64 > 0xFFFFFFFFull || entsz < 128 || (entsz % 128) != 0)
        return SDBOOT_GPT_ERR_RANGE;
    if (nent == 0)
        return SDBOOT_GPT_ERR_NONE;
    entry_lba = (uint32_t)entry_lba64;

    max_ent = (uint32_t)(sizeof(out->parts) / sizeof(out->parts[0]));
    nent_cap = nent;
    if (nent_cap > 128)
        nent_cap = 128; /* enough to find a FAT volume */

    for (i = 0; i < (uint32_t)nent_cap && out->count < (int)max_ent; i++) {
        uint32_t byte_off = i * entsz;
        uint32_t lba = entry_lba + (byte_off / SDBOOT_SECTOR_SIZE);
        uint32_t off = byte_off % SDBOOT_SECTOR_SIZE;
        const uint8_t *ent;
        uint64_t start, end;
        uint32_t secs;

        if (off + 128 > SDBOOT_SECTOR_SIZE)
            continue;
        rc = read(ctx, lba, sec);
        if (rc != 0)
            return SDBOOT_GPT_ERR_IO;
        ent = sec + off;
        if (memcmp(ent, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) == 0)
            continue; /* unused (all-zero type GUID) */
        start = sdboot_le64(ent + 32);
        end = sdboot_le64(ent + 40);
        if (start > 0xFFFFFFFFull || end < start || end > 0xFFFFFFFFull)
            continue;
        secs = (uint32_t)(end - start + 1ull);
        if (secs == 0)
            continue;
        memcpy(out->parts[out->count].type_guid, ent, 16);
        out->parts[out->count].start_lba = (uint32_t)start;
        out->parts[out->count].sectors = secs;
        out->count++;
    }
    return out->count ? SDBOOT_GPT_OK : SDBOOT_GPT_ERR_NONE;
}
