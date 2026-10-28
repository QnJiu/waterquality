cmake_minimum_required(VERSION 3.20)
project(tp_f030_skeleton C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(KERNEL ${ROOT}/third_party/FreeRTOS-Kernel)
set(DEVICE ${ROOT}/third_party/STM32F0-CMSIS)
option(TP_F030_SUBORDINATE "Build the subordinate reference entry" OFF)
option(TP_F030_IDLE_SLEEP "Use shallow CPU sleep in the FreeRTOS idle hook" OFF)
option(TP_F030_REFERENCE_INPUTS "Use reference GPIO and EXTI inputs" OFF)
option(TP_F030_WAKE_INPUT "Use PA8 as the active-high master wake input" OFF)
if(TP_F030_WAKE_INPUT AND NOT TP_F030_SUBORDINATE)
    message(FATAL_ERROR "TP_F030_WAKE_INPUT requires TP_F030_SUBORDINATE")
endif()
set(TP_F030_INPUT_CONFIG "" CACHE FILEPATH "Board input callback implementation")
set(TP_F030_DOOR_CONFIG "" CACHE FILEPATH "Board PN532 and door configuration implementation")
if(TP_F030_SUBORDINATE)
    set(ENTRY main_subordinate.c tp_f030_subordinate.c tp_f030_i2c.c tp_f030_reference_lock.c tp_f030_reference_pn532_reset.c
        ${ROOT}/app/subordinate_app.c ${ROOT}/app/subordinate_node.c
        ${ROOT}/app/door.c ${ROOT}/app/door_service.c ${ROOT}/app/door_allowlist.c
        ${ROOT}/drivers/pn532.c ${ROOT}/drivers/pn532_scan.c ${ROOT}/drivers/pn532_i2c.c
        ${ROOT}/protocol/rtu_receiver.c ${ROOT}/protocol/subordinate.c
        ${ROOT}/protocol/subordinate_poll.c ${ROOT}/protocol/subordinate_link.c)
    if(TP_F030_DOOR_CONFIG)
        list(APPEND ENTRY tp_f030_pn532_door.c "${TP_F030_DOOR_CONFIG}")
    endif()
    if(TP_F030_REFERENCE_INPUTS AND TP_F030_INPUT_CONFIG)
        message(FATAL_ERROR "Choose reference inputs or a custom input callback, not both")
    elseif(TP_F030_REFERENCE_INPUTS)
        list(APPEND ENTRY tp_f030_reference_inputs.c)
    elseif(TP_F030_INPUT_CONFIG)
        list(APPEND ENTRY "${TP_F030_INPUT_CONFIG}")
    else()
        list(APPEND ENTRY tp_f030_unconfigured.c)
    endif()
else()
    set(ENTRY main.c)
endif()
add_executable(tp_f030.elf ${ENTRY} tp_f030_rtc.c tp_f030_rtc_time.c
    ${DEVICE}/Source/startup_stm32f030x8.s ${DEVICE}/Source/system_stm32f0xx.c
    ${KERNEL}/tasks.c ${KERNEL}/queue.c ${KERNEL}/list.c
    ${KERNEL}/portable/GCC/ARM_CM0/port.c ${KERNEL}/portable/MemMang/heap_4.c)
target_include_directories(tp_f030.elf PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${ROOT}/include
    ${KERNEL}/include ${KERNEL}/portable/GCC/ARM_CM0 ${DEVICE}/Include
    ${ROOT}/third_party/CMSIS-Core/Include)
target_compile_definitions(tp_f030.elf PRIVATE STM32F030x8)
if(TP_F030_IDLE_SLEEP)
    target_sources(tp_f030.elf PRIVATE tp_f030_idle.c)
    target_compile_definitions(tp_f030.elf PRIVATE TP_F030_IDLE_SLEEP)
endif()
if(TP_F030_WAKE_INPUT)
    target_sources(tp_f030.elf PRIVATE tp_f030_wake_input.c)
    target_compile_definitions(tp_f030.elf PRIVATE TP_F030_WAKE_INPUT)
endif()
if(TP_F030_SUBORDINATE AND TP_F030_REFERENCE_INPUTS)
    target_compile_definitions(tp_f030.elf PRIVATE TP_F030_REFERENCE_INPUTS)
endif()
if(TP_F030_SUBORDINATE AND TP_F030_DOOR_CONFIG)
    target_compile_definitions(tp_f030.elf PRIVATE TP_F030_DOOR_CONFIG)
endif()
target_compile_options(tp_f030.elf PRIVATE -mcpu=cortex-m0 -mthumb -Os -ffunction-sections -fdata-sections)
target_link_options(tp_f030.elf PRIVATE -mcpu=cortex-m0 -mthumb --specs=nosys.specs
    -T${CMAKE_CURRENT_LIST_DIR}/stm32f030c8.ld -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/tp_f030.map)
add_custom_command(TARGET tp_f030.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:tp_f030.elf> ${CMAKE_CURRENT_BINARY_DIR}/tp_f030.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:tp_f030.elf>)
