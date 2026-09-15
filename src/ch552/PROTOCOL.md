# CH552 USB protocol

The device uses the MU3IO.NET Ontroller identity, VID `0E8F` and PID `1216`.
It exposes one vendor-defined WinUSB interface so applications that search by
VID/PID open the WinUSB device itself rather than a composite parent:

- MU3IO.NET uses bulk OUT `0x03` and bulk IN `0x84`.
- The WebUI uses bulk OUT `0x01` and bulk IN `0x81` through WebUSB.

The Microsoft OS 1.0 compatible-ID descriptor requests the built-in Windows
WinUSB driver. The OS string is at index `0xEE`, and vendor request `0x21` with
feature index `0x0004` returns the `WINUSB` compatible ID. The OS 2.0
descriptor remains available as a secondary path. A separate driver
installation is not required.

## MU3IO.NET WinUSB packets

The 8-byte input packet begins with ASCII `DDT`. Bytes 3 and 4 contain the six
game buttons, side controls, menu buttons, test, and service bits in Ontroller
layout. Bytes 5 and 6 contain the lever position as a big-endian value in the
default calibrated range `100..600`. Byte 7 is ignored by MU3IO.NET and carries
the raw STM32 high button byte for diagnostics. Test and Service transitions
are published only after three consecutive matching STM32 reports.

The 33-byte LED output packet is accepted at USB endpoint `0x03` for protocol
compatibility but deliberately discarded. MU3IO.NET cannot take ownership of
any lamp. The STM32 continues rendering its configured local/reactive lighting.

## WebUSB packets

## Input report (16 bytes)

| Offset | Size | Meaning |
| --- | --- | --- |
| 0 | 1 | Protocol version (`1`) |
| 1 | 1 | Report sequence counter |
| 2 | 2 | Button bits 0..11, little-endian |
| 4 | 1 | Raw lever ADC, upper 8 bits |
| 5 | 1 | Filtered lever value |
| 6 | 1 | STM32 link valid (`1` after a valid frame) |
| 7 | 1 | Reserved, currently `0` |
| 8 | 2 | Valid STM32 frames, saturating little-endian counter |
| 10 | 2 | Invalid STM32 frames, saturating little-endian counter |
| 12 | 2 | UART RX overflows, saturating little-endian counter |
| 14 | 2 | UART TX overflows, saturating little-endian counter |

While updating, byte 0 is `2`: byte 1 is the command token, byte 2 is updater
state, byte 3 is an error code, and bytes 4..7 are the current address in
little-endian order. Terminal states are ready (`2`), complete (`7`), or error
(`0x80`).

After applying strip settings, byte 0 is `3` and bytes 1..8 echo effect,
brightness, speed, red, green, blue, color order, and pixel count. The WebUI
waits for this acknowledgement before issuing a separate save command.

After a settings save completes, byte 0 is `4`, byte 1 echoes the save token,
and byte 2 is `1` only when the STM32 flash record passed read-back CRC
validation. A zero result means the save failed.

## Output report (32 bytes)

The first byte selects the operation:

- `01 LEVEL_0 ... LEVEL_11`: directly set the 12 button-light brightnesses.
- `02 EFFECT VALUE SPEED REACTIVE OFF_LEVEL ON_LEVEL [SAVE]`: apply lighting
  settings. If optional `SAVE` bit 0 is set, persist them on the STM32.
- `03`: release direct host lighting and return to local/reactive lighting.
- `04 TOKEN`: persist the current STM32 configuration and return a correlated
  type-4 success/failure report after flash verification.
- `05 EFFECT BRIGHTNESS SPEED R G B ORDER PIXELS`: configure the SK6812 output.
  Use operation `04` after receiving the type-3 acknowledgement to persist it.
  For atomic apply-and-save, set byte 9 bit 0 and put a nonzero save token in
  byte 30; the STM32 returns the correlated type-4 result.
- `20 TOKEN 59 54 47 21`: enter and synchronize the STM32 ROM bootloader.
- `21 TOKEN PAGE_HI PAGE_LO`: erase one application page. Page 31 is rejected.
- `22 TOKEN ADDRESS_LE LENGTH DATA...`: write `4..24` aligned bytes and verify
  them immediately with AN3155 Read Memory.
- `23 TOKEN ADDRESS_LE`: run the verified application.
- `24 TOKEN`: retry ROM synchronization after an error. If ROM never became
  ready, the CH552 first resends the application boot-entry request.
- `30 59 54 47 43 21`: detach and enter the CH552 factory bootloader.

Update operations are tokenized and asynchronous. The CH552 uses AN3155 at
57600 8E1, never mass-erases, and rejects writes outside
  `0x08000000..0x0800EFFF` so settings journal pages 30–31 survive.

Pad unused bytes with zero. The firmware also accepts shorter USB transfers,
but hosts should follow the fixed 32-byte size advertised by the HID report
descriptor. Commands shorter than their required fields are ignored safely.

The CH552 factory-bootloader command only enters update mode. Flash the device
after it re-enumerates using WCHISP or `wchisp`. Initial installation still
requires the hardware boot strap or an external programmer.
