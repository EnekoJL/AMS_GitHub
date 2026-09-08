# Next Steps

Last updated: 2026-09-08, branch `claude_code`.

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
- Architecture audit (Opus-model agent review of every Broker/task/Algorithm
  source file against the docs) confirmed the shared-state Broker pattern is
  the right call for this workload — recommended against pub/sub, per-pair
  queues, or a real embedded DB. Fixed everything the audit found:
  - **`Vehicle_Data_t` lost-update race** (real bug, not the "safe" exception
    the doc claimed): split `inverter_rpm` into its own domain/mutex,
    `AMS_Powertrain_Data_t`, written only by `AMS_CAN_Task`. Restores
    one-writer-per-struct with no exceptions.
  - **GPS on USART3 would hard-fault**: `AMS_gps_driver.c` unconditionally
    called `HAL_UARTEx_ReceiveToIdle_DMA`, which dereferences a NULL
    `hdmarx` on USART3 (no DMA stream wired there — see
    `stm32f4xx_hal_msp.c`). Driver now checks `hdmarx` and falls back to
    `HAL_UARTEx_ReceiveToIdle_IT()` automatically. USART3 stays wired for
    bench testing (NMEA fed over the ST-LINK USB/VCP from a PC) — **swap to
    `&huart6` in `main.c`'s `GPS_Start_Task()` before flashing onto the
    bike** (commented at the call site).
  - **Uninitialized-struct writes on Broker read failure** in
    `AMS_CAN_Task.c` and `AMS_GPS_Task.c` — fixed by zero-initializing and
    checking the return value before using the data.
  - **`vd_LED_Manager_Init()` was never called** — all 4 LEDs stayed ON from
    boot (active-low + CubeMX leaves pins RESET). Now called from `main()`.
  - **Task stacks were 512B while calling `printf`** (which can use
    600-1500B) with no overflow detection at all. Bumped to 1024B (2048B for
    the Logger/dashboard task, unchanged), enabled
    `configCHECK_FOR_STACK_OVERFLOW`, added a hook that lights the RED LED
    and halts instead of corrupting adjacent RAM silently.
  - Cut Broker Get/Update boilerplate via two shared static helpers
    (`prv_Broker_Read`/`prv_Broker_Write`) — every domain's function body is
    now ~1 line instead of ~15 duplicated ones.
  - Switched the Broker's mutex from recursive to plain
    `osMutexPrioInherit` — nothing recurses, and staying non-recursive means
    a future bug that nests two calls on the same domain deadlocks loudly
    instead of silently "working."
  - Thread creation in `main.c` now respects `AMS_task_config.h`'s
    `TASK_*_ENABLE` flags (previously all 6 threads were always created;
    each disabled one just exited immediately on first run).
  - 3 doc/code mismatches fixed: README's LED polarity claim (was
    "active-high", code is active-low), Flash task README's magic-word
    constant (was `0xDEADBEEF`, code is `0xAEC01AD0`), Broker header's stale
    "(Pub/Sub pattern)" comment.
  - Test suite: 50/50 passing (2 new tests for the Powertrain domain split).
- Battery-stats calculator layer, SRP-decomposed per an architecture-design
  agent's proposal (each module owns one accumulator struct + one pure fold
  function, no Broker/RTOS calls inside — same shape as
  `AMS_telemetry_algorithms.c`):
  - `Algorithms/AMS_charge_algorithms.c` — Ah discharged/charged, session +
    lifetime (coulomb counting, mA*ms integration to avoid per-sample
    truncation).
  - `Algorithms/AMS_thermal_algorithms.c` — max/min/avg/delta cell temp,
    this-sample and session-worst. Session avg is a TIME-average of the
    spatial mean, not the same number as the this-sample spatial mean —
    named apart deliberately, see the struct comment.
  - `Algorithms/AMS_current_algorithms.c` — max discharge/charge current,
    session + lifetime.
  - Composed into a new placeholder Broker domain, `AMS_BatteryStats_Data_t`
    (own mutex, 1500ms max-age, `b_battery_stats_fresh` flag) — same
    placeholder status as `AMS_BMS_Data_t`, waiting on the BMS task below.
  - Extended `AMS_Telemetry_Data_t`/`AMS_TelemetryAccumulator_t` with
    lifetime distance/max-speed/max-accel/max-decel alongside the existing
    session fields (`vd_TelemetryCalc_SeedLifetime()`) — live today
    (lifetime == session until flash seeding exists, see below).
  - **Deliberately NOT done this round:** seeding any lifetime baseline
    from flash (`AMS_Persistent_Config_t`'s schema for historic stats isn't
    decided — see item #3 below). Every seed function
    (`vd_ChargeCalc_SeedLifetime`, `vd_CurrentCalc_SeedLifetime`,
    `vd_TelemetryCalc_SeedLifetime`) exists and is tested in isolation,
    just not called from production code yet — grep for "TODO" in
    `AMS_Algorithms_Task.c` and the three new Algorithms headers for the
    exact plug-in points.
  - Test suite: 70/70 passing (20 new: 6 charge, 4 thermal, 5 current, 4
    telemetry-lifetime, 1 battery-stats Broker roundtrip).
- Consolidated `AMS_Data_Calculator_Task` (GPS telemetry only) into a new
  `AMS_Algorithms_Task` — one task, four clearly separated, independent
  sections: Current, Charge/SOC, Telemetry, LED (see
  `docs/Tasks/AMS_Algorithms_Task/README.md`). Current and Charge/SOC now
  fold from the Broker's latest `AMS_BMS_Data_t.i32_pack_current_mA`
  directly in this task, rather than waiting for a future BMS task's own
  loop as originally proposed — accepted tradeoff, documented in the new
  task's file header and README: this only sees the Broker's latest
  current sample once per 10ms poll, so if a future BMS task ends up
  sampling current faster than that, transients between polls won't be
  integrated. Revisit if that turns out to matter once real BMS hardware
  exists. Thermal is still not wired anywhere (see item #2 below — needs a
  per-cell array `AMS_BMS_Data_t` doesn't have yet).

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

Current and Charge/SOC (`b_CurrentCalc_Fold`, `b_ChargeCalc_Fold`) already
fold inside `AMS_Algorithms_Task` today, reading the Broker's latest
`AMS_BMS_Data_t.i32_pack_current_mA` — so once this task writes real
current samples, those two just start producing real numbers, no further
wiring needed. Thermal (`b_ThermalCalc_Fold`) is the one still unwired: it
needs a per-cell temperature array, and `AMS_BMS_Data_t` only has one
aggregate `i16_max_cell_temp_cC` field today — extend the struct with a
real per-cell array (sized to the actual chosen hardware's cell count) and
add a fifth section to `AMS_Algorithms_Task`, same shape as Current/Charge.

### 3. Flash schema for historic/lifetime stats

`AMS_Persistent_Config_t` still only has `soc_percent_x10` and
`cycle_count`. Lifetime distance/max-speed/max-accel/max-decel (telemetry)
and lifetime Ah/max-discharge-current (battery stats) all have working,
tested `SeedLifetime()` functions ready to consume baseline values — none
are called yet because the flash record layout to carry them isn't decided.
Two real gotchas to handle when this gets built (raised during design, not
yet hit in code since nothing writes these fields today):
- Growing `AMS_Persistent_Config_t` past two `uint16_t`s introduces struct
  padding — `AMS_Flash_Task.c`'s CRC32 runs over raw `sizeof(...)` bytes,
  so un-initialized padding will eventually produce a CRC mismatch on
  read-back. Pack the struct explicitly (or hand-pad) before adding fields.
- Growing the record size invalidates the byte-stride `prv_scan_sector()`
  uses to walk existing flash records — needs a one-time version-mismatch
  detection + erase-and-reset in `vd_Persist_Init()` before this ships.
`FEATURE_FLASH_WRITE_ENABLE` is `0` today, so there's no rush, but don't
skip the packing/versioning care just because writes are currently disabled.

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
