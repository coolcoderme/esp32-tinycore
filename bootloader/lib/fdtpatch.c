#include "sdboot/fdtpatch.h"
#include "sdboot/endian.h"

#include <string.h>

#define FDT_BEGIN_NODE 0x00000001u
#define FDT_END_NODE   0x00000002u
#define FDT_PROP       0x00000003u
#define FDT_NOP        0x00000004u
#define FDT_END        0x00000009u

int sdboot_fdt_valid(const void *fdt, size_t cap, uint32_t *totalsize_out)
{
    const uint8_t *b = (const uint8_t *)fdt;
    uint32_t total;

    if (!b || cap < 8)
        return SDBOOT_FDT_ERR_RANGE;
    if (sdboot_be32(b) != SDBOOT_FDT_MAGIC)
        return SDBOOT_FDT_ERR_MAGIC;
    total = sdboot_be32(b + 4);
    if (total < 32 || total > cap)
        return SDBOOT_FDT_ERR_RANGE;
    if (totalsize_out)
        *totalsize_out = total;
    return SDBOOT_FDT_OK;
}

int sdboot_fdt_replace_be32(void *fdt, size_t cap, uint32_t old, uint32_t newv)
{
    uint8_t *b = (uint8_t *)fdt;
    uint32_t total, i;
    uint8_t needle[4];
    int rc = sdboot_fdt_valid(fdt, cap, &total);

    if (rc)
        return rc;
    sdboot_wbe32(needle, old);
    for (i = 0; i + 4 <= total; i++) {
        if (memcmp(b + i, needle, 4) == 0) {
            sdboot_wbe32(b + i, newv);
            return SDBOOT_FDT_OK;
        }
    }
    return SDBOOT_FDT_ERR_MISS;
}

int sdboot_fdt_replace_be32_pair(void *fdt, size_t cap,
                                 uint32_t a, uint32_t b,
                                 uint32_t na, uint32_t nb)
{
    uint8_t *p = (uint8_t *)fdt;
    uint32_t total, i;
    uint8_t needle[8];
    int rc = sdboot_fdt_valid(fdt, cap, &total);

    if (rc)
        return rc;
    sdboot_wbe32(needle, a);
    sdboot_wbe32(needle + 4, b);
    for (i = 0; i + 8 <= total; i++) {
        if (memcmp(p + i, needle, 8) == 0) {
            sdboot_wbe32(p + i, na);
            sdboot_wbe32(p + i + 4, nb);
            return SDBOOT_FDT_OK;
        }
    }
    return SDBOOT_FDT_ERR_MISS;
}

int sdboot_fdt_set_bootargs(void *fdt, size_t cap, const char *args)
{
    uint8_t *b = (uint8_t *)fdt;
    uint32_t total, off_struct, off_strings, size_struct;
    uint32_t pos;
    size_t arglen;
    int rc;

    if (!args)
        return SDBOOT_FDT_ERR_RANGE;
    rc = sdboot_fdt_valid(fdt, cap, &total);
    if (rc)
        return rc;
    if (total < 40)
        return SDBOOT_FDT_ERR_RANGE;

    off_struct = sdboot_be32(b + 8);
    off_strings = sdboot_be32(b + 12);
    size_struct = sdboot_be32(b + 36);
    if (off_struct >= total || off_strings >= total)
        return SDBOOT_FDT_ERR_RANGE;
    if (size_struct == 0 || off_struct + size_struct > total)
        size_struct = total - off_struct;

    arglen = strlen(args) + 1;
    pos = off_struct;
    while (pos + 4 <= off_struct + size_struct) {
        uint32_t tag = sdboot_be32(b + pos);
        pos += 4;
        if (tag == FDT_END)
            break;
        if (tag == FDT_NOP || tag == FDT_END_NODE)
            continue;
        if (tag == FDT_BEGIN_NODE) {
            while (pos < off_struct + size_struct && b[pos] != 0)
                pos++;
            pos++;
            pos = (pos + 3u) & ~3u;
            continue;
        }
        if (tag == FDT_PROP) {
            uint32_t plen, nameoff;
            const char *name;
            if (pos + 8 > total)
                return SDBOOT_FDT_ERR_RANGE;
            plen = sdboot_be32(b + pos);
            nameoff = sdboot_be32(b + pos + 4);
            pos += 8;
            if (off_strings + nameoff >= total)
                return SDBOOT_FDT_ERR_RANGE;
            name = (const char *)(b + off_strings + nameoff);
            if (strcmp(name, "bootargs") == 0) {
                if (arglen > plen)
                    return SDBOOT_FDT_ERR_NOSPC;
                memset(b + pos, 0, plen);
                memcpy(b + pos, args, arglen);
                return SDBOOT_FDT_OK;
            }
            pos += (plen + 3u) & ~3u;
            continue;
        }
        return SDBOOT_FDT_ERR_RANGE;
    }
    return SDBOOT_FDT_ERR_MISS;
}
