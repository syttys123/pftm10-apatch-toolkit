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

## Third-Party Preload Partitions

Some product partitions contain a `del-app` tree whose APKs are copied into
`/data/app` on first boot. Removing the source APK alone does not remove the
runtime copy. Use two separate stages.

### Stage 1: Runtime Copies

Snapshot every target first:

```sh
pm list packages -f
dumpsys package PACKAGE
du -sk /data/app/PACKAGE
```

Remove the user copy and verify the package path is empty:

```sh
pm uninstall --user 0 PACKAGE
pm path PACKAGE
```

Inspect `/data/app/PACKAGE` after uninstall. Only remove that exact directory
after confirming it is the expected package directory and no longer referenced
by `pm path`.

### Stage 2: Physical Image

Rebuild the preload EROFS image with each `del-app/<name>` directory excluded.
Keep the partition UUID, mount point, file contexts, and filesystem config from
the stock image.

Verify before writing:

```sh
fsck.erofs IMAGE
dump.erofs --path=/del-app/NAME IMAGE
```

Mount the compact image through a temporary loop device on the phone and verify:

- retained files such as `build.prop` are visible
- every excluded directory is absent
- the mounted EROFS reports no corruption
- the loop device and mount point are removed afterward

For a non-verity partition backed by one or more `dm-linear` extents, a
temporary writable dm map can be used for a bounded image write. The final
acceptance criteria are:

- exact SHA256 readback of the written image length
- temporary map deleted
- normal cold boot completed
- mounted block and local image agree at a sampled offset
- excluded directories are absent after boot
- launcher, Settings, camera, dialer, contacts, and browser still resolve
- no EROFS corruption or I/O errors in kernel logs

Keep the original partition image and its SHA256 before any write. That original
image is the rollback source.
