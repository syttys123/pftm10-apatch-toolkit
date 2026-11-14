#!/system/bin/sh

MODDIR=${0%/*}
STATE=/data/adb/pftm10-control
LOG="$STATE/boot.log"

mkdir -p "$STATE"
chmod 0755 "$MODDIR/bin/pftm10ctl" 2>/dev/null

{
    echo "---"
    echo "event=post-fs-data"
    echo "time_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null)"
    echo "uptime=$(cat /proc/uptime)"
    echo "build=$(getprop ro.build.display.id)"
    echo "locked=$(getprop ro.boot.flash.locked)"
    echo "verified=$(getprop ro.boot.verifiedbootstate)"
    echo "control_prop=$(getprop ro.pftm10.control)"
} >> "$LOG"

chmod 0600 "$LOG"
