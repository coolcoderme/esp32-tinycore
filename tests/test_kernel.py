#!/usr/bin/env python3
"""Sanity-check the P4 kernel series: gpio-esp32p4, ESP-Hosted, MicroCore DTS."""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]


def fail(msg: str) -> None:
    print("FAIL", msg, file=sys.stderr)
    raise SystemExit(1)


def main() -> int:
    patches = REPO / "linux" / "patches"
    required = (
        "0001-riscv-esp32p4-baseline.patch",
        "0002-gpio-esp32p4.patch",
        "0007-mmc-dw_mmc-esp32p4-fixes.patch",
        "0008-net-wireless-esp-hosted-ng-spi.patch",
        "0040-dts-esp32p4-microcore.patch",
        "0041-esp-hosted-sdio-host.patch",
        "0042-mmc-dw_mmc-esp32p4-dual-slot.patch",
    )
    for name in required:
        p = patches / name
        if not p.is_file() or p.stat().st_size < 200:
            fail(f"missing or tiny patch {p}")

    gpio = (patches / "0002-gpio-esp32p4.patch").read_text(encoding="utf-8", errors="replace")
    if "config GPIO_ESP32P4" not in gpio or "gpio-esp32p4.c" not in gpio:
        fail("0002 does not add gpio-esp32p4")

    hosted = (patches / "0008-net-wireless-esp-hosted-ng-spi.patch").read_text(
        encoding="utf-8", errors="replace"
    )
    if "sdio/esp_sdio.c" not in hosted or "ESP_HOSTED_NG" not in hosted:
        fail("0008 does not vendor ESP-Hosted SDIO sources")

    sdio = (patches / "0041-esp-hosted-sdio-host.patch").read_text(encoding="utf-8", errors="replace")
    if "config ESP_HOSTED_NG_SDIO" not in sdio:
        fail("0041 missing ESP_HOSTED_NG_SDIO")
    if "sdio/esp_sdio.o" not in sdio:
        fail("0041 does not compile sdio/esp_sdio.o")

    dual = (patches / "0042-mmc-dw_mmc-esp32p4-dual-slot.patch").read_text(
        encoding="utf-8", errors="replace"
    )
    if "snps,num-slots" not in dual:
        fail("0042 missing snps,num-slots")

    series = sorted(patches.glob("00*.patch"))
    names = [p.name for p in series]
    if names != sorted(names):
        fail(f"patch names are not sorted: {names}")

    kcfg = (REPO / "linux" / "kernel.config").read_text(encoding="utf-8")
    for token in (
        "CONFIG_SOC_ESP32P4=y",
        "CONFIG_GPIO_ESP32P4=y",
        "CONFIG_ESP_HOSTED_NG=y",
        "CONFIG_ESP_HOSTED_NG_SDIO=y",
        "CONFIG_BLK_DEV_INITRD=y",
        "CONFIG_MMC_DW=y",
        "CONFIG_CMDLINE_EXTEND=y",
    ):
        if token not in kcfg:
            fail(f"kernel.config missing {token}")
    if "CONFIG_CMDLINE_FORCE=y" in kcfg:
        fail("kernel.config must not FORCE cmdline (loader patches bootargs)")
    if "@WHY2025_LINUX@" in kcfg:
        fail("kernel.config still has @WHY2025_LINUX@")

    frag = (REPO / "linux" / "microcore.config").read_text(encoding="utf-8")
    for token in ("CONFIG_GPIO_ESP32P4=y", "CONFIG_ESP_HOSTED_NG_SDIO=y", "CONFIG_CFG80211=y"):
        if token not in frag:
            fail(f"microcore.config missing {token}")

    dts = (REPO / "dts" / "esp32p4-microcore.dts").read_text(encoding="utf-8")
    for token in (
        "espressif,esp32p4-gpio",
        "espressif,esp_sdio",
        "espressif,esp-hosted-mcu",
        "snps,num-slots",
        "C6_EN",
        "espressif,esp32p4-wdt",
        "IRQ_TYPE_LEVEL_HIGH",
        "ipv6.disable=1",
    ):
        if token not in dts:
            fail(f"esp32p4-microcore.dts missing {token}")

    defcfg = (REPO / "configs" / "buildroot" / "esp32p4_microcore_defconfig").read_text(
        encoding="utf-8"
    )
    if "v6.18.35" not in defcfg:
        fail("defconfig must pin Linux 6.18.35")
    if "BR2_LINUX_KERNEL_PATCH=" not in defcfg:
        fail("defconfig missing BR2_LINUX_KERNEL_PATCH")
    if "esp32p4-microcore" not in defcfg:
        fail("defconfig missing in-tree DTS name")

    # Patch files look like unified diffs (not empty placeholders).
    for p in series:
        text = p.read_text(encoding="utf-8", errors="replace")
        if not re.search(r"^--- ", text, re.M) or not re.search(r"^\+\+\+ ", text, re.M):
            fail(f"{p.name} is not a unified diff")

    print(f"kernel series ok: {len(series)} patches, gpio-esp32p4 + ESP-Hosted SDIO")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
