#!/system/bin/sh

MODDIR=${0%/*}
STATE=/data/adb/pftm10-control
LOG="$STATE/cpu-max.log"
SENTINEL="$STATE/cpu-max.enabled"
LAST_STATE=
LAST_SAFETY=

mkdir -p "$STATE"

read_thermal_max() {
    thermal_max=0
    for temp_file in /sys/class/thermal/thermal_zone*/temp; do
        [ -r "$temp_file" ] || continue
        value=$(cat "$temp_file" 2>/dev/null)
        case "$value" in
            ''|*[!0-9]*) continue ;;
        esac
        [ "$value" -gt "$thermal_max" ] && thermal_max=$value
    done
    printf '%s\n' "$thermal_max"
}

while [ -f "$SENTINEL" ]; do
    thermal_max=$(read_thermal_max)
    battery_temp=$(cat /sys/class/power_supply/battery/temp 2>/dev/null)
    case "$battery_temp" in
        ''|*[!0-9-]*) battery_temp=0 ;;
    esac
    battery_capacity=$(cat /sys/class/power_supply/battery/capacity 2>/dev/null)
    case "$battery_capacity" in
        ''|*[!0-9]*) battery_capacity=100 ;;
    esac

    if [ "$thermal_max" -ge 70000 ] ||
       [ "$battery_temp" -ge 450 ] ||
       [ "$battery_capacity" -le 5 ]; then
        echo "$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null) rollback thermal_max=$thermal_max battery_temp=$battery_temp capacity=$battery_capacity" >> "$LOG"
        PFTM10_CPUMAX_SELF=1 "$MODDIR/pftm10ctl" cpu-max disable >> "$LOG" 2>&1
        exit 0
    fi

    if [ "$thermal_max" != "$LAST_SAFETY" ]; then
        echo "$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null) telemetry thermal_max=$thermal_max battery_temp=$battery_temp capacity=$battery_capacity" >> "$LOG"
        LAST_SAFETY=$thermal_max
    fi

    output=$("$MODDIR/pftm10ctl" cpu-max apply 2>&1)
    case "$output" in
        *cpu_max_enabled=1*)
            current_state=applied
            ;;
        *)
            current_state=waiting
            ;;
    esac

    if [ "$current_state" != "$LAST_STATE" ]; then
        echo "$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null) state=$current_state output=$output" >> "$LOG"
        LAST_STATE=$current_state
    fi

    sleep 5
done

echo "$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null) state=stopped" >> "$LOG"
