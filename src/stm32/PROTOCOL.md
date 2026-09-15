# STM32 to CH552 protocol

All frames use `250000 baud, 8N1` and this envelope:

```
AA 55 LENGTH PAYLOAD... CHECKSUM
```

`LENGTH` is the payload byte count. `CHECKSUM` is XOR of the payload bytes
only. The receiver rejects lengths outside `1..32` and searches for the next
header after malformed traffic.

## STM32 input report

The STM32 sends a typed, sequenced report every 4 ms:

```
AA 55 06 A1 SEQUENCE BUTTON_LO BUTTON_HI LEVER_RAW_8 LEVER_FILTERED_8 CHECKSUM
```

Buttons are active-high in the report after active-low GPIO sampling and
debounce. Bits 0 through 11 follow `enum button_id` in `include/board.h`.
The lever bytes are the 12-bit ADC values reduced to their upper eight bits.
The `A1` marker prevents a shifted UART stream from being mistaken for an
input report; `SEQUENCE` increments modulo 256.

## CH552 commands

- `01 EFFECT VALUE SPEED REACTIVE OFF_LEVEL ON_LEVEL`: apply the known stock
  lighting-settings payload. `REACTIVE != 0` selects `ON_LEVEL` for pressed
  buttons and `OFF_LEVEL` otherwise. When reactive mode is disabled, `VALUE`
  sets every button-light level. Effect and speed are retained for later
  effect implementation.
- `03 [TOKEN]`: persist the current configuration to the flash journal in pages
  30–31. The optional token is echoed in result `93 TOKEN SUCCESS`.
- `14 LEVEL_0 ... LEVEL_11`: WebUI direct-lighting command. Controls the six
  main and two menu lamps; WAD, Test, and Service remain local.
- `11`: Yuan'tGeki extension. Release direct host control and return to the
  local/reactive lighting settings.
- `12 EFFECT BRIGHTNESS SPEED RED GREEN BLUE ORDER PIXELS`: configure PA12
  SK6812 output. Effects are off, solid, rainbow, and chase (`0..3`); order is
  GRB (`0`) or RGB (`1`); pixel count is `1..32`. A successful application is
  echoed as payload `92 EFFECT BRIGHTNESS SPEED RED GREEN BLUE ORDER PIXELS`.
- `12 TOKEN EFFECT BRIGHTNESS SPEED RED GREEN BLUE ORDER PIXELS FLAGS`:
  atomically apply strip settings. With `FLAGS` bit 0 set, save them before
  returning `93 TOKEN SUCCESS`; no apply-only `92` acknowledgement is sent.
- `20 42 4F 4F 54`: authenticated application request that drains UART,
  resets application peripherals, and jumps directly into STM32 system
  memory. The CH552 then owns the UART.

The local Test-then-Service shortcut still applies its normal/dim/off master
mode on top of either reactive or direct host levels.

During ROM update, the UART changes to approximately 57600 baud with eight
data bits, even parity, and one stop bit. This phase follows ST AN3155 and is
not framed with the application envelope above.
