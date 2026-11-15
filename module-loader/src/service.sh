#!/system/bin/sh

MODDIR=${0%/*}

i=0
while [ "$(getprop sys.boot_completed)" != "1" ] && [ "$i" -lt 120 ]; do
    sleep 1
    i=$((i + 1))
done

exec "$MODDIR/bin/load-modules.sh" service
