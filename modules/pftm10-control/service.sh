#!/system/bin/sh

MODDIR=${0%/*}
STATE=/data/adb/pftm10-control
LOG="$STATE/boot.log"

mkdir -p "$STATE"
chmod 0755 "$MODDIR/bin/pftm10ctl" "$MODDIR/bin/pftm10-kpmctl" \
    "$MODDIR/bin/pftm10-cpuholdctl" "$MODDIR/bin/cpu-max-watchdog.sh" \
    "$MODDIR/bin/charge-3a-watchdog.sh" "$MODDIR/bin/boot-audit.sh" 2>/dev/null

i=0
while [ "$(getprop sys.boot_completed)" != "1" ] && [ "$i" -lt 60 ]; do
    sleep 2
    i=$((i + 1))
done

boot_audit_output=$("$MODDIR/bin/boot-audit.sh" boot_completed 2>&1)
boot_audit_rc=$?

camera_fine_rc=99
camera_coarse_rc=99
oshare_revoke_rc=99
oshare_flags_rc=99
privacy_policy=disabled
charger_unsuspend_output="not_run"
charger_unsuspend_rc=99
charger_3a=disabled
charger_3a_output="not_run"
charger_3a_rc=99
charger_3a_watchdog_pid=none
soc_reconcile_output="not_run"
soc_reconcile_rc=99
cpu_max=disabled
cpu_max_output="not_run"
cpu_max_rc=99

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

charger_unsuspend_output=$("$MODDIR/bin/pftm10ctl" kprobe-ctl charger-unsuspend 2>&1)
charger_unsuspend_rc=$?

if [ -f "$STATE/cpu-max.enabled" ]; then
    cpu_max=enabled
    cpu_max_output=$("$MODDIR/bin/pftm10ctl" cpu-max replay 2>&1)
    cpu_max_rc=$?
    case "$cpu_max_output" in
        *cpu_max=armed*)
            [ "$cpu_max_rc" -eq 0 ] || cpu_max=retry_pending
            ;;
        *) cpu_max=enable_failed ;;
    esac
fi

if [ -f "$STATE/charge-3a.enabled" ]; then
    if [ -f "$STATE/charge-3a.pid" ]; then
        old_pid=$(cat "$STATE/charge-3a.pid" 2>/dev/null)
        if [ -n "$old_pid" ]; then
            kill "$old_pid" 2>/dev/null
        fi
        rm -f "$STATE/charge-3a.pid"
    fi

    charger_3a=enabled
    charger_3a_rc=1
    usb_online=$(cat /sys/class/power_supply/usb/online 2>/dev/null)
    usb_type=$(cat /sys/class/power_supply/usb/type 2>/dev/null)
    usb_current_max=$(cat /sys/class/power_supply/usb/current_max 2>/dev/null)
    usb_voltage_max=$(cat /sys/class/power_supply/usb/voltage_max 2>/dev/null)
    case "$usb_current_max" in
        ''|*[!0-9]*) usb_current_max=0 ;;
    esac
    case "$usb_voltage_max" in
        ''|*[!0-9]*) usb_voltage_max=0 ;;
    esac
    pd_candidate=0
    case "$usb_type" in
        USB_PD|USB_PD_DRP) pd_candidate=1 ;;
    esac
    if [ "$usb_current_max" -ge 3000000 ] ||
        [ "$usb_voltage_max" -ge 9000000 ]; then
        pd_candidate=1
    fi
    if [ "$usb_online" = "1" ] && [ "$pd_candidate" = "1" ]; then
        i=0
        while [ "$i" -lt 15 ]; do
            charger_3a_output=$("$MODDIR/bin/pftm10ctl" charge-3a apply 2>&1)
            case "$charger_3a_output" in
                *charger_3a_applied=1*)
                    charger_3a_rc=0
                    break
                    ;;
            esac
            sleep 2
            i=$((i + 1))
        done
    else
        charger_3a_output="deferred: usb_online=$usb_online usb_type=$usb_type usb_current_max=$usb_current_max usb_voltage_max=$usb_voltage_max"
    fi

    setsid /system/bin/sh "$MODDIR/bin/charge-3a-watchdog.sh" \
        >/dev/null 2>&1 &
    charger_3a_watchdog_pid=$!
    echo "$charger_3a_watchdog_pid" > "$STATE/charge-3a.pid"
fi

soc_reconcile_rc=1
i=0
while [ "$i" -lt 6 ]; do
    soc_reconcile_output=$("$MODDIR/bin/pftm10ctl" kprobe-ctl soc-reconcile 2>&1)
    command_rc=$?
    if [ "$command_rc" -eq 0 ]; then
        case "$soc_reconcile_output" in
            *soc_reconcile_applied=1*|*reason=already_aligned*)
                soc_reconcile_rc=0
                break
                ;;
        esac
    fi
    sleep 5
    i=$((i + 1))
done

{
    echo "event=service"
    echo "time_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null)"
    echo "boot_completed=$(getprop sys.boot_completed)"
    echo "build=$(getprop ro.build.display.id)"
    echo "control_prop=$(getprop ro.pftm10.control)"
    echo "boot_audit_rc=$boot_audit_rc"
    printf '%s\n' "$boot_audit_output"
    echo "selinux=$(getenforce 2>&1)"
    echo "apd=$(/data/adb/apd -V 2>&1)"
    echo "kernel=$(uname -a 2>&1)"
    echo "privacy_policy=$privacy_policy"
    echo "privacy_policy_rc=$privacy_policy_rc"
    echo "camera_fine_rc=$camera_fine_rc"
    echo "camera_coarse_rc=$camera_coarse_rc"
    echo "oshare_permission_rc=$oshare_revoke_rc"
    echo "oshare_flags_rc=$oshare_flags_rc"
    echo "charger_unsuspend_rc=$charger_unsuspend_rc"
    echo "charger_unsuspend=$charger_unsuspend_output"
    echo "charger_3a=$charger_3a"
    echo "charger_3a_rc=$charger_3a_rc"
    echo "charger_3a_output=$charger_3a_output"
    echo "charger_3a_watchdog_pid=$charger_3a_watchdog_pid"
    echo "cpu_max=$cpu_max"
    echo "cpu_max_rc=$cpu_max_rc"
    echo "cpu_max_output=$cpu_max_output"
    echo "soc_reconcile_rc=$soc_reconcile_rc"
    echo "soc_reconcile=$soc_reconcile_output"
} >> "$LOG"

"$MODDIR/bin/pftm10ctl" health >> "$LOG" 2>&1
echo "health_rc=$?" >> "$LOG"
"$MODDIR/bin/pftm10ctl" battery-restrict status >> "$LOG" 2>&1
echo "battery_restrict_status_rc=$?" >> "$LOG"

chmod 0600 "$LOG"
