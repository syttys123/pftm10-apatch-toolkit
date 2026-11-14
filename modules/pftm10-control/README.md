# PFTM10 Control Module

This is a local APatch system module used to prove the post-root boot hook and
provide a small read-only diagnostic entry point.

Files:

- `post-fs-data.sh`: records the early boot stage.
- `service.sh`: waits for `sys.boot_completed`, then records the late stage.
- `system.prop`: sets `ro.pftm10.control=active`.
- `bin/pftm10ctl`: root-only status, health, preload, KPM integrity, root
  context, boot-log, KPM, and SELinux mode helper.
- `uninstall.sh`: removes `/data/adb/pftm10-control`.

The module does not write Android partitions, boot images, vbmeta, SELinux
policy, or kernel memory.

`service.sh` appends a `health=ok` or `health=failed` result to
`/data/adb/pftm10-control/boot.log` after every boot. The current control
surface is available through:

```text
pftm10ctl status
pftm10ctl health
pftm10ctl preload
pftm10ctl kprobe
pftm10ctl root
pftm10ctl bootlog
pftm10ctl kpm
pftm10ctl enforcing
pftm10ctl permissive
```
