# Security Policy

This repository changes boot-chain, root, kernel, and system-image state. Treat
every workflow as destructive until a verified rollback exists.

## Never Commit

- device serial numbers, ADB identifiers, or account identifiers
- APatch superkeys, GitHub tokens, signing keys, or passwords
- stock firmware archives or extracted vendor images
- private logs, screenshots, package inventories, or workspace paths
- KPM builds containing local debug paths

## Reporting

Open a minimal issue containing only the failing command category and the
observed error code. Do not include raw device identifiers or full logs.
