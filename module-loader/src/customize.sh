#!/system/bin/sh

ui_print "- Setting executable module entry points"
set_perm "$MODPATH/post-fs-data.sh" 0 0 0755
set_perm "$MODPATH/service.sh" 0 0 0755
set_perm "$MODPATH/bin/load-modules.sh" 0 0 0755
