# Yuan'tGeki STM32 firmware

Standalone bare-metal firmware for the STM32G030C8 on the Yuangeki PCB.

Implemented in the initial bring-up image:

- 12 named, active-low button inputs with 5 ms integrator debounce
- 10 active-high button-light outputs with 64-step, 125 Hz software PWM
- WAD outputs use eight-step, 1 kHz software PWM with cycle-boundary duty
  updates, sharing the existing 8 kHz scheduler (nine levels including off)
- PA0 ADC sampling with median spike rejection, two-count jitter hold, and an
  adaptive fixed-point moving filter
- verified 250000-baud legacy UART input frame on PA9/PA10
- interrupt-driven UART RX queue with checksum validation and parser recovery
- stock command `0x01` lighting settings and command `0x03` save request parsing
- WebUI-only command `0x14` for direct main/menu lamp testing and command
  `0x11` to return control to local/reactive lighting
- CRC32-protected, generation-numbered settings journal in reserved flash pages
  30–31; saves occur only on command `0x03` or a local lighting-mode change
- Test-then-Service local lighting mode cycle: normal, dim, off
- stock-shaped 50% PA11 periodic output at 800 Hz; its suspected optical-sensor
  `LDK` role must still be confirmed electrically
- safe startup with button lights driven low before output mode is enabled
- PA12 SK6812MINI-E output using DMA-fed, datasheet-timed 800 kHz symbols
- PB1/PA6 active-low WAD inputs and PB0/PA7 scalar WAD light outputs,
  matching the stock firmware's 32-step GPIO PWM topology
- reset-clean entry into system-memory bootloader using a two-word retained
  clean direct handoff to STM32 system-memory bootloader

The default strip topology is 16 GRB pixels with output off. Parallel boards
and GRB order remain hardware inferences; both pixel count and order can be
changed from the WebUI for measurement.

Build:

```powershell
cmake -S src/stm32 -B build/stm32 -G Ninja
cmake --build build/stm32
```

The build produces ELF, HEX, BIN, and linker map files. Do not flash the image
until the generated memory map and GPIO startup behavior have been reviewed.
