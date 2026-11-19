#!/system/bin/sh

BASE=${1:?test directory required}
WATCHDOG="$BASE/watchdog.sh"
FAKE_CONTROL="$BASE/fake-pftm10ctl.sh"
FAILURES=0

run_case() {
    name=$1
    usb_online=$2
    usb_type=$3
    current_max=$4
    voltage_max=$5
    battery_status=$6
    battery_capacity=$7
    apply_result=$8
    expected_state=${9}
    expected_calls=${10}

    case_dir="$BASE/cases/$name"
    state_dir="$case_dir/state"
    usb_dir="$case_dir/usb"
    battery_dir="$case_dir/battery"
    call_log="$case_dir/calls.log"

    rm -rf "$case_dir"
    mkdir -p "$state_dir" "$usb_dir" "$battery_dir"
    printf '%s\n' "$usb_online" > "$usb_dir/online"
    printf '%s\n' "$usb_type" > "$usb_dir/type"
    printf '%s\n' "$current_max" > "$usb_dir/current_max"
    printf '%s\n' "$voltage_max" > "$usb_dir/voltage_max"
    printf '%s\n' "$battery_status" > "$battery_dir/status"
    printf '%s\n' "$battery_capacity" > "$battery_dir/capacity"
    : > "$state_dir/charge-3a.enabled"
    : > "$call_log"

    PFTM10_CONTROL_STATE="$state_dir" \
    PFTM10_USB_PS="$usb_dir" \
    PFTM10_BAT_PS="$battery_dir" \
    PFTM10_CONTROLCTL="$FAKE_CONTROL" \
    PFTM10_CHARGE_SLOW_INTERVAL=0 \
    PFTM10_CHARGE_FAST_INTERVAL=0 \
    PFTM10_CHARGE_MAX_ITERATIONS=1 \
    PFTM10_TEST_CALL_LOG="$call_log" \
    PFTM10_TEST_APPLY_RESULT="$apply_result" \
        /system/bin/sh "$WATCHDOG"

    if [ "$?" -ne 0 ]; then
        echo "CASE_FAIL name=$name reason=watchdog_exit"
        FAILURES=$((FAILURES + 1))
        return
    fi

    actual_state=$(sed -n 's/.*state=\([^ ]*\).*/\1/p' \
        "$state_dir/charge-3a.log" 2>/dev/null | tail -n 2 | head -n 1)
    actual_calls=$(wc -l < "$call_log" | tr -d ' ')

    if [ "$actual_state" != "$expected_state" ]; then
        echo "CASE_FAIL name=$name state=$actual_state expected=$expected_state"
        FAILURES=$((FAILURES + 1))
        return
    fi

    if [ "$actual_calls" != "$expected_calls" ]; then
        echo "CASE_FAIL name=$name calls=$actual_calls expected=$expected_calls"
        FAILURES=$((FAILURES + 1))
        return
    fi

    echo "CASE_OK name=$name state=$actual_state calls=$actual_calls"
}

run_case no_input 0 USB_CDP 500000 5000000 Charging 50 applied no_input 0
run_case no_pd 1 USB_CDP 500000 5000000 Charging 50 applied no_pd 0
run_case full 1 USB_PD 3000000 5000000 Full 100 applied full 0
run_case applied 1 USB_PD 3000000 5000000 Charging 50 applied applied 1
run_case pd_pending 1 USB_PD_DRP 500000 9000000 Charging 50 pending pd_pending 1

if [ "$FAILURES" -ne 0 ]; then
    echo "CHARGER_WATCHDOG_TEST_FAILED count=$FAILURES"
    exit 1
fi

echo "CHARGER_WATCHDOG_TEST_OK cases=5"
