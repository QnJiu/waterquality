#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
kernel="$root/third_party/FreeRTOS-Kernel"
port="$kernel/portable/ThirdParty/GCC/Posix"
bin=$(mktemp "${TMPDIR:-/tmp}/tp-freertos-control.XXXXXX")
trap 'rm -f "$bin"' EXIT HUP INT TERM
cc -std=gnu11 -pthread -I"$root/examples/posix_slice" -I"$root/include" \
  -I"$kernel/include" -I"$port" "$root/examples/posix_control/main.c" \
  "$kernel/tasks.c" "$kernel/queue.c" "$kernel/list.c" \
  "$kernel/portable/MemMang/heap_4.c" "$port/port.c" "$port/utils/wait_for_event.c" \
  "$root/os/freertos_runtime.c" "$root/app/controller.c" "$root/app/measurement.c" \
  "$root/app/auto_calibration.c" "$root/app/schedule.c" "$root/app/service.c" \
  "$root/app/preflight.c" "$root/app/event_transition.c" "$root/protocol/modbus_rtu.c" \
  "$root/protocol/rtu_port.c" "$root/protocol/rtu_receiver.c" \
  "$root/storage/config_store.c" "$root/storage/event_log.c" -lm -o "$bin"
timeout 10 "$bin"
