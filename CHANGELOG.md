# Changelog

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
