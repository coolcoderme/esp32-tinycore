# esp32-tinycore

Boot **MicroCore-style Linux** on an **ESP32-P4** from the onboard
microSD card. The P4's flash holds only a small loader; the kernel,
`core.gz`, extensions, and extra storage live on the card.

Flash the SD image with **Rufus** or **BalenaEtcher**. Flash the loader
to the chip once with `esptool`.

> Official TinyCore/MicroCore ISOs from tinycorelinux.net are **x86**.
> They will not run on the P4 (RV32). This project ships a RISC-V
> MicroCore image that uses the same frugal layout (`/boot/vmlinuz`,
> `/boot/core.gz`, `/tce`) and the same USB-imager workflow.

## Status

Planning. Architecture, boot chain, SD layout, and implementation
phases are in [PLAN.md](PLAN.md).

## Intended user flow

1. Build or download `linux-loader` and flash it to the ESP32-P4.
2. Build or download `MicroCore-ESP32P4-*.img`.
3. Write that image to a microSD card in Rufus (DD / Image mode) or
   BalenaEtcher.
4. Insert the card into the board's SD reader and reset.
5. Serial console at **115200 8N1** on UART0.

## Hardware

- ESP32-P4 with onboard SDMMC microSD slot
- 64 MB external PSRAM preferred (32 MB boards work if `core.gz` stays
  small; the P4 maps at most 64 MB of PSRAM)

Default pin map is the Espressif Function EV Board SD slot (4-bit,
GPIO 39–44). Other boards are overlays.

## License

MIT. Kernel patches, when added, will be GPL-2.0 as required.
