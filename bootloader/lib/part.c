#include "sdboot/part.h"
#include "sdboot/mbr.h"
#include "sdboot/gpt.h"

#include <string.h>

int sdboot_find_fat_lba(sdboot_read_fn read, void *ctx, uint32_t *lba_out,
                        enum sdboot_media *media_out)
{
    uint8_t sec0[SDBOOT_SECTOR_SIZE];
    uint8_t sec16[SDBOOT_SECTOR_SIZE];
    enum sdboot_media media;
    sdboot_mbr_t mbr;
    int i, have_protective = 0;

    if (!read || !lba_out)
        return SDBOOT_PART_ERR_IO;

    if (read(ctx, 0, sec0) != 0)
        return SDBOOT_PART_ERR_IO;
    memset(sec16, 0, sizeof(sec16));
    (void)read(ctx, 16, sec16);

    media = sdboot_detect_media(sec0, sec16);
    if (media_out)
        *media_out = media;
    if (media == SDBOOT_MEDIA_ISO9660)
        return SDBOOT_PART_ERR_ISO;
    if (media == SDBOOT_MEDIA_SUPERFLOPPY) {
        *lba_out = 0;
        return SDBOOT_PART_OK;
    }

    if (media == SDBOOT_MEDIA_MBR && sdboot_mbr_parse(sec0, &mbr) == 0) {
        for (i = 0; i < mbr.count; i++) {
            if (sdboot_part_is_fat(mbr.parts[i].type)) {
                *lba_out = mbr.parts[i].start_lba;
                return SDBOOT_PART_OK;
            }
            if (mbr.parts[i].type == SDBOOT_PART_GPT_PROTECTIVE)
                have_protective = 1;
        }
    }

    if (media == SDBOOT_MEDIA_GPT || media == SDBOOT_MEDIA_MBR ||
        have_protective || media == SDBOOT_MEDIA_UNKNOWN) {
        sdboot_gpt_t gpt;
        int rc = sdboot_gpt_parse(read, ctx, &gpt);
        if (rc == SDBOOT_GPT_OK) {
            for (i = 0; i < gpt.count; i++) {
                if (sdboot_gpt_type_is_fat(gpt.parts[i].type_guid)) {
                    *lba_out = gpt.parts[i].start_lba;
                    if (media_out)
                        *media_out = SDBOOT_MEDIA_GPT;
                    return SDBOOT_PART_OK;
                }
            }
        }
    }

    return SDBOOT_PART_ERR_NONE;
}
