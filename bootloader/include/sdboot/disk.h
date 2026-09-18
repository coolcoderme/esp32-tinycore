#ifndef SDBOOT_DISK_H
#define SDBOOT_DISK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDBOOT_SECTOR_SIZE 512u

/* Read exactly one 512-byte sector. Return 0 on success, negative on error. */
typedef int (*sdboot_read_fn)(void *ctx, uint32_t lba, void *buf);

#ifdef __cplusplus
}
#endif

#endif
