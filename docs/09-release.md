# Release Checklist

## Build

```powershell
.\tools\build-all.ps1 `
  -Zig C:\tools\zig.exe `
  -KernelPatchRoot C:\src\KernelPatch
```

The build output contains:

- `pftm10-control-v<version>.zip`
- `pftm10-kprobe.kpm`
- `artifacts.json`

## Verify

```powershell
.\tools\validate-public-tree.ps1
.\tools\verify-kpm.ps1 -KpmPath .\artifacts\pftm10-kprobe.kpm -RzBin rz-bin.exe
git diff --check
git status --short
```

## Publish

Create a tag only after the commit is pushed:

```text
v1.3.0
```

Attach only sanitized build outputs. Do not attach firmware, partition images,
private logs, device state, or debug builds.
