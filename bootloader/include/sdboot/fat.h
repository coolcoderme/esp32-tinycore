#ifndef SDBOOT_FAT_H
#define SDBOOT_FAT_H

#include "sdboot/disk.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDBOOT_FAT_NAME_MAX 255

enum {
    SDBOOT_FAT_OK          = 0,
    SDBOOT_FAT_ERR_IO      = -1,
    SDBOOT_FAT_ERR_FMT     = -2,
    SDBOOT_FAT_ERR_NOENT   = -3,
    SDBOOT_FAT_ERR_RANGE   = -4,
    SDBOOT_FAT_ERR_NOSPC   = -5,
};

typedef struct {
    sdboot_read_fn read;
    void          *ctx;
    uint8_t        sector[SDBOOT_SECTOR_SIZE];
    uint32_t       cached_lba;
    int            cached_valid;

    uint32_t part_lba;
    uint8_t  fat_bits;          /* 16 or 32 */
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint32_t fat_size_sectors;
    uint32_t root_cluster;      /* FAT32; unused on FAT16 */
    uint16_t root_entries;      /* FAT16 */
    uint32_t fat_lba;
    uint32_t root_lba;          /* FAT16 root dir */
    uint32_t data_lba;
    uint32_t cluster_count;
    uint32_t bytes_per_cluster;
} sdboot_fat_t;

typedef struct {
    char     name[SDBOOT_FAT_NAME_MAX + 1];
    uint32_t size;
    uint32_t first_cluster;
    int      is_dir;
} sdboot_fat_stat_t;

typedef void (*sdboot_fat_list_cb)(void *user, const sdboot_fat_stat_t *ent);

int sdboot_fat_mount(sdboot_fat_t *fs, sdboot_read_fn read, void *ctx, uint32_t part_lba);

int sdboot_fat_stat(sdboot_fat_t *fs, const char *path, sdboot_fat_stat_t *out);

int sdboot_fat_list(sdboot_fat_t *fs, const char *dir_path,
                    sdboot_fat_list_cb cb, void *user);

/* Read up to `len` bytes of `path` starting at `off` into `dst`.
 * Returns bytes read (>=0) or a negative SDBOOT_FAT_ERR_*. */
int sdboot_fat_read(sdboot_fat_t *fs, const char *path, uint32_t off,
                    void *dst, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif
