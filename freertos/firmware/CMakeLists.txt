cmake_minimum_required(VERSION 3.20)
project(tp_f407_skeleton C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(KERNEL ${ROOT}/third_party/FreeRTOS-Kernel)
set(DEVICE ${ROOT}/third_party/STM32F4-CMSIS)
add_executable(tp_f407.elf main.c tp_f407_emergency.c tp_f407_i2c.c tp_f407_ads1220_spi.c tp_f407_usart3_rtu.c tp_f407_valves.c tp_f407_pump.c tp_f407_heater.c tp_f407_temperature.c
    ${DEVICE}/Source/startup_stm32f407xx.s ${DEVICE}/Source/system_stm32f4xx.c
    ${KERNEL}/tasks.c ${KERNEL}/queue.c ${KERNEL}/list.c ${KERNEL}/timers.c
    ${KERNEL}/portable/GCC/ARM_CM4F/port.c ${KERNEL}/portable/MemMang/heap_4.c
    ${ROOT}/os/freertos_runtime.c ${ROOT}/os/freertos_clock.c ${ROOT}/app/clock_cache.c ${ROOT}/app/valve_policy.c ${ROOT}/app/pulse_counter.c ${ROOT}/app/heater_duty.c ${ROOT}/app/controller.c ${ROOT}/app/measurement.c
    ${ROOT}/app/schedule.c ${ROOT}/app/service.c ${ROOT}/storage/config_store.c ${ROOT}/drivers/mb85rc16.c ${ROOT}/drivers/ds3231.c ${ROOT}/drivers/ntc.c ${ROOT}/drivers/ads1220.c ${ROOT}/drivers/adc_reader.c
    ${ROOT}/protocol/modbus_rtu.c ${ROOT}/protocol/rtu_receiver.c ${ROOT}/protocol/rtu_port.c)
target_include_directories(tp_f407.elf PRIVATE ${ROOT}/include ${ROOT}/config
    ${KERNEL}/include ${KERNEL}/portable/GCC/ARM_CM4F ${DEVICE}/Include
    ${ROOT}/third_party/CMSIS-Core/Include ${ROOT}/third_party/STM32F4-LL/Inc)
target_compile_definitions(tp_f407.elf PRIVATE STM32F407xx)
target_compile_options(tp_f407.elf PRIVATE -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16
    -mfloat-abi=hard -ffunction-sections -fdata-sections)
target_link_options(tp_f407.elf PRIVATE -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16
    -mfloat-abi=hard --specs=nosys.specs -T${CMAKE_CURRENT_LIST_DIR}/stm32f407vg.ld
    -Wl,--gc-sections -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/tp_f407.map)
target_link_libraries(tp_f407.elf PRIVATE m)
add_custom_command(TARGET tp_f407.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:tp_f407.elf> ${CMAKE_CURRENT_BINARY_DIR}/tp_f407.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:tp_f407.elf>)
