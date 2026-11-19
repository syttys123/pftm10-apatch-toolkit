# APatch Modules and KPMs

## System Modules

The verified example is `modules/pftm10-control`.

```powershell
& $ADB -s $DEVICE push .\dist\pftm10-control.zip /data/local/tmp/module.zip
& $ADB -s $DEVICE shell "su -c '/data/adb/apd module install /data/local/tmp/module.zip'"
```

Installation stages files under `/data/adb/modules_update`. A normal reboot
applies them to `/data/adb/modules`.

Verify:

```sh
su -c "/data/adb/apd module list"
getprop ro.pftm10.control
cat /data/adb/pftm10-control/boot.log
```

Host-side read-only acceptance:

```powershell
.\tools\check-device.ps1 -Adb C:\Android\platform-tools\adb.exe
```

The check requires the applied module version and verifies `health=ok`,
`capabilities=ok`, KPM `1.4.6`, the `post-fs-data` load event, boot-time
charger suspend recovery, the gated 3A control surface, the CR.39 battery
restrict crash mask, the DEBUG_FS VINTF compatibility override, and the
privacy policy. It prints no device serial or ADB identity.

### DEBUG_FS VINTF Compatibility Override

Kernel `#5` intentionally enables `CONFIG_DEBUG_FS` for Trustonic TEE and
driver diagnostics. Framework compatibility matrix level 5 requires
`CONFIG_DEBUG_FS=n`, so the stock `VintfObject.verifyWithoutAvb()` result is
`1`. Android reports that as:

```text
Build: Vendor interface is incompatible, error=1
ActivityTaskManager: Build fingerprint is not consistent, warning user
```

The second line is misleading for this path: the problem is the kernel
configuration, not the fingerprint strings. `pftm10-control` 1.5.5 carries a
prepared matrix with only the level-5 `CONFIG_DEBUG_FS` requirement removed
from the applicable 4.19.110 kernel node. At `post-fs-data` it checks both
hashes before bind-mounting the prepared file over:

```text
/system/etc/vintf/compatibility_matrix.5.xml
```

Verified hashes:

```text
stock matrix   C3F5339CB6FE1C3FEC3C80F34845ABC80E460E75132E8C167B3ED4C2970895A2
patched matrix 961A5FF2E4B833EB7C7AC229AB8547FAE49B61C0856D0C56446B03F6A322576A
```

An unexpected stock matrix is never replaced. The boot log records:

```text
vintf_debugfs_override=mounted
vintf_debugfs_override_rc=0
```

Rollback without uninstalling:

```sh
su -c "touch /data/adb/pftm10-control/vintf-debugfs-allow.disabled"
```

Reboot afterward. The stock matrix and the original system dialog behavior
return. The override does not write a partition and does not change SELinux,
boot, TEE, RPMB, modem, or kernel memory.

`pftm10-control` 1.4.0 calls the official
`oplus_chg_unsuspend_charger()` kernel function after `sys.boot_completed`.
This clears the hidden standard-host suspend state that can leave MT6360 at
`IBUS=0 / IBAT=0 / CHG_STATUS=ready` despite `CHG_EN=1`. The return value is
recorded as `charger_unsuspend_rc` and `charger_unsuspend` in
`/data/adb/pftm10-control/boot.log`.

The optional 3A policy is armed with:

```sh
su -c "/data/adb/modules/pftm10-control/bin/pftm10ctl charge-3a enable"
```

The KPM first queries `type_c_port0` through `tcpm_inquire_pd_contract()` and
only writes MT6360 AICR when the live contract is `5V/3000mA` or higher. OPPO
policy periodically rewrites the limit back to 500mA, so an armed boot starts
a watchdog that reapplies the gated 3A request every five seconds. If the PD
contract changes or the cable moves to a non-PD host, the KPM refuses the
write. Disarm and restore 500mA with:

```sh
su -c "/data/adb/modules/pftm10-control/bin/pftm10ctl charge-3a disable"
```

`charge-3a status` reports the armed state, watchdog state, live PD contract,
and `input_current_settled`. The 3A value is an input-current ceiling, not a
claim that the battery always accepts 3A; IBAT continues to follow OPPO
CC/CV, temperature, and fuel-gauge policy.

`pftm10-control` 1.2.1 adds boot-time privacy replay:

```text
com.oplus.camera FINE_LOCATION   ignore
com.oplus.camera COARSE_LOCATION ignore
com.coloros.oshare ACCESS_BACKGROUND_LOCATION denied + USER_FIXED
```

The camera runtime permission remains granted because the OPPO camera UI treats
missing location permission as a blocking startup dialog. AppOps still denies
the actual location operations. OShare keeps foreground location for nearby
sharing while background location is denied.

Create `/data/adb/pftm10-control/privacy.disabled` and reboot to skip the
replay without uninstalling the module. In that mode the service restores the
camera location AppOps to `allow` and grants OShare background location.
Remove that file and reboot to enable the privacy policy again.

### Battery Restrict Compatibility Mask

Some CR.39 builds ship `BatteryRestrictPlugin.apk` without the corresponding
GMS package. Its `google_check` worker calls the package manager for
`com.google.android.gms` and raises `IllegalArgumentException: Unknown
package`, repeatedly terminating the shared `com.oplus.battery` process during
boot.

The control module masks only that plugin file with a zero-length bind mount:

```sh
su -c "/data/adb/modules/pftm10-control/bin/pftm10ctl battery-restrict status"
su -c "/data/adb/modules/pftm10-control/bin/pftm10ctl battery-restrict mask"
su -c "/data/adb/modules/pftm10-control/bin/pftm10ctl battery-restrict restore"
```

`mask` is applied from `post-fs-data`. `restore` removes the runtime override
without writing an Android partition. The battery service continues to expose
charging voltage, current, temperature, and health telemetry.

### Rebuilt Vendor Module Loader

The self-built kernel requires a matching set of rebuilt vendor modules. The
public source for their APatch loader is under `module-loader/`; vendor `.ko`
binaries are intentionally excluded.

Supply the ten matching modules:

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

Build and install:

```powershell
.\module-loader\build-module.ps1 -ModulesPath C:\path\to\rebuilt-modules
& $ADB -s $DEVICE push .\module-loader\dist\pftm10-kernel-modules-*.zip /data/local/tmp/module.zip
& $ADB -s $DEVICE shell "su -c '/data/adb/apd module install /data/local/tmp/module.zip'"
```

Reboot once to apply the staged update.

The loader accepts only the verified self-built kernel timestamps:

```text
Sat Nov 14 21:43:18 CST 2026
Sun Nov 15 17:28:38 CST 2026
Mon Nov 16 08:35:50 CST 2026
Tue Nov 17 13:24:56 CST 2026
```

Stock and unrelated kernels skip every `insmod`.

APatch resets module files to `0644` during the default extraction. The
module's `customize.sh` therefore uses `set_perm ... 0755` for:

```text
post-fs-data.sh
service.sh
bin/load-modules.sh
```

Without this installation hook, APatch stages the module but never executes
its boot scripts. The loader also uses `/data/adb/ap/bin/resetprop -n` for
post-fs-data status properties.

Verify:

```sh
getprop pftm10.kernel_modules.state
cat /data/adb/pftm10-kernel-modules/boot.log
su -c "grep -E '^(wmt_drv|wmt_chrdev_wifi|wlan_drv_gen4m) ' /proc/modules"
```

Expected successful state:

```text
pftm10.kernel_modules.state=loaded
WMT turn on WIFI success
wlanProbe: probe success
```

Never remove `wlan_drv_gen4m` while Wi-Fi is enabled or scanning.

## KernelPatch KPMs

The verified example is `kpms/pftm10-kprobe`.

```powershell
$env:PFTM10_KERNELPATCH_ROOT = "<KernelPatch-source>"
.\kpms\pftm10-kprobe\build.ps1
```

Build with:

```text
target: aarch64-freestanding
debug: disabled
```

Install through the APatch KPM page. A permanent installation requires a
reboot; the temporary “load” action is useful for a pre-reboot test.

Verify durable load state through the APatch manager UID:

```sh
manager_uid=$(stat -c %u /data/user/0/me.bmax.apatch)
su "$manager_uid" -c "/system/bin/truncate su module info pftm10-kprobe"
```

The result includes `version=1.4.6`, `load_event=post-fs-data`, and
`load_source=file`. `pftm10ctl health` and `pftm10ctl caps` use this check
because `dmesg` may rotate the original load message out of the ring.

`pftm10-control` also ships a static root-only adapter for direct KPM control0
access:

```sh
pftm10ctl kprobe-ctl info
pftm10ctl kprobe-ctl help
pftm10ctl kprobe-ctl banner
```

The adapter fixes the KPM name to `pftm10-kprobe`, drops to the APatch manager
UID, and invokes only `SUPERCALL_KPM_CONTROL`. It cannot load, unload, or choose
another KPM. Read-only probes are bounded. The only state-changing commands are
the explicit charger recovery and PD-gated 3A controls described above.

Boot-log context only:

```sh
su -c "dmesg | grep -i pftm10-kprobe"
```

Expected cold-boot event when the ring still retains it:

```text
pftm10-kprobe init, event: post-fs-data
load_module_ex: [pftm10-kprobe] succeed
```

## First KPM Load Failure

Directly accessing `current` can import task and thread-layout symbols. If
APatch reports `nativeLoadKernelPatchModule error: -1`, reduce the module to
`current_uid()`, `kallsyms_lookup_name()`, and the minimal string helpers, then
load again before adding task fields.
