/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <compiler.h>
#include <kpmodule.h>
#include <common.h>
#include <hook.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <kputils.h>

KPM_NAME("pftm10-cpuhold");
KPM_VERSION("0.2.2");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("syttys123");
KPM_DESCRIPTION("PFTM10 reversible cpufreq max-policy hook");

#define CPUFREQ_POLICY_CPU_OFFSET 0x1cU
#define CPUFREQ_POLICY_MIN_OFFSET 0x34U
#define CPUFREQ_POLICY_MAX_OFFSET 0x38U
#define MT_CPU_DVFS_POLICY_OFFSET 0x18U
#define MT_CPU_DVFS_IDX_PPM_BASE_OFFSET 0x48U
#define MT_CPU_DVFS_IDX_PPM_LIMIT_OFFSET 0x4cU
#define MT_CPU_DVFS_L_ID 1U

static unsigned long g_cpufreq_set_policy;
static unsigned long g_cpufreq_update_policy;
static unsigned long g_mt_cpufreq_set;
static unsigned long g_mt_cpufreq_wrapper;
static unsigned long (*g_id_to_cpu_dvfs)(unsigned int id);
static unsigned long g_ppm_notifier;
static unsigned long g_update_freq_limit_info;
static unsigned long g_update_userlimit_cpu_freq;
static unsigned long g_mt_ppm_userlimit_cpu_freq;
static unsigned long g_mt_ppm_main;
static unsigned long g_thermolmt_cpu;
static unsigned long g_ppm_cpu_thermal;
static unsigned long g_cpufreq_transition_end;
static volatile unsigned int g_target_khz;
static volatile unsigned int g_policy_cpu = 6;
static volatile unsigned long g_rewrites;
static volatile unsigned long g_calls;
static volatile unsigned long g_target_hits;
static volatile unsigned long g_after_calls;
static volatile unsigned long g_after_rewrites;
static volatile unsigned int g_seen_cpu_mask;
static volatile unsigned int g_logged_cpu_mask;
static volatile unsigned int g_last_cpu;
static volatile unsigned int g_last_old_max;
static volatile unsigned int g_last_new_max;
static volatile unsigned long g_mtk_calls;
static volatile unsigned long g_mtk_rewrites;
static volatile unsigned long g_wrapper_calls;
static volatile unsigned long g_wrapper_rewrites;
static volatile unsigned long g_ppm_calls;
static volatile unsigned long g_ppm_rewrites;
static volatile unsigned long g_ppm_no_policy;
static volatile unsigned long g_update_policy_calls;
static volatile unsigned long g_ppm_notifier_calls;
static volatile unsigned long g_freq_limit_info_calls;
static volatile unsigned long g_policy6_addr;
static volatile unsigned long g_update_userlimit_calls;
static volatile unsigned long g_mt_userlimit_calls;
static volatile unsigned long g_ppm_main_calls;
static volatile unsigned long g_thermolmt_calls;
static volatile unsigned long g_ppm_thermal_calls;
static volatile unsigned long g_transition_end_calls;
static volatile unsigned long g_transition_end_rewrites;
static int g_hook_err = -1;
static int g_mtk_hook_err = -1;
static int g_wrapper_hook_err = -1;
static int g_update_policy_hook_err = -1;
static int g_ppm_notifier_hook_err = -1;
static int g_freq_limit_info_hook_err = -1;
static int g_update_userlimit_hook_err = -1;
static int g_mt_userlimit_hook_err = -1;
static int g_ppm_main_hook_err = -1;
static int g_thermolmt_hook_err = -1;
static int g_ppm_thermal_hook_err = -1;
static int g_transition_end_hook_err = -1;

static long copy_reply(char *__user out_msg, int outlen, const char *reply)
{
    int len;

    if (!out_msg || outlen <= 0)
        return -1;

    len = strlen(reply) + 1;
    if (len > outlen)
        len = outlen;
    compat_copy_to_user(out_msg, reply, len);
    return 0;
}

static unsigned int load_u32(unsigned long address)
{
    return *(volatile unsigned int *)address;
}

static void store_u32(unsigned long address, unsigned int value)
{
    *(volatile unsigned int *)address = value;
}

static void before_cpufreq_set_policy(hook_fargs2_t *args, void *udata)
{
    unsigned long policy = (unsigned long)args->arg0;
    unsigned long new_policy = (unsigned long)args->arg1;
    unsigned int target = g_target_khz;
    unsigned int cpu;
    unsigned int old_max;

    (void)udata;
    if (!policy || !new_policy)
        return;

    cpu = load_u32(policy + CPUFREQ_POLICY_CPU_OFFSET);
    old_max = load_u32(new_policy + CPUFREQ_POLICY_MAX_OFFSET);
    g_calls++;
    g_last_cpu = cpu;
    g_last_old_max = old_max;
    g_last_new_max = old_max;

    if (cpu < 32) {
        unsigned int bit = 1U << cpu;

        g_seen_cpu_mask |= bit;
        if (!(g_logged_cpu_mask & bit)) {
            g_logged_cpu_mask |= bit;
            pr_info("pftm10-cpuhold seen cpu=%u old_max=%u target=%u\n",
                    cpu, old_max, target);
        }
    }

    if (!target)
        return;
    if (cpu != g_policy_cpu)
        return;

    g_target_hits++;
    if (old_max != target) {
        store_u32(new_policy + CPUFREQ_POLICY_MAX_OFFSET, target);
        g_last_new_max = target;
        g_rewrites++;
    }
}

static void after_cpufreq_set_policy(hook_fargs2_t *args, void *udata)
{
    unsigned long policy = (unsigned long)args->arg0;
    unsigned int target = g_target_khz;
    unsigned int cpu;
    unsigned int current_max;

    (void)udata;
    if (!target || !policy)
        return;

    cpu = load_u32(policy + CPUFREQ_POLICY_CPU_OFFSET);
    if (cpu != g_policy_cpu)
        return;

    current_max = load_u32(policy + CPUFREQ_POLICY_MAX_OFFSET);
    if (cpu == g_policy_cpu)
        g_policy6_addr = policy;
    g_after_calls++;
    if (g_after_calls <= 20) {
        pr_info("pftm10-cpuhold after set_policy cpu=%u max=%u target=%u\n",
                cpu, current_max, target);
    }
    if (current_max != target) {
        store_u32(policy + CPUFREQ_POLICY_MAX_OFFSET, target);
        g_after_rewrites++;
        if (g_after_rewrites == 1)
            pr_info("pftm10-cpuhold after rewrite max %u->%u\n",
                    current_max, target);
    }
}

static void before_cpufreq_update_policy(hook_fargs2_t *args, void *udata)
{
    (void)udata;
    g_update_policy_calls++;
    if (g_update_policy_calls <= 20)
        pr_info("pftm10-cpuhold update_policy cpu=%u target=%u\n",
                (unsigned int)args->arg0, g_target_khz);
}

static void before_ppm_notifier(hook_fargs4_t *args, void *udata)
{
    unsigned long policy = (unsigned long)args->arg2;
    unsigned int event = (unsigned int)args->arg1;
    unsigned int cpu;
    unsigned int max;

    (void)udata;
    g_ppm_notifier_calls++;
    if (!policy)
        return;
    cpu = load_u32(policy + CPUFREQ_POLICY_CPU_OFFSET);
    max = load_u32(policy + CPUFREQ_POLICY_MAX_OFFSET);
    if (g_ppm_notifier_calls <= 20 || cpu == g_policy_cpu)
        pr_info("pftm10-cpuhold ppm_notifier before event=%u cpu=%u "
                "max=%u target=%u calls=%lu\n",
                event, cpu, max, g_target_khz, g_ppm_notifier_calls);
}

static void after_ppm_notifier(hook_fargs4_t *args, void *udata)
{
    unsigned long policy = (unsigned long)args->arg2;
    unsigned int event = (unsigned int)args->arg1;
    unsigned int cpu;
    unsigned int max;

    (void)udata;
    if (!policy)
        return;
    cpu = load_u32(policy + CPUFREQ_POLICY_CPU_OFFSET);
    max = load_u32(policy + CPUFREQ_POLICY_MAX_OFFSET);
    if (cpu == g_policy_cpu)
        pr_info("pftm10-cpuhold ppm_notifier after event=%u cpu=%u "
                "max=%u target=%u\n", event, cpu, max, g_target_khz);
}

static void after_update_freq_limit_info(hook_fargs2_t *args, void *udata)
{
    unsigned long policy = (unsigned long)args->arg0;
    unsigned int cpu;
    unsigned int max;

    (void)udata;
    g_freq_limit_info_calls++;
    if (!policy)
        return;
    cpu = load_u32(policy + CPUFREQ_POLICY_CPU_OFFSET);
    max = load_u32(policy + CPUFREQ_POLICY_MAX_OFFSET);
    if (g_freq_limit_info_calls <= 20 || cpu == g_policy_cpu)
        pr_info("pftm10-cpuhold freq_limit_info cpu=%u max=%u target=%u "
                "calls=%lu\n",
                cpu, max, g_target_khz, g_freq_limit_info_calls);
}

static void log_ppm_limit_pair(const char *name, unsigned long p)
{
    if (p < 0xffff000000000000UL)
        return;
    pr_info("pftm10-cpuhold %s cluster0 min=%d max=%d "
            "cluster1 min=%d max=%d target=%u\n",
            name, *(volatile int *)p, *(volatile int *)(p + 4),
            *(volatile int *)(p + 8), *(volatile int *)(p + 12),
            g_target_khz);
}

static void before_update_userlimit_cpu_freq(hook_fargs4_t *args, void *udata)
{
    unsigned long limits = (unsigned long)args->arg2;

    (void)udata;
    g_update_userlimit_calls++;
    if (g_update_userlimit_calls <= 20) {
        pr_info("pftm10-cpuhold update_userlimit kicker=%d clusters=%u "
                "calls=%lu\n",
                (int)args->arg0, (unsigned int)args->arg1,
                g_update_userlimit_calls);
        log_ppm_limit_pair("update_userlimit", limits);
    }
}

static void before_mt_ppm_userlimit_cpu_freq(hook_fargs4_t *args, void *udata)
{
    unsigned long limits = (unsigned long)args->arg1;

    (void)udata;
    g_mt_userlimit_calls++;
    if (g_mt_userlimit_calls <= 20) {
        pr_info("pftm10-cpuhold mt_userlimit clusters=%u calls=%lu\n",
                (unsigned int)args->arg0, g_mt_userlimit_calls);
        log_ppm_limit_pair("mt_userlimit", limits);
    }
}

static void before_mt_ppm_main(hook_fargs4_t *args, void *udata)
{
    (void)args;
    (void)udata;
    g_ppm_main_calls++;
    if (g_ppm_main_calls <= 20)
        pr_info("pftm10-cpuhold mt_ppm_main calls=%lu target=%u\n",
                g_ppm_main_calls, g_target_khz);
}

static void before_thermolmt_cpu(hook_fargs4_t *args, void *udata)
{
    (void)udata;
    g_thermolmt_calls++;
    if (g_thermolmt_calls <= 20)
        pr_info("pftm10-cpuhold thermolmt_cpu limit=%u target=%u calls=%lu\n",
                (unsigned int)args->arg1, g_target_khz,
                g_thermolmt_calls);
}

static void before_ppm_cpu_thermal(hook_fargs4_t *args, void *udata)
{
    (void)udata;
    g_ppm_thermal_calls++;
    if (g_ppm_thermal_calls <= 20)
        pr_info("pftm10-cpuhold ppm_cpu_thermal limit=%u target=%u "
                "calls=%lu\n",
                (unsigned int)args->arg0, g_target_khz,
                g_ppm_thermal_calls);
}

static void after_cpufreq_transition_end(hook_fargs4_t *args, void *udata)
{
    unsigned long policy = (unsigned long)args->arg0;
    unsigned int target = g_target_khz;
    unsigned int cpu;
    unsigned int current_max;

    (void)udata;
    if (!target || !policy)
        return;
    cpu = load_u32(policy + CPUFREQ_POLICY_CPU_OFFSET);
    if (cpu != g_policy_cpu)
        return;

    g_transition_end_calls++;
    current_max = load_u32(policy + CPUFREQ_POLICY_MAX_OFFSET);
    if (current_max != target) {
        store_u32(policy + CPUFREQ_POLICY_MAX_OFFSET, target);
        g_transition_end_rewrites++;
        if (g_transition_end_rewrites <= 12)
            pr_info("pftm10-cpuhold transition_end rewrite cpu=%u "
                    "max=%u->%u calls=%lu\n",
                    cpu, current_max, target, g_transition_end_calls);
    }
}

static void after_mt_cpufreq_set(hook_fargs4_t *args, void *udata)
{
    unsigned long policy = (unsigned long)args->arg0;
    unsigned int target = g_target_khz;
    unsigned int cpu;
    unsigned int current_max;

    (void)udata;
    if (!target || !policy)
        return;

    cpu = load_u32(policy + CPUFREQ_POLICY_CPU_OFFSET);
    if (cpu != g_policy_cpu)
        return;

    current_max = load_u32(policy + CPUFREQ_POLICY_MAX_OFFSET);
    g_mtk_calls++;
    if (current_max != target) {
        store_u32(policy + CPUFREQ_POLICY_MAX_OFFSET, target);
        g_mtk_rewrites++;
    }
}

static void after_mt_cpufreq_wrapper(hook_fargs4_t *args, void *udata)
{
    unsigned long p = (unsigned long)args->arg0;
    unsigned int action = (unsigned int)args->arg2;
    unsigned long policy;
    unsigned int target = g_target_khz;
    unsigned int cpu;
    unsigned int current_max;

    (void)udata;
    if (!target)
        return;

    if (action == 1) {
        g_ppm_calls++;
        if (!p && g_id_to_cpu_dvfs)
            p = (unsigned long)g_id_to_cpu_dvfs(MT_CPU_DVFS_L_ID);
        if (p < 0xffff000000000000UL) {
            g_ppm_no_policy++;
            return;
        }
    } else if (!p) {
        return;
    }

    policy = *(volatile unsigned long *)(p + MT_CPU_DVFS_POLICY_OFFSET);
    if (policy < 0xffff000000000000UL)
        return;

    cpu = load_u32(policy + CPUFREQ_POLICY_CPU_OFFSET);
    if (cpu != g_policy_cpu)
        return;

    current_max = load_u32(policy + CPUFREQ_POLICY_MAX_OFFSET);
    g_wrapper_calls++;
    if (action == 1 && g_ppm_rewrites == 0)
        pr_info("pftm10-cpuhold ppm hook p=0x%lx policy=0x%lx cpu=%u "
                "max=%u target=%u idx_base=%d idx_limit=%d\n",
                p, policy, cpu, current_max, target,
                *(volatile int *)(p + MT_CPU_DVFS_IDX_PPM_BASE_OFFSET),
                *(volatile int *)(p + MT_CPU_DVFS_IDX_PPM_LIMIT_OFFSET));
    if (current_max != target) {
        store_u32(policy + CPUFREQ_POLICY_MAX_OFFSET, target);
        g_wrapper_rewrites++;
        if (action == 1)
            g_ppm_rewrites++;
    }
}

static int parse_u32(const char **text, unsigned int *out)
{
    const char *p = *text;
    unsigned long value = 0;
    unsigned int base = 10;
    int digits = 0;

    while (*p == ' ')
        p++;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        base = 16;
        p += 2;
    }

    while (*p) {
        unsigned int digit;

        if (*p >= '0' && *p <= '9')
            digit = (unsigned int)(*p - '0');
        else if (base == 16 && *p >= 'a' && *p <= 'f')
            digit = (unsigned int)(*p - 'a' + 10);
        else if (base == 16 && *p >= 'A' && *p <= 'F')
            digit = (unsigned int)(*p - 'A' + 10);
        else
            break;

        if (digit >= base)
            break;
        value = value * base + digit;
        if (value > 0xffffffffUL)
            return -1;
        digits++;
        p++;
    }

    while (*p == ' ')
        p++;
    if (!digits || *p)
        return -1;

    *out = (unsigned int)value;
    *text = p;
    return 0;
}

static int parse_u32_token(const char *cmd, const char *prefix,
                           unsigned int *out)
{
    size_t prefix_len = strlen(prefix);
    const char *value;

    if (strncmp(cmd, prefix, prefix_len))
        return -1;

    value = cmd + prefix_len;
    return parse_u32(&value, out);
}

static void update_policy_cpu(void)
{
    if (g_cpufreq_update_policy)
        ((void (*)(unsigned int))g_cpufreq_update_policy)(g_policy_cpu);
}

static long cpuhold_control0(const char *args, char *__user out_msg, int outlen)
{
    char reply[1024];
    const char *cmd = (args && args[0]) ? args : "status";

    pr_info("pftm10-cpuhold control0 command: %s\n", cmd);

    if (!strcmp(cmd, "status") || !strcmp(cmd, "hook-status")) {
        snprintf(reply, sizeof(reply),
                 "enabled=%u target_khz=%u policy_cpu=%u "
                 "hook=%d calls=%lu hits=%lu rewrites=%lu "
                 "after_calls=%lu after_rewrites=%lu "
                 "seen_cpu_mask=0x%x last_cpu=%u last_old=%u last_new=%u "
                 "mtk_hook=%d mtk_calls=%lu mtk_rewrites=%lu "
                 "wrapper_hook=%d wrapper_calls=%lu wrapper_rewrites=%lu "
                 "ppm_calls=%lu ppm_rewrites=%lu ppm_no_policy=%lu "
                 "update_policy_calls=%lu ppm_notifier_calls=%lu "
                 "freq_limit_info_calls=%lu update_userlimit_calls=%lu "
                 "mt_userlimit_calls=%lu ppm_main_calls=%lu "
                 "thermolmt_calls=%lu ppm_thermal_calls=%lu "
                 "transition_end_calls=%lu transition_end_rewrites=%lu "
                 "mtk_set=0x%lx wrapper=0x%lx id_to_cpu_dvfs=0x%lx "
                 "ppm_notifier=0x%lx freq_limit_info=0x%lx "
                 "update_userlimit=0x%lx mt_userlimit=0x%lx "
                 "ppm_main=0x%lx thermolmt=0x%lx ppm_thermal=0x%lx "
                 "transition_end=0x%lx "
                 "set_policy=0x%lx update_policy=0x%lx",
                 g_target_khz ? 1U : 0U, g_target_khz, g_policy_cpu,
                 g_hook_err, g_calls, g_target_hits, g_rewrites,
                 g_after_calls, g_after_rewrites,
                 g_seen_cpu_mask, g_last_cpu, g_last_old_max,
                 g_last_new_max, g_mtk_hook_err, g_mtk_calls,
                 g_mtk_rewrites, g_wrapper_hook_err, g_wrapper_calls,
                 g_wrapper_rewrites, g_ppm_calls, g_ppm_rewrites,
                 g_ppm_no_policy, g_update_policy_calls,
                 g_ppm_notifier_calls, g_freq_limit_info_calls,
                 g_update_userlimit_calls, g_mt_userlimit_calls,
                 g_ppm_main_calls, g_thermolmt_calls,
                 g_ppm_thermal_calls, g_transition_end_calls,
                 g_transition_end_rewrites,
                 g_mt_cpufreq_set,
                 g_mt_cpufreq_wrapper, (unsigned long)g_id_to_cpu_dvfs,
                 g_ppm_notifier, g_update_freq_limit_info,
                 g_update_userlimit_cpu_freq, g_mt_ppm_userlimit_cpu_freq,
                 g_mt_ppm_main, g_thermolmt_cpu, g_ppm_cpu_thermal,
                 g_cpufreq_transition_end,
                 g_cpufreq_set_policy,
                 g_cpufreq_update_policy);
        if (g_policy6_addr) {
            size_t used = strlen(reply);
            unsigned int live_cpu =
                load_u32(g_policy6_addr + CPUFREQ_POLICY_CPU_OFFSET);
            unsigned int live_min =
                load_u32(g_policy6_addr + CPUFREQ_POLICY_MIN_OFFSET);
            unsigned int live_max =
                load_u32(g_policy6_addr + CPUFREQ_POLICY_MAX_OFFSET);
            unsigned int user_min =
                load_u32(g_policy6_addr + 0xa0);
            unsigned int user_max =
                load_u32(g_policy6_addr + 0xa4);

            snprintf(reply + used, sizeof(reply) - used,
                     " policy6=0x%lx live_cpu=%u live_min=%u live_max=%u "
                     "user_min=%u user_max=%u",
                     g_policy6_addr, live_cpu, live_min, live_max,
                     user_min, user_max);
        }
        pr_info("pftm10-cpuhold status %s\n", reply);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "help")) {
        snprintf(reply, sizeof(reply),
                 "commands: status | cpu-max:<khz> | cpu-max <khz> | "
                 "cpu-max-off | policy-cpu:<cpu> | policy-cpu <cpu> | "
                 "hook-status | help");
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strncmp(cmd, "cpu-max:", 8)) {
        unsigned int target;

        if (g_hook_err != HOOK_NO_ERR) {
            snprintf(reply, sizeof(reply),
                     "error: cpufreq_set_policy hook unavailable err=%d",
                     g_hook_err);
            return copy_reply(out_msg, outlen, reply);
        }
        if (parse_u32_token(cmd, "cpu-max:", &target) != 0 ||
            target < 300000 || target > 3000000) {
            snprintf(reply, sizeof(reply),
                     "error: use cpu-max:<300000..3000000 kHz>");
            return copy_reply(out_msg, outlen, reply);
        }

        g_target_khz = target;
        update_policy_cpu();
        snprintf(reply, sizeof(reply),
                 "cpu_max_enabled=1 target_khz=%u policy_cpu=%u "
                 "update_issued=1 rewrites=%lu",
                 g_target_khz, g_policy_cpu, g_rewrites);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strncmp(cmd, "cpu-max ", 8)) {
        const char *value = cmd + 8;
        unsigned int target;

        if (g_hook_err != HOOK_NO_ERR) {
            snprintf(reply, sizeof(reply),
                     "error: cpufreq_set_policy hook unavailable err=%d",
                     g_hook_err);
            return copy_reply(out_msg, outlen, reply);
        }
        if (parse_u32(&value, &target) != 0 ||
            target < 300000 || target > 3000000) {
            snprintf(reply, sizeof(reply),
                     "error: target must be 300000..3000000 kHz");
            return copy_reply(out_msg, outlen, reply);
        }

        g_target_khz = target;
        update_policy_cpu();
        snprintf(reply, sizeof(reply),
                 "cpu_max_enabled=1 target_khz=%u policy_cpu=%u "
                 "update_issued=1 rewrites=%lu",
                 g_target_khz, g_policy_cpu, g_rewrites);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "cpu-max-off")) {
        g_target_khz = 0;
        update_policy_cpu();
        snprintf(reply, sizeof(reply),
                 "cpu_max_enabled=0 policy_cpu=%u update_issued=1 "
                 "rewrites=%lu",
                 g_policy_cpu, g_rewrites);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strncmp(cmd, "policy-cpu:", 11)) {
        unsigned int cpu;

        if (parse_u32_token(cmd, "policy-cpu:", &cpu) != 0 || cpu > 7) {
            snprintf(reply, sizeof(reply),
                     "error: policy CPU must be 0..7");
            return copy_reply(out_msg, outlen, reply);
        }
        g_policy_cpu = cpu;
        if (g_target_khz)
            update_policy_cpu();
        snprintf(reply, sizeof(reply),
                 "policy_cpu=%u update_issued=%u", g_policy_cpu,
                 g_target_khz ? 1U : 0U);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strncmp(cmd, "policy-cpu ", 11)) {
        const char *value = cmd + 11;
        unsigned int cpu;

        if (parse_u32(&value, &cpu) != 0 || cpu > 7) {
            snprintf(reply, sizeof(reply),
                     "error: policy CPU must be 0..7");
            return copy_reply(out_msg, outlen, reply);
        }
        g_policy_cpu = cpu;
        if (g_target_khz)
            update_policy_cpu();
        snprintf(reply, sizeof(reply),
                 "policy_cpu=%u update_issued=%u", g_policy_cpu,
                 g_target_khz ? 1U : 0U);
        return copy_reply(out_msg, outlen, reply);
    }

    snprintf(reply, sizeof(reply), "error: unknown command; use help");
    return copy_reply(out_msg, outlen, reply);
}

static long cpuhold_init(const char *args, const char *event, void *__user reserved)
{
    (void)reserved;

    g_cpufreq_set_policy = kallsyms_lookup_name("cpufreq_set_policy");
    g_cpufreq_update_policy = kallsyms_lookup_name("cpufreq_update_policy");
    g_mt_cpufreq_set = kallsyms_lookup_name("_mt_cpufreq_set");
    g_mt_cpufreq_wrapper =
        kallsyms_lookup_name("_mt_cpufreq_dvfs_request_wrapper");
    g_id_to_cpu_dvfs = (void *)kallsyms_lookup_name("id_to_cpu_dvfs");
    g_ppm_notifier = kallsyms_lookup_name("ppm_notifier");
    g_update_freq_limit_info =
        kallsyms_lookup_name("update_freq_limit_info");
    g_update_userlimit_cpu_freq =
        kallsyms_lookup_name("update_userlimit_cpu_freq");
    g_mt_ppm_userlimit_cpu_freq =
        kallsyms_lookup_name("mt_ppm_userlimit_cpu_freq");
    g_mt_ppm_main = kallsyms_lookup_name("mt_ppm_main");
    g_thermolmt_cpu =
        kallsyms_lookup_name("apthermolmt_set_cpu_power_limit");
    g_ppm_cpu_thermal =
        kallsyms_lookup_name("mt_ppm_cpu_thermal_protect");
    g_cpufreq_transition_end =
        kallsyms_lookup_name("cpufreq_freq_transition_end");
    if (!g_cpufreq_set_policy || !g_cpufreq_update_policy) {
        g_hook_err = -2;
        pr_err("pftm10-cpuhold missing symbols set=0x%lx update=0x%lx\n",
               g_cpufreq_set_policy, g_cpufreq_update_policy);
        return 0;
    }

    g_hook_err = hook_wrap2((void *)g_cpufreq_set_policy,
                            before_cpufreq_set_policy,
                            after_cpufreq_set_policy, 0);
    if (g_mt_cpufreq_set)
        g_mtk_hook_err = hook_wrap4((void *)g_mt_cpufreq_set,
                                    0, after_mt_cpufreq_set, 0);
    else
        g_mtk_hook_err = -3;
    if (g_mt_cpufreq_wrapper)
        g_wrapper_hook_err = hook_wrap4((void *)g_mt_cpufreq_wrapper,
                                        0, after_mt_cpufreq_wrapper, 0);
    else
        g_wrapper_hook_err = -4;
    if (g_cpufreq_update_policy)
        g_update_policy_hook_err =
            hook_wrap2((void *)g_cpufreq_update_policy,
                       before_cpufreq_update_policy, 0, 0);
    if (g_ppm_notifier)
        g_ppm_notifier_hook_err =
            hook_wrap4((void *)g_ppm_notifier,
                       before_ppm_notifier, after_ppm_notifier, 0);
    else
        g_ppm_notifier_hook_err = -5;
    if (g_update_freq_limit_info)
        g_freq_limit_info_hook_err =
            hook_wrap2((void *)g_update_freq_limit_info,
                       0, after_update_freq_limit_info, 0);
    else
        g_freq_limit_info_hook_err = -6;
    if (g_update_userlimit_cpu_freq)
        g_update_userlimit_hook_err =
            hook_wrap4((void *)g_update_userlimit_cpu_freq,
                       before_update_userlimit_cpu_freq, 0, 0);
    else
        g_update_userlimit_hook_err = -7;
    if (g_mt_ppm_userlimit_cpu_freq)
        g_mt_userlimit_hook_err =
            hook_wrap4((void *)g_mt_ppm_userlimit_cpu_freq,
                       before_mt_ppm_userlimit_cpu_freq, 0, 0);
    else
        g_mt_userlimit_hook_err = -8;
    if (g_mt_ppm_main)
        g_ppm_main_hook_err =
            hook_wrap4((void *)g_mt_ppm_main, before_mt_ppm_main, 0, 0);
    else
        g_ppm_main_hook_err = -9;
    if (g_thermolmt_cpu)
        g_thermolmt_hook_err =
            hook_wrap4((void *)g_thermolmt_cpu, before_thermolmt_cpu, 0, 0);
    else
        g_thermolmt_hook_err = -10;
    if (g_ppm_cpu_thermal)
        g_ppm_thermal_hook_err =
            hook_wrap4((void *)g_ppm_cpu_thermal,
                       before_ppm_cpu_thermal, 0, 0);
    else
        g_ppm_thermal_hook_err = -11;
    if (g_cpufreq_transition_end)
        g_transition_end_hook_err =
            hook_wrap4((void *)g_cpufreq_transition_end,
                       0, after_cpufreq_transition_end, 0);
    else
        g_transition_end_hook_err = -12;
    pr_info("pftm10-cpuhold init event=%s args=%s set=0x%lx update=0x%lx "
            "mt_set=0x%lx wrapper=0x%lx id_to=0x%lx hook=%d mtk_hook=%d "
            "wrapper_hook=%d ppm_notifier=0x%lx notifier_hook=%d "
            "freq_limit_info=0x%lx freq_limit_hook=%d update_hook=%d "
            "update_userlimit=0x%lx userlimit_hook=%d "
            "mt_userlimit=0x%lx mt_userlimit_hook=%d ppm_main=0x%lx "
            "ppm_main_hook=%d thermolmt=0x%lx thermolmt_hook=%d "
            "ppm_thermal=0x%lx ppm_thermal_hook=%d transition_end=0x%lx "
            "transition_end_hook=%d\n",
            event ? event : "(null)", args ? args : "(null)",
            g_cpufreq_set_policy, g_cpufreq_update_policy,
            g_mt_cpufreq_set, g_mt_cpufreq_wrapper,
            (unsigned long)g_id_to_cpu_dvfs, g_hook_err,
            g_mtk_hook_err, g_wrapper_hook_err, g_ppm_notifier,
            g_ppm_notifier_hook_err, g_update_freq_limit_info,
            g_freq_limit_info_hook_err, g_update_policy_hook_err,
            g_update_userlimit_cpu_freq, g_update_userlimit_hook_err,
            g_mt_ppm_userlimit_cpu_freq, g_mt_userlimit_hook_err,
            g_mt_ppm_main, g_ppm_main_hook_err, g_thermolmt_cpu,
            g_thermolmt_hook_err, g_ppm_cpu_thermal,
            g_ppm_thermal_hook_err, g_cpufreq_transition_end,
            g_transition_end_hook_err);
    return 0;
}

static long cpuhold_exit(void *__user reserved)
{
    (void)reserved;

    g_target_khz = 0;
    update_policy_cpu();
    if (g_cpufreq_set_policy && g_hook_err == HOOK_NO_ERR)
        hook_unwrap((void *)g_cpufreq_set_policy,
                    before_cpufreq_set_policy, after_cpufreq_set_policy);
    if (g_mt_cpufreq_set && g_mtk_hook_err == HOOK_NO_ERR)
        hook_unwrap((void *)g_mt_cpufreq_set,
                    0, after_mt_cpufreq_set);
    if (g_mt_cpufreq_wrapper && g_wrapper_hook_err == HOOK_NO_ERR)
        hook_unwrap((void *)g_mt_cpufreq_wrapper,
                    0, after_mt_cpufreq_wrapper);
    if (g_cpufreq_update_policy && g_update_policy_hook_err == HOOK_NO_ERR)
        hook_unwrap((void *)g_cpufreq_update_policy,
                    before_cpufreq_update_policy, 0);
    if (g_ppm_notifier && g_ppm_notifier_hook_err == HOOK_NO_ERR)
        hook_unwrap((void *)g_ppm_notifier,
                    before_ppm_notifier, after_ppm_notifier);
    if (g_update_freq_limit_info &&
        g_freq_limit_info_hook_err == HOOK_NO_ERR)
        hook_unwrap((void *)g_update_freq_limit_info,
                    0, after_update_freq_limit_info);
    if (g_update_userlimit_cpu_freq &&
        g_update_userlimit_hook_err == HOOK_NO_ERR)
        hook_unwrap((void *)g_update_userlimit_cpu_freq,
                    before_update_userlimit_cpu_freq, 0);
    if (g_mt_ppm_userlimit_cpu_freq &&
        g_mt_userlimit_hook_err == HOOK_NO_ERR)
        hook_unwrap((void *)g_mt_ppm_userlimit_cpu_freq,
                    before_mt_ppm_userlimit_cpu_freq, 0);
    if (g_mt_ppm_main && g_ppm_main_hook_err == HOOK_NO_ERR)
        hook_unwrap((void *)g_mt_ppm_main, before_mt_ppm_main, 0);
    if (g_thermolmt_cpu && g_thermolmt_hook_err == HOOK_NO_ERR)
        hook_unwrap((void *)g_thermolmt_cpu, before_thermolmt_cpu, 0);
    if (g_ppm_cpu_thermal && g_ppm_thermal_hook_err == HOOK_NO_ERR)
        hook_unwrap((void *)g_ppm_cpu_thermal, before_ppm_cpu_thermal, 0);
    if (g_cpufreq_transition_end &&
        g_transition_end_hook_err == HOOK_NO_ERR)
        hook_unwrap((void *)g_cpufreq_transition_end,
                    0, after_cpufreq_transition_end);
    pr_info("pftm10-cpuhold exit\n");
    return 0;
}

KPM_INIT(cpuhold_init);
KPM_CTL0(cpuhold_control0);
KPM_EXIT(cpuhold_exit);
