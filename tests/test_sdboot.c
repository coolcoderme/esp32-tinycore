// SPDX-License-Identifier: MIT
/*
 * Host tests for the portable SD boot helpers: MBR, FAT, loader.cfg,
 * kernel identification, PSRAM layout, and DTB patching.
 */
#include "sdboot/mbr.h"
#include "sdboot/fat.h"
#include "sdboot/cfg.h"
#include "sdboot/detect.h"
#include "sdboot/layout.h"
#include "sdboot/fdtpatch.h"
#include "sdboot/endian.h"
#include "sdboot/gpt.h"
#include "sdboot/part.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <strings.h>

static int g_fail;
static int g_ok;

#define CHECK(cond, ...) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__); \
            fprintf(stderr, "\n"); \
            g_fail++; \
        } else { \
            g_ok++; \
        } \
    } while (0)

typedef struct {
    FILE *f;
} file_disk_t;

static int file_read(void *ctx, uint32_t lba, void *buf)
{
    file_disk_t *d = (file_disk_t *)ctx;
    if (fseek(d->f, (long)lba * 512L, SEEK_SET) != 0)
        return -1;
    return fread(buf, 1, 512, d->f) == 512 ? 0 : -1;
}

static void test_cfg(void)
{
    sdboot_cfg_t cfg;
    const char *txt =
        "default microcore\n"
        "timeout 20\n"
        "\n"
        "label other\n"
        "  kernel /boot/other\n"
        "\n"
        "label microcore\n"
        "  kernel /boot/vmlinuz\n"
        "  initrd /boot/core.gz,/boot/extra.gz\n"
        "  fdt    /boot/esp32p4.dtb\n"
        "  append console=ttyS0,115200n8 earlycon rdinit=/init\n";

    CHECK(sdboot_cfg_parse(txt, &cfg) == 0, "parse");
    CHECK(strcmp(cfg.label, "microcore") == 0, "label=%s", cfg.label);
    CHECK(strcmp(cfg.kernel, "/boot/vmlinuz") == 0, "kernel=%s", cfg.kernel);
    CHECK(strcmp(cfg.initrd, "/boot/core.gz") == 0, "initrd=%s", cfg.initrd);
    CHECK(strcmp(cfg.fdt, "/boot/esp32p4.dtb") == 0, "fdt=%s", cfg.fdt);
    CHECK(strstr(cfg.append, "rdinit=/init") != NULL, "append");
    CHECK(cfg.timeout_ds == 20, "timeout=%u", cfg.timeout_ds);
}

static void test_isolinux(void)
{
    sdboot_cfg_t cfg;
    const char *txt =
        "DEFAULT microcore\n"
        "TIMEOUT 20\n"
        "LABEL microcore\n"
        "KERNEL /boot/vmlinuz\n"
        "INITRD /boot/core.gz\n"
        "APPEND console=ttyS0,115200n8 rdinit=/init\n";

    CHECK(sdboot_cfg_parse(txt, &cfg) == 0, "isolinux parse");
    CHECK(strcmp(cfg.kernel, "/boot/vmlinuz") == 0, "isolinux kernel");
    CHECK(strcmp(cfg.initrd, "/boot/core.gz") == 0, "isolinux initrd");
    CHECK(strstr(cfg.append, "rdinit=/init") != NULL, "isolinux append");
}

static void test_detect(void)
{
    uint8_t sec0[512];
    uint8_t sec16[512];
    uint8_t riscv[64];
    uint8_t x86[0x210];

    memset(sec0, 0, sizeof(sec0));
    memset(sec16, 0, sizeof(sec16));
    sec0[510] = 0x55;
    sec0[511] = 0xAA;
    sec0[0x1BE + 4] = 0x0C;
    sec0[0x1BE + 8] = 0x00;
    sec0[0x1BE + 9] = 0x08; /* start 2048 */
    sec0[0x1BE + 12] = 0x00;
    sec0[0x1BE + 13] = 0x10;
    CHECK(sdboot_detect_media(sec0, sec16) == SDBOOT_MEDIA_MBR, "mbr media");

    memcpy(sec16 + 1, "CD001", 5);
    CHECK(sdboot_detect_media(sec0, sec16) == SDBOOT_MEDIA_ISO9660, "iso media");

    memset(riscv, 0, sizeof(riscv));
    memcpy(riscv + 48, "RISCV", 5);
    riscv[16] = 0x80;
    riscv[17] = 0x00; /* 128 byte image_size */
    CHECK(sdboot_detect_kernel(riscv, sizeof(riscv)) == SDBOOT_KERN_RISCV, "riscv");
    CHECK(sdboot_riscv_image_size(riscv, sizeof(riscv)) == 0x80, "img size");

    memset(x86, 0, sizeof(x86));
    x86[510] = 0x55;
    x86[511] = 0xAA;
    memcpy(x86 + 0x202, "HdrS", 4);
    CHECK(sdboot_detect_kernel(x86, sizeof(x86)) == SDBOOT_KERN_X86, "x86");
}

static void test_layout(void)
{
    sdboot_layout_in_t in;
    sdboot_layout_out_t out;

    memset(&in, 0, sizeof(in));
    in.psram_base = SDBOOT_PSRAM_BASE;
    in.psram_size = 64u * 1024u * 1024u;
    in.kernel_size = 5u * 1024u * 1024u;
    in.dtb_size = 4096;
    in.initrd_size = 6u * 1024u * 1024u;
    CHECK(sdboot_layout(&in, &out) == 0, "64MB layout");
    CHECK(out.kernel_pa == SDBOOT_PSRAM_BASE, "kpa");
    CHECK(out.initrd_pa > out.dtb_pa, "initrd above dtb");
    CHECK(out.dtb_pa > out.kernel_pa + in.kernel_size, "no overlap");
    CHECK((out.initrd_pa & (SDBOOT_INITRD_ALIGN - 1)) == 0, "initrd align");
    CHECK(out.free_ram >= SDBOOT_MIN_FREE_RAM, "free");

    in.psram_size = 16u * 1024u * 1024u;
    in.kernel_size = 8u * 1024u * 1024u;
    in.initrd_size = 8u * 1024u * 1024u;
    CHECK(sdboot_layout(&in, &out) == SDBOOT_LAYOUT_ERR_NOSPC, "too small");
}

typedef struct {
    uint8_t *buf;
    uint32_t nsec;
} mem_disk_t;

static int mem_read(void *ctx, uint32_t lba, void *buf)
{
    mem_disk_t *d = (mem_disk_t *)ctx;
    if (lba >= d->nsec)
        return -1;
    memcpy(buf, d->buf + (size_t)lba * 512u, 512);
    return 0;
}

static void test_gpt(void)
{
    uint8_t disk[4 * 512];
    mem_disk_t md;
    sdboot_gpt_t gpt;
    uint32_t lba = 0;
    enum sdboot_media media = SDBOOT_MEDIA_UNKNOWN;
    static const uint8_t ms_basic[16] = {
        0xa2, 0xa0, 0xd0, 0xeb, 0xe5, 0xb9, 0x33, 0x44,
        0x87, 0xc9, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7
    };

    memset(disk, 0, sizeof(disk));
    /* Protective MBR */
    disk[510] = 0x55;
    disk[511] = 0xAA;
    disk[0x1BE + 4] = 0xEE;
    disk[0x1BE + 8] = 1; /* start LBA 1 */
    disk[0x1BE + 12] = 0xFF;
    disk[0x1BE + 13] = 0xFF;

    /* GPT header at LBA 1 */
    memcpy(disk + 512, "EFI PART", 8);
    disk[512 + 72] = 2; /* entry LBA */
    disk[512 + 80] = 4; /* 4 entries */
    disk[512 + 84] = 128;

    /* Entry 0 at LBA 2: Microsoft Basic Data starting at 2048 */
    memcpy(disk + 1024, ms_basic, 16);
    disk[1024 + 32] = 0x00;
    disk[1024 + 33] = 0x08; /* start 2048 */
    disk[1024 + 40] = 0xFF;
    disk[1024 + 41] = 0xFF; /* end */

    md.buf = disk;
    md.nsec = 4;
    CHECK(sdboot_detect_media(disk, NULL) == SDBOOT_MEDIA_GPT,
          "gpt media (protective MBR)");
    CHECK(sdboot_gpt_parse(mem_read, &md, &gpt) == 0, "gpt parse");
    CHECK(gpt.count >= 1, "gpt count=%d", gpt.count);
    CHECK(sdboot_gpt_type_is_fat(gpt.parts[0].type_guid), "gpt fat guid");
    CHECK(gpt.parts[0].start_lba == 2048, "gpt start=%u", gpt.parts[0].start_lba);
    CHECK(sdboot_find_fat_lba(mem_read, &md, &lba, &media) == 0, "find gpt fat");
    CHECK(lba == 2048, "found lba=%u", lba);
}

static void test_fdt(const char *dtb_path)
{
    FILE *f;
    long sz;
    uint8_t *buf;
    uint32_t total = 0;
    char bootargs[256];

    if (!dtb_path)
        return;
    f = fopen(dtb_path, "rb");
    CHECK(f != NULL, "open dtb %s", dtb_path);
    if (!f)
        return;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)sz);
    CHECK(buf != NULL, "malloc dtb");
    CHECK(fread(buf, 1, (size_t)sz, f) == (size_t)sz, "read dtb");
    fclose(f);

    CHECK(sdboot_fdt_valid(buf, (size_t)sz, &total) == 0, "fdt valid");
    CHECK(sdboot_fdt_replace_be32_pair(buf, total,
                                       SDBOOT_PSRAM_BASE, SDBOOT_PSRAM_WINDOW,
                                       SDBOOT_PSRAM_BASE, 0x02000000u) == 0,
          "patch memory size");
    CHECK(sdboot_fdt_replace_be32(buf, total, SDBOOT_FDT_INITRD_START_PH, 0x4A000000u) == 0,
          "initrd-start");
    CHECK(sdboot_fdt_replace_be32(buf, total, SDBOOT_FDT_INITRD_END_PH, 0x4B000000u) == 0,
          "initrd-end");
    CHECK(sdboot_fdt_set_bootargs(buf, total, "console=ttyS0,115200n8 rdinit=/init") == 0,
          "bootargs");
    (void)bootargs;
    free(buf);
}

static int g_saw_boot;
static int g_saw_tce;

static void root_cb(void *user, const sdboot_fat_stat_t *ent)
{
    (void)user;
    if (strcasecmp(ent->name, "boot") == 0 && ent->is_dir)
        g_saw_boot = 1;
    if (strcasecmp(ent->name, "tce") == 0 && ent->is_dir)
        g_saw_tce = 1;
}

static void test_fat_image(const char *img)
{
    FILE *f;
    file_disk_t disk;
    uint8_t sec0[512];
    sdboot_mbr_t mbr;
    sdboot_fat_t fs;
    sdboot_fat_stat_t st;
    char buf[1024];
    sdboot_cfg_t cfg;
    uint8_t khdr[64];
    int n;

    f = fopen(img, "rb");
    CHECK(f != NULL, "open %s", img);
    if (!f)
        return;
    disk.f = f;
    CHECK(file_read(&disk, 0, sec0) == 0, "read mbr");
    CHECK(sdboot_mbr_parse(sec0, &mbr) == 0 && mbr.count >= 1, "mbr");
    CHECK(mbr.parts[0].type == SDBOOT_PART_FAT32_LBA, "type 0x0c got %02x", mbr.parts[0].type);
    CHECK(mbr.parts[0].status & 0x80, "bootable");
    CHECK(sdboot_fat_mount(&fs, file_read, &disk, mbr.parts[0].start_lba) == 0, "mount");
    CHECK(fs.fat_bits == 32, "fat32 bits=%u", fs.fat_bits);

    g_saw_boot = g_saw_tce = 0;
    CHECK(sdboot_fat_list(&fs, "/", root_cb, NULL) == 0, "list /");
    CHECK(g_saw_boot, "found /boot");
    CHECK(g_saw_tce, "found /tce");

    CHECK(sdboot_fat_stat(&fs, "/boot/loader.cfg", &st) == 0, "stat loader.cfg");
    CHECK(st.size > 0 && !st.is_dir, "loader.cfg file");
    n = sdboot_fat_read(&fs, "/boot/loader.cfg", 0, buf, sizeof(buf) - 1);
    CHECK(n > 0, "read loader.cfg");
    buf[n] = '\0';
    CHECK(sdboot_cfg_parse(buf, &cfg) == 0, "parse loader.cfg from fat");
    CHECK(strcmp(cfg.kernel, "/boot/vmlinuz") == 0, "cfg kernel");

    n = sdboot_fat_read(&fs, "/boot/vmlinuz", 0, khdr, sizeof(khdr));
    CHECK(n == (int)sizeof(khdr), "read vmlinuz hdr");
    CHECK(sdboot_detect_kernel(khdr, (size_t)n) == SDBOOT_KERN_RISCV, "vmlinuz is riscv");

    CHECK(sdboot_fat_stat(&fs, "/boot/core.gz", &st) == 0, "core.gz");
    CHECK(sdboot_fat_stat(&fs, "/boot/esp32p4.dtb", &st) == 0, "dtb");
    CHECK(sdboot_fat_stat(&fs, "/tce/onboot.lst", &st) == 0, "onboot.lst");
    CHECK(sdboot_fat_stat(&fs, "/home/hello.txt", &st) == 0, "persist hello");

    n = sdboot_fat_read(&fs, "/boot/LOADER.CFG", 0, buf, sizeof(buf) - 1);
    CHECK(n > 0, "8.3 LOADER.CFG alias");

    {
        uint32_t fat_lba = 0;
        enum sdboot_media media = SDBOOT_MEDIA_UNKNOWN;
        uint8_t *kbuf;
        sdboot_layout_in_t lin;
        sdboot_layout_out_t layout;
        sdboot_fat_stat_t kst2;
        memset(&kst2, 0, sizeof(kst2));

        CHECK(sdboot_find_fat_lba(file_read, &disk, &fat_lba, &media) == 0, "find fat lba");
        CHECK(fat_lba == mbr.parts[0].start_lba, "fat lba matches mbr");
        CHECK(media == SDBOOT_MEDIA_MBR, "media mbr");

        CHECK(sdboot_fat_stat(&fs, "/boot/vmlinuz", &kst2) == 0, "stat vmlinuz");
        kbuf = malloc(kst2.size ? kst2.size : 1);
        CHECK(kbuf != NULL, "malloc kernel");
        if (kbuf) {
            n = sdboot_fat_read(&fs, "/boot/vmlinuz", 0, kbuf, kst2.size);
            CHECK(n == (int)kst2.size, "read full vmlinuz");
            CHECK(sdboot_detect_kernel(kbuf, (size_t)n) == SDBOOT_KERN_RISCV, "full image riscv");
            free(kbuf);
        }

        memset(&lin, 0, sizeof(lin));
        lin.psram_base = SDBOOT_PSRAM_BASE;
        lin.psram_size = 32u * 1024u * 1024u;
        lin.kernel_size = kst2.size;
        lin.dtb_size = 4096;
        lin.initrd_size = 4096;
        lin.min_free = SDBOOT_MIN_FREE_RAM;
        CHECK(sdboot_layout(&lin, &layout) == 0, "32MB layout of placeholders");
        CHECK(layout.kernel_pa == SDBOOT_PSRAM_BASE, "sim kernel at psram base");
    }

    fclose(f);
}

int main(int argc, char **argv)
{
    const char *img = NULL;
    const char *dtb = NULL;
    int i;

    for (i = 1; i < argc; i++) {
        if (strstr(argv[i], ".dtb"))
            dtb = argv[i];
        else
            img = argv[i];
    }

    test_cfg();
    test_isolinux();
    test_detect();
    test_layout();
    test_gpt();
    if (dtb)
        test_fdt(dtb);
    else
        fprintf(stderr, "skip fdt (no .dtb argument)\n");
    if (img)
        test_fat_image(img);
    else
        fprintf(stderr, "skip fat (no .img argument)\n");

    printf("%d ok, %d failed\n", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
