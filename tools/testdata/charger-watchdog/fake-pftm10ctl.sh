#!/system/bin/sh

echo "$*" >> "$PFTM10_TEST_CALL_LOG"

case "${PFTM10_TEST_APPLY_RESULT:-applied}" in
    applied)
        echo "charger_3a_applied=1"
        exit 0
        ;;
    pending)
        echo "error: synthetic PD contract pending"
        exit 1
        ;;
    *)
        echo "error: unknown synthetic result"
        exit 2
        ;;
esac
