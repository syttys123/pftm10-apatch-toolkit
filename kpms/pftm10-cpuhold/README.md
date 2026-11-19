# pftm10-cpuhold

This is a separate, reversible KernelPatch module for the PFTM10 big-core
frequency policy.

It hooks `cpufreq_freq_transition_end` and keeps the requested `policy6` maximum
from being restored by OPPO's performance policy while the feature is enabled.
The module defaults to disabled. Its control0 commands are:

```text
status
cpu-max:<khz>
cpu-max-off
policy-cpu:<cpu>
help
```

The integration in `pftm10-control` owns the enable/disable state, sentinel,
watchdog, and boot replay. Keep the persistent `pftm10-kprobe` module separate.
Do not stamp the CPUHOLD KPM into the boot image or replace the kernel probe.
