#!/usr/bin/env python3
"""Validate Wi-Fi/SSH example files and gpio/wifi-setup script contracts."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]


def fail(msg: str) -> None:
    print("FAIL", msg, file=sys.stderr)
    raise SystemExit(1)


def parse_wifi_conf(text: str) -> tuple[str, str]:
    ssid = psk = ""
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("ssid="):
            ssid = line[5:]
        elif line.startswith("psk="):
            psk = line[4:]
    return ssid, psk


def main() -> int:
    wifi = (REPO / "image" / "wifi.conf.example").read_text(encoding="utf-8")
    ssid, psk = parse_wifi_conf(wifi)
    if ssid != "YourNetwork" or psk != "YourPassphrase":
        fail(f"wifi.conf.example parsed as ssid={ssid!r} psk={psk!r}")
    if '"' in ssid or "\\" in ssid or '"' in psk or "\\" in psk:
        fail("example must not contain quotes or backslashes")

    auth = (REPO / "image" / "authorized_keys.example").read_text(encoding="utf-8")
    if "authorized_keys" not in auth and "ssh-ed25519" not in auth:
        fail("authorized_keys.example should mention ssh-ed25519")

    gpio = REPO / "rootfs" / "overlay" / "usr/bin/gpio"
    wifi_sh = REPO / "rootfs" / "overlay" / "usr/bin/wifi-setup"
    ssh_sh = REPO / "rootfs" / "overlay" / "usr/bin/ssh-setup"
    for p in (gpio, wifi_sh, ssh_sh):
        if not p.is_file():
            fail(f"missing {p}")
        mode = p.stat().st_mode
        if not (mode & 0o111):
            fail(f"{p} is not executable")

    out = subprocess.check_output(["sh", str(gpio), "help"], text=True)
    if "gpio list" not in out and "usage:" not in out:
        fail(f"gpio help: {out!r}")

    for script, token in (
        (wifi_sh, "ssid="),
        (ssh_sh, "dropbear"),
        (REPO / "rootfs" / "overlay" / "init", "wifi-setup --auto"),
        (REPO / "rootfs" / "overlay" / "init", "ssh-setup --auto"),
    ):
        text = script.read_text(encoding="utf-8")
        if token not in text:
            fail(f"{script} missing {token!r}")

    dts = (REPO / "dts" / "esp32p4-microcore.dts").read_text(encoding="utf-8")
    for token in ("espressif,esp32p4-gpio", "C6_EN", "esp-hosted-mcu",
                  "espressif,esp_sdio", "snps,num-slots"):
        if token not in dts:
            fail(f"DTS missing {token}")

    kcfg = (REPO / "linux" / "microcore.config").read_text(encoding="utf-8")
    for token in ("CONFIG_GPIO_ESP32P4=y", "CONFIG_CFG80211=y", "CONFIG_INET=y",
                  "CONFIG_ESP_HOSTED_NG_SDIO=y"):
        if token not in kcfg:
            fail(f"microcore.config missing {token}")

    full = (REPO / "linux" / "kernel.config").read_text(encoding="utf-8")
    for token in ("CONFIG_GPIO_ESP32P4=y", "CONFIG_ESP_HOSTED_NG_SDIO=y"):
        if token not in full:
            fail(f"kernel.config missing {token}")

    print("netconf ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
