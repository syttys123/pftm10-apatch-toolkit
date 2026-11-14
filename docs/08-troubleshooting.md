# Troubleshooting

## Preloader Handshake Exhausted

Stop issuing DA commands. Create a genuinely fresh phone-side Preloader window
with the required power/volume sequence and replug. Do not reuse a stale USB
node.

## DA Disconnect Around 396 MiB

Split the write into bounded 64 MiB chunks and restore the complete stock image
first if a prior write was interrupted.

## `apd module list` Is Empty

This command lists system modules. It is not the KPM list. Check the APatch
Kernel Modules page and `dmesg`.

## KPM Load Returns `-1`

Rebuild with a smaller import set. Confirm all imports are KernelPatch exports,
remove debug data, then run a temporary load before reboot.

## Camera or Telephony Regression After De-bloat

Restore the known-good stock image for the affected partition. Do not continue
editing the same mounted image in place.
