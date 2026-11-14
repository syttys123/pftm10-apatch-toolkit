#!/system/bin/sh

MODDIR=${0%/*}
STATE=/data/adb/pftm10-control
LOG="$STATE/boot.log"

mkdir -p "$STATE"
chmod 0755 "$MODDIR/bin/pftm10ctl" "$MODDIR/bin/pftm10-kpmctl" 2>/dev/null

i=0
while [ "$(getprop sys.boot_completed)" != "1" ] && [ "$i" -lt 60 ]; do
    sleep 2
    i=$((i + 1))
done

{
    echo "event=service"
    echo "time_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null)"
    echo "boot_completed=$(getprop sys.boot_completed)"
    echo "build=$(getprop ro.build.display.id)"
    echo "control_prop=$(getprop ro.pftm10.control)"
    echo "selinux=$(getenforce 2>&1)"
    echo "apd=$(/data/adb/apd -V 2>&1)"
    echo "kernel=$(uname -a 2>&1)"
} >> "$LOG"

"$MODDIR/bin/pftm10ctl" health >> "$LOG" 2>&1
echo "health_rc=$?" >> "$LOG"

chmod 0600 "$LOG"
