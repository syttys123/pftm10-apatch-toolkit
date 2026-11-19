# Changelog

## v1.6.4 - 2026-11-19

- publish the current `pftm10-kernel-modules` 1.0.7 loader instead of the
  stale 1.0.6 source
- accept the verified Nov 16 and Nov 17 self-built kernel timestamps, matching
  the already installed module and the current `#5` kernel
- update the public module documentation to list all four accepted kernel
  timestamps

## v1.6.3 - 2026-11-18

- diagnose the boot-time `Android 系统` / internal-problem dialog as
  `VintfObject.verifyWithoutAvb()` returning `1`, not a UID-data error or a
  package-crash dialog
- prove the exact mismatch: diagnostic kernel `#5` has
  `CONFIG_DEBUG_FS=y`, while framework compatibility matrix level 5 requires
  `CONFIG_DEBUG_FS=n`
- bump `pftm10-control` to `1.5.5` / versionCode `16` with a hash-guarded,
  reversible bind mount of a level-5 compatibility matrix that removes only
  that one kernel-config requirement
- record the original matrix hash, patched matrix hash, mount result, and
  return code in the boot log; add the
  `/data/adb/pftm10-control/vintf-debugfs-allow.disabled` rollback sentinel
- verify after a normal reboot:
  `isBuildConsistent=true`, no `Vendor interface is incompatible` log, no
  system-error window, and all runtime/device checks passing
- prefer `charge_counter` over stale `charge_now` when reconciling OPPO UI
  battery state during host-side device acceptance

## v1.6.2 - 2026-11-18

- correct the MT6360 `mt6360_set_aicr` fallback offset to `0x969618` for the
  current self-built `#5` kernel
- replace the unsafe cross-module charger invocation with a direct call that
  preserves the verified `charger-3a` control name and keeps the hooked value
  across later OPPO policy writes
- retain a bounded `charger-500-direct` restore path and return the input limit
  to `500mA` immediately when 3A mode is disabled
- bump `pftm10-kprobe` to `1.4.4` and `pftm10-control` to `1.5.3`
- verify post-reboot `post-fs-data` loading, watchdog recovery, sustained
  `3000mA` input ceiling, and measurable `battery_rm` growth

## v1.6.1 - 2026-11-16

- serialize every `pftm10-kpmctl` control0 call with a root-owned cross-process
  file lock
- document the `sym:l_ctx` plus `u32:mobicore_ready` concurrent-query panic in
  the private campaign record and keep concurrent KPM queries prohibited
- bump `pftm10-control` to 1.5.2 without changing the verified
  `pftm10-kprobe` 1.3.0 payload

## v1.6.0 - 2026-11-16

- bump `pftm10-control` to 1.5.1 with a durable boot and pstore crash audit
- record boot reason, full boot-reason history, kernel banner, boot ID, and
  pstore hashes without copying the complete ramoops buffer
- classify changed pstore data as `panic`, warning-only, or `other`, and rerun
  the audit after boot completion without duplicating an unchanged snapshot
- add `pftm10ctl crashlog` and bounded 512 KiB audit-log rotation
- extend host-side device acceptance to require the post-boot crash audit
- retain the `#4` SCP suspend fix evidence and `boot_b` rollback boundary

## v1.5.1 - 2026-11-15

- add public source for the rebuilt vendor module loader
- whitelist the verified Nov 14 and Nov 15 self-built kernel timestamps
- restore executable module entry points from `customize.sh` after APatch's
  default `0644` extraction
- use `resetprop -n` for post-fs-data status properties
- add a Windows builder that accepts the ten matching vendor modules, verifies
  their hashes, and emits a deterministic APatch ZIP layout

## v1.5.0 - 2026-11-15

- document the self-built MT6833 APatch kernel boot path with the rebuilt
  vendor module set
- add the `wlan_drv_gen4m` EMI startup-order patch so Wi-Fi refreshes the WMT
  exported EMI base before firmware download
- record Wi-Fi, Bluetooth, camera, APatch health, and two-cold-boot acceptance
  without publishing device identifiers or private logs

## v1.4.0 - 2026-11-14

- bump `pftm10-kprobe` to 1.3.0 with explicit charger suspend recovery,
  PD contract inspection, and gated `5V/3A` input controls
- correct the MT6360 AICR units to microamps and verify the controller accepts
  an AICR request of 3000mA
- bump `pftm10-control` to 1.4.0 with a PD-gated 3A watchdog that restores the
  input limit after OPPO policy rewrites it
- add `pftm10ctl charge-3a status|enable|disable|apply` and a 500mA restore path
- extend host device acceptance to verify charger recovery and the 3A control
  surface

## v1.3.2 - 2026-11-14

- bump `pftm10-control` to 1.2.1
- replay camera location AppOps denial after every boot while leaving the
  runtime permission granted for OPPO camera compatibility
- persist OShare background-location denial with `user-fixed` and `user-set`
- extend host device acceptance to verify both privacy policies

## v1.3.1 - 2026-11-14

- bump `pftm10-control` to 1.2.0 with a reversible bind-mount mask for the
  CR.39 battery restriction plugin
- prevent the `google_check` crash loop on builds without GMS while keeping
  the main battery service and charging telemetry active
- add `pftm10ctl battery-restrict status|mask|restore` and boot-time status
  logging

## v1.3.0 - 2026-11-14

- add a root-only static adapter for direct manager-authorized
  `pftm10-kprobe` control0 calls
- add `pftm10ctl kprobe-ctl` and include control0 reachability in health and
  device acceptance

## v1.2.1 - 2026-11-14

- add manager-authorized KPM load verification and a read-only capability
  matrix to `pftm10-control` 1.0.4
- add a sanitized host-side device acceptance check

## v1.2.0 - 2026-11-14

- document runtime and physical cleanup for third-party preload partitions
- add post-boot acceptance checks for non-verity dynamic partition writes
- bump `pftm10-control` to 1.0.2 with health, preload, root-context, and KPM
  diagnostics
- record `health=ok` or `health=failed` automatically on every boot
- bump `pftm10-kprobe` to 1.1.0 with bounded read-only kernel introspection:
  `banner`, `cmdline`, `u32`, `u64`, `bytes`, `str`, and whitelisted `pstr`
- add boot-time KPM banner/system-state diagnostics and module-side KPM file
  integrity/load checks

## v1.0.1 - 2026-11-14

- initial public toolkit structure
- MT6833 unlock, APatch, recovery, de-bloat, and rollback documentation
- `pftm10-control` APatch system module
- `pftm10-kprobe` read-only KernelPatch KPM
- privacy validation and reproducible build helpers
- no vendor firmware, device identifiers, or private records
