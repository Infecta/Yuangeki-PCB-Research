if(NOT DEFINED IMAGE OR NOT EXISTS "${IMAGE}")
    message(FATAL_ERROR "CH552 image was not generated: ${IMAGE}")
endif()

file(SIZE "${IMAGE}" IMAGE_SIZE)
if(IMAGE_SIZE GREATER 14336)
    message(FATAL_ERROR
        "CH552 image is ${IMAGE_SIZE} bytes and overlaps the bootloader at 0x3800")
endif()

file(READ "${IMAGE}" IMAGE_HEX HEX)

# Each CH552 interrupt vector must begin with an 8051 LJMP (opcode 0x02).
# String positions are byte addresses multiplied by two because IMAGE_HEX has
# two hexadecimal characters per byte.
string(SUBSTRING "${IMAGE_HEX}" 70 2 UART0_VECTOR_OPCODE)
string(SUBSTRING "${IMAGE_HEX}" 134 2 USB_VECTOR_OPCODE)

if(NOT UART0_VECTOR_OPCODE STREQUAL "02")
    message(FATAL_ERROR "UART0 interrupt vector at 0x23 is missing")
endif()
if(NOT USB_VECTOR_OPCODE STREQUAL "02")
    message(FATAL_ERROR "USB interrupt vector at 0x43 is missing")
endif()

message(STATUS
    "Verified CH552 image: ${IMAGE_SIZE} bytes, UART0 and USB vectors present")
