#include "sdboot/fat.h"
#include "sdboot/endian.h"

#include <string.h>

#define FAT_ATTR_RO     0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYS    0x04
#define FAT_ATTR_VOL    0x08
#define FAT_ATTR_DIR    0x10
#define FAT_ATTR_ARCH   0x20
#define FAT_ATTR_LFN    0x0F

#define FAT_EOC16  0xFFF8u
#define FAT_EOC32  0x0FFFFFF8u
#define FAT_MASK32 0x0FFFFFFFu

static int read_sector(sdboot_fat_t *fs, uint32_t abs_lba)
{
    int rc;

    if (fs->cached_valid && fs->cached_lba == abs_lba)
        return 0;
    rc = fs->read(fs->ctx, abs_lba, fs->sector);
    if (rc != 0)
        return SDBOOT_FAT_ERR_IO;
    fs->cached_lba = abs_lba;
    fs->cached_valid = 1;
    return 0;
}

static uint32_t cluster_to_lba(const sdboot_fat_t *fs, uint32_t cluster)
{
    return fs->data_lba + (cluster - 2u) * fs->sectors_per_cluster;
}

static int fat_entry(sdboot_fat_t *fs, uint32_t cluster, uint32_t *out)
{
    uint32_t offset, lba, ent;
    int rc;

    if (fs->fat_bits == 32) {
        offset = cluster * 4u;
        lba = fs->fat_lba + (offset / SDBOOT_SECTOR_SIZE);
        rc = read_sector(fs, lba);
        if (rc)
            return rc;
        ent = sdboot_le32(fs->sector + (offset % SDBOOT_SECTOR_SIZE)) & FAT_MASK32;
        *out = ent;
        return 0;
    }
    offset = cluster * 2u;
    lba = fs->fat_lba + (offset / SDBOOT_SECTOR_SIZE);
    rc = read_sector(fs, lba);
    if (rc)
        return rc;
    *out = sdboot_le16(fs->sector + (offset % SDBOOT_SECTOR_SIZE));
    return 0;
}

static int is_eoc(const sdboot_fat_t *fs, uint32_t ent)
{
    if (fs->fat_bits == 32)
        return ent >= FAT_EOC32;
    return ent >= FAT_EOC16;
}

int sdboot_fat_mount(sdboot_fat_t *fs, sdboot_read_fn read, void *ctx, uint32_t part_lba)
{
    uint16_t bps, res, root_ent, fat16;
    uint8_t spc, nfats;
    uint32_t tot16, tot32, fatsz32, fatsz, tot, root_secs, data_secs, clusters;
    int rc;

    if (!fs || !read)
        return SDBOOT_FAT_ERR_FMT;
    memset(fs, 0, sizeof(*fs));
    fs->read = read;
    fs->ctx = ctx;
    fs->part_lba = part_lba;

    rc = read_sector(fs, part_lba);
    if (rc)
        return rc;

    bps = sdboot_le16(fs->sector + 11);
    if (bps != SDBOOT_SECTOR_SIZE)
        return SDBOOT_FAT_ERR_FMT;
    spc = fs->sector[13];
    res = sdboot_le16(fs->sector + 14);
    nfats = fs->sector[16];
    root_ent = sdboot_le16(fs->sector + 17);
    tot16 = sdboot_le16(fs->sector + 19);
    fat16 = sdboot_le16(fs->sector + 22);
    tot32 = sdboot_le32(fs->sector + 32);
    fatsz32 = sdboot_le32(fs->sector + 36);

    if (spc == 0 || nfats == 0 || res == 0)
        return SDBOOT_FAT_ERR_FMT;

    fatsz = fat16 ? fat16 : fatsz32;
    tot = tot16 ? tot16 : tot32;
    if (fatsz == 0 || tot == 0)
        return SDBOOT_FAT_ERR_FMT;

    root_secs = ((uint32_t)root_ent * 32u + (bps - 1u)) / bps;
    data_secs = tot - res - (uint32_t)nfats * fatsz - root_secs;
    clusters = data_secs / spc;

    fs->sectors_per_cluster = spc;
    fs->reserved_sectors = res;
    fs->num_fats = nfats;
    fs->fat_size_sectors = fatsz;
    fs->root_entries = root_ent;
    fs->fat_lba = part_lba + res;
    fs->root_lba = fs->fat_lba + (uint32_t)nfats * fatsz;
    fs->data_lba = fs->root_lba + root_secs;
    fs->cluster_count = clusters;
    fs->bytes_per_cluster = (uint32_t)spc * SDBOOT_SECTOR_SIZE;

    if (clusters >= 65525u) {
        fs->fat_bits = 32;
        fs->root_cluster = sdboot_le32(fs->sector + 44);
        if (fs->root_cluster < 2)
            fs->root_cluster = 2;
        if (memcmp(fs->sector + 0x52, "FAT32", 5) != 0 &&
            memcmp(fs->sector + 0x52, "FAT3", 4) != 0) {
            /* Still treat as FAT32 by cluster count. */
        }
    } else if (clusters >= 4085u) {
        fs->fat_bits = 16;
        fs->root_cluster = 0;
    } else {
        return SDBOOT_FAT_ERR_FMT;
    }
    return 0;
}

static void sfn_to_name(const uint8_t *ent, char *out, size_t outsz)
{
    char tmp[13];
    int n = 0;
    int i;

    for (i = 0; i < 8 && ent[i] != ' '; i++) {
        char c = (char)ent[i];
        if (i == 0 && (uint8_t)c == 0x05)
            c = (char)0xE5;
        tmp[n++] = c;
    }
    if (ent[8] != ' ') {
        tmp[n++] = '.';
        for (i = 8; i < 11 && ent[i] != ' '; i++)
            tmp[n++] = (char)ent[i];
    }
    tmp[n] = '\0';
    if ((size_t)n >= outsz)
        n = (int)outsz - 1;
    memcpy(out, tmp, (size_t)n + 1);
}

static int utf16le_ascii(const uint8_t *p, int count, char *dst, size_t *used, size_t cap)
{
    int i;
    for (i = 0; i < count; i++) {
        uint16_t ch = sdboot_le16(p + i * 2);
        if (ch == 0)
            return 1;
        if (*used + 1 >= cap)
            return -1;
        dst[(*used)++] = (ch < 128) ? (char)ch : '_';
    }
    return 0;
}

static int collect_lfn(const uint8_t *ent, char *lfn, size_t cap)
{
    /* Caller walks LFN entries in reverse (on-disk order is last-first). */
    uint8_t seq = ent[0];
    size_t used = 0;
    int last = (seq & 0x40) != 0;
    int ord = seq & 0x1F;
    size_t slot;

    if (ord == 0 || ord > 20)
        return -1;
    slot = (size_t)(ord - 1) * 13u;
    if (slot >= cap)
        return -1;
    used = slot;
    if (utf16le_ascii(ent + 1, 5, lfn, &used, cap) < 0)
        return -1;
    if (utf16le_ascii(ent + 14, 6, lfn, &used, cap) < 0)
        return -1;
    if (utf16le_ascii(ent + 28, 2, lfn, &used, cap) < 0)
        return -1;
    if (last)
        lfn[used] = '\0';
    (void)last;
    return 0;
}

static int name_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = *a;
        int cb = *b;
        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';
        if (ca != cb)
            return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int next_component(const char **path, char *comp, size_t compsize)
{
    const char *p = *path;
    size_t n = 0;

    while (*p == '/' || *p == '\\')
        p++;
    if (*p == '\0') {
        *path = p;
        return 0;
    }
    while (*p && *p != '/' && *p != '\\') {
        if (n + 1 >= compsize)
            return -1;
        comp[n++] = *p++;
    }
    comp[n] = '\0';
    *path = p;
    return 1;
}

typedef struct {
    uint32_t cluster;
    uint32_t sector_in_cluster;
    uint32_t index_in_sector; /* 0..15 */
    uint32_t root16_sector;   /* FAT16 root linear sector index */
    int      fat16_root;
    int      eof;
} dir_iter_t;

static int dir_iter_init(sdboot_fat_t *fs, uint32_t dir_cluster, dir_iter_t *it)
{
    memset(it, 0, sizeof(*it));
    if (fs->fat_bits != 32 && dir_cluster == 0) {
        it->fat16_root = 1;
        it->cluster = 0;
        return 0;
    }
    it->cluster = dir_cluster ? dir_cluster : fs->root_cluster;
    return 0;
}

static int dir_iter_next(sdboot_fat_t *fs, dir_iter_t *it, uint8_t **ent)
{
    int rc;
    uint32_t lba;

    if (it->eof)
        return 0;

    if (it->fat16_root) {
        uint32_t root_secs = ((uint32_t)fs->root_entries * 32u) / SDBOOT_SECTOR_SIZE;
        if (it->index_in_sector >= 16) {
            it->index_in_sector = 0;
            it->root16_sector++;
        }
        if (it->root16_sector >= root_secs) {
            it->eof = 1;
            return 0;
        }
        lba = fs->root_lba + it->root16_sector;
        rc = read_sector(fs, lba);
        if (rc)
            return rc;
        *ent = fs->sector + it->index_in_sector * 32u;
        it->index_in_sector++;
        return 1;
    }

    if (it->index_in_sector >= 16) {
        it->index_in_sector = 0;
        it->sector_in_cluster++;
    }
    if (it->sector_in_cluster >= fs->sectors_per_cluster) {
        uint32_t next;
        it->sector_in_cluster = 0;
        rc = fat_entry(fs, it->cluster, &next);
        if (rc)
            return rc;
        if (is_eoc(fs, next) || next < 2) {
            it->eof = 1;
            return 0;
        }
        it->cluster = next;
    }
    lba = cluster_to_lba(fs, it->cluster) + it->sector_in_cluster;
    rc = read_sector(fs, lba);
    if (rc)
        return rc;
    *ent = fs->sector + it->index_in_sector * 32u;
    it->index_in_sector++;
    return 1;
}

static uint32_t ent_cluster(const uint8_t *ent)
{
    return ((uint32_t)sdboot_le16(ent + 20) << 16) | sdboot_le16(ent + 26);
}

static int scan_dir(sdboot_fat_t *fs, uint32_t dir_cluster, const char *want,
                    sdboot_fat_list_cb cb, void *user, sdboot_fat_stat_t *found)
{
    dir_iter_t it;
    char lfn[SDBOOT_FAT_NAME_MAX + 1];
    int have_lfn = 0;
    int rc;

    lfn[0] = '\0';
    rc = dir_iter_init(fs, dir_cluster, &it);
    if (rc)
        return rc;

    for (;;) {
        uint8_t *ent;
        uint8_t attr;
        sdboot_fat_stat_t st;
        int got;

        got = dir_iter_next(fs, &it, &ent);
        if (got < 0)
            return got;
        if (got == 0)
            break;
        if (ent[0] == 0x00)
            break;
        if (ent[0] == 0xE5) {
            have_lfn = 0;
            continue;
        }
        attr = ent[11];
        if ((attr & FAT_ATTR_LFN) == FAT_ATTR_LFN) {
            if (ent[0] & 0x40)
                memset(lfn, 0, sizeof(lfn));
            if (collect_lfn(ent, lfn, sizeof(lfn)) == 0)
                have_lfn = 1;
            continue;
        }
        if (attr & FAT_ATTR_VOL) {
            have_lfn = 0;
            continue;
        }

        {
            char sfn[13];

            memset(&st, 0, sizeof(st));
            sfn_to_name(ent, sfn, sizeof(sfn));
            if (have_lfn && lfn[0]) {
                size_t n = strlen(lfn);
                if (n > SDBOOT_FAT_NAME_MAX)
                    n = SDBOOT_FAT_NAME_MAX;
                memcpy(st.name, lfn, n);
                st.name[n] = '\0';
            } else {
                memcpy(st.name, sfn, sizeof(sfn));
            }
            st.size = sdboot_le32(ent + 28);
            st.first_cluster = ent_cluster(ent);
            st.is_dir = (attr & FAT_ATTR_DIR) != 0;
            have_lfn = 0;
            lfn[0] = '\0';

            if (cb)
                cb(user, &st);
            if (want && (name_ieq(st.name, want) || name_ieq(sfn, want))) {
                if (found)
                    *found = st;
                return 1;
            }
        }
    }
    return 0;
}

static int lookup(sdboot_fat_t *fs, const char *path, sdboot_fat_stat_t *out)
{
    const char *p = path ? path : "/";
    uint32_t cluster = (fs->fat_bits == 32) ? fs->root_cluster : 0;
    char comp[SDBOOT_FAT_NAME_MAX + 1];
    int last_was_dir = 1;
    sdboot_fat_stat_t st;
    int rc;

    memset(&st, 0, sizeof(st));
    st.is_dir = 1;
    st.first_cluster = cluster;
    strcpy(st.name, "/");

    while ((rc = next_component(&p, comp, sizeof(comp))) == 1) {
        sdboot_fat_stat_t child;
        int found;

        if (!last_was_dir)
            return SDBOOT_FAT_ERR_NOENT;
        found = scan_dir(fs, cluster, comp, NULL, NULL, &child);
        if (found < 0)
            return found;
        if (found == 0)
            return SDBOOT_FAT_ERR_NOENT;
        st = child;
        cluster = child.first_cluster;
        last_was_dir = child.is_dir;
    }
    if (rc < 0)
        return SDBOOT_FAT_ERR_RANGE;
    if (out)
        *out = st;
    return 0;
}

int sdboot_fat_stat(sdboot_fat_t *fs, const char *path, sdboot_fat_stat_t *out)
{
    if (!fs || !out)
        return SDBOOT_FAT_ERR_FMT;
    return lookup(fs, path, out);
}

int sdboot_fat_list(sdboot_fat_t *fs, const char *dir_path,
                    sdboot_fat_list_cb cb, void *user)
{
    sdboot_fat_stat_t st;
    int rc;

    rc = lookup(fs, dir_path, &st);
    if (rc)
        return rc;
    if (!st.is_dir)
        return SDBOOT_FAT_ERR_NOENT;
    rc = scan_dir(fs, st.first_cluster, NULL, cb, user, NULL);
    if (rc < 0)
        return rc;
    return 0;
}

int sdboot_fat_read(sdboot_fat_t *fs, const char *path, uint32_t off,
                    void *dst, uint32_t len)
{
    sdboot_fat_stat_t st;
    uint32_t cluster, skipped, copied;
    uint8_t *out = (uint8_t *)dst;
    int rc;

    if (!dst && len)
        return SDBOOT_FAT_ERR_RANGE;
    rc = lookup(fs, path, &st);
    if (rc)
        return rc;
    if (st.is_dir)
        return SDBOOT_FAT_ERR_NOENT;
    if (off > st.size)
        return SDBOOT_FAT_ERR_RANGE;
    if (off + len > st.size)
        len = st.size - off;
    if (len == 0)
        return 0;

    cluster = st.first_cluster;
    skipped = 0;
    while (skipped + fs->bytes_per_cluster <= off) {
        uint32_t next;
        if (cluster < 2)
            return SDBOOT_FAT_ERR_FMT;
        rc = fat_entry(fs, cluster, &next);
        if (rc)
            return rc;
        if (is_eoc(fs, next))
            return SDBOOT_FAT_ERR_FMT;
        cluster = next;
        skipped += fs->bytes_per_cluster;
    }

    copied = 0;
    while (copied < len) {
        uint32_t in_cluster = off - skipped;
        uint32_t sec = in_cluster / SDBOOT_SECTOR_SIZE;
        uint32_t b_off = in_cluster % SDBOOT_SECTOR_SIZE;
        uint32_t chunk;
        uint32_t lba;

        if (cluster < 2)
            return SDBOOT_FAT_ERR_FMT;
        lba = cluster_to_lba(fs, cluster) + sec;
        rc = read_sector(fs, lba);
        if (rc)
            return rc;
        chunk = SDBOOT_SECTOR_SIZE - b_off;
        if (chunk > len - copied)
            chunk = len - copied;
        memcpy(out + copied, fs->sector + b_off, chunk);
        copied += chunk;
        off += chunk;
        in_cluster += chunk;
        if (in_cluster >= fs->bytes_per_cluster) {
            uint32_t next;
            skipped += fs->bytes_per_cluster;
            rc = fat_entry(fs, cluster, &next);
            if (rc)
                return rc;
            cluster = next;
        }
    }
    return (int)copied;
}
