#ifndef SDBOOT_CFG_H
#define SDBOOT_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

#define SDBOOT_CFG_PATH_MAX  191
#define SDBOOT_CFG_ARGS_MAX  255
#define SDBOOT_CFG_LABEL_MAX 31

typedef struct {
    char     label[SDBOOT_CFG_LABEL_MAX + 1];
    char     kernel[SDBOOT_CFG_PATH_MAX + 1];
    char     initrd[SDBOOT_CFG_PATH_MAX + 1];
    char     fdt[SDBOOT_CFG_PATH_MAX + 1];
    char     append[SDBOOT_CFG_ARGS_MAX + 1];
    unsigned timeout_ds; /* deciseconds, like syslinux */
} sdboot_cfg_t;

/* Parse a TinyCore / syslinux / extlinux-style loader.cfg (or isolinux.cfg).
 * Selects the `default` label, or the first label, or top-level kernel. */
int sdboot_cfg_parse(const char *text, sdboot_cfg_t *out);

#ifdef __cplusplus
}
#endif

#endif
