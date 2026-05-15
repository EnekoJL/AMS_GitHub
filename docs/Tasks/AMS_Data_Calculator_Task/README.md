# AMS_Data_Calculator_Task

## Overview
The `AMS_Data_Calculator_Task` is responsible for generating advanced telemetry metrics by analyzing GPS data over time. It continuously tracks the vehicle's position, speed, and time elapsed to calculate distance, averages, and extreme metrics (max speed, acceleration).

## Execution Model: Periodic Polling
Unlike the raw GPS reception task, this calculator operates on a strict polling loop to process accumulated state.

1. **Periodic Loop:** The task wakes up every `10ms` (`osDelay(10)`).
2. **State Verification:** It polls the Data Broker for the latest `GPS_Data_t`.
3. **Tick Comparison:** It checks if `ui32_last_fix_tick_ms` has changed since the last loop iteration. If not, no new GPS data has arrived, and it goes back to sleep.
4. **Calculations:** If new data is present, it computes the delta time (`dt`) and updates its internal accumulators.

## Core Features
1. **Distance Tracking (Odometer):** 
   * Calculates distance traveled in the latest timestep (`dt`) using the current velocity.
   * Accumulates this into `ui32_total_distance_m`.
2. **Speed Analytics:**
   * Tracks the maximum speed reached (`i32_max_vel_kmh_x1000`).
   * Accumulates speed samples to maintain an accurate average speed (`i32_avg_vel_kmh_x1000`).
3. **Acceleration / Deceleration Tracking:**
   * Derives acceleration by comparing the current velocity with the velocity from the previous tick over the measured time delta.
   * Tracks peak positive acceleration (`max_accel`) and peak negative acceleration (`max_decel`) in $m/s^2$.
4. **Broker Update & Output:**
   * Writes the calculated struct (`AMS_Telemetry_Data_t`) safely to the Data Broker.
   * Formats and prints the results over the serial terminal.
