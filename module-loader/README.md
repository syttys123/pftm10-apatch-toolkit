# PFTM10 rebuilt vendor module loader

This directory contains the APatch module source that loads the vendor kernel
modules rebuilt against the self-built PFTM10 `4.19.191+` kernel.

The public tree intentionally excludes all `.ko` binaries. Supply the ten
matching modules built from the pinned vendor source:

```text
trace_mmstat.ko
connfem.ko
wmt_drv.ko
wmt_chrdev_wifi.ko
bt_drv_connac1x.ko
wlan_drv_gen4m.ko
gps_drv.ko
fmradio_drv_mt6631_6635.ko
fpsgo.ko
met.ko
```

Build the sanitized module ZIP:

```powershell
.\module-loader\build-module.ps1 `
  -ModulesPath C:\path\to\rebuilt-modules
```

The builder creates `dist/pftm10-kernel-modules-v<version>-<stamp>.zip` and
sets executable ZIP attributes for shell entry points. It also marks the ZIP
central directory as Unix-created.

## APatch compatibility

APatch's default installer resets extracted files to `0644` before sourcing
`customize.sh`. Therefore executable ZIP attributes alone are not sufficient.
The included `src/customize.sh` restores `0755` on:

```text
post-fs-data.sh
service.sh
bin/load-modules.sh
```

Status updates during post-fs-data use `/data/adb/ap/bin/resetprop -n` instead
of `setprop`.

## Kernel whitelist

The loader accepts only these verified self-built kernel timestamps:

```text
Sat Nov 14 21:43:18 CST 2026
Sun Nov 15 17:28:38 CST 2026
```

Stock and unrelated kernels skip every `insmod`.

## Installation

```powershell
$ADB = "C:\Android\platform-tools\adb.exe"
$DEVICE = "<serial>"
$ZIP = ".\dist\pftm10-kernel-modules.zip"

& $ADB -s $DEVICE push $ZIP /data/local/tmp/pftm10-kernel-modules.zip
& $ADB -s $DEVICE shell "su -c '/data/adb/apd module install /data/local/tmp/pftm10-kernel-modules.zip'"
```

Reboot once to apply the staged module.

Verify:

```sh
getprop pftm10.kernel_modules.state
cat /data/adb/pftm10-kernel-modules/boot.log
grep -E '^(wmt_drv|wmt_chrdev_wifi|wlan_drv_gen4m) ' /proc/modules
```

Do not remove `wlan_drv_gen4m` while Wi-Fi is enabled or scanning.
