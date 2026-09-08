# AMS_Algorithms_Task

## Overview
`AMS_Algorithms_Task` is the single home for every derived/calculated value in
the system: peak battery current, charge/SOC (Ah in-out), GPS telemetry
(odometer, speed, acceleration), and the LED state machine tick. It replaces
the earlier `AMS_Data_Calculator_Task`, which only handled GPS telemetry.

Raw acquisition tasks (ADC, CAN, GPS, a future BMS task) only ever write raw
Broker domains (`AMS_ADC_Data_t`, `AMS_BMS_Data_t`, `GPS_Data_t`, ...). This
task is where raw data becomes derived data — everything downstream (Logger,
dashboard, future safety logic) reads the derived output from here, not the
raw domains directly.

## Execution Model: Periodic Polling, Four Independent Sections
The task wakes up every `10ms` (`osDelay(10)`) and runs four sections in a
fixed order. Each section reads its own Broker domain, calls its own
single-responsibility Algorithms module, and writes its own output — they
share no state and don't call into each other. Deleting or reordering one
section has no effect on the others.

### 1. Current
Reads `AMS_BMS_Data_t.i32_pack_current_mA`, folds it through
`Algorithms/AMS_current_algorithms.c` (`b_CurrentCalc_Fold`) to track peak
discharge/charge current (session + lifetime), writes into
`AMS_BatteryStats_Data_t.current`.

### 2. Charge / SOC
Same input as Current, different question: `Algorithms/AMS_charge_algorithms.c`
(`b_ChargeCalc_Fold`) integrates current over time (coulomb counting) into Ah
discharged/charged (session + lifetime), writes into
`AMS_BatteryStats_Data_t.charge`.

**Known tradeoff:** this task polls the Broker's *latest* current sample once
per loop. The Broker only ever holds the latest value, so if a future BMS
task samples current faster than this loop's 10ms period, current transients
between polls are invisible to the Ah integral. If that turns out to matter
once real BMS hardware exists, move just these two sections into the BMS
task's own acquisition loop instead (it sees every sample with an exact
`dt`) — see `docs/Next_Steps.md`.

### 3. Telemetry
Reads `GPS_Data_t`, folds it through `Algorithms/AMS_telemetry_algorithms.c`
(`b_TelemetryCalc_ProcessFix`) into odometer distance, max/avg speed, and
max accel/decel (session + lifetime), writes into `AMS_Telemetry_Data_t`.
Also prints a one-line summary to the debug terminal on every new fix.

### 4. LED
Calls `vd_LED_Manager_Process()` to drive the ON/OFF/TOGGLE/BLINK state
machine (`Middleware/AMS_Led_Task.c`). No Broker domain of its own — purely
local state. Ticked here simply because this task already runs at a steady,
frequent rate; historically it lived here by accident rather than design,
now it's deliberate.

## Not included yet: Thermal
`Algorithms/AMS_thermal_algorithms.c` (max/min/avg/delta cell temperature)
isn't wired into this task. `AMS_BMS_Data_t` only carries one aggregate
`i16_max_cell_temp_cC` field today, not a per-cell array, and
`b_ThermalCalc_Fold()` needs the whole array. It slots in as a fifth
section, same shape as Current/Charge above, once `AMS_BMS_Data_t` carries
real per-cell readings — which needs the BMS hardware's actual cell count,
not yet chosen.
