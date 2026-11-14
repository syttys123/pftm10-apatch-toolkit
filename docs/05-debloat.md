# Physical Image De-bloat

Live package disabling is reversible and useful for discovery. Physical image
de-bloat is a separate, higher-risk operation.

## Classification

Classify every package before removal:

- debug and engineering tools
- telemetry and cloud-control services
- OTA and security-center components
- scenario and registration services
- removable preload applications

Retain camera, telephony, emergency, GPS, scheduler, thermal, and the only
available browser unless a tested replacement exists.

## Build

Rebuild the matching EROFS image with explicit excluded paths:

```sh
mkfs.erofs --exclude-path=/path/to/package ...
fsck.erofs image.img
dump.erofs --path=/path/to/package image.img
```

The compact image is not a flash-ready partition image. Pad it to the target
partition size or use an accepted sparse/extent write plan.

## Write and Verify

- prefer raw DA extents for verity-protected partitions
- use 64 MiB bounded chunks for large extents
- hash every chunk and the final partition slice
- cold boot before claiming success
- verify launcher, Settings, SMS, dialer, camera, telephony, and EROFS logs

Runtime writable dm maps are not accepted as persistent evidence when a cold
boot loads a different tree.
