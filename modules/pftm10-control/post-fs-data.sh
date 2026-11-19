#!/system/bin/sh

MODDIR=${0%/*}
STATE=/data/adb/pftm10-control
LOG="$STATE/boot.log"
VINTF_ORIG=/system/etc/vintf/compatibility_matrix.5.xml
VINTF_PATCHED="$MODDIR/vintf/compatibility_matrix.5.xml"
VINTF_ORIG_SHA=c3f5339cb6fe1c3fec3c80f34845abc80e460e75132e8c167b3ed4c2970895a2
VINTF_PATCHED_SHA=961a5ff2e4b833eb7c7ac229ab8547fae49b61c0856d0c56446b03f6a322576a

mkdir -p "$STATE"
chmod 0755 "$MODDIR/bin/pftm10ctl" "$MODDIR/bin/pftm10-kpmctl" \
    "$MODDIR/bin/pftm10-cpuholdctl" "$MODDIR/bin/cpu-max-watchdog.sh" \
    "$MODDIR/bin/boot-audit.sh" 2>/dev/null

boot_audit_output=$("$MODDIR/bin/boot-audit.sh" post_fs_data 2>&1)
boot_audit_rc=$?

vintf_debugfs_override=disabled
vintf_debugfs_override_rc=99
if [ ! -f "$STATE/vintf-debugfs-allow.disabled" ]; then
    vintf_debugfs_override=checking
    current_sha=$(sha256sum "$VINTF_ORIG" 2>/dev/null | cut -d ' ' -f 1)
    patched_sha=$(sha256sum "$VINTF_PATCHED" 2>/dev/null | cut -d ' ' -f 1)
    if [ "$current_sha" = "$VINTF_ORIG_SHA" ] && [ "$patched_sha" = "$VINTF_PATCHED_SHA" ]; then
        /system/bin/mount --bind "$VINTF_PATCHED" "$VINTF_ORIG"
        vintf_debugfs_override_rc=$?
        mounted_sha=$(sha256sum "$VINTF_ORIG" 2>/dev/null | cut -d ' ' -f 1)
        if [ "$vintf_debugfs_override_rc" -eq 0 ] && [ "$mounted_sha" = "$VINTF_PATCHED_SHA" ]; then
            vintf_debugfs_override=mounted
        else
            /system/bin/umount "$VINTF_ORIG" 2>/dev/null
            vintf_debugfs_override=failed
            [ "$vintf_debugfs_override_rc" -eq 0 ] && vintf_debugfs_override_rc=65
        fi
    elif [ "$current_sha" = "$VINTF_PATCHED_SHA" ]; then
        vintf_debugfs_override=already_mounted
        vintf_debugfs_override_rc=0
    else
        vintf_debugfs_override=unexpected_source
        vintf_debugfs_override_rc=66
    fi
fi

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
    echo "boot_audit_rc=$boot_audit_rc"
    printf '%s\n' "$boot_audit_output"
    echo "vintf_debugfs_override=$vintf_debugfs_override"
    echo "vintf_debugfs_override_rc=$vintf_debugfs_override_rc"
    echo "vintf_orig_sha=$current_sha"
    echo "vintf_patched_sha=$patched_sha"
    echo "battery_restrict_rc=$battery_restrict_rc"
    printf '%s\n' "$battery_restrict_output"
} >> "$LOG"

chmod 0600 "$LOG"
