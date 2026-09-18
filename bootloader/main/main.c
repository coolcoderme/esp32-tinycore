#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_psram.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "sdmmc_io.h"
#include "handoff.h"

#include "sdboot/mbr.h"
#include "sdboot/fat.h"
#include "sdboot/cfg.h"
#include "sdboot/detect.h"
#include "sdboot/layout.h"
#include "sdboot/fdtpatch.h"

static const char *TAG = "linux-loader";

#define X86_ISO_MSG \
    "SD contains an x86 TinyCore/MicroCore image.\n" \
    "ESP32-P4 cannot run that ISO. Flash MicroCore-ESP32P4-*.img instead."

static const char *k_cfg_paths[] = {
    "/boot/loader.cfg",
    "/loader.cfg",
    "/boot/isolinux/isolinux.cfg",
    "/isolinux/isolinux.cfg",
    "/boot/syslinux/syslinux.cfg",
    "/syslinux.cfg",
    "/extlinux/extlinux.conf",
    "/boot/extlinux/extlinux.conf",
};

static void recovery_hang(const char *why)
{
    int n = 0;

    ESP_LOGE(TAG, "%s", why);
    while (1) {
        ESP_LOGE(TAG, "recovery heartbeat %d — esptool can still reflash", n++);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

typedef struct {
    int count;
} list_ctx_t;

static void list_cb(void *user, const sdboot_fat_stat_t *ent)
{
    list_ctx_t *c = (list_ctx_t *)user;
    c->count++;
    ESP_LOGI(TAG, "  %c %8" PRIu32 "  %s", ent->is_dir ? 'd' : '-',
             ent->size, ent->name);
}

static int fat_read_all(sdboot_fat_t *fs, const char *path, void *dst, uint32_t expect)
{
    uint32_t off = 0;
    uint8_t *p = (uint8_t *)dst;

    while (off < expect) {
        uint32_t chunk = expect - off;
        int n;

        if (chunk > (256u * 1024u))
            chunk = 256u * 1024u;
        n = sdboot_fat_read(fs, path, off, p + off, chunk);
        if (n < 0)
            return n;
        if ((uint32_t)n != chunk)
            return SDBOOT_FAT_ERR_IO;
        off += (uint32_t)n;
        ESP_LOGI(TAG, "  %s %" PRIu32 " / %" PRIu32 " KB",
                 path, off / 1024, expect / 1024);
    }
    return 0;
}

static int load_text_file(sdboot_fat_t *fs, const char *path, char *buf, uint32_t cap)
{
    sdboot_fat_stat_t st;
    int n, rc;

    rc = sdboot_fat_stat(fs, path, &st);
    if (rc)
        return rc;
    if (st.size + 1 > cap)
        return SDBOOT_FAT_ERR_NOSPC;
    n = sdboot_fat_read(fs, path, 0, buf, st.size);
    if (n < 0)
        return n;
    buf[st.size] = '\0';
    return 0;
}

void app_main(void)
{
    const sdboot_board_t *board = sdboot_board();
    sdmmc_card_t *card = NULL;
    uint8_t sec0[SDBOOT_SECTOR_SIZE];
    uint8_t sec16[SDBOOT_SECTOR_SIZE];
    enum sdboot_media media;
    sdboot_mbr_t mbr;
    uint32_t part_lba = 0;
    sdboot_fat_t fs;
    list_ctx_t lc;
    char cfgtext[2048];
    sdboot_cfg_t cfg;
    sdboot_fat_stat_t kst, ist, dst;
    uint8_t khdr[64];
    enum sdboot_kernel kkind;
    size_t psram_size;
    uint32_t psram_base = SDBOOT_PSRAM_BASE;
    sdboot_layout_in_t lin;
    sdboot_layout_out_t layout;
    uint32_t ksize, dsize, isize;
    int i, rc;
    int have_cfg = 0;
    int have_dtb = 0;
    int have_initrd = 0;

    ESP_LOGI(TAG, "=== ESP32-P4 linux-loader (SD MicroCore) ===");
    ESP_LOGI(TAG, "board: %s  SDMMC CLK=%d CMD=%d D0=%d width=%d LDO=%d",
             board->name, board->clk, board->cmd, board->d0, board->width, board->ldo_chan);

    if (!esp_psram_is_initialized()) {
        recovery_hang("PSRAM not initialized — enable CONFIG_SPIRAM_BOOT_INIT");
    }
    psram_size = esp_psram_get_size();
    ESP_LOGI(TAG, "PSRAM: %u MB at 0x%08" PRIx32,
             (unsigned)(psram_size / (1024 * 1024)), psram_base);
    if (psram_size < 16u * 1024u * 1024u)
        recovery_hang("PSRAM too small (need >= 16 MB)");
    if (psram_size > SDBOOT_PSRAM_WINDOW)
        psram_size = SDBOOT_PSRAM_WINDOW;

    if (sdboot_sdmmc_init(board, &card) != ESP_OK)
        recovery_hang("SD card init failed — insert a FAT32 MicroCore image");

    if (sdboot_sdmmc_read(card, 0, sec0) != 0)
        recovery_hang("failed to read sector 0");
    memset(sec16, 0, sizeof(sec16));
    (void)sdboot_sdmmc_read(card, 16, sec16);

    media = sdboot_detect_media(sec0, sec16);
    if (media == SDBOOT_MEDIA_ISO9660)
        recovery_hang(X86_ISO_MSG);
    if (media == SDBOOT_MEDIA_UNKNOWN)
        recovery_hang("no MBR/FAT on SD — flash MicroCore-ESP32P4-*.img with Rufus/Etcher");

    if (media == SDBOOT_MEDIA_MBR) {
        if (sdboot_mbr_parse(sec0, &mbr) != 0 || mbr.count == 0)
            recovery_hang("MBR parse failed");
        part_lba = 0;
        for (i = 0; i < mbr.count; i++) {
            ESP_LOGI(TAG, "part %d type=%02x start=%" PRIu32 " sectors=%" PRIu32 "%s",
                     i, mbr.parts[i].type, mbr.parts[i].start_lba, mbr.parts[i].sectors,
                     (mbr.parts[i].status & 0x80) ? " boot" : "");
            if (part_lba == 0 && sdboot_part_is_fat(mbr.parts[i].type))
                part_lba = mbr.parts[i].start_lba;
        }
        if (part_lba == 0)
            recovery_hang("no FAT partition in MBR");
    }

    rc = sdboot_fat_mount(&fs, sdboot_sdmmc_read, card, part_lba);
    if (rc)
        recovery_hang("FAT mount failed");
    ESP_LOGI(TAG, "FAT%u mounted, cluster %u bytes, %" PRIu32 " clusters",
             fs.fat_bits, (unsigned)fs.bytes_per_cluster, fs.cluster_count);

    ESP_LOGI(TAG, "SD /");
    lc.count = 0;
    (void)sdboot_fat_list(&fs, "/", list_cb, &lc);
    ESP_LOGI(TAG, "SD /boot");
    lc.count = 0;
    (void)sdboot_fat_list(&fs, "/boot", list_cb, &lc);

    memset(&cfg, 0, sizeof(cfg));
    for (i = 0; i < (int)(sizeof(k_cfg_paths) / sizeof(k_cfg_paths[0])); i++) {
        if (load_text_file(&fs, k_cfg_paths[i], cfgtext, sizeof(cfgtext)) == 0 &&
            sdboot_cfg_parse(cfgtext, &cfg) == 0 && cfg.kernel[0]) {
            ESP_LOGI(TAG, "using %s", k_cfg_paths[i]);
            have_cfg = 1;
            break;
        }
    }
    if (!have_cfg) {
        strcpy(cfg.kernel, "/boot/vmlinuz");
        strcpy(cfg.initrd, "/boot/core.gz");
        strcpy(cfg.fdt, "/boot/esp32p4.dtb");
        strcpy(cfg.append, "console=ttyS0,115200n8 earlycon rdinit=/init loglevel=4");
        ESP_LOGW(TAG, "no loader.cfg; trying TinyCore defaults");
    }
    ESP_LOGI(TAG, "kernel=%s initrd=%s fdt=%s", cfg.kernel, cfg.initrd, cfg.fdt);
    if (cfg.append[0])
        ESP_LOGI(TAG, "append=%s", cfg.append);

    if (sdboot_fat_stat(&fs, cfg.kernel, &kst) != 0)
        recovery_hang("kernel file missing");
    rc = sdboot_fat_read(&fs, cfg.kernel, 0, khdr, sizeof(khdr));
    if (rc < 0)
        recovery_hang("kernel header read failed");
    kkind = sdboot_detect_kernel(khdr, (size_t)rc);
    if (kkind == SDBOOT_KERN_X86)
        recovery_hang(X86_ISO_MSG);
    if (kkind != SDBOOT_KERN_RISCV)
        ESP_LOGW(TAG, "kernel has no RISC-V Image magic — jumping anyway");

    ksize = sdboot_riscv_image_size(khdr, (size_t)rc);
    if (ksize == 0 || ksize > kst.size)
        ksize = kst.size;

    isize = 0;
    if (cfg.initrd[0] && sdboot_fat_stat(&fs, cfg.initrd, &ist) == 0) {
        have_initrd = 1;
        isize = ist.size;
    }
    dsize = 0;
    if (cfg.fdt[0] && sdboot_fat_stat(&fs, cfg.fdt, &dst) == 0) {
        have_dtb = 1;
        dsize = dst.size;
    }

    memset(&lin, 0, sizeof(lin));
    lin.psram_base = psram_base;
    lin.psram_size = (uint32_t)psram_size;
    lin.kernel_size = ksize;
    lin.dtb_size = have_dtb ? dsize : 0;
    lin.initrd_size = have_initrd ? isize : 0;
    lin.min_free = SDBOOT_MIN_FREE_RAM;
    if (sdboot_layout(&lin, &layout) != 0)
        recovery_hang("payloads do not fit in PSRAM (need 64 MB or a smaller core.gz)");

    ESP_LOGI(TAG, "layout kernel@0x%08" PRIx32 " dtb@0x%08" PRIx32
             " initrd@0x%08" PRIx32 "-0x%08" PRIx32 " free=%" PRIu32 " KB",
             layout.kernel_pa, layout.dtb_pa, layout.initrd_pa, layout.initrd_end,
             layout.free_ram / 1024);

    ESP_LOGI(TAG, "Loading kernel %s (%" PRIu32 " KB)", cfg.kernel, ksize / 1024);
    if (fat_read_all(&fs, cfg.kernel, (void *)(uintptr_t)layout.kernel_pa, ksize) != 0)
        recovery_hang("kernel load failed");
    sdboot_cache_commit(layout.kernel_pa, ksize);

    if (have_dtb) {
        ESP_LOGI(TAG, "Loading DTB %s (%" PRIu32 " KB)", cfg.fdt, dsize / 1024);
        if (fat_read_all(&fs, cfg.fdt, (void *)(uintptr_t)layout.dtb_pa, dsize) != 0)
            recovery_hang("DTB load failed");
    }
    if (have_initrd) {
        ESP_LOGI(TAG, "Loading initrd %s (%" PRIu32 " KB)", cfg.initrd, isize / 1024);
        if (fat_read_all(&fs, cfg.initrd, (void *)(uintptr_t)layout.initrd_pa, isize) != 0)
            recovery_hang("initrd load failed");
        sdboot_cache_commit(layout.initrd_pa, isize);
    }

    if (have_dtb) {
        void *dtb = (void *)(uintptr_t)layout.dtb_pa;
        uint32_t total = 0;
        if (sdboot_fdt_valid(dtb, SDBOOT_DTB_RESERVE > dsize ? SDBOOT_DTB_RESERVE : dsize, &total) != 0)
            recovery_hang("DTB magic missing");
        (void)sdboot_fdt_replace_be32_pair(dtb, total,
                                           SDBOOT_PSRAM_BASE, SDBOOT_PSRAM_WINDOW,
                                           SDBOOT_PSRAM_BASE, (uint32_t)psram_size);
        if (have_initrd) {
            (void)sdboot_fdt_replace_be32(dtb, total, SDBOOT_FDT_INITRD_START_PH, layout.initrd_pa);
            (void)sdboot_fdt_replace_be32(dtb, total, SDBOOT_FDT_INITRD_END_PH, layout.initrd_end);
        }
        if (cfg.append[0])
            (void)sdboot_fdt_set_bootargs(dtb, total, cfg.append);
        sdboot_cache_commit(layout.dtb_pa, total);
    } else {
        ESP_LOGW(TAG, "no DTB on SD — kernel may panic without a device tree");
    }

    ESP_LOGI(TAG, "Jumping to kernel 0x%08" PRIx32 " dtb 0x%08" PRIx32, layout.kernel_pa,
             have_dtb ? layout.dtb_pa : 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    sdboot_jump_linux(layout.kernel_pa, have_dtb ? layout.dtb_pa : 0);
    recovery_hang("kernel returned");
}
