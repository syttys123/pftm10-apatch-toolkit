#!/system/bin/sh

MODDIR=${0%/*}
STATE=/data/adb/pftm10-control
LOG="$STATE/charge-3a.log"
SENTINEL="$STATE/charge-3a.enabled"
USB_PS=/sys/class/power_supply/usb
BAT_PS=/sys/class/power_supply/battery
SLOW_INTERVAL=30
FAST_INTERVAL=5
LAST_STATE=

mkdir -p "$STATE"

read_value() {
    if [ -r "$1" ]; then
        cat "$1" 2>/dev/null
    else
        echo unknown
    fi
}

while [ -f "$SENTINEL" ]; do
    usb_online=$(read_value "$USB_PS/online")
    usb_type=$(read_value "$USB_PS/type")
    usb_current_max=$(read_value "$USB_PS/current_max")
    usb_voltage_max=$(read_value "$USB_PS/voltage_max")
    battery_status=$(read_value "$BAT_PS/status")
    battery_capacity=$(read_value "$BAT_PS/capacity")
    output=none

    case "$usb_current_max" in
        ''|*[!0-9]*) usb_current_max=0 ;;
    esac
    case "$usb_voltage_max" in
        ''|*[!0-9]*) usb_voltage_max=0 ;;
    esac
    case "$battery_capacity" in
        ''|*[!0-9]*) battery_capacity=0 ;;
    esac

    pd_candidate=0
    case "$usb_type" in
        USB_PD|USB_PD_DRP) pd_candidate=1 ;;
    esac
    if [ "$usb_current_max" -ge 3000000 ] ||
        [ "$usb_voltage_max" -ge 9000000 ]; then
        pd_candidate=1
    fi

    if [ "$usb_online" != "1" ]; then
        current_state=no_input
        sleep_seconds=$SLOW_INTERVAL
    elif [ "$pd_candidate" != "1" ]; then
        current_state=no_pd
        sleep_seconds=$SLOW_INTERVAL
    elif [ "$battery_status" = "Full" ] &&
        [ "$battery_capacity" -ge 100 ]; then
        current_state=full
        sleep_seconds=$SLOW_INTERVAL
    else
        output=$("$MODDIR/pftm10ctl" charge-3a apply 2>&1)
        case "$output" in
            *charger_3a_applied=1*)
                current_state=applied
                ;;
            *)
                current_state=pd_pending
                ;;
        esac
        sleep_seconds=$FAST_INTERVAL
    fi

    if [ "$current_state" != "$LAST_STATE" ]; then
        echo "$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null) state=$current_state usb_online=$usb_online usb_type=$usb_type usb_current_max=$usb_current_max usb_voltage_max=$usb_voltage_max battery_status=$battery_status battery_capacity=$battery_capacity output=$output" >> "$LOG"
        LAST_STATE=$current_state
    fi

    sleep "$sleep_seconds"
done

echo "$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null) state=stopped" >> "$LOG"
