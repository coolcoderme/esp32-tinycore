#ifndef SDBOOT_BOARD_H
#define SDBOOT_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    int slot;
    int clk;
    int cmd;
    int d0;
    int d1;
    int d2;
    int d3;
    int width;
    int ldo_chan; /* 0 = no on-chip LDO */
    int internal_pullup;
} sdboot_board_t;

const sdboot_board_t *sdboot_board(void);

#ifdef __cplusplus
}
#endif

#endif
