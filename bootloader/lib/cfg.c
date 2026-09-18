#include "sdboot/cfg.h"

#include <ctype.h>
#include <string.h>

static void trim_copy(char *dst, size_t dstsz, const char *s, size_t n)
{
    while (n && (*s == ' ' || *s == '\t')) {
        s++;
        n--;
    }
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                 s[n - 1] == '\r' || s[n - 1] == '\n'))
        n--;
    if (n >= dstsz)
        n = dstsz - 1;
    memcpy(dst, s, n);
    dst[n] = '\0';
}

static int keyeq(const char *k, size_t kn, const char *lit)
{
    size_t i;
    size_t ln = strlen(lit);
    if (kn != ln)
        return 0;
    for (i = 0; i < kn; i++) {
        int a = tolower((unsigned char)k[i]);
        int b = tolower((unsigned char)lit[i]);
        if (a != b)
            return 0;
    }
    return 1;
}

int sdboot_cfg_parse(const char *text, sdboot_cfg_t *out)
{
    const char *p;
    char default_label[SDBOOT_CFG_LABEL_MAX + 1];
    sdboot_cfg_t labels[8];
    sdboot_cfg_t global;
    int nlab = 0;
    int cur = -1;
    int i;

    if (!text || !out)
        return -1;

    memset(out, 0, sizeof(*out));
    memset(&global, 0, sizeof(global));
    memset(labels, 0, sizeof(labels));
    default_label[0] = '\0';

    p = text;
    while (*p) {
        const char *line = p;
        const char *nl = strchr(p, '\n');
        size_t linelen;
        const char *s;
        const char *key;
        size_t keyn;
        const char *val;
        size_t rest;

        if (!nl)
            nl = p + strlen(p);
        linelen = (size_t)(nl - line);
        p = *nl ? nl + 1 : nl;

        s = line;
        rest = linelen;
        while (rest && (*s == ' ' || *s == '\t')) {
            s++;
            rest--;
        }
        if (rest == 0 || *s == '#' || *s == ';')
            continue;

        key = s;
        keyn = 0;
        while (keyn < rest && s[keyn] != ' ' && s[keyn] != '\t' && s[keyn] != '=')
            keyn++;
        val = s + keyn;
        rest -= keyn;
        if (rest && (*val == ' ' || *val == '\t' || *val == '=')) {
            val++;
            rest--;
        }
        while (rest && (*val == ' ' || *val == '\t' || *val == '=')) {
            val++;
            rest--;
        }

        if (keyeq(key, keyn, "default") || keyeq(key, keyn, "ontimeout")) {
            trim_copy(default_label, sizeof(default_label), val, rest);
            continue;
        }
        if (keyeq(key, keyn, "timeout")) {
            unsigned t = 0;
            const char *vp = val;
            size_t vr = rest;
            while (vr && *vp >= '0' && *vp <= '9') {
                t = t * 10u + (unsigned)(*vp - '0');
                vp++;
                vr--;
            }
            global.timeout_ds = t;
            if (cur >= 0)
                labels[cur].timeout_ds = t;
            continue;
        }
        if (keyeq(key, keyn, "label") || keyeq(key, keyn, "menu")) {
            if (keyeq(key, keyn, "menu"))
                continue;
            if (nlab >= (int)(sizeof(labels) / sizeof(labels[0])))
                continue;
            cur = nlab++;
            memset(&labels[cur], 0, sizeof(labels[cur]));
            trim_copy(labels[cur].label, sizeof(labels[cur].label), val, rest);
            if (global.kernel[0])
                memcpy(&labels[cur], &global, sizeof(global));
            trim_copy(labels[cur].label, sizeof(labels[cur].label), val, rest);
            continue;
        }

        {
            sdboot_cfg_t *dst = (cur >= 0) ? &labels[cur] : &global;
            if (keyeq(key, keyn, "kernel") || keyeq(key, keyn, "linux"))
                trim_copy(dst->kernel, sizeof(dst->kernel), val, rest);
            else if (keyeq(key, keyn, "initrd") || keyeq(key, keyn, "initramfs")) {
                /* Take the first of a comma-separated list. */
                const char *comma = memchr(val, ',', rest);
                size_t n = comma ? (size_t)(comma - val) : rest;
                trim_copy(dst->initrd, sizeof(dst->initrd), val, n);
            }             else if (keyeq(key, keyn, "fdt") || keyeq(key, keyn, "devicetree") ||
                       keyeq(key, keyn, "dtb"))
                trim_copy(dst->fdt, sizeof(dst->fdt), val, rest);
            else if (keyeq(key, keyn, "append"))
                trim_copy(dst->append, sizeof(dst->append), val, rest);
        }
    }

    if (default_label[0] && nlab) {
        for (i = 0; i < nlab; i++) {
            if (strcmp(labels[i].label, default_label) == 0) {
                *out = labels[i];
                if (!out->timeout_ds)
                    out->timeout_ds = global.timeout_ds;
                return 0;
            }
        }
    }
    if (nlab > 0) {
        *out = labels[0];
        if (!out->timeout_ds)
            out->timeout_ds = global.timeout_ds;
        return out->kernel[0] ? 0 : -1;
    }
    *out = global;
    return out->kernel[0] ? 0 : -1;
}
