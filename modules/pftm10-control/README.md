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

Host-side acceptance can be run with `tools/check-device.ps1`.
