#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
kernel="$root/third_party/FreeRTOS-Kernel"
port="$kernel/portable/ThirdParty/GCC/Posix"
bin=$(mktemp "${TMPDIR:-/tmp}/tp-freertos-slice.XXXXXX")
trap 'rm -f "$bin"' EXIT HUP INT TERM
cc -std=gnu11 -pthread -I"$root/examples/posix_slice" -I"$root/include" -I"$root/tests" \
  -I"$kernel/include" -I"$port" "$root/examples/posix_slice/main.c" \
  "$kernel/tasks.c" "$kernel/queue.c" "$kernel/list.c" \
  "$kernel/portable/MemMang/heap_4.c" "$port/port.c" "$port/utils/wait_for_event.c" \
  "$root/drivers/ads1220.c" "$root/drivers/adc_reader.c" "$root/app/adc_status.c" \
  "$root/protocol/modbus_rtu.c" -lm -o "$bin"
timeout 10 "$bin"
