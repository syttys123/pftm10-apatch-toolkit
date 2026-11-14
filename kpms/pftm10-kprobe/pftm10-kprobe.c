/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <compiler.h>
#include <kpmodule.h>
#include <common.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <kputils.h>

KPM_NAME("pftm10-kprobe");
KPM_VERSION("1.0.0");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("syttys123");
KPM_DESCRIPTION("Read-only PFTM10 kernel facts and symbol lookup");

static const char *boot_args;

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

static long kprobe_init(const char *args, const char *event, void *__user reserved)
{
    boot_args = args;
    pr_info("pftm10-kprobe init, event: %s, args: %s, kpver: %x\n",
            event ? event : "(null)", args ? args : "(null)", kpver);
    return 0;
}

static long kprobe_control0(const char *args, char *__user out_msg, int outlen)
{
    char reply[320];
    unsigned long init_task;
    const char *cmd = (args && args[0]) ? args : "info";

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
                 "commands: info | sym:<kernel_symbol> | help");
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
