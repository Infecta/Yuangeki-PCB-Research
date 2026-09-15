# Host-side tests

The protocol parser test uses the desktop C compiler and stub implementations
for the hardware and lighting boundaries.

```powershell
gcc -std=c17 -Wall -Wextra -Werror -DYTG_UNIT_TEST -Isrc/stm32/include `
  src/stm32/protocol.c src/stm32/tests/protocol_test.c `
  -o build/stm32/protocol_test.exe
./build/stm32/protocol_test.exe
```

`lighting_test.c` verifies PWM levels, host override, reactive behavior, and the
Test-then-Service normal/dim/off sequence.

`input_test.c` verifies the 5 ms press/release integrator and edge reporting.
`lever_test.c` verifies filter convergence and legacy range conversion.

`strip_test.c` verifies the SK6812 raw color order, safe off output, and
GRB byte ordering.
