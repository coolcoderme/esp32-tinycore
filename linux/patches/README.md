# Kernel patches

`dts/esp32p4-microcore.dts` is the board DTS. Drop it into an ESP32-P4
kernel tree (the why2025-linux series, or any tree with
`arch/riscv/boot/dts/espressif/` and the P4 CLIC/UART drivers) and add
it to that directory's Makefile:

```
dtb-$(CONFIG_SOC_ESP32P4) += esp32p4-microcore.dtb
```

`linux/microcore.config` is a fragment of extra options (initrd, VFAT,
dw_mmc, squashfs, loop) to merge into that port's defconfig.
