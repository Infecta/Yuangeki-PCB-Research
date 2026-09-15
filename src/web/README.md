# Yuan'tGeki WebUSB control deck

Local browser interface for testing and configuring Yuan'tGekiCFW through the
CH552 WinUSB interface. The controller uses the MU3IO.NET Ontroller identity
(`VID 0E8F / PID 1216`). MU3IO.NET uses endpoints `03/84`; the browser uses
endpoints `01/81`. They cannot open the controller simultaneously.

Features:

- automatic reconnect after WebUSB permission has been granted
- live visualization of all 12 button inputs
- raw and filtered lever display with observed range and filter delta
- STM32 UART frame and overflow diagnostics
- direct 0..255 control of each button light
- all-on, all-off, and chase light tests
- reactive, idle, pressed, and uniform lighting settings
- explicit apply and apply-and-save actions
- SK6812 solid, rainbow, and chase control with topology/order selection
- STM32 HEX/BIN updates through CH552, selective page erase, per-write
  read-back verification, progress display, and retryable error reporting

Run locally from this directory:

```powershell
python -m http.server 4173 --directory dist
```

Then open `http://localhost:4173` in Chrome or Edge. WebUSB requires a secure
context; browsers treat `localhost` as secure for this purpose.

The direct light output is refreshed every 100 ms. If the page closes or loses
the device, the STM32 automatically drops direct control after 250 ms and
returns to its configured local lighting behavior.

The updater accepts only STM32G030 application images below `0x0800F800` and
validates their vector table. The CH552 and the bootloader-capable STM32 image
must first be installed conventionally. Do not remove power during an update.
