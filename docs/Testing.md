# Unit Testing (Unity + CMock)

This project has a host-side unit test suite, separate from the STM32CubeIDE
target build. Tests compile and run with `gcc` on your PC — no MCU, no
FreeRTOS actually running, no SD card or CAN bus needed. This lets you catch
logic bugs (bad math, wrong CRC, a mutex that never gets released) in
seconds, without flashing hardware.

The framework is **Ceedling**, which bundles **Unity** (the test/assert
runner) and **CMock** (generates fake versions of functions so you can
control exactly what a dependency returns).

## Running the tests

```
./executeTests.sh                    # run every test file
./executeTests.sh test_AMS_sensors   # run just one file (no "test:" prefix)
```

`executeTests.sh` finds `ceedling` on your machine automatically (it lives in
your Ruby gem bin directory, which may not be on `PATH` in a fresh shell) and
runs it for you. Under the hood this is just `ceedling test:all` /
`ceedling test:<name>`.

### One-time setup (if `ceedling` isn't installed yet)

```
sudo apt install -y ruby-full build-essential
gem install ceedling --user-install
```

`executeTests.sh` will tell you if `ceedling` is missing and print this same
command.

## What's tested today

| File | Tests | Covers |
|---|---|---|
| `test/test_AMS_sensors.c` | 11 | `Algorithms/AMS_sensors.c` — voltage/temperature conversion. No mocks: this file has zero hardware/RTOS dependency, so it runs completely as-is. |
| `test/test_AMS_gps_algorithms.c` | 7 | `Algorithms/AMS_gps_algorithms.c` — NMEA RMC/GGA frame → `GPS_Data_t` conversion. No mocks. |
| `test/test_AMS_telemetry_algorithms.c` | 10 | `Algorithms/AMS_telemetry_algorithms.c` — odometer/max-speed/avg-speed/accel-decel folding, plus lifetime baseline seeding (`vd_TelemetryCalc_SeedLifetime`). No mocks. |
| `test/test_AMS_charge_algorithms.c` | 6 | `Algorithms/AMS_charge_algorithms.c` — coulomb counting (Ah discharged/charged, session + lifetime). No mocks. |
| `test/test_AMS_thermal_algorithms.c` | 4 | `Algorithms/AMS_thermal_algorithms.c` — max/min/avg/delta cell temperature, this-sample and session-worst. No mocks. |
| `test/test_AMS_current_algorithms.c` | 5 | `Algorithms/AMS_current_algorithms.c` — peak discharge/charge current, session + lifetime. No mocks. |
| `test/test_AMS_bms_safety_algorithms.c` | 10 | `Algorithms/AMS_bms_safety_algorithms.c` — per-cell UV/OV debounce, NTC voltage→temperature lookup/interpolation/clamping. No mocks. |
| `test/test_AMS_DataBroker.c` | 16 | `Middleware/AMS_DataBroker.c` — mutex acquire/release per domain (all 8), the 50ms timeout + fault counter, per-domain freshness (`AMS_Safety_Flags_t`), `Get_AllData()`'s fixed visit order. Mocks `cmsis_os.h`. |
| `test/test_AMS_Flash_Task.c` | 7 | `Middleware/AMS_Flash_Task.c` — sector scanning, CRC32 record validation, wear-leveling sector selection (which of the two sectors wins on boot). Mocks the flash driver + the Broker. |
| `test/test_AMS_Led_Task.c` | 8 | `Middleware/AMS_Led_Task.c` — the ON/OFF/TOGGLE/BLINK state machine, per-channel independence, blink-deadline expiry. Mocks `AMS_led_driver.h` + `HAL_GetTick`. |
| `test/test_AMS_CAN_Task.c` | 4 | `parse_inverter_status()` — the CAN→RPM parser, including a Broker-write-failure path. See "Testing a `static` function" below for how this one works without touching the original file. |
| `test/test_AMS_gps_driver.c` | 4 | `Drivers_Custom/AMS_gps_driver.c` — the DMA-vs-interrupt reception fallback (USART6 has a DMA stream wired, USART3 doesn't). Mocks `HAL_UARTEx_ReceiveToIdle_DMA/IT` via `main.h`. |

**Total: 92 tests, all passing.**

### Not covered yet, and why

The task **loop wrappers** themselves aren't unit-tested — only the pure
logic that's been extracted out of them. Concretely still untested:
`vd_CAN_Manager_TaskProcess()`'s queue-drain + button-TX loop,
`vd_GPS_Manager_TaskProcess()`'s queue-drain + multi-sentence-split loop,
`AMS_ADC_Task.c`'s ISR/shadow-buffer handling, and
`AMS_Algorithms_Task.c`'s/`AMS_Logger_Task.c`'s/`AMS_BMS_Task.c`'s outer
`for(;;)` bodies.
These all block forever on `osMessageQueueGet`/`osDelay` inside an infinite
loop with no way to run "one iteration" and get control back — the same
reason GPS/telemetry math used to be untestable until it was pulled out into
`Algorithms/AMS_gps_algorithms.c` / `AMS_telemetry_algorithms.c` (now
tested, see the table above). Extracting more of these loop bodies the same
way is possible but is a production-code change worth asking about first,
not doing silently.

`AMS_BMS_Data_t`'s and `AMS_BatteryStats_Data_t`'s broker plumbing
(write/read/freshness) is tested inside `test_AMS_DataBroker.c`.
`AMS_BMS_Task`'s own logic (`b_BMS_Driver_Measure`, `b_BMS_Driver_Init`) is
not unit-tested — it's a thin HAL/vendor-library wrapper with no pure logic
left to extract (see `docs/Tasks/AMS_BMS_Task/README.md`); the math it
feeds (`b_BmsSafety_CheckCellVoltage`, NTC lookup) is tested separately in
`test_AMS_bms_safety_algorithms.c` above.

## How this is wired together

### `project.yml`

Ceedling's config, at the repo root. Points it at:
- `:source: Core/Src/**` — real production code
- `:include: Core/Inc/**` — real headers
- `:include: test/vendor_stubs` (listed **first**, see below)
- `:include: Core/Src/**` — lets a test `#include` a `.c` file directly (see the CAN test)
- `:test: test/**` — the test files themselves

It also configures CMock (`:cmock:` section) — notably `:when_ptr:
:compare_data`, which means when a mocked function is expected with a
pointer argument, CMock compares the **data pointed to**, not the pointer's
address. That's what lets a test write `..._Expect(&my_local_struct)` and
have it match the real struct passed at runtime, even though they're
different variables in memory.

### `test/vendor_stubs/` — minimal stand-ins for vendor headers

The real `cmsis_os.h` (FreeRTOS's CMSIS-RTOS v2 wrapper) and `main.h`
(CubeMX-generated, pulls in the entire STM32 HAL) are both far too complex
for CMock's header parser to handle correctly:

- `cmsis_os.h` has version guards like `#if (osCMSIS < 0x20000U)` that
  CMock's lightweight parser doesn't evaluate the way a real C preprocessor
  does — it silently mocked the *wrong* API version (legacy v1 calls instead
  of the v2 calls this project actually uses), with no error, just missing
  symbols at link time.
- `main.h` pulls in ARM CMSIS core intrinsics and register-level HAL code
  that plain `gcc` on x86 can't compile at all.

Rather than fight the real headers, `test/vendor_stubs/cmsis_os.h` and
`test/vendor_stubs/main.h` are small, hand-written headers that declare
*only* the specific types/functions the Middleware layer actually calls
(`osMutexNew`, `osMutexAcquire`, `CAN_HandleTypeDef`, `HAL_GPIO_ReadPin`,
etc.) — copied verbatim from the real signatures. Because
`test/vendor_stubs` is listed **first** in `project.yml`'s `:include` path,
the compiler finds these instead of the real vendor headers when building
for tests. The real STM32CubeIDE target build never sees this directory —
it's test-only.

**If a new file under test needs another cmsis_os/HAL function or type**:
add its prototype to the relevant stub header (copy the real signature from
the vendor header) — don't try to mock the real header directly, you'll hit
the same parsing problems.

### Testing a `static` function without changing the source

`parse_inverter_status()` in `AMS_CAN_Task.c` is `static` (private to that
file) and takes a type (`CAN_RxPacket_t`) that's also defined privately
inside that same file. Per project decision, we do **not** modify
production code just to make something testable (no `STATIC` macro trick,
no new public getter added to the header).

Instead, `test_AMS_CAN_Task.c` does this:

```c
#include "Middleware/AMS_CAN_Task.c"   /* the .c file, not the .h */
```

`static` in C only restricts a symbol's visibility *across separate
translation units*. Once the preprocessor pastes `AMS_CAN_Task.c`'s text
directly into `test_AMS_CAN_Task.c`, they're the same translation unit —
`parse_inverter_status` and `CAN_RxPacket_t` are just as visible as anything
declared directly in the test file, with zero changes to the original file.

**The cost**: the *whole* file must now compile, not just the one function
we care about. Every dependency `AMS_CAN_Task.c` has — `main.h`,
`cmsis_os.h`, `AMS_Led_Task.h`, `AMS_can_driver.h`, `AMS_DataBroker.h` —
needs a working mock or stub, even though this test file never calls
`vd_CAN_Task_Init()` or `vd_CAN_Manager_TaskProcess()` (the functions that
actually use most of those). The linker still needs every symbol their
(uncompiled-but-present) object code references to resolve, even if nothing
in the test suite ever calls them.

**Do not** also add `TEST_SOURCE_FILE("Middleware/AMS_CAN_Task.c")` to a test
that already `#include`s the `.c` file directly — that would compile and
link the same file a second time, causing duplicate-symbol errors.

If you need this trick for another file later, first check whether it's
worth it: if the target function only needs one or two small mocks, it's
cheap. If it pulls in `main.h`/a big vendor header like CAN did, budget real
time for building the stub headers first (see previous section).

### `TEST_SOURCE_FILE(...)`

You'll see this macro at the top of `test_AMS_sensors.c` (implied via
Ceedling's normal convention), `test_AMS_DataBroker.c`, and
`test_AMS_Flash_Task.c`:

```c
TEST_SOURCE_FILE("Middleware/AMS_DataBroker.c")
```

Normally Ceedling figures out which `.c` file to compile alongside a test by
convention — `test_Foo.c` pairs with `Foo.c` because the test `#include`s
`Foo.h`. In this Ceedling version, that automatic matching didn't reliably
find files that live in a subdirectory (like `Core/Src/Middleware/`), and
failed *silently* — the build would compile fine but fail to *link*, with no
clear error pointing at the real cause. `TEST_SOURCE_FILE(...)` forces
Ceedling to compile that exact file into the test executable, sidestepping
the guessing. **Add this line to any new test file** rather than relying on
automatic detection — it's cheap insurance against the same confusing link
failure.

## Key CMock patterns used in this suite

- **`<function>_ExpectAndReturn(args..., return_value)`** — "I expect this
  exact call, with these exact argument values, and when it happens, make it
  return this."
- **`<function>_ExpectAnyArgsAndReturn(return_value)`** — same, but don't
  check the arguments at all (used for `osMutexNew()`, where we don't care
  about the attribute struct's exact contents, only that it was called and
  what handle it returns).
- **`<function>_IgnoreArg_<paramname>()`** — called right after an `_Expect`,
  relaxes the check on *one specific* argument while still checking the
  others. Used throughout for the mutex `timeout` argument — tests don't
  want to hardcode the exact tick conversion of `pdMS_TO_TICKS(50)`, just
  confirm *some* timeout was passed.
- **`<function>_ReturnThruPtr_<paramname>(&value)`** — for output-parameter
  functions (`bool b_Flash_ReadRecord(uint32_t addr, AMS_Flash_Record_t
  *p_out)`), this makes the mock write `value` into `*p_out` during the
  call, simulating "the flash driver read back this exact record."

## Order-sensitivity — read before adding new tests to an existing file

`test_AMS_DataBroker.c`, `test_AMS_Flash_Task.c`, and `test_AMS_gps_driver.c`
all test modules that keep real state in file-scope `static` variables
(mutex handles, the fault counter, `s_state` in the Flash Task, `s_phuart`
in the GPS driver). Unity does **not** restart the process between test
functions in the same file — those statics persist for the whole test
binary's run.

All three files have a comment at the top explaining exactly what must run
first and why (typically: a "before anything is initialized, everything
fails closed" test has to be the very first function declared, before any
other test calls the real `_Init()`). **Read that comment before reordering
or inserting a new test into any of these files** — Unity runs test functions in
the order they're declared in the file, not alphabetically, and this is
relied upon.

## Adding a new test file

1. Create `test/test_<Something>.c`.
2. `#include "unity.h"`, then any `mock_<Header>.h` you need (CMock
   generates one for any header you mock — pick the real dependencies of the
   file you're testing).
3. `#include` the real header of the file under test (or the real `.c` file
   directly, if you need to reach a `static` function — see above).
4. Add `TEST_SOURCE_FILE("Middleware/YourFile.c")` (skip this if you're
   `#include`-ing the `.c` file directly instead — see the "do not" note
   above).
5. Write `void setUp(void) {}` / `void tearDown(void) {}` (required by
   Unity, can stay empty) and your `void test_SomeDescriptiveName(void)`
   functions.
6. Run `./executeTests.sh test_Something` to iterate quickly, then
   `./executeTests.sh` to confirm nothing else broke before committing.
