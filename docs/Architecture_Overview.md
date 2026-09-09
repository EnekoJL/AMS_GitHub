# System Architecture Overview

This project follows a strict layered architecture pattern based on SOLID principles, utilizing FreeRTOS for task management and a Shared-State (Blackboard) model for inter-task communication.

> **Naming note:** earlier revisions of this doc called this a "Publisher/Subscriber" model. It isn't one — there is no topic subscription and no notify-on-change. It's a mutex-protected shared repository: producers write a snapshot, consumers read a copy. That's the right tool for *state* data (latest battery voltage, latest position) as opposed to *event* data (a CAN frame arriving, a GPS burst finishing) — event data already goes through real FreeRTOS Queues/Notifications, see Section 5.

## 1. Architectural Layers

The architecture is designed to decouple the hardware from the business logic, ensuring testability and modularity.

1. **Hardware Abstraction Layer (HAL) / Drivers**:
   - Contains custom drivers (`Drivers_Custom/`) that interface directly with the STM32 peripherals (ADC, CAN, UART, SDIO).
   - Isolates the RTOS tasks from register-level operations.

2. **Middleware Layer (FreeRTOS Tasks)**:
   - Contains the core application threads (`AMS_ADC_Task`, `AMS_CAN_Task`, etc.).
   - Each task runs as an independent infinite loop.
   - Tasks consume driver APIs and communicate *exclusively* via the Data Broker.
   - They do not communicate directly with each other to prevent tight coupling.

3. **Data Broker (Shared-State / Blackboard)**:
   - The central nervous system of the firmware (`AMS_DataBroker.c/h`).
   - All vehicle data (`Vehicle_Data_t`), telemetry (`AMS_Telemetry_Data_t`), and sensor readings (`AMS_ADC_Data_t`) are encapsulated here as static private variables.

4. **Domain Logic (Algorithms)**:
   - Pure C algorithms that have zero dependencies on hardware or FreeRTOS (`#include "FreeRTOS.h"` is forbidden here).
   - They process data and return results. Example: Voltage conversion, telemetry calculations.

## 2. Inter-Task Communication (Data Broker)

To avoid race conditions and ensure thread safety across FreeRTOS tasks, **global variables are strictly prohibited**.

Instead, tasks must use the **Data Broker**:

* **Thread-Safety:** Every getter and setter in the Data Broker uses a FreeRTOS Mutex (`osMutexAcquire` / `osMutexRelease`).
* **Fast Execution:** The Mutex is held only for the duration of a memory copy (`memcpy`), ensuring no task is blocked for an extended period.
* **Usage Pattern:**
  1. A task reads a copy of a struct via a Getter (e.g., `b_Broker_Get_VehicleState(&local_veh)`).
  2. The task modifies its local copy.
  3. The task writes the entire struct back via a Setter (e.g., `b_Broker_Update_VehicleState(&local_veh)`).

## 3. Broker Quickstart (for new developers)

If you take nothing else from this doc: **the Broker holds the real data privately. You never touch it directly — you always get your own copy first.**

### The four-step pattern

1. A task calls a Getter, e.g. `b_Broker_Get_VehicleState(&local_copy)`.
2. Internally, the Broker locks that struct's mutex, `memcpy`s its private data into `local_copy`, then unlocks. The lock is only held for that copy — microseconds.
3. From that point on, `local_copy` is **yours**. It's an ordinary local variable. No lock, no Broker involvement, nobody else can see it. Read it, modify it, whatever you like.
4. If you want to write your changes back, call the matching Setter, e.g. `b_Broker_Update_VehicleState(&local_copy)` — same lock → copy → unlock, in the other direction.

That's the entire mechanism. Every Getter/Setter pair in `AMS_DataBroker.c` follows exactly this shape.

### Getting everything at once: `AMS_Data_t`

`AMS_Data_t` bundles every domain into one struct, so you can fetch a full snapshot in one call instead of five:

```c
AMS_Data_t prototype_1;
b_Broker_Get_AllData(&prototype_1);          // fills the whole snapshot

uint32_t batt_mV = prototype_1.vehicle.bateria_12v_mV;
int32_t  speed    = prototype_1.gps.i32_vel_kmh_x1000;
bool     gps_ok   = prototype_1.safety.b_gps_data_fresh;   // check before trusting stale data
```

Field names on `AMS_Data_t` are: `vehicle`, `bms`, `sensors`, `gps`, `telemetry`, `powertrain`, `battery_stats`, `safety` — check `AMS_DataStructs.h` for what's inside each one before guessing a field name (e.g. `Vehicle_Data_t` has no `speed` field; speed lives in `gps.i32_vel_kmh_x1000`, RPM lives in `powertrain.inverter_rpm`, and Ah/thermal/peak-current stats live in `battery_stats.{charge,thermal,current}`, not `bms`).

Two things to know:

* **You must call `b_Broker_Get_AllData()` before reading anything.** Declaring `AMS_Data_t prototype_1;` alone gives you an empty/garbage struct — it is not automatically wired to the Broker. Call it again any time you want fresher values.
* **`AMS_Data_t` needs no mutex of its own.** It's just a plain struct sitting in your own stack/memory. `b_Broker_Get_AllData()` internally calls the five existing Getters one after another, each briefly using its own already-existing mutex — no new locks are created. Because it copies domain-by-domain rather than locking everything at once, the result is not a perfectly atomic instant — vehicle data might be copied a few microseconds before gps data. Fine for dashboards/logging. If you ever need true all-or-nothing consistency across domains, that's a bigger design question — ask before assuming `Get_AllData` gives you that.

## 4. Data Ownership & Safety Tiering

Every struct in the Broker has exactly **one writer task**. Any other task may read it. This table is the single source of truth for "who is allowed to call which Setter" — if you're adding a new field or a new task, check here first, and update this table when you change ownership.

| Struct | Domain | Writer (only this task may call `Update_*`) | Typical readers | Max age before "stale" |
|---|---|---|---|---|
| `Vehicle_Data_t` | Physical vehicle state (battery, suspension) | `AMS_ADC_Task` | Logger | 300 ms |
| `AMS_Powertrain_Data_t` | Inverter RPM (from CAN) | `AMS_CAN_Task` | Logger | 300 ms |
| `AMS_ADC_Data_t` | Raw + converted ADC readings | `AMS_ADC_Task` | Logger | 300 ms |
| `GPS_Data_t` | Parsed NMEA position/speed | `AMS_GPS_Task` | Algorithms Task, Logger | 2000 ms |
| `AMS_Telemetry_Data_t` | Derived GPS metrics (distance, max speed, accel, session + lifetime) | `AMS_Algorithms_Task` | Logger | 300 ms |
| `AMS_BMS_Data_t` | Battery pack safety data — per-cell voltage/temp arrays, fault flags. `i32_pack_current_mA`/`soc_percent_x10` still placeholders — pack current arrives over CAN, no parser yet, and per one-writer-per-struct it must NOT be written here once one exists (see the WARNING on this struct in AMS_DataStructs.h) | `AMS_BMS_Task` (LTC6813 over SPI2) | Algorithms Task, Logger | 500 ms |
| `AMS_BatteryStats_Data_t` | Derived Ah in/out, thermal extremes, peak current — composes `AMS_ChargeStats_t`/`AMS_ThermalStats_t`/`AMS_CurrentStats_t`, each the output of one Algorithms module (`AMS_charge_algorithms.c`, `AMS_thermal_algorithms.c`, `AMS_current_algorithms.c`) | `AMS_Algorithms_Task` (current/charge still fold zero — no pack-current producer; thermal real as of `AMS_BMS_Task` — see that task's README) | Logger | 1500 ms |
| `AMS_Persistent_Config_t` | Flash-backed config (SOC, cycle count) | `AMS_Flash_Task` | Logger | n/a (see Flash Task doc) |

**Every struct has exactly one writer today.** `Vehicle_Data_t` and
`AMS_Powertrain_Data_t` used to be one struct (`Vehicle_Data_t` with an
`inverter_rpm` field) written by both `AMS_ADC_Task` and `AMS_CAN_Task`. That
was a **real bug**, not a safe exception: the mutex is released *between*
a task's Get and its matching Update (see the four-step pattern in Section 3),
so a CAN write landing in that window while ADC held its local copy would be
silently overwritten and lost on ADC's write-back. Splitting `inverter_rpm`
into its own struct/mutex (`AMS_Powertrain_Data_t`) removed the second writer
entirely instead of trying to synchronize it — if you're ever tempted to add
a second writer to an existing struct, split the struct instead.

### Safety flags (freshness)

`b_Broker_Get_SafetyFlags()` returns an `AMS_Safety_Flags_t` — one `fresh`/`stale` bool per domain above, computed by comparing each domain's last-write timestamp against its "max age" column. **This is flag-only**: the Broker does not shut anything down, override a task, or take any action when a domain goes stale — it only reports it. Each consuming task decides what a stale flag means for it (log a warning, hold last known value, refuse to act, etc). `AMS_BMS_Data_t` reads fresh once `AMS_BMS_Task` is enabled (`TASK_BMS_ENABLE` in `AMS_task_config.h`, default off until bench-tested).

`b_Broker_Get_AllData()` fetches every domain (vehicle, bms, sensors, gps, telemetry, powertrain, battery_stats, safety) in one call, as an `AMS_Data_t`. It does **not** introduce a single global lock — internally it just calls each individual Getter in sequence, so it is not an atomic all-or-nothing snapshot across domains.

### Mutex-ordering rule

Never acquire a second Broker mutex while already holding one (e.g. don't call `b_Broker_Get_GPSData()` from inside a block where you're still holding the lock from `b_Broker_Get_VehicleState()` — release first, then call the next Getter). No code today violates this, but nothing enforces it either — as more tasks are added, holding two locks in inconsistent order across two call sites is the classic path to a deadlock. If you ever need two structs "atomically," ask before implementing — it needs a real design decision, not just grabbing both mutexes.

## 5. General Execution Models

The tasks in this project generally follow one of two execution models:

* **Event-Driven (Interrupt -> Queue/Notify -> Task):**
  - Tasks like **ADC**, **CAN**, and **GPS** spend most of their time asleep.
  - When hardware receives data, an Interrupt Service Routine (ISR) fires.
  - The ISR safely wakes the task using `vTaskNotifyGiveFromISR` or posts data to a FreeRTOS Queue using `osMessageQueuePut`.
  - The task wakes up, processes the data, updates the Broker, and goes back to sleep.
  - This ensures maximum CPU efficiency and zero polling.

* **Periodic Polling:**
  - Tasks like **Logger** and **Algorithms** (which also drives the LED state machine — see its section 4) wake up at fixed intervals using `osDelay()`.
  - They check the latest state from the Broker and perform their operations (e.g., blinking an LED, saving a file to the SD card).
