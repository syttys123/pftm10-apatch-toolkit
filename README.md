# PFTM10 APatch Toolkit

Maintainer: [syttys123](https://github.com/syttys123)

This repository documents and automates a from-scratch, local-only workflow for
the OPPO PFTM10 / OP526D on the MediaTek MT6833 platform:

- bootloader unlock and recovery on the MT6833 boot chain
- APatch / KernelPatch root with reproducible verification checkpoints
- APatch system modules with boot-stage hooks
- an APatch vendor-module loader for self-built kernels
- KernelPatch KPM examples and a read-only kernel probe
- physical image de-bloat and rollback planning
- public-tree privacy checks

The repository intentionally does not contain vendor firmware, boot images,
preloader images, DA binaries, user data, device serial numbers, ADB state
dumps, private logs, or local assistant workspace records. Supply your own matching
firmware and backups.

## Public Attribution

All project metadata, commits, module metadata, and KPM metadata use the
GitHub identity `syttys123`.

## Repository Layout

```text
docs/                         Reproducible workflow and recovery notes
kernel/                       Pinned MT6833 kernel cloud-build inputs
kernel/patches/               Source patches required by rebuilt vendor modules
kpms/pftm10-kprobe/           Bounded KPM with charger recovery and PD-gated 3A
modules/pftm10-control/       Minimal APatch system module
module-loader/                Rebuilt vendor-module loader source and builder
tools/                        Build, install, and privacy-validation helpers
```

## Start Here

1. Read [docs/00-principles.md](docs/00-principles.md).
2. Record a complete stock rollback before changing any critical partition.
3. Follow [docs/01-recon.md](docs/01-recon.md) and stop if the live state does
   not match the firmware package.
4. Use the recovery and verification checkpoints in every later document.

## Build the Examples

Set the KernelPatch source tree outside this repository:

```powershell
$env:PFTM10_KERNELPATCH_ROOT = "E:\src\KernelPatch"
.\kpms\pftm10-kprobe\build.ps1
.\modules\build-pftm10-control.ps1 -Zig C:\tools\zig.exe
.\module-loader\build-module.ps1 -ModulesPath C:\path\to\rebuilt-modules
```

The documented Windows toolchain uses Zig `0.14.1` with the
`aarch64-freestanding` target. Do not publish build outputs containing debug
paths. The vendor `.ko` files are not part of this repository; supply a
matching locally rebuilt set to the module loader.

## Build the Matching Kernel

The `kernel-build` GitHub Actions workflow compiles the public OPPO MT6833
kernel with the live device configuration and the matching Android Clang
r383902 toolchain. It publishes `Image.gz`, `System.map`, kernel config,
`mt6833.dtb`, and `oplus6833_21101.dtb` as release assets.

The workflow never flashes a device or uploads a boot/vendor image. See
[kernel/README.md](kernel/README.md) for pinned inputs, module-signature
compatibility, outputs, and the pre-flash acceptance gate.

Build and verify all release assets:

```powershell
.\tools\build-all.ps1 `
  -Zig C:\tools\zig.exe `
  -KernelPatchRoot C:\src\KernelPatch
```

The `public-tree` GitHub Actions workflow runs the privacy guard on every push
and pull request.

## Releases

Sanitized module and KPM builds are published under this repository's Releases
page. Firmware and partition images are never release assets.

## Scope

This is an experimental device-modification toolkit. It is not a generic OPPO,
MediaTek, or anti-theft bypass guide. Use only on hardware you own and can
recover over USB.

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).
