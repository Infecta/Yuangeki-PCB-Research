cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER arm-none-eabi-gcc CACHE FILEPATH "ARM C compiler")
set(CMAKE_OBJCOPY arm-none-eabi-objcopy CACHE FILEPATH "ARM objcopy")
set(CMAKE_SIZE arm-none-eabi-size CACHE FILEPATH "ARM size tool")

project(yuantgeki_stm32 C ASM)

get_filename_component(ARM_TOOLCHAIN_BIN "${CMAKE_C_COMPILER}" DIRECTORY)
find_program(ARM_OBJCOPY_EXECUTABLE arm-none-eabi-objcopy
    HINTS "${ARM_TOOLCHAIN_BIN}" REQUIRED NO_CACHE)
find_program(ARM_SIZE_EXECUTABLE arm-none-eabi-size
    HINTS "${ARM_TOOLCHAIN_BIN}" REQUIRED NO_CACHE)

add_executable(yuantgeki-stm32.elf
    startup.c
    main.c
    hardware.c
    input.c
    lever.c
    protocol.c
    lighting.c
    strip.c
    config.c
    settings_journal.c
)

target_include_directories(yuantgeki-stm32.elf PRIVATE include)
target_compile_definitions(yuantgeki-stm32.elf PRIVATE STM32G030xx)
target_compile_options(yuantgeki-stm32.elf PRIVATE
    -mcpu=cortex-m0plus
    -mthumb
    -Os
    -ffreestanding
    -fno-builtin
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -Werror
)
target_link_options(yuantgeki-stm32.elf PRIVATE
    -mcpu=cortex-m0plus
    -mthumb
    -nostdlib
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/yuantgeki-stm32.map
    -T${CMAKE_CURRENT_SOURCE_DIR}/stm32g030c8.ld
)
target_link_libraries(yuantgeki-stm32.elf PRIVATE gcc)

add_custom_command(TARGET yuantgeki-stm32.elf POST_BUILD
    COMMAND ${ARM_OBJCOPY_EXECUTABLE} -O ihex $<TARGET_FILE:yuantgeki-stm32.elf> ${CMAKE_CURRENT_BINARY_DIR}/yuantgeki-stm32.hex
    COMMAND ${ARM_OBJCOPY_EXECUTABLE} -O binary $<TARGET_FILE:yuantgeki-stm32.elf> ${CMAKE_CURRENT_BINARY_DIR}/yuantgeki-stm32.bin
    COMMAND ${ARM_SIZE_EXECUTABLE} $<TARGET_FILE:yuantgeki-stm32.elf>
    COMMENT "Generating HEX and BIN images"
)
