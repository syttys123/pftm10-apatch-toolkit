# PFTM10 Rebuilt Kernel Modules

This APatch module installs the ten vendor modules rebuilt against the local
`4.19.191+` test kernel.

Safety gate:

```text
Sat Nov 14 21:43:18 CST 2026
Sun Nov 15 17:28:38 CST 2026
Mon Nov 16 08:35:50 CST 2026
Tue Nov 17 13:24:56 CST 2026
```

The loader skips every `insmod` unless `/proc/version` contains one of those
exact self-built kernel timestamps. Stock and OTA kernels therefore use their
original modules.

On the test kernel, modules load in dependency order and the result is written
to:

```text
/data/adb/pftm10-kernel-modules/boot.log
```

State property:

```text
getprop pftm10.kernel_modules.state
```

Version 1.0.7 uses `resetprop -n` for status properties during post-fs-data,
avoiding the property-service deadlock documented by APatch.

Do not package this module with Windows `Compress-Archive`; it stores the shell
scripts as `0644`, and its Windows host marker causes Android `unzip` to ignore
Unix mode fields. Use `build-module.ps1`, which sets external ZIP attributes and
marks central-directory entries as Unix-created. `customize.sh` is still the
authoritative fix because APatch's default installer explicitly resets files to
`0644` before sourcing it.

Version 1.0.1 fixes an early-boot ordering race in `wlan_drv_gen4m`. WMT can
initialize its exported EMI base after the Wi-Fi module has cached it as zero.
`wlanDownloadEMISection()` now refreshes the cached EMI base and size from the
WMT exports before downloading firmware.

The source change is preserved at the repository-level kernel patch:

```text
../../kernel/patches/wlan-drv-gen4m-emi-refresh.patch
```

The `.ko` binaries are intentionally not part of the public repository. Use
the repository-level builder with the matching rebuilt module directory:

```powershell
..\build-module.ps1 -ModulesPath C:\path\to\rebuilt-modules
```
