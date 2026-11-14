# Principles

## Evidence Before Mutation

Before writing a partition, record:

- current firmware build and active slot
- USB identity and transport state
- source image path and SHA256
- target partition and byte length
- exact write script or command
- readback SHA256
- immediate rollback image and command

Do not call a write successful until the device boots and the expected
properties and services are verified.

## Keep Stock Recovery Local

Keep all vendor firmware, extracted partition images, DA loaders, and
device-specific backups outside the repository. Use placeholders in public
commands.

## Prefer Reversible Layers

Use APatch system modules and KPMs before rewriting Android partitions. Runtime
mapping experiments are not rollback evidence unless a cold boot proves the
result.

## Minimum Privilege

Root exists. Do not flash preloader, LK, vbmeta, or boot again just to repeat an
already validated step. Add kernel imports, hooks, and credential changes only
with a focused load test and unload path.

## Privacy

Public artifacts contain reproducible logic, not machine state. Build with
debug information disabled and validate the tree before every push.
