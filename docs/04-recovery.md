# Recovery and Segmented Writes

## DA Session Discipline

The safe pattern is:

1. start the DA client first
2. request a fresh EDL/Preloader window from the phone
3. confirm the exact USB VID/PID and one responsive instance
4. execute one bounded action
5. read back and hash
6. send reset
7. perform the required phone-side cold boot

Do not start the DA client only after observing a short-lived handshake node.

## Segmented Dynamic Partitions

For difficult large dynamic partitions, generate independent chunks no larger
than 64 MiB. Every chunk must record:

```text
image offset
target extent offset
chunk length
chunk SHA256
```

The script must contain an explicit reset only after all chunks succeed.

## Stock Restore

Keep a stock restore script that was already proven to cold boot. A recovery
script becomes trusted only after:

- every chunk completes
- the writer reports success
- the phone cold boots
- package manager and core device services work

Never overwrite a known-good stock restore with an unverified experimental
image.
