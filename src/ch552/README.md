# Yuan'tGeki CH552 firmware

Standalone SDCC firmware for the CH552T USB bridge.

- UART0 is remapped to package pin 17 (`P1.2/RXD_`) and pin 16
  (`P1.3/TXD_`) at 250000 baud.
- A full-speed WinUSB device uses the MU3IO.NET Ontroller identity
  (`VID 0E8F / PID 1216`). Interface 0 exposes MU3 bulk OUT `0x03` and IN
  `0x84`, plus WebUSB configuration endpoints OUT `0x01` and IN `0x81`.
- The STM32 legacy input frame is validated and translated into the HID input
  report.
- WebUSB output reports are translated into framed STM32 configuration and
  direct-test commands. MU3IO.NET LED reports are acknowledged and discarded.
- The updater proxies WebUSB firmware chunks into the STM32G030 factory ROM
  USART protocol at 57600 8E1, with per-chunk read-back verification.
- USB endpoint DMA memory is explicitly reserved below xRAM address `0x0140`.
- Microsoft OS 1.0 descriptors bind the single vendor interface to WinUSB
  automatically on Windows; the OS 2.0 descriptor remains available as a
  secondary path.
- The linker code limit is `0x3800`, protecting the CH552 bootloader region.
- WebUSB can request a clean detach and jump into the protected CH552 factory
  bootloader for subsequent flashing with WCHISP or `wchisp`.

The Ontroller VID/PID are used strictly because the bundled MU3IO.NET module
hard-codes them as its compatibility contract.

Build:

```powershell
cmake -S src/ch552 -B build/ch552 -G Ninja
cmake --build build/ch552
```

The outputs are `build/ch552/yuantgeki-ch552.ihx` and the packed
`build/ch552/yuantgeki-ch552.bin`. The host report layout is documented in
`PROTOCOL.md`. The build also rejects images that overlap the bootloader or
omit the UART0/USB interrupt vectors.

Browser updating through WebUSB requires this CH552 image and the matching STM32 application
to be flashed once with the normal external tools. It preserves STM32 page 31;
ST-Link remains the recovery path for power loss during an update.

Flashing this image changes the USB identity to the Ontroller VID/PID. Windows
will enumerate the controller again, and the browser will require a WebUSB
permission grant for that identity. MU3IO.NET and the WebUI cannot open the
WinUSB device simultaneously.
