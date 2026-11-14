# Changelog

## Unreleased

- document runtime and physical cleanup for third-party preload partitions
- add post-boot acceptance checks for non-verity dynamic partition writes
- bump `pftm10-control` to 1.0.2 with health, preload, root-context, and KPM
  diagnostics
- record `health=ok` or `health=failed` automatically on every boot
- bump `pftm10-kprobe` to 1.1.0 with bounded read-only kernel introspection:
  `banner`, `cmdline`, `u32`, `u64`, `bytes`, `str`, and whitelisted `pstr`

## v1.0.1 - 2026-11-14

- initial public toolkit structure
- MT6833 unlock, APatch, recovery, de-bloat, and rollback documentation
- `pftm10-control` APatch system module
- `pftm10-kprobe` read-only KernelPatch KPM
- privacy validation and reproducible build helpers
- no vendor firmware, device identifiers, or private records
