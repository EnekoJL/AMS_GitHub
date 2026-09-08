# Next Steps

Last updated: 2026-07-06, branch `feat/Tasks_FreeRTOS` (commit `f2adb07`).

Snapshot of where things stand and what's queued up next, so a session can
pick this back up without re-deriving context.

## What's done (this round of work)

- Data Broker: safety-tier types (`AMS_BMS_Data_t` placeholder,
  `AMS_Safety_Flags_t` freshness flags, `AMS_Data_t` full-snapshot
  aggregate), bounded mutex timeout (50ms) + fault counter + RED LED
  indicator instead of `osWaitForever` hangs.
- `Architecture_Overview.md`: ownership table (struct -> single writer ->
  readers -> max-age), Pub/Sub renamed to Shared-State/Blackboard with
  reasoning, mutex-ordering rule documented, Broker quickstart for new devs.
- SD-card CSV logger: one row per `AMS_Data_t` snapshot (vehicle, GPS,
  telemetry, BMS placeholder) instead of just 4 vehicle fields, with a
  fresh/stale flag per domain per row, decoupled `LOGGER_SD_PERIOD_MS` from
  the terminal print rate.
- Unit test suite (Unity + CMock via Ceedling): 48 tests across sensors,
  Broker, Flash Task, CAN parsing, GPS conversion, telemetry math. Run with
  `./executeTests.sh`. Full writeup in `docs/Testing.md`.
- Refactored `AMS_GPS_Task.c` and `AMS_Data_Calculator_Task.c` to pull their
  real logic (NMEA frame conversion, distance/speed/accel math) out of the
  task's `for(;;)` loop into pure, testable functions
  (`Algorithms/AMS_gps_algorithms.c`, `Algorithms/AMS_telemetry_algorithms.c`)
  — SRP compliance + made previously-untestable code testable.
- Fixed `Algorithms_Sensors_ProcessVoltages()` reading hardcoded STM32 ROM
  addresses directly (would've segfaulted on host, violated the
  hardware-free Domain Logic rule) — now takes calibration values as params.
- Fixed the Flash boot-order race: `vd_Persist_Task_Init()` now runs from
  `main()` before `osKernelStart()` instead of from inside
  `Flash_Memory_Start()`'s task body, so `AMS_Persistent_Config_t` is loaded
  before any other task can read it. Doc and code now agree
  (`AMS_Flash_Task/README.md`, `AMS_Flash_Task.h`).

## Open items, in recommended order

### 1. CI — tests aren't run automatically

`./executeTests.sh` only runs when someone remembers to run it. Nothing
gates a PR or push. A GitHub Actions workflow (`ubuntu-latest`, install
Ruby + `gem install ceedling`, run `ceedling test:all`) would close this
cheaply — no self-hosted runner needed, this is a host-side build.

### 2. Real BMS producer task

The whole safety-tier push this session was motivated by wanting a real
place for battery/BMS safety data — but `AMS_BMS_Data_t` is still a
placeholder with no producer. Nothing calls `b_Broker_Update_BMSData()`.
This is a hardware/product decision (what BMS chip, CAN IDs or SPI/UART
interface, what fault bits actually mean) more than a pure coding task —
needs your input on the real BMS interface before it can be built.

## Smaller/lower-priority gaps

- Task **loop wrappers** aren't unit-tested, only the pure math extracted
  out of them: GPS's queue-drain loop, CAN's queue-drain + button-TX loop,
  ADC's ISR/shadow-buffer handling, the LED state machine. All of
  `AMS_Led_Task.c` is currently untested.
- `b_Persist_SaveConfig()`'s real write path (erase + write + CRC
  construction) isn't exercised by tests — `FEATURE_FLASH_WRITE_ENABLE` is
  `0` in the current build, so it always short-circuits. Would need a
  per-test define override in `project.yml` to test the enabled path.
- The one-writer-per-struct rule and the mutex-ordering rule
  (`Architecture_Overview.md`) are documented conventions, not enforced by
  code or a lint check — relies on developer discipline.
