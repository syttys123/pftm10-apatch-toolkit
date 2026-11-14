/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <compiler.h>
#include <kpmodule.h>
#include <common.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <kputils.h>

KPM_NAME("pftm10-kprobe");
KPM_VERSION("1.3.0");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("syttys123");
KPM_DESCRIPTION("PFTM10 probes with charger recovery and gated 5V/3A input control");

static const char *boot_args;

#define MT6360_SET_AICR_OFFSET 0x94dad8UL

typedef int (*pd_contract_fn)(void *tcpc, int *voltage_mv, int *current_ma);
typedef int (*aicl_enable_fn)(int enable);
typedef int (*mt6360_set_aicr_fn)(void *chip, int current_ua);

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

    return 0;
}

static long kprobe_control0(const char *args, char *__user out_msg, int outlen)
{
    char reply[320];
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
                 "pstr:<name> | charger-unsuspend | pd-contract | "
                 "charger-3a | charger-restore | help");
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

    if (!strcmp(cmd, "charger-3a")) {
        unsigned long text_addr;
        unsigned long set_aicr_addr;
        unsigned long aicl_addr;
        unsigned long unsuspend_addr;
        unsigned long chip;
        int voltage_mv = -1;
        int current_ma = -1;
        int contract_rc;
        int aicl_rc;
        int aicr_rc;

        contract_rc = pd_contract_status(&voltage_mv, &current_ma);
        if (contract_rc != 0 || voltage_mv < 4500 || voltage_mv > 5500 ||
            current_ma < 3000) {
            snprintf(reply, sizeof(reply),
                     "error: PD contract does not permit 5V/3A "
                     "rc=%d mv=%d ma=%d",
                     contract_rc, voltage_mv, current_ma);
            return copy_reply(out_msg, outlen, reply);
        }

        chip = mt6360_chip_address();
        set_aicr_addr = kallsyms_lookup_name("mt6360_set_aicr");
        if (!set_aicr_addr) {
            text_addr = kallsyms_lookup_name("_text");
            if (text_addr)
                set_aicr_addr = text_addr + MT6360_SET_AICR_OFFSET;
        }
        aicl_addr = kallsyms_lookup_name("mt6360_aicl_enable");
        unsuspend_addr = kallsyms_lookup_name("oplus_chg_unsuspend_charger");
        if (!chip || !set_aicr_addr || !aicl_addr || !unsuspend_addr) {
            snprintf(reply, sizeof(reply),
                     "error: charger controls unavailable "
                     "chip=0x%lx aicr=0x%lx aicl=0x%lx unsuspend=0x%lx",
                     chip, set_aicr_addr, aicl_addr, unsuspend_addr);
            return copy_reply(out_msg, outlen, reply);
        }

        aicl_rc = ((aicl_enable_fn)aicl_addr)(0);
        ((void (*)(void))unsuspend_addr)();
        aicr_rc = ((mt6360_set_aicr_fn)set_aicr_addr)(
            (void *)chip, 3000000);
        pr_info("pftm10-kprobe charger 3A: contract=%dmV/%dmA "
                "aicl_rc=%d aicr_rc=%d chip=0x%lx\n",
                voltage_mv, current_ma, aicl_rc, aicr_rc, chip);
        snprintf(reply, sizeof(reply),
                 "charger_3a_applied=1 contract_mv=%d contract_ma=%d "
                 "request_ma=3000 aicl_rc=%d aicr_rc=%d chip=0x%lx",
                 voltage_mv, current_ma, aicl_rc, aicr_rc, chip);
        return copy_reply(out_msg, outlen, reply);
    }

    if (!strcmp(cmd, "charger-restore")) {
        unsigned long text_addr;
        unsigned long set_aicr_addr;
        unsigned long aicl_addr;
        unsigned long chip;
        int aicl_rc;
        int aicr_rc;

        chip = mt6360_chip_address();
        set_aicr_addr = kallsyms_lookup_name("mt6360_set_aicr");
        if (!set_aicr_addr) {
            text_addr = kallsyms_lookup_name("_text");
            if (text_addr)
                set_aicr_addr = text_addr + MT6360_SET_AICR_OFFSET;
        }
        aicl_addr = kallsyms_lookup_name("mt6360_aicl_enable");
        if (!chip || !set_aicr_addr || !aicl_addr) {
            snprintf(reply, sizeof(reply),
                     "error: charger restore controls unavailable "
                     "chip=0x%lx aicr=0x%lx aicl=0x%lx",
                     chip, set_aicr_addr, aicl_addr);
            return copy_reply(out_msg, outlen, reply);
        }

        aicl_rc = ((aicl_enable_fn)aicl_addr)(1);
        aicr_rc = ((mt6360_set_aicr_fn)set_aicr_addr)(
            (void *)chip, 500000);
        pr_info("pftm10-kprobe charger restore: aicl_rc=%d aicr_rc=%d "
                "chip=0x%lx\n", aicl_rc, aicr_rc, chip);
        snprintf(reply, sizeof(reply),
                 "charger_restore_applied=1 request_ma=500 "
                 "aicl_rc=%d aicr_rc=%d chip=0x%lx",
                 aicl_rc, aicr_rc, chip);
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
    pr_info("pftm10-kprobe exit\n");
    return 0;
}

KPM_INIT(kprobe_init);
KPM_CTL0(kprobe_control0);
KPM_EXIT(kprobe_exit);
