#ifndef SDBOOT_FDTPATCH_H
#define SDBOOT_FDTPATCH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDBOOT_FDT_MAGIC           0xD00DFEEDu
#define SDBOOT_FDT_INITRD_START_PH 0x11111111u
#define SDBOOT_FDT_INITRD_END_PH   0x22222222u

enum {
    SDBOOT_FDT_OK        = 0,
    SDBOOT_FDT_ERR_MAGIC = -1,
    SDBOOT_FDT_ERR_RANGE = -2,
    SDBOOT_FDT_ERR_MISS  = -3,
    SDBOOT_FDT_ERR_NOSPC = -4,
};

int sdboot_fdt_valid(const void *fdt, size_t cap, uint32_t *totalsize_out);

/* Replace the first big-endian u32 `old` with `new`. */
int sdboot_fdt_replace_be32(void *fdt, size_t cap, uint32_t old, uint32_t newv);

/* Replace first occurrence of BE pair (a,b) with (na,nb). */
int sdboot_fdt_replace_be32_pair(void *fdt, size_t cap,
                                 uint32_t a, uint32_t b,
                                 uint32_t na, uint32_t nb);

/* Overwrite an existing "bootargs" property in place (must fit). */
int sdboot_fdt_set_bootargs(void *fdt, size_t cap, const char *args);

#ifdef __cplusplus
}
#endif

#endif
