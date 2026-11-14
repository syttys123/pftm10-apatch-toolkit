#define _GNU_SOURCE

#include <errno.h>
#include <grp.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SUPERCALL_NR 45L
#define KP_VERSION_CODE 0x0d09L
#define KP_COMMAND_MAGIC 0x1158L
#define SUPERCALL_KPM_CONTROL 0x1022L
#define KPM_NAME "pftm10-kprobe"
#define MAX_CONTROL_ARGS 2048
#define MAX_REPLY 4096

static long version_and_cmd(long command)
{
    return (KP_VERSION_CODE << 32) | (KP_COMMAND_MAGIC << 16) | (command & 0xffff);
}

static int apatch_manager_uid(void)
{
    struct stat info;

    if (stat("/data/user/0/me.bmax.apatch", &info) == 0)
        return (int)info.st_uid;
    if (stat("/data/data/me.bmax.apatch", &info) == 0)
        return (int)info.st_uid;
    return -1;
}

static int join_control_args(int argc, char **argv, char *out, size_t outlen)
{
    size_t used = 0;
    int i;

    if (!out || outlen < 2)
        return -1;

    out[0] = '\0';
    for (i = 1; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (len == 0)
            continue;
        if (used && out[used - 1] != ' ') {
            if (used + 1 >= outlen)
                return -1;
            out[used++] = ' ';
        }
        if (used + len >= outlen)
            return -1;
        memcpy(out + used, argv[i], len);
        used += len;
        out[used] = '\0';
    }

    return used > 0 ? 0 : -1;
}

int main(int argc, char **argv)
{
    char control_args[MAX_CONTROL_ARGS];
    char reply[MAX_REPLY];
    int manager_uid;
    long rc;

    if (getuid() != 0) {
        fprintf(stderr, "root required\n");
        return 1;
    }
    if (argc < 2) {
        fprintf(stderr, "usage: pftm10-kpmctl <control-args>\n");
        return 2;
    }
    if (join_control_args(argc, argv, control_args, sizeof(control_args)) != 0) {
        fprintf(stderr, "invalid or too long control arguments\n");
        return 2;
    }

    manager_uid = apatch_manager_uid();
    if (manager_uid < 0) {
        fprintf(stderr, "APatch manager data directory not found\n");
        return 1;
    }

    if (setgroups(0, NULL) != 0 || setgid((gid_t)manager_uid) != 0 ||
        setuid((uid_t)manager_uid) != 0) {
        fprintf(stderr, "drop to APatch manager uid failed: %s\n", strerror(errno));
        return 1;
    }

    memset(reply, 0, sizeof(reply));
    rc = syscall(SUPERCALL_NR, "su", version_and_cmd(SUPERCALL_KPM_CONTROL),
                 KPM_NAME, control_args, reply, sizeof(reply));
    if (rc != 0) {
        fprintf(stderr, "KPM control failed: rc=%ld\n", rc);
        return 1;
    }

    reply[sizeof(reply) - 1] = '\0';
    fputs(reply, stdout);
    if (reply[0] && reply[strlen(reply) - 1] != '\n')
        fputc('\n', stdout);
    return 0;
}
