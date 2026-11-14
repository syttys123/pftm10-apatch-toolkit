# Device Reconnaissance

Use placeholders for the local ADB path and device selector.

```powershell
$ADB = "C:\Android\platform-tools\adb.exe"
$DEVICE = "<adb-selector>"

& $ADB devices -l
& $ADB -s $DEVICE shell getprop ro.product.model
& $ADB -s $DEVICE shell getprop ro.product.device
& $ADB -s $DEVICE shell getprop ro.build.display.id
& $ADB -s $DEVICE shell getprop ro.boot.slot_suffix
& $ADB -s $DEVICE shell getprop ro.boot.flash.locked
& $ADB -s $DEVICE shell getprop ro.boot.verifiedbootstate
& $ADB -s $DEVICE shell getenforce
& $ADB -s $DEVICE shell "su -c id"
```

Expected checkpoint after unlock and APatch:

```text
ro.boot.flash.locked        0
ro.boot.verifiedbootstate   orange
su -c id                    uid=0(root)
SELinux                     Enforcing
```

## Partition Inventory

Record by-name entries, block sizes, GPT layout, dynamic partition metadata,
and AVB descriptors. Keep the resulting CSV and GPT dump private.

```sh
ls -l /dev/block/by-name
cat /proc/partitions
lpdump
```

Verify the active slots and any dm-linear or dm-verity layers before choosing a
write path.
