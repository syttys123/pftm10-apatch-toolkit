# APatch Root

## Boot Image

Patch the exact stock boot image with the matching APatch / KernelPatch version,
then write it through the validated raw USB path.

Before writing:

- hash the stock boot image
- hash the patched boot image
- save the exact rollback image
- record the target slot and block mapping

After writing:

```sh
su -v
su -c id
getprop ro.boot.flash.locked
getprop ro.boot.verifiedbootstate
```

Expected:

```text
11282:APatch
uid=0(root) context=u:r:magisk:s0
0
orange
```

## APatch Userspace

Use the APatch app's own userspace installation action. It initializes
`/data/adb/apd`; it does not rewrite `system` or `system_ext`.

```powershell
& $ADB -s $DEVICE shell "su -c '/data/adb/apd -V'"
& $ADB -s $DEVICE shell "su -c '/data/adb/apd module list'"
```

`apd` is event-driven and may not stay in `ps`. `module list` manages system
modules, not KPMs.

## Cold-Boot Checkpoint

Reboot normally and require:

- APatch page reports `Full`
- root shell returns UID 0
- SELinux remains Enforcing by default
- `apd -V` returns the expected version
