# MT6833 Bootloader Unlock

This workflow is specific to an owned PFTM10 / OP526D on the validated C.39
chain. Do not substitute a preloader, LK image, or DA loader from another build.

## Prerequisites

- complete stock firmware and extracted partition images
- verified preloader, LK, vbmeta, boot, and dynamic-partition backups
- a fresh USB Preloader window for every DA session
- a known-good USB data cable and direct USB path

No vendor binary is distributed in this repository.

## Unlock Sequence

1. Serialize and hash the stock preloader, LK, vbmeta, boot, and `super`
   metadata.
2. Build and verify the plain-unlock preloader for the exact firmware.
3. Write `boot1` and `boot2` only after a complete readback and rollback image
   exist.
4. Cold boot into LK fastboot and query unlock ability with the raw USB client
   that works with the target interface.
5. Issue the unlock flow, then cold boot and verify:

```text
flash.locked        0
verifiedbootstate   orange
```

6. Keep the stock preloader and stock recovery write scripts.

## Failure Rules

- Stop on a stale Preloader instance. Start a new phone-side window.
- Do not send DA commands to a node that repeatedly reports handshake failure.
- Do not treat a successful host write as success until the phone cold boots.
- Do not use a network transport for firmware writes.
