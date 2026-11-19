# PFTM10 Control Module

This is a local APatch system module used to prove the post-root boot hook and
provide a small read-only diagnostic entry point.

Files:

- `post-fs-data.sh`: records the early boot stage.
- `service.sh`: waits for `sys.boot_completed`, then records the late stage.
- `bin/boot-audit.sh`: records boot reason history at early boot and after
  `sys.boot_completed`, hashes the current pstore console buffer, classifies
  its contents as `panic`, `warning`, or `other`, and saves only a bounded
  excerpt when the buffer changes.
- `system.prop`: sets `ro.pftm10.control=active`.
- `bin/pftm10ctl`: root-only capability matrix, health, preload, manager
  supercommand, KPM integrity/load verification, root context, boot-log, KPM
  logs, and SELinux mode helper.
- `bin/pftm10-kpmctl`: static root-only adapter that drops to the APatch
  manager UID and calls `pftm10-kprobe` control0 through the KernelPatch
  supercall ABI.
- `bin/pftm10-cpuholdctl`: the same bounded adapter compiled for the separate
  `pftm10-cpuhold` KPM.
- `bin/cpu-max-watchdog.sh`: reapplies the independent CPUHOLD policy while its
  sentinel exists and rolls it back on thermal or battery limits.
- `bin/charge-3a-watchdog.sh`: while explicitly armed, polls ordinary USB
  power-supply sysfs first, skips KPM calls when no PD-capable source is
  present or the battery is full, and reapplies the gated 3A input request
  after OPPO charger policy rewrites it when PD becomes available.
- `post-fs-data.sh`: optionally masks the known CR.39
  `BatteryRestrictPlugin.apk` that crashes when GMS is absent.
- `vintf/compatibility_matrix.5.xml`: systemless compatibility-matrix override
  that removes only the framework level-5 `CONFIG_DEBUG_FS=n` kernel
  requirement while the diagnostic kernel intentionally exposes debugfs.
- `service.sh`: after `sys.boot_completed`, reapplies the camera location
  AppOps denial, fixes the OShare background-location permission as denied,
  and clears the hidden kernel charger suspend state.
- `uninstall.sh`: removes `/data/adb/pftm10-control`.

The module does not write Android partitions, boot images, vbmeta, SELinux
policy, or kernel memory.

`service.sh` appends a `health=ok` or `health=failed` result to
`/data/adb/pftm10-control/boot.log` after every boot. The current control
surface is available through:

`boot-audit.sh` keeps `/data/adb/pftm10-control/crash-audit.log` separate from
the larger IPC control audit. Every boot records `ro.boot.bootreason`, the
complete `persist.sys.boot.reason.history`, kernel banner, boot ID, and the
current pstore hash. A changed pstore hash records a bounded keyword-filtered
snapshot, distinguishing a new panic from the unchanged old ramoops export.
The pstore verdict is `panic` for panic signatures, `warning` for
warning-only shutdown data, and `other` when neither is present. Running the
audit again after boot completion captures the settled boot-reason history
without duplicating a snapshot when the pstore hash is unchanged. The audit log
rotates at 512 KiB to `crash-audit.log.1`.

```text
pftm10ctl status
pftm10ctl caps
pftm10ctl health
pftm10ctl preload
pftm10ctl kprobe
pftm10ctl kprobe-ctl <control-args>
pftm10ctl kpm-info
pftm10ctl kpm-list
pftm10ctl battery-restrict status
pftm10ctl battery-restrict mask
pftm10ctl battery-restrict restore
pftm10ctl charge-3a status
pftm10ctl charge-3a enable
pftm10ctl charge-3a disable
pftm10ctl charge-3a apply
pftm10ctl cpu-max status
pftm10ctl cpu-max enable
pftm10ctl cpu-max disable
pftm10ctl cpu-max apply
pftm10ctl cpu-max replay
pftm10ctl root
pftm10ctl bootlog
pftm10ctl crashlog
pftm10ctl kpm
pftm10ctl enforcing
pftm10ctl permissive
```

`health` and `caps` verify `pftm10-kprobe` through the APatch manager UID and
the read-only KernelPatch `module info` supercommand. This does not depend on
the kernel `dmesg` ring retaining the original boot message.

`kprobe-ctl` is fixed to `pftm10-kprobe`; it cannot load, unload, or select a
different KPM. The KPM's ordinary probes are bounded and read-only. Its explicit
state-changing commands are charger suspend recovery and PD-gated AICR control.
`charger-3a` refuses to write 3A unless `tcpm_inquire_pd_contract()` confirms a
`5V/3000mA` contract. `charge-3a disable` stops the watchdog and restores AICL
plus the 500mA request.

The watchdog uses a 30-second idle interval for `no_input`, `no_pd`, and
`full` states. It switches to 5-second retries only when the USB supply reports
`USB_PD` / `USB_PD_DRP`, at least 3A, or at least 9V. This keeps a standard
USB-CDP port from triggering a KPM call every five seconds.

`cpu-max` is intentionally opt-in. `cpu-max enable` creates
`/data/adb/pftm10-control/cpu-max.enabled`, loads the separate
`pftm10-cpuhold` KPM, applies the 2.4GHz big-core policy ceiling, and starts
the watchdog. `cpu-max disable` removes the sentinel, disables the hook,
unloads the KPM, and restores the normal OPPO policy. The watchdog rolls back
at `70C` thermal, `45C` battery, or `5%` battery. The default state is
disabled. `cpu-max replay` is the boot hook: it keeps the sentinel and leaves
the watchdog retrying if the first KPM load in a boot is transiently
unavailable.

On the current standard USB host connection, the hidden suspend state leaves
MT6360 at `IBUS=0 / IBAT=0` while `CHG_EN=1`. `service.sh` records
`charger_unsuspend_rc` and the function result in the boot log. After the call,
the hardware reports active input/battery current and `CHG_STATUS=progress`.

`battery-restrict mask` bind-mounts a zero-length file over
`BatteryRestrictPlugin.apk`. This prevents the plugin's `google_check` worker
from calling `getApplicationEnabledSetting("com.google.android.gms")` on ROMs
where GMS is not installed. The rest of `com.oplus.battery` stays active.
`restore` unmounts the override immediately. The mask is applied during
`post-fs-data` and does not modify an Android partition.

## DEBUG_FS VINTF Compatibility Override

The current diagnostic kernel is built with `CONFIG_DEBUG_FS=y`. Android 13
framework compatibility matrix level 5 requires `CONFIG_DEBUG_FS=n`, which
makes `VintfObject.verifyWithoutAvb()` return `1` and produces the boot-time
`Android 系统` / “internal problem” dialog.

`post-fs-data.sh` verifies both the stock matrix hash and the prepared patched
matrix hash, then bind-mounts the patched copy over
`/system/etc/vintf/compatibility_matrix.5.xml`. The override removes only the
`CONFIG_DEBUG_FS` requirement from the applicable `4.19.110` kernel node. It
does not change SELinux, boot, TEE, RPMB, modem, or any Android partition.

To disable the override without uninstalling the module:

```sh
su -c "touch /data/adb/pftm10-control/vintf-debugfs-allow.disabled"
```

Reboot afterward. The boot log records
`vintf_debugfs_override=mounted|disabled|unexpected_source|failed` and both
input hashes.

## Privacy Policy Replay

The OPPO camera app treats missing location permission as a startup prerequisite
and shows its own blocking dialog. Revoking the runtime permission directly is
therefore not usable. The module keeps the runtime permission granted for app
compatibility but replays these AppOps after every boot:

```text
com.oplus.camera FINE_LOCATION   ignore
com.oplus.camera COARSE_LOCATION ignore
```

The camera UI opens normally and camera device 0 remains available, while
location access is denied by AppOps. For OShare, background location is
revoked and marked `user-fixed` and `user-set` after boot. Foreground location
remains available for nearby sharing.

`service.sh` records all four return codes and `privacy_policy_rc` in
`/data/adb/pftm10-control/boot.log`.

To disable the replay without uninstalling the module:

```sh
su -c "touch /data/adb/pftm10-control/privacy.disabled"
```

Reboot afterward; the service restores the camera location AppOps to `allow`
and grants OShare background location again. Remove the file and reboot to
re-enable the privacy policy.

Host-side acceptance can be run with `tools/check-device.ps1`.

When `charge-3a` is armed on a non-PD USB source, the idle regression can be
run with:

```powershell
.\tools\check-charger-idle.ps1 -Adb C:\path\to\adb.exe
```

The check waits 35 seconds and requires no new `charger-3a` KPM calls while
the watchdog remains alive in `no_pd`, `no_input`, or `full` state.
