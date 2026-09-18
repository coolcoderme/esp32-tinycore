#ifndef SDBOOT_GPT_H
#define SDBOOT_GPT_H

#include "sdboot/disk.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    SDBOOT_GPT_OK        = 0,
    SDBOOT_GPT_ERR_IO    = -1,
    SDBOOT_GPT_ERR_MAGIC = -2,
    SDBOOT_GPT_ERR_RANGE = -3,
    SDBOOT_GPT_ERR_NONE  = -4,
};

typedef struct {
    uint32_t start_lba;
    uint32_t sectors;
    uint8_t  type_guid[16];
} sdboot_gpt_part_t;

typedef struct {
    int               count;
    sdboot_gpt_part_t parts[8];
} sdboot_gpt_t;

/* True if this GPT type GUID is a FAT volume (EFI System or Microsoft Basic Data). */
int sdboot_gpt_type_is_fat(const uint8_t type_guid[16]);

/* Parse a GPT from a sector reader. Reads LBA 1 (header) and the entry array. */
int sdboot_gpt_parse(sdboot_read_fn read, void *ctx, sdboot_gpt_t *out);

#ifdef __cplusplus
}
#endif

#endif
