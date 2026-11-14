# PFTM10 Control Module

This is a local APatch system module used to prove the post-root boot hook and
provide a small read-only diagnostic entry point.

Files:

- `post-fs-data.sh`: records the early boot stage.
- `service.sh`: waits for `sys.boot_completed`, then records the late stage.
- `system.prop`: sets `ro.pftm10.control=active`.
- `bin/pftm10ctl`: root-only capability matrix, health, preload, manager
  supercommand, KPM integrity/load verification, root context, boot-log, KPM
  logs, and SELinux mode helper.
- `bin/pftm10-kpmctl`: static root-only adapter that drops to the APatch
  manager UID and calls `pftm10-kprobe` control0 through the KernelPatch
  supercall ABI.
- `post-fs-data.sh`: optionally masks the known CR.39
  `BatteryRestrictPlugin.apk` that crashes when GMS is absent.
- `service.sh`: after `sys.boot_completed`, reapplies the camera location
  AppOps denial and fixes the OShare background-location permission as denied.
- `uninstall.sh`: removes `/data/adb/pftm10-control`.

The module does not write Android partitions, boot images, vbmeta, SELinux
policy, or kernel memory.

`service.sh` appends a `health=ok` or `health=failed` result to
`/data/adb/pftm10-control/boot.log` after every boot. The current control
surface is available through:

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
pftm10ctl root
pftm10ctl bootlog
pftm10ctl kpm
pftm10ctl enforcing
pftm10ctl permissive
```

`health` and `caps` verify `pftm10-kprobe` through the APatch manager UID and
the read-only KernelPatch `module info` supercommand. This does not depend on
the kernel `dmesg` ring retaining the original boot message.

`kprobe-ctl` is fixed to `pftm10-kprobe`; it cannot load, unload, or select a
different KPM. The current KPM control0 surface is bounded and read-only.

`battery-restrict mask` bind-mounts a zero-length file over
`BatteryRestrictPlugin.apk`. This prevents the plugin's `google_check` worker
from calling `getApplicationEnabledSetting("com.google.android.gms")` on ROMs
where GMS is not installed. The rest of `com.oplus.battery` stays active.
`restore` unmounts the override immediately. The mask is applied during
`post-fs-data` and does not modify an Android partition.

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
