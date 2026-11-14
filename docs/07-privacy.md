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

## Boot Privacy Policy

The `pftm10-control` service stage reapplies privacy state that Android or
vendor defaults can reset during boot:

- camera fine and coarse location AppOps are `ignore`
- OShare background location is revoked with `user-fixed` and `user-set`

Do not revoke the camera runtime location permission directly: the OPPO camera
app treats that state as a blocking prerequisite dialog and does not reach the
camera preview until its own prompt is dismissed. AppOps denial keeps the app
compatible while preventing access to location data.

The boot replay is controlled by
`/data/adb/pftm10-control/privacy.disabled`. Presence of that file disables the
policy and restores the default location grants on the next boot; removal
enables the policy again.

## Pre-Push Check

```powershell
.\tools\validate-public-tree.ps1
git diff --cached
```

The validation script is a guardrail, not a substitute for reviewing changes.
