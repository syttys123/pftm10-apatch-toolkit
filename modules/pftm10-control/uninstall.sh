#!/system/bin/sh

STATE=/data/adb/pftm10-control
for pid_file in "$STATE/charge-3a.pid" "$STATE/cpu-max.pid"; do
    if [ -f "$pid_file" ]; then
        pid=$(cat "$pid_file" 2>/dev/null)
        [ -n "$pid" ] && kill "$pid" 2>/dev/null
    fi
done

rm -rf /data/adb/pftm10-control
