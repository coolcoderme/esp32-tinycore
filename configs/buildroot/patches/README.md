# Buildroot patches

`buildroot-tree/0001-package-wpa_supplicant-allow-nommu.patch` is applied
**to the Buildroot checkout** (not via `BR2_GLOBAL_PATCH_DIR`):

```sh
patch -p1 -d buildroot -i configs/buildroot/patches/buildroot-tree/0001-package-wpa_supplicant-allow-nommu.patch
```

`global-patches/wpa_supplicant/` is applied to wpa_supplicant **source**
via `BR2_GLOBAL_PATCH_DIR` (vfork instead of fork on NOMMU).
