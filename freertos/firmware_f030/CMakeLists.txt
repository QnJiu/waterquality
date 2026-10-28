cmake_minimum_required(VERSION 3.20)
project(tp_f030_skeleton C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(KERNEL ${ROOT}/third_party/FreeRTOS-Kernel)
set(DEVICE ${ROOT}/third_party/STM32F0-CMSIS)
add_executable(tp_f030.elf main.c
    ${DEVICE}/Source/startup_stm32f030x8.s ${DEVICE}/Source/system_stm32f0xx.c
    ${KERNEL}/tasks.c ${KERNEL}/queue.c ${KERNEL}/list.c
    ${KERNEL}/portable/GCC/ARM_CM0/port.c ${KERNEL}/portable/MemMang/heap_4.c)
target_include_directories(tp_f030.elf PRIVATE ${CMAKE_CURRENT_LIST_DIR}
    ${KERNEL}/include ${KERNEL}/portable/GCC/ARM_CM0 ${DEVICE}/Include
    ${ROOT}/third_party/CMSIS-Core/Include)
target_compile_definitions(tp_f030.elf PRIVATE STM32F030x8)
target_compile_options(tp_f030.elf PRIVATE -mcpu=cortex-m0 -mthumb -ffunction-sections -fdata-sections)
target_link_options(tp_f030.elf PRIVATE -mcpu=cortex-m0 -mthumb --specs=nosys.specs
    -T${CMAKE_CURRENT_LIST_DIR}/stm32f030c8.ld -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/tp_f030.map)
add_custom_command(TARGET tp_f030.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:tp_f030.elf> ${CMAKE_CURRENT_BINARY_DIR}/tp_f030.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:tp_f030.elf>)
