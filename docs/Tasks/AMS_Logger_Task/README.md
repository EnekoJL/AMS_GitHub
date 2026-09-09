# AMS_Logger_Task

## Overview
The `AMS_Logger_Task` is a dual-purpose diagnostics and telemetry recording task. It handles appending real-time system data to an SD card for post-drive analysis, and it prints highly-formatted, ANSI-colored debug dashboards to the serial terminal.

## Execution Model: Periodic Polling
The task runs a continuous periodic loop (`osDelay(100)`) to check if logging or printing operations need to occur.

1. **SD Card Sub-Feature:** Executes every loop iteration (100ms default) to ensure fast data logging without blocking other tasks.
2. **Terminal Print Sub-Feature:** Gated by a separate timer (`LOGGER_PRINT_PERIOD_MS`), which limits serial output spam while allowing SD logging to happen much faster.

## Core Features
1. **SD-Card CSV Logging (`TASK_SD_CARD_ENABLE`):**
   * Automatically mounts the SD card using the FATFS driver upon initialization.
   * Searches for the next available filename (e.g., `gekko_01.csv`, `gekko_02.csv`) to prevent overwriting past logs.
   * Every `LOGGER_SD_PERIOD_MS` (default 100 ms, independent of the terminal print rate), fetches one full `AMS_Data_t` snapshot from the Data Broker via `b_Broker_Get_AllData()` — vehicle state, inverter RPM, GPS, telemetry (session + lifetime), BMS, derived battery stats (charge/thermal/current) — and appends it as one CSV row. That's all 8 Broker domains; see the CSV header string in `vd_Logger_Task_Init()` for the exact column order.
   * Each row also carries a fresh/stale flag per domain (`VEH_FRESH`, `ADC_FRESH`, `GPS_FRESH`, `BMS_FRESH`, `TELEM_FRESH`, `RPM_FRESH`, `BATTSTATS_FRESH` — 7 flags, one per `AMS_Safety_Flags_t` field; `AMS_Persistent_Config_t` has no freshness tracking, see `Architecture_Overview.md` §4). Slower-updating domains (e.g. BMS) repeat their last value across several rows between real updates — the flag tells you whether a given row's value is a new sample or a repeat, so post-processing doesn't misread a held-over value as a fresh reading. **This is the only place in the firmware any freshness flag is consumed today** — and even here it's written out as data for offline review, not branched on live (see `Architecture_Overview.md` §4's "Freshness-flag consumers" note).
   * `AMS_BMS_Data_t`'s per-cell voltage/temperature columns are real as of `AMS_BMS_Task` existing (250ms cadence, LTC6813 over SPI2) — no longer a placeholder. `BMS_PACK_MA` (pack current) and `BMS_SOC_X10` still read `0`: the LTC6813 doesn't measure pack current, and no CAN parser produces it yet (see `docs/Tasks/AMS_BMS_Task/README.md`).
   * Triggers a Green LED blink upon successful writes.
2. **Live Terminal Dashboard (`FEATURE_LOGGER_PRINT_ENABLE`):**
   * Acts as a live debugger, reading *every* struct available in the Data Broker via the individual `Get_*` calls (not `Get_AllData()`, so each domain's success/failure can be reported independently) and printing it in 7 numbered, colour-banner sections: `[1]` ADC, `[2]` Vehicle State (+ Powertrain/RPM), `[3]` GPS, `[4]` Telemetry, `[5]` BMS, `[6]` Battery Stats (derived), `[7]` Persistent Config — see `vd_Logger_PrintBrokerData()`. That's all 8 domains (Vehicle and Powertrain share section `[2]`, since they're both "current physical state" from a reader's point of view even though they're separate Broker structs/mutexes).
   * Uses VT100 ANSI escape codes (defined in `AMS_ansi_colors.h`) to draw styled boxes, distinct colored banners, and perfectly aligned key-value pairs.
   * Formats raw micro-degrees and millivolts into human-readable degrees and volts without using `printf` floating-point math (which consumes excessive stack space).
   * Unlike the SD-CSV path, the dashboard does **not** print the fresh/stale flags — each section only reports whether its `Get_*` call succeeded (mutex didn't time out), not whether the data it got back is stale. A domain that hasn't been updated in minutes prints identically to one updated milliseconds ago.
3. **Compile-Time Switches:**
   * Both sub-features are isolated by preprocessor macros defined in `AMS_task_config.h`. If either is disabled, the code compiles out entirely, saving flash and RAM.
