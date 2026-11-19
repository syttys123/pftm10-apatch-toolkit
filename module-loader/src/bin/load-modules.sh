#!/system/bin/sh

set -u

MODDIR=${0%/*}
MODDIR=${MODDIR%/*}
STAGE=${1:-manual}
MODULE_DIR="$MODDIR/modules/4.19.191+"
STATE=/data/adb/pftm10-kernel-modules
LOG="$STATE/boot.log"
EXPECTED_NOV14='Sat Nov 14 21:43:18 CST 2026'
EXPECTED_NOV15='Sun Nov 15 17:28:38 CST 2026'
EXPECTED_NOV16='Mon Nov 16 08:35:50 CST 2026'
EXPECTED_NOV17='Tue Nov 17 13:24:56 CST 2026'
INSMOD=/system/bin/insmod
RESETPROP=/data/adb/ap/bin/resetprop

mkdir -p "$STATE"

log_line() {
    printf '%s\n' "$*" >> "$LOG"
}

set_state() {
    if [ -x "$RESETPROP" ]; then
        "$RESETPROP" -n pftm10.kernel_modules.state "$1"
    else
        setprop pftm10.kernel_modules.state "$1" 2>/dev/null
    fi
}

log_line "---"
log_line "event=$STAGE"
log_line "time_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null)"
log_line "kernel=$(uname -v 2>/dev/null)"
log_line "expected_kernels=nov14,nov15,nov16,nov17"

if ! grep -Fq "$EXPECTED_NOV14" /proc/version 2>/dev/null && \
   ! grep -Fq "$EXPECTED_NOV15" /proc/version 2>/dev/null && \
   ! grep -Fq "$EXPECTED_NOV16" /proc/version 2>/dev/null && \
   ! grep -Fq "$EXPECTED_NOV17" /proc/version 2>/dev/null; then
    log_line "result=skip-kernel-mismatch"
    set_state skipped
    exit 0
fi

if [ ! -x "$INSMOD" ]; then
    log_line "result=error-insmod-missing"
    set_state error
    exit 1
fi

load_module() {
    module_name=$1
    module_file=$2

    if grep -q "^${module_name} " /proc/modules 2>/dev/null; then
        log_line "load=$module_name result=already-loaded"
        return 0
    fi

    output=$("$INSMOD" "$MODULE_DIR/$module_file" 2>&1)
    rc=$?
    if [ "$rc" -eq 0 ]; then
        log_line "load=$module_name result=ok"
        return 0
    fi

    log_line "load=$module_name result=failed rc=$rc output=$output"
    return "$rc"
}

failed=0
load_module trace_mmstat trace_mmstat.ko || failed=1
load_module connfem connfem.ko || failed=1
load_module wmt_drv wmt_drv.ko || failed=1
load_module wmt_chrdev_wifi wmt_chrdev_wifi.ko || failed=1
load_module bt_drv_connac1x bt_drv_connac1x.ko || failed=1
load_module wlan_drv_gen4m wlan_drv_gen4m.ko || failed=1
load_module gps_drv gps_drv.ko || failed=1
load_module fmradio_drv_mt6631_6635 fmradio_drv_mt6631_6635.ko || failed=1
load_module fpsgo fpsgo.ko || failed=1
load_module met met.ko || failed=1

if [ "$failed" -eq 0 ]; then
    log_line "result=loaded"
    set_state loaded
    exit 0
fi

log_line "result=partial-failure"
set_state failed
exit 1
