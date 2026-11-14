# Changelog

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
