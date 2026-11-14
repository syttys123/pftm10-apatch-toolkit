# pftm10-kprobe

A read-only KernelPatch module for PFTM10. It exposes:

- `info`: caller UID, KernelPatch version, `init_task` address, and module init
  arguments.
- `sym:<name>`: resolve a kernel symbol through `kallsyms_lookup_name`.
- `help`: print the command list.

The module does not write kernel memory, hook functions, change credentials, or
modify SELinux policy.
