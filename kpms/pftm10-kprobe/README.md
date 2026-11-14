# pftm10-kprobe

A bounded KernelPatch module for PFTM10. It exposes:

- boot-time `linux_banner` and `system_state` diagnostics.
- `info`: caller UID, KernelPatch version, `init_task` address, and module init
  arguments.
- `banner`: bounded read of the kernel `linux_banner` string.
- `cmdline`: bounded read of `saved_command_line`.
- `sym:<name>`: resolve a kernel symbol through `kallsyms_lookup_name`.
- `u32:<name>` / `u64:<name>`: read the first scalar at a resolved symbol.
- `bytes:<name>`: hex-dump the first 16 bytes at a resolved symbol.
- `str:<name>`: bounded printable string read at a resolved symbol address.
- `pstr:saved_command_line`: read the only whitelisted pointer-symbol string.
- `charger-unsuspend`: call only the exact
  `oplus_chg_unsuspend_charger()` kernel symbol and report its `stop_chg`
  state.
- `pd-contract`: query `type_c_port0` through
  `tcpm_inquire_pd_contract()` and report the live voltage/current contract.
- `charger-3a`: apply a 3000mA MT6360 AICR request only when the live PD
  contract is at least `5V/3000mA`.
- `charger-restore`: restore AICL and request the 500mA input tier.
- `help`: print the command list.

All reads are bounded. `charger-unsuspend`, `charger-3a`, and
`charger-restore` are the only state-changing commands. `charger-3a` is gated
by a live PD contract check. The module does not accept arbitrary addresses or
values, write arbitrary kernel memory, hook functions, change credentials, or
modify SELinux policy.
