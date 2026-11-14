#!/system/bin/sh

MODDIR=${0%/*}
STATE=/data/adb/pftm10-control
LOG="$STATE/boot.log"

mkdir -p "$STATE"
chmod 0755 "$MODDIR/bin/pftm10ctl" "$MODDIR/bin/pftm10-kpmctl" 2>/dev/null

battery_restrict_output=$("$MODDIR/bin/pftm10ctl" battery-restrict mask 2>&1)
battery_restrict_rc=$?

{
    echo "---"
    echo "event=post-fs-data"
    echo "time_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null)"
    echo "uptime=$(cat /proc/uptime)"
    echo "build=$(getprop ro.build.display.id)"
    echo "locked=$(getprop ro.boot.flash.locked)"
    echo "verified=$(getprop ro.boot.verifiedbootstate)"
    echo "control_prop=$(getprop ro.pftm10.control)"
    echo "battery_restrict_rc=$battery_restrict_rc"
    printf '%s\n' "$battery_restrict_output"
} >> "$LOG"

chmod 0600 "$LOG"
