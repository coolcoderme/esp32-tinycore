#ifndef SDBOOT_ENDIAN_H
#define SDBOOT_ENDIAN_H

#include <stdint.h>

static inline uint16_t sdboot_le16(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static inline uint32_t sdboot_le32(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static inline uint64_t sdboot_le64(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return (uint64_t)sdboot_le32(b) | ((uint64_t)sdboot_le32(b + 4) << 32);
}

static inline uint32_t sdboot_be32(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

static inline void sdboot_wbe32(void *p, uint32_t v)
{
    uint8_t *b = (uint8_t *)p;
    b[0] = (uint8_t)(v >> 24);
    b[1] = (uint8_t)(v >> 16);
    b[2] = (uint8_t)(v >> 8);
    b[3] = (uint8_t)v;
}

static inline uint32_t sdboot_align_up(uint32_t v, uint32_t a)
{
    return (v + (a - 1u)) & ~(a - 1u);
}

static inline uint32_t sdboot_align_down(uint32_t v, uint32_t a)
{
    return v & ~(a - 1u);
}

#endif
