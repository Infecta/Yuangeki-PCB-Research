cmake_minimum_required(VERSION 3.20)
project(yuantgeki_ch552 NONE)

find_program(SDCC_EXECUTABLE sdcc
    HINTS "C:/Program Files/SDCC/bin"
    REQUIRED)
get_filename_component(SDCC_BIN_DIR "${SDCC_EXECUTABLE}" DIRECTORY)
find_program(MAKEBIN_EXECUTABLE makebin
    HINTS "${SDCC_BIN_DIR}"
    REQUIRED)

set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(OUTPUT_IHX "${OUTPUT_DIR}/yuantgeki-ch552.ihx")
set(OUTPUT_BIN "${OUTPUT_DIR}/yuantgeki-ch552.bin")
set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/uart.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/bridge.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/usb.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/updater.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/bootloader.c"
)
set(HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ch552.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/uart.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/bridge.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/usb.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/updater.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/bootloader.h"
)

set(COMMON_FLAGS
    -mmcs51
    --std-sdcc11
    --model-small
    -I${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(OBJECTS)
foreach(SOURCE IN LISTS SOURCES)
    get_filename_component(MODULE "${SOURCE}" NAME_WE)
    set(OBJECT "${OUTPUT_DIR}/${MODULE}.rel")
    add_custom_command(
        OUTPUT "${OBJECT}"
        COMMAND "${CMAKE_COMMAND}" -E env
            "PATH=${SDCC_BIN_DIR}\;$ENV{PATH}"
            "${SDCC_EXECUTABLE}"
            ${COMMON_FLAGS}
            -c "${SOURCE}"
            -o "${OBJECT}"
        DEPENDS "${SOURCE}" ${HEADERS}
        VERBATIM
        COMMENT "Compiling CH552 ${MODULE}.c"
    )
    list(APPEND OBJECTS "${OBJECT}")
endforeach()

add_custom_command(
    OUTPUT "${OUTPUT_IHX}"
    COMMAND "${CMAKE_COMMAND}" -E env
        "PATH=${SDCC_BIN_DIR}\;$ENV{PATH}"
        "${SDCC_EXECUTABLE}"
        -mmcs51
        --std-sdcc11
        --model-small
        --iram-size 0x100
        --xram-loc 0x0140
        --xram-size 0x02C0
        --code-size 0x3800
        --out-fmt-ihx
        ${OBJECTS}
        -o "${OUTPUT_IHX}"
    DEPENDS ${OBJECTS}
    VERBATIM
    COMMENT "Building CH552 firmware with SDCC"
)

add_custom_command(
    OUTPUT "${OUTPUT_BIN}"
    COMMAND "${MAKEBIN_EXECUTABLE}" -p "${OUTPUT_IHX}" "${OUTPUT_BIN}"
    COMMAND "${CMAKE_COMMAND}"
        -DIMAGE=${OUTPUT_BIN}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_image.cmake"
    DEPENDS "${OUTPUT_IHX}" "${CMAKE_CURRENT_SOURCE_DIR}/verify_image.cmake"
    VERBATIM
    COMMENT "Creating packed CH552 binary"
)

add_custom_target(yuantgeki-ch552 ALL DEPENDS "${OUTPUT_IHX}" "${OUTPUT_BIN}")
