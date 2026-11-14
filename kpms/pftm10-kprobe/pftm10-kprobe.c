/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <compiler.h>
#include <kpmodule.h>
#include <common.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <kputils.h>

KPM_NAME("pftm10-kprobe");
KPM_VERSION("1.1.1");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("syttys123");
KPM_DESCRIPTION("Read-only PFTM10 kernel facts, symbol lookup, and bounded reads");

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
                 "u32:<name> | u64:<name> | bytes:<name> | str:<name> | pstr:<name> | help");
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
