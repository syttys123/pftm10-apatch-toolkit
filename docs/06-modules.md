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
`capabilities=ok`, KPM `1.1.1`, the `post-fs-data` load event, and that the
CR.39 battery restrict crash mask and privacy policy are active. It prints no
device serial or ADB identity.

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

The result includes `version=1.1.1`, `load_event=post-fs-data`, and
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
another KPM. The KPM control0 commands remain bounded and read-only.

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
