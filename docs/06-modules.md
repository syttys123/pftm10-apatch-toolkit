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
`capabilities=ok`, KPM `1.1.1`, and the `post-fs-data` load event. It prints no
device serial or ADB identity.

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
