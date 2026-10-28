cmake_minimum_required(VERSION 3.20)
project(tp_f407_skeleton C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(KERNEL ${ROOT}/third_party/FreeRTOS-Kernel)
set(DEVICE ${ROOT}/third_party/STM32F4-CMSIS)
set(TP_F407_IMAGE_SLOT "standalone" CACHE STRING "Link for standalone flash, slot 0, or slot 1")
set_property(CACHE TP_F407_IMAGE_SLOT PROPERTY STRINGS standalone 0 1)
if(TP_F407_IMAGE_SLOT STREQUAL "standalone")
    set(TP_FLASH_ORIGIN 0x08000000)
    set(TP_FLASH_LENGTH 0x100000)
elseif(TP_F407_IMAGE_SLOT STREQUAL "0")
    set(TP_FLASH_ORIGIN 0x08020000)
    set(TP_FLASH_LENGTH 0x60000)
elseif(TP_F407_IMAGE_SLOT STREQUAL "1")
    set(TP_FLASH_ORIGIN 0x08080000)
    set(TP_FLASH_LENGTH 0x60000)
else()
    message(FATAL_ERROR "TP_F407_IMAGE_SLOT must be standalone, 0, or 1")
endif()
# Rename only the vendor SystemInit; the wrapper sets VTOR to this image.
set_source_files_properties(${DEVICE}/Source/system_stm32f4xx.c PROPERTIES COMPILE_DEFINITIONS SystemInit=tp_vendor_system_init)
option(TP_USE_FREEMODBUS "Use serialized FreeModbus RTU in the control task" OFF)
option(TP_F407_BUSINESS "Build the configured business entry point" OFF)
option(TP_F407_DIAGNOSTICS "Collect task stack and heap measurements for debugging" OFF)
if(TP_F407_DIAGNOSTICS AND NOT TP_F407_BUSINESS)
    message(FATAL_ERROR "TP_F407_DIAGNOSTICS requires TP_F407_BUSINESS")
endif()
option(TP_F407_TEACHING_SUBORDINATE "Enable F030-backed teaching configuration" OFF)
option(TP_F407_TEACHING_STATUS_INPUT "Read the F030 status line on PC12" OFF)
option(TP_F407_TEACHING_WAKE "Enable the teaching F030 wake line" OFF)
option(TP_F407_TEACHING_OTA "Enable the teaching upgrade assembly" OFF)
set(TP_F407_TEACHING_IDENTITY_SOURCE "" CACHE FILEPATH "Trusted product identity source")
set(TP_F407_BOARD_CONFIG "" CACHE FILEPATH "Board configuration implementation")
if(TP_F407_BUSINESS)
    set(ENTRY main_business.c)
    if(TP_F407_BOARD_CONFIG)
        list(APPEND ENTRY "${TP_F407_BOARD_CONFIG}")
    else()
        list(APPEND ENTRY tp_f407_unconfigured.c)
    endif()
else()
    set(ENTRY main.c)
endif()
add_executable(tp_f407.elf ${ENTRY} tp_f407_system_init.c tp_f407_handoff.c tp_f407_iwdg.c tp_f407_emergency.c tp_f407_app.c tp_f407_recipe.c tp_f407_i2c.c tp_f407_ads1220_spi.c tp_f407_w25q128_spi.c tp_f407_timebase.c tp_f407_usart3_rtu.c tp_f407_uart4_subordinate.c tp_f407_upgrade_uart.c tp_f407_stage_uart.c tp_f407_stage_reference.c tp_f407_upgrade_reference.c tp_f407_ota_reference.c tp_f407_upgrade_service.c tp_f407_stage_service.c tp_f407_upgrade.c tp_f407_image_flash.c tp_f407_flash_controller.c tp_f407_valves.c tp_f407_pump.c tp_f407_heater.c tp_f407_temperature.c
    ${DEVICE}/Source/startup_stm32f407xx.s ${DEVICE}/Source/system_stm32f4xx.c
    ${KERNEL}/tasks.c ${KERNEL}/queue.c ${KERNEL}/list.c ${KERNEL}/timers.c
    ${KERNEL}/portable/GCC/ARM_CM4F/port.c ${KERNEL}/portable/MemMang/heap_4.c
    ${ROOT}/os/freertos_stage_transport.c ${ROOT}/os/freertos_trial.c ${ROOT}/os/freertos_health.c ${ROOT}/os/freertos_runtime.c ${ROOT}/os/freertos_event_log.c ${ROOT}/os/freertos_clock.c ${ROOT}/os/freertos_flash_clock.c ${ROOT}/os/freertos_subordinate.c ${ROOT}/app/preflight.c ${ROOT}/app/health.c ${ROOT}/app/event_transition.c ${ROOT}/app/clock_cache.c ${ROOT}/app/valve_policy.c ${ROOT}/app/pulse_counter.c ${ROOT}/app/heater_duty.c ${ROOT}/app/controller.c ${ROOT}/app/auto_calibration.c ${ROOT}/app/measurement.c
    ${ROOT}/app/schedule.c ${ROOT}/app/service.c ${ROOT}/app/door.c ${ROOT}/app/door_service.c ${ROOT}/storage/config_store.c ${ROOT}/storage/event_log.c ${ROOT}/drivers/mb85rc16.c ${ROOT}/drivers/m24c02.c ${ROOT}/drivers/m24c02_store.c ${ROOT}/drivers/upgrade_fram.c ${ROOT}/drivers/ds3231.c ${ROOT}/drivers/ntc.c ${ROOT}/drivers/ads1220.c ${ROOT}/drivers/w25q128.c ${ROOT}/drivers/w25q128_image.c ${ROOT}/drivers/adc_reader.c ${ROOT}/drivers/pn532.c ${ROOT}/drivers/pn532_scan.c
    ${ROOT}/bootloader/boot.c ${ROOT}/bootloader/boot_target.c ${ROOT}/bootloader/download.c ${ROOT}/bootloader/staged_copy.c ${ROOT}/bootloader/stage_session.c ${ROOT}/bootloader/stage_commands.c ${ROOT}/bootloader/stage_receiver.c ${ROOT}/bootloader/staged_upgrade.c ${ROOT}/bootloader/staged_guard.c ${ROOT}/bootloader/signed_staged_source.c ${ROOT}/bootloader/upgrade.c ${ROOT}/bootloader/upgrade_commands.c ${ROOT}/bootloader/upgrade_wire.c ${ROOT}/bootloader/upgrade_stream.c ${ROOT}/bootloader/image_hash.c ${ROOT}/bootloader/signed_manifest.c ${ROOT}/bootloader/signed_boot.c ${ROOT}/bootloader/factory_provision.c ${ROOT}/bootloader/trial_confirm.c ${ROOT}/bootloader/secure_upgrade.c ${ROOT}/third_party/tinycrypt/source/ecc.c ${ROOT}/third_party/tinycrypt/source/ecc_dsa.c ${ROOT}/third_party/tinycrypt/source/sha256.c ${ROOT}/third_party/tinycrypt/source/utils.c
    ${ROOT}/protocol/modbus_rtu.c ${ROOT}/protocol/rtu_receiver.c ${ROOT}/protocol/rtu_port.c ${ROOT}/protocol/subordinate.c ${ROOT}/protocol/subordinate_poll.c ${ROOT}/protocol/subordinate_link.c)
target_include_directories(tp_f407.elf PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${ROOT}/include ${ROOT}/config
    ${KERNEL}/include ${KERNEL}/portable/GCC/ARM_CM4F ${DEVICE}/Include
    ${ROOT}/third_party/tinycrypt/include ${ROOT}/third_party/CMSIS-Core/Include ${ROOT}/third_party/STM32F4-LL/Inc)
if(TP_F407_DIAGNOSTICS)
    target_sources(tp_f407.elf PRIVATE tp_f407_diagnostics.c)
    target_compile_definitions(tp_f407.elf PRIVATE TP_F407_DIAGNOSTICS)
endif()
if(TP_USE_FREEMODBUS)
    target_sources(tp_f407.elf PRIVATE ${ROOT}/protocol/freemodbus/frame.c
        ${ROOT}/third_party/FreeModbus/mb.c ${ROOT}/third_party/FreeModbus/rtu/mbrtu.c
        ${ROOT}/third_party/FreeModbus/rtu/mbcrc.c ${ROOT}/third_party/FreeModbus/functions/mbfuncholding.c
        ${ROOT}/third_party/FreeModbus/functions/mbutils.c)
    target_include_directories(tp_f407.elf PRIVATE ${ROOT}/protocol/freemodbus
        ${ROOT}/third_party/FreeModbus/include ${ROOT}/third_party/FreeModbus/rtu)
    target_compile_definitions(tp_f407.elf PRIVATE TP_USE_FREEMODBUS)
endif()
if(TP_F407_TEACHING_OTA)
    if(NOT TP_F407_TEACHING_SUBORDINATE OR TP_F407_IMAGE_SLOT STREQUAL "standalone" OR
       NOT EXISTS "${TP_F407_TEACHING_IDENTITY_SOURCE}" OR IS_DIRECTORY "${TP_F407_TEACHING_IDENTITY_SOURCE}")
        message(FATAL_ERROR "Teaching OTA requires subordinate, image slot 0/1 and an identity source")
    endif()
    target_sources(tp_f407.elf PRIVATE tp_f407_teaching_ota.c "${TP_F407_TEACHING_IDENTITY_SOURCE}")
    target_compile_definitions(tp_f407.elf PRIVATE TP_F407_TEACHING_OTA)
endif()
if(TP_F407_TEACHING_STATUS_INPUT AND NOT TP_F407_TEACHING_SUBORDINATE)
    message(FATAL_ERROR "TP_F407_TEACHING_STATUS_INPUT requires TP_F407_TEACHING_SUBORDINATE")
endif()
if(TP_F407_TEACHING_WAKE AND NOT TP_F407_TEACHING_SUBORDINATE)
    message(FATAL_ERROR "TP_F407_TEACHING_WAKE requires TP_F407_TEACHING_SUBORDINATE")
endif()
if(TP_F407_TEACHING_SUBORDINATE)
    file(TO_CMAKE_PATH "${TP_F407_BOARD_CONFIG}" teaching_config_path)
    get_filename_component(teaching_config_name "${teaching_config_path}" NAME)
    if(NOT TP_F407_BUSINESS OR NOT teaching_config_name STREQUAL "tp_f407_teaching_config.c")
        message(FATAL_ERROR "TP_F407_TEACHING_SUBORDINATE requires the teaching business configuration")
    endif()
    target_compile_definitions(tp_f407.elf PRIVATE TP_F407_TEACHING_SUBORDINATE)
    if(TP_F407_TEACHING_STATUS_INPUT)
        target_sources(tp_f407.elf PRIVATE tp_f407_status_input.c)
        target_compile_definitions(tp_f407.elf PRIVATE TP_F407_TEACHING_STATUS_INPUT)
    endif()
    if(TP_F407_TEACHING_WAKE)
        target_compile_definitions(tp_f407.elf PRIVATE TP_F407_TEACHING_WAKE)
    endif()
endif()
target_compile_definitions(tp_f407.elf PRIVATE STM32F407xx)
target_compile_options(tp_f407.elf PRIVATE -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16
    -mfloat-abi=hard -ffunction-sections -fdata-sections)
target_link_options(tp_f407.elf PRIVATE -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16
    -mfloat-abi=hard --specs=nosys.specs -Wl,--defsym=TP_FLASH_ORIGIN=${TP_FLASH_ORIGIN} -Wl,--defsym=TP_FLASH_LENGTH=${TP_FLASH_LENGTH} -T${CMAKE_CURRENT_LIST_DIR}/stm32f407vg.ld
    -Wl,--gc-sections -Wl,--undefined=tp_f407_handoff -Wl,--undefined=tp_boot_target_prepare -Wl,--undefined=tp_upgrade_fram_init -Wl,--undefined=tp_upgrade_fram_store -Wl,--undefined=tp_f407_app_init -Wl,--undefined=tp_f407_upgrade_uart_init -Wl,--undefined=tp_image_sha256_verify -Wl,--undefined=tp_f407_flash_controller_init -Wl,--undefined=tp_f407_image_write -Wl,--undefined=tp_f407_image_erase -Wl,--undefined=tp_secure_upgrade_init -Wl,--undefined=tp_signed_boot_verify -Wl,--undefined=tp_signed_boot_download_verify -Wl,--undefined=tp_signed_boot_authenticate -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/tp_f407.map)
target_link_libraries(tp_f407.elf PRIVATE m)
add_custom_command(TARGET tp_f407.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:tp_f407.elf> ${CMAKE_CURRENT_BINARY_DIR}/tp_f407.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:tp_f407.elf> ${CMAKE_CURRENT_BINARY_DIR}/tp_f407.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:tp_f407.elf>)
