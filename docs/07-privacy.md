# Public Tree Privacy

The public repository contains code and procedures only.

## Prohibited Content

- device serial numbers and ADB selectors
- account names tied to the device or vendor services
- local Windows user paths or assistant workspace paths
- private boot, kernel, package, or network logs
- stock firmware, extracted vendor images, or proprietary loaders
- APatch superkeys, GitHub tokens, signing material, or passwords

## KPM Build Requirements

Use `-g0` and inspect the final ELF:

- no `.debug_*` sections
- no local path strings
- no device identifier strings
- metadata author `syttys123`

## Pre-Push Check

```powershell
.\tools\validate-public-tree.ps1
git diff --cached
```

The validation script is a guardrail, not a substitute for reviewing changes.
