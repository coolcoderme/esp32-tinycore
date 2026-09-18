#ifndef SDBOOT_DETECT_H
#define SDBOOT_DETECT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum sdboot_media {
    SDBOOT_MEDIA_UNKNOWN    = 0,
    SDBOOT_MEDIA_MBR        = 1,
    SDBOOT_MEDIA_SUPERFLOPPY = 2,
    SDBOOT_MEDIA_ISO9660    = 3,
};

enum sdboot_kernel {
    SDBOOT_KERN_UNKNOWN = 0,
    SDBOOT_KERN_RISCV   = 1,
    SDBOOT_KERN_X86     = 2,
};

enum sdboot_media sdboot_detect_media(const uint8_t sector0[512],
                                      const uint8_t sector16[512]);

enum sdboot_kernel sdboot_detect_kernel(const void *buf, size_t len);

/* RISC-V Image magic "RISCV" at byte 48, little-endian u64. */
#define SDBOOT_RISCV_IMAGE_MAGIC  0x5643534952ULL
#define SDBOOT_RISCV_IMAGE_MAGIC2 0x05435352u

/* Image size field at byte 16 of a RISC-V Image header. 0 if not a header. */
uint32_t sdboot_riscv_image_size(const void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
