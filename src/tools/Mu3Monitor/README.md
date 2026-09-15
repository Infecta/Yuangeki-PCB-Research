# MU3IO.NET input monitor

This console utility opens the Ontroller through the bundled MU3IO.NET code and
prints every input transition. Raw button bytes make brief Test or Service
assertions visible even when they last for only one USB report. It uses the
Ontroller input mapping but deliberately disables MU3IO.NET's automatic LED
refresh while monitoring. `stmHi` shows the raw STM32 high button byte before
the CH552's Test/Service confirmation filter.

Build and run:

```powershell
dotnet build src/tools/Mu3Monitor/Mu3Monitor.csproj -c Release
dotnet run --project src/tools/Mu3Monitor/Mu3Monitor.csproj -c Release
```

For a controlled comparison, run with `--led-stress`. This enables all MU3
lighting outputs at maximum and refreshes them every two input polls. The
default mode sends no LED reports. Current firmware acknowledges and discards
all MU3 LED output, so these options test USB writes only and do not control
physical lamps.

Use `--led-level 128` for constant half-brightness traffic. This does not
generate a flashing pattern. The monitor sends an immutable 33-byte LED
packet every two input polls and checks success and byte count on every
write. It prints the largest write-completion gap every five seconds and
immediately flags gaps of 200 ms or more (the STM32 host timeout is 250 ms).
USB write success does not prove that the STM32 accepted the UART command.

Compare `--led-level 128` with `--led-level 255`. If both remain steady but
the game flickers, investigate the game's changing RGB values and report
cadence. If constant packets also flicker, the fault remains in transport,
firmware, or the electrical output. Level 0 at initial connection is ignored
by the current CH552 hotplug workaround; use nonzero levels for this test.

Use `--right-c-only --led-level 255` to write only the sixth MU3 RGB triplet.
The current firmware discards it; this mode now verifies USB output transport
only.

Use `--left-wad-only --led-level 255` or
`--right-wad-only --led-level 255` to write Ontroller triplets 6 and 9 in
isolation. The current firmware acknowledges but does not apply them.

Do not run it alongside the game, translator, or WebUI because the WinUSB
device is exclusive.
