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

camera_fine_rc=99
camera_coarse_rc=99
oshare_revoke_rc=99
oshare_flags_rc=99
privacy_policy=disabled

if [ ! -f "$STATE/privacy.disabled" ]; then
    privacy_policy=enabled

    if pm path com.oplus.camera >/dev/null 2>&1; then
        /system/bin/appops set com.oplus.camera FINE_LOCATION ignore
        camera_fine_rc=$?
        /system/bin/appops set com.oplus.camera COARSE_LOCATION ignore
        camera_coarse_rc=$?
    fi

    if pm path com.coloros.oshare >/dev/null 2>&1; then
        pm revoke --user 0 com.coloros.oshare android.permission.ACCESS_BACKGROUND_LOCATION >/dev/null 2>&1
        oshare_revoke_rc=$?
        pm set-permission-flags --user 0 com.coloros.oshare android.permission.ACCESS_BACKGROUND_LOCATION user-fixed user-set >/dev/null 2>&1
        oshare_flags_rc=$?
    fi
else
    privacy_policy=disabled

    if pm path com.oplus.camera >/dev/null 2>&1; then
        /system/bin/appops set com.oplus.camera FINE_LOCATION allow
        camera_fine_rc=$?
        /system/bin/appops set com.oplus.camera COARSE_LOCATION allow
        camera_coarse_rc=$?
    fi

    if pm path com.coloros.oshare >/dev/null 2>&1; then
        pm clear-permission-flags --user 0 com.coloros.oshare android.permission.ACCESS_BACKGROUND_LOCATION user-fixed user-set >/dev/null 2>&1
        oshare_flags_rc=$?
        pm grant --user 0 com.coloros.oshare android.permission.ACCESS_BACKGROUND_LOCATION >/dev/null 2>&1
        oshare_revoke_rc=$?
    fi
fi

privacy_policy_rc=0
for rc in "$camera_fine_rc" "$camera_coarse_rc" "$oshare_revoke_rc" "$oshare_flags_rc"; do
    [ "$rc" -eq 0 ] || privacy_policy_rc=1
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
    echo "privacy_policy=$privacy_policy"
    echo "privacy_policy_rc=$privacy_policy_rc"
    echo "camera_fine_rc=$camera_fine_rc"
    echo "camera_coarse_rc=$camera_coarse_rc"
    echo "oshare_permission_rc=$oshare_revoke_rc"
    echo "oshare_flags_rc=$oshare_flags_rc"
} >> "$LOG"

"$MODDIR/bin/pftm10ctl" health >> "$LOG" 2>&1
echo "health_rc=$?" >> "$LOG"
"$MODDIR/bin/pftm10ctl" battery-restrict status >> "$LOG" 2>&1
echo "battery_restrict_status_rc=$?" >> "$LOG"

chmod 0600 "$LOG"
