# PFTM10 kernel build

This directory contains the reproducible cloud-build inputs for the OPPO
PFTM10 / OP526D MT6833 kernel. The workflow builds from the public
`oppo-source/android_kernel_oppo_mt6833` repository and pins the source commit.

## Inputs

```text
source branch   oppo/mt6833_t_13.0
source commit   1d9fa359f26e7b7c5e5a2ef7041f7569ec68aaf0
module branch   oppo/mt6833_t_13.0
module commit   a9ed2381b546fe472a4bdb5c8a0d874abc1ad968
compiler        Android Clang r383902 / 11.0.1
base config     kernel/config/pftm10-c39.config
release         must resolve to 4.19.191+
```

The kernel repository contains relative symlinks into OPPO's companion
`android_kernel_modules_oppo_mt6833` tree. The workflow fetches that pinned
module source and exposes its `vendor/` subtree beside the kernel checkout,
matching the original Android build layout.

The build uses the live device `kernel.config` as its base instead of trusting
the public defconfig alone. Two signature settings are changed for compatibility
testing:

```text
CONFIG_MODULE_SIG_FORCE=n
CONFIG_MODULE_SIG_ALL=n
```

The original vendor kernel has `CONFIG_MODULE_SIG_FORCE=y`. Rebuilding with
that setting would reject the installed vendor modules because their private
signing key is not published. The custom kernel keeps `CONFIG_MODVERSIONS=y`
and the exact `4.19.191+` release string so module compatibility can be measured
before any boot-image work.

## Outputs

Each successful workflow run publishes a `kernel-build-<run_id>` release with:

```text
Image.gz
System.map
kernel.config
kernelrelease.txt
dtb/mt6833.dtb
dtb/oplus6833_21101.dtb
BUILD-INFO.txt
SHA256SUMS
```

No boot image, vendor image, device serial, ADB state, or local log is uploaded.
Building does not flash or mutate the phone.

## Safety gate

The next step after a successful build is read-only comparison against the live
kernel and vendor modules. Do not write `boot_a` until all of the following pass:

```text
kernelrelease equals 4.19.191+
vendor module vermagic remains 4.19.191+ SMP preempt mod_unload modversions aarch64
DTB selection resolves to mediatek/oplus6833_21101
APatch boot-image patch and rollback image are hash-verified
```
