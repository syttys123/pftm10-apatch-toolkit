# pftm10-kprobe

A read-only KernelPatch module for PFTM10. It exposes:

- `info`: caller UID, KernelPatch version, `init_task` address, and module init
  arguments.
- `banner`: bounded read of the kernel `linux_banner` string.
- `cmdline`: bounded read of `saved_command_line`.
- `sym:<name>`: resolve a kernel symbol through `kallsyms_lookup_name`.
- `u32:<name>` / `u64:<name>`: read the first scalar at a resolved symbol.
- `bytes:<name>`: hex-dump the first 16 bytes at a resolved symbol.
- `str:<name>`: bounded printable string read at a resolved symbol address.
- `pstr:saved_command_line`: read the only whitelisted pointer-symbol string.
- `help`: print the command list.

All reads are bounded. The module does not write kernel memory, hook functions,
change credentials, or modify SELinux policy.
