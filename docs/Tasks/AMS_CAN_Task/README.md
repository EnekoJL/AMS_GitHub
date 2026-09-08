# AMS_CAN_Task

## Overview
The `AMS_CAN_Task` manages bidirectional communication over the CAN2 bus. It handles parsing incoming CAN frames (such as Inverter telemetry) and dispatching outgoing CAN frames safely within the FreeRTOS environment.

## Execution Model: Queue-Driven
The reception mechanism is strictly event-driven to avoid missing fast CAN frames, while the processing loop runs periodically to drain the queue and handle on-demand transmissions.

1. **Hardware Reception:** The CAN peripheral receives a frame that passes the configured hardware filters (e.g., Bank 14, ID `0x181`).
2. **ISR Execution:** The `HAL_CAN_RxFifo0MsgPendingCallback` fires.
3. **Queue Push:** The ISR packages the raw CAN ID and payload into a `CAN_RxPacket_t` and pushes it into a FreeRTOS `osMessageQueue` using zero timeout (ISR-safe).
4. **Task Processing:** The task periodically (`osDelay(20)`) checks the queue using a non-blocking read (`timeout=0`), parsing all available frames before yielding.

## Core Features
1. **Inverter Status Parsing:** 
   * Specifically listens for Standard ID `0x181` (Inverter Status).
   * Extracts the current Motor RPM (Little Endian, signed 16-bit integer).
   * Safely updates the `AMS_Powertrain_Data_t` struct within the Data Broker (own domain/mutex, single writer: `AMS_CAN_Task`).
2. **Hardware Filtering:** 
   * Utilizes STM32 CAN hardware filters to reject unwanted CAN IDs at the silicon level, preventing unnecessary CPU wakeups and queue flooding.
3. **On-Demand Transmission (Debugging):** 
   * Monitors the STM32 Blue Button. When pressed, it queues an outgoing CAN frame (`StdId=0x201`) containing diagnostic data.
   * Blinks the BLUE LED via `AMS_Led_Task` upon successful transmission.
