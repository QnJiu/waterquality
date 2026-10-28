cmake_minimum_required(VERSION 3.20)
project(tp_f407_boot C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(DEVICE ${ROOT}/third_party/STM32F4-CMSIS)
set(TP_BOOT_BOARD_CONFIG "" CACHE FILEPATH "Trusted boot board implementation")
if(TP_BOOT_BOARD_CONFIG)
    set(BOARD ${TP_BOOT_BOARD_CONFIG})
else()
    set(BOARD unconfigured.c)
endif()
set_source_files_properties(${DEVICE}/Source/system_stm32f4xx.c PROPERTIES COMPILE_DEFINITIONS SystemInit=tp_vendor_system_init)
add_executable(tp_f407_boot.elf main.c tp_f407_boot.c ${BOARD}
    ${ROOT}/firmware/tp_f407_system_init.c ${ROOT}/firmware/tp_f407_handoff.c ${ROOT}/firmware/tp_f407_emergency.c
    ${DEVICE}/Source/startup_stm32f407xx.s ${DEVICE}/Source/system_stm32f4xx.c
    ${ROOT}/drivers/upgrade_fram.c ${ROOT}/bootloader/boot.c ${ROOT}/bootloader/boot_target.c
    ${ROOT}/bootloader/signed_boot.c ${ROOT}/bootloader/signed_manifest.c ${ROOT}/bootloader/image_hash.c
    ${ROOT}/third_party/tinycrypt/source/ecc.c ${ROOT}/third_party/tinycrypt/source/ecc_dsa.c
    ${ROOT}/third_party/tinycrypt/source/sha256.c ${ROOT}/third_party/tinycrypt/source/utils.c)
target_include_directories(tp_f407_boot.elf PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${ROOT}/firmware ${ROOT}/include
    ${DEVICE}/Include ${ROOT}/third_party/CMSIS-Core/Include ${ROOT}/third_party/tinycrypt/include)
target_compile_definitions(tp_f407_boot.elf PRIVATE STM32F407xx)
target_compile_options(tp_f407_boot.elf PRIVATE -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -ffunction-sections -fdata-sections)
target_link_options(tp_f407_boot.elf PRIVATE -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard --specs=nosys.specs
    -Wl,--defsym=TP_FLASH_ORIGIN=0x08000000 -Wl,--defsym=TP_FLASH_LENGTH=0x20000 -T${ROOT}/firmware/stm32f407vg.ld
    -Wl,--gc-sections -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/tp_f407_boot.map)
add_custom_command(TARGET tp_f407_boot.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:tp_f407_boot.elf> ${CMAKE_CURRENT_BINARY_DIR}/tp_f407_boot.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:tp_f407_boot.elf> ${CMAKE_CURRENT_BINARY_DIR}/tp_f407_boot.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:tp_f407_boot.elf>)
