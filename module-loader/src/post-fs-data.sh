#!/system/bin/sh

MODDIR=${0%/*}
exec "$MODDIR/bin/load-modules.sh" post-fs-data
