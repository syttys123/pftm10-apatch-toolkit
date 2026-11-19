#!/system/bin/sh

STATE=/data/adb/pftm10-control
AUDIT_LOG="$STATE/crash-audit.log"
PSTORE_HASH="$STATE/pstore.sha256"
PSTORE=/sys/fs/pstore/console-ramoops-0
MAX_LOG_BYTES=524288
audit_stage=${1:-manual}
case "$audit_stage" in
    post_fs_data|boot_completed|manual)
        ;;
    *)
        audit_stage=manual
        ;;
esac

mkdir -p "$STATE"
chmod 0700 "$STATE" 2>/dev/null
chmod 0600 "$AUDIT_LOG" 2>/dev/null

if [ -f "$AUDIT_LOG" ]; then
    audit_size=$(stat -c %s "$AUDIT_LOG" 2>/dev/null)
    if [ -n "$audit_size" ] && [ "$audit_size" -gt "$MAX_LOG_BYTES" ]; then
        mv -f "$AUDIT_LOG" "$AUDIT_LOG.1"
    fi
fi

bootreason=$(getprop ro.boot.bootreason)
history=$(getprop persist.sys.boot.reason.history | tr '\n' ';' | sed 's/;*$//')
history_panics=$(printf '%s\n' "$history" | grep -o 'kernel_panic' | wc -l | tr -d ' ')
boot_id=$(cat /proc/sys/kernel/random/boot_id 2>/dev/null)

pstore_state=missing
pstore_sha=unknown
pstore_size=unknown
pstore_mtime=unknown
pstore_change=none
pstore_verdict=unavailable

if [ -r "$PSTORE" ]; then
    pstore_state=present
    pstore_sha=$(sha256sum "$PSTORE" 2>/dev/null | cut -d ' ' -f 1)
    pstore_size=$(stat -c %s "$PSTORE" 2>/dev/null)
    pstore_mtime=$(stat -c %y "$PSTORE" 2>/dev/null)
    previous_sha=$(cat "$PSTORE_HASH" 2>/dev/null)

    if grep -aqE 'Kernel panic|Unable to handle kernel|kernel_panic' "$PSTORE"; then
        pstore_verdict=panic
    elif grep -aqE 'WARNING:' "$PSTORE"; then
        pstore_verdict=warning
    else
        pstore_verdict=other
    fi

    if [ "$pstore_sha" != "$previous_sha" ]; then
        if [ -n "$previous_sha" ]; then
            pstore_change=changed
        else
            pstore_change=baseline
        fi
        printf '%s\n' "$pstore_sha" > "$PSTORE_HASH.tmp"
        chmod 0600 "$PSTORE_HASH.tmp" 2>/dev/null
        mv -f "$PSTORE_HASH.tmp" "$PSTORE_HASH"
        chmod 0600 "$PSTORE_HASH" 2>/dev/null
    fi
fi

{
    echo "---"
    echo "event=boot_audit"
    echo "audit_stage=$audit_stage"
    echo "time_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null)"
    echo "boot_id=$boot_id"
    echo "uptime=$(cat /proc/uptime)"
    echo "kernel=$(uname -a 2>&1)"
    echo "bootreason=$bootreason"
    echo "bootreason_history=$history"
    echo "bootreason_history_panics=$history_panics"
    echo "pstore_state=$pstore_state"
    echo "pstore_change=$pstore_change"
    echo "pstore_size=$pstore_size"
    echo "pstore_mtime=$pstore_mtime"
    echo "pstore_sha256=$pstore_sha"
    echo "pstore_verdict=$pstore_verdict"
} >> "$AUDIT_LOG"

if [ "$pstore_change" != "none" ] && [ "$pstore_state" = "present" ]; then
    {
        echo "event=pstore_snapshot"
        echo "pstore_change=$pstore_change"
        echo "pstore_sha256=$pstore_sha"
        echo "pstore_verdict=$pstore_verdict"
        grep -aE \
            'Unable to handle kernel|Kernel panic|Internal error|BUG:|WARNING:|Oops:|Call trace:|pc :|lr :|PM: suspend|mt_scp_dvfs_state_dump|scp_pm_event' \
            "$PSTORE" 2>/dev/null | head -n 120
    } >> "$AUDIT_LOG"
fi

chmod 0600 "$AUDIT_LOG" 2>/dev/null
exit 0
