/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <compiler.h>
#include <kpmodule.h>
#include <common.h>
#include <hook.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <kputils.h>

KPM_NAME("pftm10-kprobe");
KPM_VERSION("1.4.6");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("syttys123");
KPM_DESCRIPTION("PFTM10 PD-gated AICR hook with conditional charger resume");

static const char *boot_args;

#define MT6360_SET_AICR_OFFSET 0x969618UL
#define TCPC_SOURCE_CAP_COUNT_OFFSET 0x2534UL
#define TCPC_SOURCE_CAP_OFFSET 0x2538UL
#define TCPC_SELECTED_CAP_OFFSET 0x25d4UL
#define OPLUS_CHIP_UI_SOC_OFFSET 0xc40UL

typedef int (*pd_contract_fn)(void *tcpc, int *voltage_mv, int *current_ma);
typedef int (*pd_source_cap_fn)(void *tcpc, unsigned int *caps);
typedef int (*pd_request_fn)(void *tcpc, int voltage_mv, int current_ma,
                             void *data);
typedef int (*aicl_enable_fn)(int enable);
typedef int (*mt6360_set_aicr_fn)(void *chip, int current_ua);

#define PD_PDO_TYPE(pdo) (((pdo) >> 30) & 0x3)
#define PD_PDO_VOLT_MV(pdo) ((((pdo) >> 10) & 0x3ff) * 50)
#define PD_PDO_CURR_MA(pdo) (((pdo) & 0x3ff) * 10)

static unsigned long g_mt6360_set_aicr_addr;
static unsigned long g_tcpc;
static volatile int g_force_aicr_ua;
static volatile unsigned long g_hook_rewrites;
static volatile unsigned long g_unsuspend_calls;
static int g_hook_err;

static unsigned int tcpc_selected_pdo(unsigned long tcpc)
{
    unsigned int index;
    unsigned int count;

    if (!tcpc)
        return 0;

    count = *(volatile unsigned char *)(
        tcpc + TCPC_SOURCE_CAP_COUNT_OFFSET);
    index = (*(volatile unsigned int *)(
        tcpc + TCPC_SELECTED_CAP_OFFSET) >> 28) & 0x7;
    if (!index || index > count || index > 32)
        return 0;

    return *(volatile unsigned int *)(
        tcpc + TCPC_SOURCE_CAP_OFFSET + (index - 1) * 4);
}

static unsigned long mt6360_chip_address(void)
{
    unsigned long slot;
    unsigned long chip;
    unsigned long charger_dev;

    slot = kallsyms_lookup_name("g_oplus_chip");
    if (!slot)
        return 0;

    chip = *(volatile unsigned long *)slot;
    if (!chip)
        return 0;

    charger_dev = *(volatile unsigned long *)(chip + 0x210);
    if (!charger_dev)
        return 0;

    return *(volatile unsigned long *)(charger_dev + 0x28);
}

static unsigned long oplus_chg_chip_address(void)
{
    unsigned long slot = kallsyms_lookup_name("g_oplus_chip");

    if (!slot)
        return 0;

    return *(volatile unsigned long *)slot;
}

static int int_abs(int value)
{
    return value < 0 ? -value : value;
}

static void before_mt6360_set_aicr(hook_fargs2_t *args, void *udata)
{
    unsigned long tcpc = g_tcpc;
    int force_ua = g_force_aicr_ua;
    unsigned int pdo;

    if (!force_ua || !tcpc)
        return;

    pdo = tcpc_selected_pdo(tcpc);
    if (PD_PDO_TYPE(pdo) != 0 ||
        PD_PDO_VOLT_MV(pdo) != 5000 ||
        PD_PDO_CURR_MA(pdo) < 3000) {
        return;
    }

    if ((int)args->arg1 < force_ua) {
        args->arg1 = (uint64_t)force_ua;
        g_hook_rewrites++;
    }
}

static int pd_contract_status(int *voltage_mv, int *current_ma)
{
    unsigned long get_tcpc;
    unsigned long contract;
    void *tcpc;

    get_tcpc = kallsyms_lookup_name("tcpc_dev_get_by_name");
    contract = kallsyms_lookup_name("tcpm_inquire_pd_contract");
    if (!get_tcpc || !contract)
        return -1000;

    tcpc = ((void *(*)(const char *))get_tcpc)("type_c_port0");
    if (!tcpc)
        return -1001;

    return ((pd_contract_fn)contract)(tcpc, voltage_mv, current_ma);
}

static int oplus_charging_status(void)
{
    unsigned long status_addr =
        kallsyms_lookup_name("oplus_chg_get_chging_status");

    if (!status_addr)
        return -1;

    return ((int (*)(void))status_addr)();
}

static int oplus_stop_charging_status(void)
{
    unsigned long status_addr =
        kallsyms_lookup_name("oplus_chg_get_stop_chg");

    if (!status_addr)
        return -1;

    return ((int (*)(void))status_addr)();
}

static int resume_charger_if_stopped(int *before_out, int *after_out)
{
    unsigned long unsuspend_addr =
        kallsyms_lookup_name("oplus_chg_unsuspend_charger");
    int before = oplus_charging_status();

    *before_out = before;
    *after_out = -1;
    if (before != 0 || !unsuspend_addr)
        return 0;

    ((void (*)(void))unsuspend_addr)();
    g_unsuspend_calls++;
    *after_out = oplus_charging_status();
    return 1;
}

static int direct_set_aicr(int current_ua, int aicl_mode,
                           unsigned long *chip_out, int *aicl_rc_out,
                           int *aicr_rc_out)
{
    unsigned long chip;
    unsigned long aicl_addr;
    int aicl_rc = -1;
    int aicr_rc;

    *chip_out = 0;
    *aicl_rc_out = -1;
    *aicr_rc_out = -1;

    if (!g_mt6360_set_aicr_addr)
        return -1;

    chip = mt6360_chip_address();
    if (!chip)
        return -2;

    aicl_addr = kallsyms_lookup_name("mt6360_aicl_enable");
    if (aicl_mode >= 0 && aicl_addr)
        aicl_rc = ((aicl_enable_fn)aicl_addr)(aicl_mode);
    aicr_rc = ((mt6360_set_aicr_fn)g_mt6360_set_aicr_addr)(
        (void *)chip, current_ua);

    *chip_out = chip;
    *aicl_rc_out = aicl_rc;
    *aicr_rc_out = aicr_rc;
    return aicr_rc;
}

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

static int copy_kernel_string(char *dst, int dstlen, const char *src)
{
    int i;

    if (!dst || dstlen <= 0 || !src)
        return -1;

    for (i = 0; i < dstlen - 1; i++) {
        unsigned char ch = (unsigned char)src[i];
        if (!ch)
            break;
        dst[i] = (ch >= 32 && ch < 127) ? (char)ch : '.';
    }
    dst[i] = '\0';
    return 0;
}

static int bytes_to_hex(char *dst, int dstlen, const unsigned char *src, int len)
{
    static const char hex[] = "0123456789abcdef";
    int i;

    if (!dst || dstlen < (len * 2 + 1) || !src)
        return -1;

    for (i = 0; i < len; i++) {
        dst[i * 2] = hex[src[i] >> 4];
        dst[i * 2 + 1] = hex[src[i] & 0x0f];
    }
    dst[len * 2] = '\0';
    return 0;
}

static long kprobe_init(const char *args, const char *event, void *__user reserved)
{
    char banner[192];
    unsigned long banner_addr;
    unsigned long state_addr;
    unsigned long text_addr;
    unsigned long get_tcpc;

    boot_args = args;
    pr_info("pftm10-kprobe init, event: %s, args: %s, kpver: %x\n",
            event ? event : "(null)", args ? args : "(null)", kpver);

    banner_addr = kallsyms_lookup_name("linux_banner");
    if (banner_addr) {
        copy_kernel_string(banner, sizeof(banner), (const char *)banner_addr);
        pr_info("pftm10-kprobe banner: %s\n", banner);
    }

    state_addr = kallsyms_lookup_name("system_state");
    if (state_addr) {
        pr_info("pftm10-kprobe system_state=0x%x\n",
                *(volatile const unsigned int *)state_addr);
    }

    get_tcpc = kallsyms_lookup_name("tcpc_dev_get_by_name");
    if (get_tcpc)
        g_tcpc = (unsigned long)
            ((void *(*)(const char *))get_tcpc)("type_c_port0");

    g_mt6360_set_aicr_addr = kallsyms_lookup_name("mt6360_set_aicr");
    if (!g_mt6360_set_aicr_addr) {
        text_addr = kallsyms_lookup_name("_text");
        if (text_addr)
            g_mt6360_set_aicr_addr = text_addr + MT6360_SET_AICR_OFFSET;
    }

    g_hook_err = -1;
    if (g_mt6360_set_aicr_addr) {
        g_hook_err = hook_wrap2(
            (void *)g_mt6360_set_aicr_addr,
            before_mt6360_set_aicr, 0, 0);
    }
    pr_info("pftm10-kprobe AICR hook: tcpc=0x%lx fn=0x%lx err=%d\n",
            g_tcpc, g_mt6360_set_aicr_addr, g_hook_err);

    return 0;
}

static long kprobe_control0(const char *args, char *__user out_msg, int outlen)
{
    char reply[640];
    unsigned long init_task;
    const char *cmd = (args && args[0]) ? args : "info";

    pr_info("pftm10-kprobe control0 command: %s\n", cmd);

    init_task = kallsyms_lookup_name("init_task");

    if (!strcmp(cmd, "info")) {
        snprintf(reply, sizeof(reply),
                 "uid=%u kpver=%x init_task=0x%lx args=%s",
                 (unsigned int)current_uid(), kpver, init_task,
                 boot_args ? boot_args : "(null)");
        pr_info("pftm10-kprobe info: %s\n", reply);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "help")) {
        snprintf(reply, sizeof(reply),
                 "commands: info | banner | cmdline | sym:<name> | "
                 "u32:<name> | u64:<name> | bytes:<name> | str:<name> | "
                 "pstr:<name> | soc-status | soc-update | soc-sync | "
                 "soc-reconcile | "
                 "charger-unsuspend | pd-contract | "
                 "pd-source-cap | pd-raw-caps | pd-request-9v | "
                 "pd-soft-reset | pd-force-9v | charger-500-direct | "
                 "charger-3a | charger-3a-direct | charger-restore | "
                 "hook-status | help");
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "soc-status")) {
        unsigned long get_soc =
            kallsyms_lookup_name("oplus_chg_get_soc");
        unsigned long get_ui_soc =
            kallsyms_lookup_name("oplus_chg_get_ui_soc");
        unsigned long battery_get_soc_addr =
            kallsyms_lookup_name("battery_get_soc");
        unsigned long battery_get_uisoc_addr =
            kallsyms_lookup_name("battery_get_uisoc");
        int chip_soc = -1;
        int ui_soc = -1;
        int gauge_soc = -1;
        int gauge_ui_soc = -1;

        if (get_soc)
            chip_soc = ((int (*)(void))get_soc)();
        if (get_ui_soc)
            ui_soc = ((int (*)(void))get_ui_soc)();
        if (battery_get_soc_addr)
            gauge_soc = ((int (*)(void))battery_get_soc_addr)();
        if (battery_get_uisoc_addr)
            gauge_ui_soc = ((int (*)(void))battery_get_uisoc_addr)();

        snprintf(reply, sizeof(reply),
                 "chip_soc=%d ui_soc=%d gauge_soc=%d gauge_ui_soc=%d",
                 chip_soc, ui_soc, gauge_soc, gauge_ui_soc);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "soc-update")) {
        unsigned long update_addr =
            kallsyms_lookup_name("oplus_chg_soc_update");
        unsigned long get_soc =
            kallsyms_lookup_name("oplus_chg_get_soc");
        unsigned long get_ui_soc =
            kallsyms_lookup_name("oplus_chg_get_ui_soc");
        int chip_before = -1;
        int ui_before = -1;
        int chip_after = -1;
        int ui_after = -1;

        if (!update_addr) {
            snprintf(reply, sizeof(reply),
                     "error: oplus_chg_soc_update not found");
            return copy_reply(out_msg, outlen, reply);
        }

        if (get_soc)
            chip_before = ((int (*)(void))get_soc)();
        if (get_ui_soc)
            ui_before = ((int (*)(void))get_ui_soc)();
        ((void (*)(void))update_addr)();
        if (get_soc)
            chip_after = ((int (*)(void))get_soc)();
        if (get_ui_soc)
            ui_after = ((int (*)(void))get_ui_soc)();

        snprintf(reply, sizeof(reply),
                 "soc_update_called=1 chip_before=%d ui_before=%d "
                 "chip_after=%d ui_after=%d",
                 chip_before, ui_before, chip_after, ui_after);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "soc-sync")) {
        unsigned long chip = oplus_chg_chip_address();
        unsigned long battery_get_uisoc_addr =
            kallsyms_lookup_name("battery_get_uisoc");
        unsigned long get_soc_addr =
            kallsyms_lookup_name("oplus_chg_get_soc");
        unsigned long set_kernel_uisoc_addr =
            kallsyms_lookup_name("set_kernel_uisoc");
        unsigned long set_rtc_ui_soc_addr =
            kallsyms_lookup_name("set_rtc_ui_soc");
        unsigned long update_addr =
            kallsyms_lookup_name("oplus_chg_soc_update");
        int chip_soc = -1;
        int gauge_ui_soc = -1;
        int target = -1;
        int ui_before = -1;
        int ui_after = -1;
        int kernel_rc = -1;
        int rtc_called = 0;

        if (!chip) {
            snprintf(reply, sizeof(reply),
                     "error: g_oplus_chip not found");
            return copy_reply(out_msg, outlen, reply);
        }
        if (get_soc_addr)
            chip_soc = ((int (*)(void))get_soc_addr)();
        if (battery_get_uisoc_addr)
            gauge_ui_soc = ((int (*)(void))battery_get_uisoc_addr)();

        target = (gauge_ui_soc >= 0 && gauge_ui_soc <= 100) ?
            gauge_ui_soc :
            ((chip_soc >= 0 && chip_soc <= 100) ? chip_soc : -1);
        if (target < 0) {
            snprintf(reply, sizeof(reply),
                     "error: invalid soc chip_soc=%d gauge_ui_soc=%d",
                     chip_soc, gauge_ui_soc);
            return copy_reply(out_msg, outlen, reply);
        }

        ui_before = *(volatile int *)(chip + OPLUS_CHIP_UI_SOC_OFFSET);
        if (set_kernel_uisoc_addr)
            kernel_rc =
                ((int (*)(int))set_kernel_uisoc_addr)(target);
        *(volatile int *)(chip + OPLUS_CHIP_UI_SOC_OFFSET) = target;
        if (set_rtc_ui_soc_addr) {
            ((void (*)(int))set_rtc_ui_soc_addr)(target);
            rtc_called = 1;
        }
        if (update_addr)
            ((void (*)(void))update_addr)();
        ui_after = *(volatile int *)(chip + OPLUS_CHIP_UI_SOC_OFFSET);

        snprintf(reply, sizeof(reply),
                 "soc_sync_applied=1 chip_soc=%d gauge_ui_soc=%d "
                 "target=%d ui_before=%d ui_after=%d "
                 "set_kernel_uisoc_rc=%d set_rtc_ui_soc_called=%d",
                 chip_soc, gauge_ui_soc, target, ui_before, ui_after,
                 kernel_rc, rtc_called);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "soc-reconcile")) {
        unsigned long chip = oplus_chg_chip_address();
        unsigned long battery_get_uisoc_addr =
            kallsyms_lookup_name("battery_get_uisoc");
        unsigned long get_soc_addr =
            kallsyms_lookup_name("oplus_chg_get_soc");
        unsigned long set_kernel_uisoc_addr =
            kallsyms_lookup_name("set_kernel_uisoc");
        unsigned long set_rtc_ui_soc_addr =
            kallsyms_lookup_name("set_rtc_ui_soc");
        unsigned long update_addr =
            kallsyms_lookup_name("oplus_chg_soc_update");
        int chip_soc = -1;
        int gauge_ui_soc = -1;
        int target = -1;
        int ui_before = -1;
        int ui_after = -1;
        int kernel_rc = -1;
        int rtc_called = 0;

        if (!chip) {
            snprintf(reply, sizeof(reply),
                     "soc_reconcile_skipped=1 reason=chip_missing");
            return copy_reply(out_msg, outlen, reply);
        }
        if (get_soc_addr)
            chip_soc = ((int (*)(void))get_soc_addr)();
        if (battery_get_uisoc_addr)
            gauge_ui_soc = ((int (*)(void))battery_get_uisoc_addr)();
        target = (gauge_ui_soc >= 0 && gauge_ui_soc <= 100) ?
            gauge_ui_soc :
            ((chip_soc >= 0 && chip_soc <= 100) ? chip_soc : -1);
        ui_before = *(volatile int *)(chip + OPLUS_CHIP_UI_SOC_OFFSET);

        if (target < 0) {
            snprintf(reply, sizeof(reply),
                     "soc_reconcile_skipped=1 reason=invalid_source "
                     "chip_soc=%d gauge_ui_soc=%d ui_soc=%d",
                     chip_soc, gauge_ui_soc, ui_before);
            return copy_reply(out_msg, outlen, reply);
        }
        if (chip_soc >= 0 && int_abs(chip_soc - gauge_ui_soc) > 8) {
            snprintf(reply, sizeof(reply),
                     "soc_reconcile_skipped=1 reason=sources_differ "
                     "chip_soc=%d gauge_ui_soc=%d ui_soc=%d",
                     chip_soc, gauge_ui_soc, ui_before);
            return copy_reply(out_msg, outlen, reply);
        }
        if (int_abs(ui_before - target) < 10) {
            snprintf(reply, sizeof(reply),
                     "soc_reconcile_skipped=1 reason=already_aligned "
                     "chip_soc=%d gauge_ui_soc=%d ui_soc=%d",
                     chip_soc, gauge_ui_soc, ui_before);
            return copy_reply(out_msg, outlen, reply);
        }

        if (set_kernel_uisoc_addr)
            kernel_rc =
                ((int (*)(int))set_kernel_uisoc_addr)(target);
        *(volatile int *)(chip + OPLUS_CHIP_UI_SOC_OFFSET) = target;
        if (set_rtc_ui_soc_addr) {
            ((void (*)(int))set_rtc_ui_soc_addr)(target);
            rtc_called = 1;
        }
        if (update_addr)
            ((void (*)(void))update_addr)();
        ui_after = *(volatile int *)(chip + OPLUS_CHIP_UI_SOC_OFFSET);

        snprintf(reply, sizeof(reply),
                 "soc_reconcile_applied=1 chip_soc=%d gauge_ui_soc=%d "
                 "target=%d ui_before=%d ui_after=%d "
                 "set_kernel_uisoc_rc=%d set_rtc_ui_soc_called=%d",
                 chip_soc, gauge_ui_soc, target, ui_before, ui_after,
                 kernel_rc, rtc_called);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "charger-unsuspend")) {
        unsigned long unsuspend_addr =
            kallsyms_lookup_name("oplus_chg_unsuspend_charger");
        unsigned long stop_addr =
            kallsyms_lookup_name("oplus_chg_get_stop_chg");
        int stop_chg = -1;

        if (!unsuspend_addr) {
            snprintf(reply, sizeof(reply),
                     "error: oplus_chg_unsuspend_charger not found");
            return copy_reply(out_msg, outlen, reply);
        }

        ((void (*)(void))unsuspend_addr)();
        if (stop_addr)
            stop_chg = ((int (*)(void))stop_addr)();

        pr_info("pftm10-kprobe charger unsuspend fn=0x%lx stop=%d\n",
                unsuspend_addr, stop_chg);
        snprintf(reply, sizeof(reply),
                 "charger_unsuspend_called=1 fn=0x%lx stop_chg=%d",
                 unsuspend_addr, stop_chg);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "pd-contract")) {
        int voltage_mv = -1;
        int current_ma = -1;
        int rc = pd_contract_status(&voltage_mv, &current_ma);
        unsigned long connected_addr =
            kallsyms_lookup_name("tcpm_inquire_pd_connected");
        unsigned long get_tcpc =
            kallsyms_lookup_name("tcpc_dev_get_by_name");
        int connected = -1;

        if (connected_addr && get_tcpc) {
            void *tcpc =
                ((void *(*)(const char *))get_tcpc)("type_c_port0");
            if (tcpc)
                connected = ((int (*)(void *))connected_addr)(tcpc);
        }

        snprintf(reply, sizeof(reply),
                 "pd_connected=%d rc=%d contract_mv=%d contract_ma=%d",
                 connected, rc, voltage_mv, current_ma);
        pr_info("pftm10-kprobe pd contract: %s\n", reply);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "pd-source-cap")) {
        unsigned long get_tcpc =
            kallsyms_lookup_name("tcpc_dev_get_by_name");
        unsigned long source_cap =
            kallsyms_lookup_name("tcpm_inquire_pd_source_cap");
        unsigned int caps[33];
        void *tcpc = 0;
        int rc;
        int count;
        int used;
        int i;

        if (!get_tcpc || !source_cap) {
            snprintf(reply, sizeof(reply),
                     "error: PD source-cap symbols unavailable");
            return copy_reply(out_msg, outlen, reply);
        }
        tcpc = ((void *(*)(const char *))get_tcpc)("type_c_port0");
        if (!tcpc) {
            snprintf(reply, sizeof(reply), "error: type_c_port0 unavailable");
            return copy_reply(out_msg, outlen, reply);
        }

        memset(caps, 0, sizeof(caps));
        rc = ((pd_source_cap_fn)source_cap)(tcpc, caps);
        count = caps[0] & 0xff;
        if (count > 32)
            count = 32;
        used = snprintf(reply, sizeof(reply),
                        "rc=%d count=%d tcpc=0x%lx", rc, count,
                        (unsigned long)tcpc);
        for (i = 0; i < count && used > 0 &&
                    used < (int)sizeof(reply); i++) {
            int written = snprintf(
                reply + used, sizeof(reply) - used,
                " pdo%d=0x%08x", i + 1, caps[i + 1]);
            if (written < 0)
                break;
            used += written;
        }
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "pd-raw-caps")) {
        unsigned long tcpc = g_tcpc;
        unsigned int count;
        unsigned int selected_raw;
        int used;
        int i;

        if (!tcpc) {
            snprintf(reply, sizeof(reply), "error: type_c_port0 unavailable");
            return copy_reply(out_msg, outlen, reply);
        }

        count = *(volatile unsigned char *)(
            tcpc + TCPC_SOURCE_CAP_COUNT_OFFSET);
        selected_raw = *(volatile unsigned int *)(
            tcpc + TCPC_SELECTED_CAP_OFFSET);
        if (count > 8)
            count = 8;
        used = snprintf(reply, sizeof(reply),
                        "count=%u selected_raw=0x%08x", count, selected_raw);
        for (i = 0; i < count && used > 0 &&
                    used < (int)sizeof(reply); i++) {
            unsigned int pdo = *(volatile unsigned int *)(
                tcpc + TCPC_SOURCE_CAP_OFFSET + i * 4);
            int written = snprintf(
                reply + used, sizeof(reply) - used,
                " pdo%d=0x%08x", i + 1, pdo);
            if (written < 0)
                break;
            used += written;
        }
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "pd-request-9v")) {
        unsigned long get_tcpc =
            kallsyms_lookup_name("tcpc_dev_get_by_name");
        unsigned long source_cap =
            kallsyms_lookup_name("tcpm_inquire_pd_source_cap");
        unsigned long request_addr =
            kallsyms_lookup_name("tcpm_dpm_pd_request");
        unsigned int caps[33];
        void *tcpc = 0;
        int source_rc;
        int count;
        int request_ma = 0;
        int i;
        int rc;

        if (!get_tcpc || !source_cap || !request_addr) {
            snprintf(reply, sizeof(reply),
                     "error: PD request symbols unavailable");
            return copy_reply(out_msg, outlen, reply);
        }
        tcpc = ((void *(*)(const char *))get_tcpc)("type_c_port0");
        if (!tcpc) {
            snprintf(reply, sizeof(reply), "error: type_c_port0 unavailable");
            return copy_reply(out_msg, outlen, reply);
        }

        memset(caps, 0, sizeof(caps));
        source_rc = ((pd_source_cap_fn)source_cap)(tcpc, caps);
        count = caps[0] & 0xff;
        if (count > 32)
            count = 32;
        for (i = 0; i < count; i++) {
            unsigned int pdo = caps[i + 1];
            if (PD_PDO_TYPE(pdo) == 0 &&
                PD_PDO_VOLT_MV(pdo) == 9000 &&
                PD_PDO_CURR_MA(pdo) >= 2000) {
                request_ma = PD_PDO_CURR_MA(pdo);
                if (request_ma > 3000)
                    request_ma = 3000;
                break;
            }
        }

        if (!request_ma) {
            snprintf(reply, sizeof(reply),
                     "pd_request_9v=0 reason=not_advertised "
                     "source_rc=%d count=%d pdo1=0x%08x",
                     source_rc, count, caps[1]);
            return copy_reply(out_msg, outlen, reply);
        }

        g_force_aicr_ua = 0;
        rc = ((pd_request_fn)request_addr)(
            tcpc, 9000, request_ma, 0);
        pr_info("pftm10-kprobe request 9V/%dmA rc=%d tcpc=0x%lx\n",
                request_ma, rc, (unsigned long)tcpc);
        snprintf(reply, sizeof(reply),
                 "pd_request_9v=1 mv=9000 ma=%d rc=%d pdo=%d",
                 request_ma, rc, i + 1);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "pd-soft-reset")) {
        unsigned long get_tcpc =
            kallsyms_lookup_name("tcpc_dev_get_by_name");
        unsigned long reset_addr =
            kallsyms_lookup_name("tcpm_dpm_pd_soft_reset");
        void *tcpc = 0;
        int rc;

        if (!get_tcpc || !reset_addr) {
            snprintf(reply, sizeof(reply),
                     "error: PD soft-reset symbols unavailable");
            return copy_reply(out_msg, outlen, reply);
        }
        tcpc = ((void *(*)(const char *))get_tcpc)("type_c_port0");
        if (!tcpc) {
            snprintf(reply, sizeof(reply), "error: type_c_port0 unavailable");
            return copy_reply(out_msg, outlen, reply);
        }

        g_force_aicr_ua = 0;
        rc = ((pd_request_fn)reset_addr)(tcpc, 0, 0, 0);
        pr_info("pftm10-kprobe PD soft reset rc=%d tcpc=0x%lx\n",
                rc, (unsigned long)tcpc);
        snprintf(reply, sizeof(reply),
                 "pd_soft_reset=1 rc=%d tcpc=0x%lx",
                 rc, (unsigned long)tcpc);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "pd-force-9v")) {
        unsigned long get_tcpc =
            kallsyms_lookup_name("tcpc_dev_get_by_name");
        unsigned long request_addr =
            kallsyms_lookup_name("tcpm_dpm_pd_request");
        void *tcpc = 0;
        int rc;

        if (!get_tcpc || !request_addr) {
            snprintf(reply, sizeof(reply),
                     "error: PD request symbols unavailable");
            return copy_reply(out_msg, outlen, reply);
        }
        tcpc = ((void *(*)(const char *))get_tcpc)("type_c_port0");
        if (!tcpc) {
            snprintf(reply, sizeof(reply), "error: type_c_port0 unavailable");
            return copy_reply(out_msg, outlen, reply);
        }

        g_force_aicr_ua = 0;
        rc = ((pd_request_fn)request_addr)(tcpc, 9000, 2000, 0);
        pr_info("pftm10-kprobe forced 9V/2A request rc=%d tcpc=0x%lx\n",
                rc, (unsigned long)tcpc);
        snprintf(reply, sizeof(reply),
                 "pd_force_9v=1 mv=9000 ma=2000 rc=%d",
                 rc);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "hook-status")) {
        unsigned long tcpc = g_tcpc;
        unsigned int selected_pdo = tcpc_selected_pdo(tcpc);
        unsigned int selected_index = 0;
        unsigned int source_count = 0;

        if (tcpc) {
            source_count = *(volatile unsigned char *)(
                tcpc + TCPC_SOURCE_CAP_COUNT_OFFSET);
            selected_index = (*(volatile unsigned int *)(
                tcpc + TCPC_SELECTED_CAP_OFFSET) >> 28) & 0x7;
        }
        snprintf(reply, sizeof(reply),
                 "hook_installed=%d hook_err=%d force_ua=%d "
                 "rewrites=%lu tcpc=0x%lx source_count=%u "
                 "selected_index=%u selected_pdo=0x%08x "
                 "selected_mv=%d selected_ma=%d",
                 g_hook_err == HOOK_NO_ERR, g_hook_err,
                 g_force_aicr_ua, g_hook_rewrites, tcpc,
                 source_count, selected_index, selected_pdo,
                 PD_PDO_VOLT_MV(selected_pdo),
                 PD_PDO_CURR_MA(selected_pdo));
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "charger-500-direct")) {
        unsigned long chip;
        int aicl_rc = -1;
        int aicr_rc;
        int rc;

        g_force_aicr_ua = 0;
        rc = direct_set_aicr(500000, 1, &chip, &aicl_rc, &aicr_rc);
        pr_info("pftm10-kprobe charger 500 direct: "
                "aicl_rc=%d aicr_rc=%d chip=0x%lx fn=0x%lx\n",
                aicl_rc, aicr_rc, chip, g_mt6360_set_aicr_addr);
        snprintf(reply, sizeof(reply),
                 "charger_500_direct_applied=%d aicl_rc=%d aicr_rc=%d "
                 "chip=0x%lx fn=0x%lx",
                 rc == 0, aicl_rc, aicr_rc, chip, g_mt6360_set_aicr_addr);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "charger-3a")) {
        unsigned long chip;
        int voltage_mv = -1;
        int current_ma = -1;
        int contract_rc;
        int chging_before = -1;
        int chging_after = -1;
        int unsuspend_called;
        int stop_chg = -1;
        int aicl_rc = -1;
        int aicr_rc;
        int rc;

        contract_rc = pd_contract_status(&voltage_mv, &current_ma);
        if (contract_rc != 0 || voltage_mv < 4500 || voltage_mv > 5500 ||
            current_ma < 3000) {
            snprintf(reply, sizeof(reply),
                     "error: PD contract does not permit 5V/3A "
                     "rc=%d mv=%d ma=%d",
                     contract_rc, voltage_mv, current_ma);
            return copy_reply(out_msg, outlen, reply);
        }

        unsuspend_called =
            resume_charger_if_stopped(&chging_before, &chging_after);
        stop_chg = oplus_stop_charging_status();

        g_force_aicr_ua = 0;
        rc = direct_set_aicr(3000000, 0, &chip, &aicl_rc, &aicr_rc);
        if (rc != 0) {
            snprintf(reply, sizeof(reply),
                     "error: charger 3A direct failed rc=%d aicl_rc=%d "
                     "aicr_rc=%d chip=0x%lx fn=0x%lx "
                     "chging_before=%d unsuspend_called=%d "
                     "chging_after=%d stop_chg=%d",
                     rc, aicl_rc, aicr_rc, chip,
                     g_mt6360_set_aicr_addr, chging_before,
                     unsuspend_called, chging_after, stop_chg);
            return copy_reply(out_msg, outlen, reply);
        }

        g_force_aicr_ua = 3000000;
        pr_info("pftm10-kprobe charger 3A applied: "
                "contract=%dmV/%dmA aicl_rc=%d aicr_rc=%d "
                "chip=0x%lx fn=0x%lx hook=%d rewrites=%lu\n",
                voltage_mv, current_ma, aicl_rc, aicr_rc, chip,
                g_mt6360_set_aicr_addr, g_hook_err, g_hook_rewrites);
        snprintf(reply, sizeof(reply),
                 "charger_3a_applied=1 contract_mv=%d contract_ma=%d "
                 "request_ma=3000 aicl_rc=%d aicr_rc=%d chip=0x%lx "
                 "fn=0x%lx hook=%d rewrites=%lu "
                 "chging_before=%d unsuspend_called=%d chging_after=%d "
                 "stop_chg=%d unsuspend_calls=%lu",
                 voltage_mv, current_ma, aicl_rc, aicr_rc, chip,
                 g_mt6360_set_aicr_addr, g_hook_err, g_hook_rewrites,
                 chging_before, unsuspend_called, chging_after, stop_chg,
                 g_unsuspend_calls);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "charger-3a-direct")) {
        unsigned long chip;
        int voltage_mv = -1;
        int current_ma = -1;
        int contract_rc;
        int aicl_rc = -1;
        int aicr_rc;
        int rc;

        contract_rc = pd_contract_status(&voltage_mv, &current_ma);
        if (contract_rc != 0 || voltage_mv < 4500 || voltage_mv > 5500 ||
            current_ma < 3000) {
            snprintf(reply, sizeof(reply),
                     "error: PD contract does not permit 5V/3A "
                     "rc=%d mv=%d ma=%d",
                     contract_rc, voltage_mv, current_ma);
            return copy_reply(out_msg, outlen, reply);
        }

        g_force_aicr_ua = 0;
        rc = direct_set_aicr(3000000, 0, &chip, &aicl_rc, &aicr_rc);
        if (rc == 0)
            g_force_aicr_ua = 3000000;
        pr_info("pftm10-kprobe charger 3A direct: "
                "contract=%dmV/%dmA aicl_rc=%d aicr_rc=%d "
                "chip=0x%lx fn=0x%lx hook=%d\n",
                voltage_mv, current_ma, aicl_rc, aicr_rc,
                chip, g_mt6360_set_aicr_addr, g_hook_err);
        snprintf(reply, sizeof(reply),
                 "charger_3a_direct_applied=%d contract_mv=%d contract_ma=%d "
                 "aicl_rc=%d aicr_rc=%d chip=0x%lx fn=0x%lx",
                 aicr_rc == 0, voltage_mv, current_ma, aicl_rc, aicr_rc,
                 chip, g_mt6360_set_aicr_addr);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "charger-restore")) {
        unsigned long chip;
        int aicl_rc = -1;
        int aicr_rc;
        int rc;

        g_force_aicr_ua = 0;
        rc = direct_set_aicr(500000, 1, &chip, &aicl_rc, &aicr_rc);
        pr_info("pftm10-kprobe charger restore direct: "
                "aicl_rc=%d aicr_rc=%d chip=0x%lx fn=0x%lx\n",
                aicl_rc, aicr_rc, chip, g_mt6360_set_aicr_addr);
        snprintf(reply, sizeof(reply),
                 "charger_restore_applied=%d request_ma=500 "
                 "aicl_rc=%d aicr_rc=%d chip=0x%lx fn=0x%lx",
                 rc == 0, aicl_rc, aicr_rc, chip,
                 g_mt6360_set_aicr_addr);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "banner")) {
        const char *text;

        if (!init_task) {
            snprintf(reply, sizeof(reply), "error: symbol table unavailable");
            return copy_reply(out_msg, outlen, reply);
        }

        text = (const char *)kallsyms_lookup_name("linux_banner");
        if (!text) {
            snprintf(reply, sizeof(reply), "linux_banner=not_found");
            return copy_reply(out_msg, outlen, reply);
        }

        copy_kernel_string(reply, sizeof(reply), text);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "cmdline")) {
        const char *const *slot;
        const char *text;

        if (!init_task) {
            snprintf(reply, sizeof(reply), "error: symbol table unavailable");
            return copy_reply(out_msg, outlen, reply);
        }

        slot = (const char *const *)kallsyms_lookup_name("saved_command_line");
        if (!slot || !*slot) {
            snprintf(reply, sizeof(reply), "saved_command_line=not_found");
            return copy_reply(out_msg, outlen, reply);
        }

        text = *slot;
        copy_kernel_string(reply, sizeof(reply), text);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strncmp(cmd, "sym:", 4)) {
        const char *name = cmd + 4;
        unsigned long addr;

        if (!name[0]) {
            snprintf(reply, sizeof(reply), "error: empty symbol name");
            return copy_reply(out_msg, outlen, reply);
        }

        addr = kallsyms_lookup_name(name);
        if (!addr) {
            snprintf(reply, sizeof(reply), "symbol=%s address=not_found", name);
            return copy_reply(out_msg, outlen, reply);
        }

        snprintf(reply, sizeof(reply), "symbol=%s address=0x%lx", name, addr);
        pr_info("pftm10-kprobe symbol: %s -> 0x%lx\n", name, addr);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strncmp(cmd, "u32:", 4)) {
        const char *name = cmd + 4;
        unsigned long addr;
        unsigned int value;

        addr = name[0] ? kallsyms_lookup_name(name) : 0;
        if (!addr) {
            snprintf(reply, sizeof(reply), "symbol=%s address=not_found", name);
            return copy_reply(out_msg, outlen, reply);
        }

        value = *(volatile const unsigned int *)addr;
        snprintf(reply, sizeof(reply), "symbol=%s value=0x%x", name, value);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strncmp(cmd, "u64:", 4)) {
        const char *name = cmd + 4;
        unsigned long addr;
        unsigned long value;

        addr = name[0] ? kallsyms_lookup_name(name) : 0;
        if (!addr) {
            snprintf(reply, sizeof(reply), "symbol=%s address=not_found", name);
            return copy_reply(out_msg, outlen, reply);
        }

        value = *(volatile const unsigned long *)addr;
        snprintf(reply, sizeof(reply), "symbol=%s value=0x%lx", name, value);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strncmp(cmd, "bytes:", 6)) {
        const char *name = cmd + 6;
        const unsigned char *src;
        unsigned long addr;
        char encoded[64];

        addr = name[0] ? kallsyms_lookup_name(name) : 0;
        if (!addr) {
            snprintf(reply, sizeof(reply), "symbol=%s address=not_found", name);
            return copy_reply(out_msg, outlen, reply);
        }

        src = (const unsigned char *)addr;
        bytes_to_hex(encoded, sizeof(encoded), src, 16);
        snprintf(reply, sizeof(reply), "symbol=%s bytes=%s", name, encoded);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strncmp(cmd, "str:", 4)) {
        const char *name = cmd + 4;
        const char *src;
        unsigned long addr;
        char text[192];

        addr = name[0] ? kallsyms_lookup_name(name) : 0;
        if (!addr) {
            snprintf(reply, sizeof(reply), "symbol=%s address=not_found", name);
            return copy_reply(out_msg, outlen, reply);
        }

        src = (const char *)addr;
        copy_kernel_string(text, sizeof(text), src);
        snprintf(reply, sizeof(reply), "symbol=%s string=%s", name, text);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strncmp(cmd, "pstr:", 5)) {
        const char *name = cmd + 5;
        const char *const *slot;
        const char *src;
        unsigned long addr;
        char text[192];

        if (strcmp(name, "saved_command_line")) {
            snprintf(reply, sizeof(reply),
                     "error: pstr supports only saved_command_line");
            return copy_reply(out_msg, outlen, reply);
        }

        addr = name[0] ? kallsyms_lookup_name(name) : 0;
        if (!addr) {
            snprintf(reply, sizeof(reply), "symbol=%s address=not_found", name);
            return copy_reply(out_msg, outlen, reply);
        }

        slot = (const char *const *)addr;
        src = *slot;
        if (!src) {
            snprintf(reply, sizeof(reply), "symbol=%s pointer=null", name);
            return copy_reply(out_msg, outlen, reply);
        }

        copy_kernel_string(text, sizeof(text), src);
        snprintf(reply, sizeof(reply), "symbol=%s string=%s", name, text);
        return copy_reply(out_msg, outlen, reply);
    }

    snprintf(reply, sizeof(reply), "error: unknown command; use help");
    return copy_reply(out_msg, outlen, reply);
}

static long kprobe_exit(void *__user reserved)
{
    g_force_aicr_ua = 0;
    if (g_mt6360_set_aicr_addr && g_hook_err == HOOK_NO_ERR)
        hook_unwrap((void *)g_mt6360_set_aicr_addr,
                    before_mt6360_set_aicr, 0);
    pr_info("pftm10-kprobe exit\n");
    return 0;
}

KPM_INIT(kprobe_init);
KPM_CTL0(kprobe_control0);
KPM_EXIT(kprobe_exit);
