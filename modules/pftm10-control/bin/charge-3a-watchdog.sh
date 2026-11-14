#!/system/bin/sh

MODDIR=${0%/*}
STATE=/data/adb/pftm10-control
LOG="$STATE/charge-3a.log"
SENTINEL="$STATE/charge-3a.enabled"
LAST_STATE=

mkdir -p "$STATE"

while [ -f "$SENTINEL" ]; do
    output=$("$MODDIR/pftm10ctl" charge-3a apply 2>&1)
    case "$output" in
        *charger_3a_applied=1*)
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
