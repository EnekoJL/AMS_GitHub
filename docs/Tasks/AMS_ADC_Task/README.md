# AMS_ADC_Task

## Overview
The `AMS_ADC_Task` is an event-driven RTOS task responsible for handling raw analog data, converting it to physical units, and securely updating the Data Broker. It processes voltages from the 12V battery, suspension travel sensors, the internal MCU temperature sensor, and the internal voltage reference (`VREFINT`).

## Execution Model: Event-Driven
This task is purely event-driven and does not use `osDelay()`. 

1. **Hardware Trigger:** Timers trigger the ADC conversions via DMA.
2. **ISR Execution:** When the DMA completes a scan, `HAL_ADC_ConvCpltCallback` executes in ISR context.
3. **Shadow Buffer & Wakeup:** The ISR safely copies the data into an atomic shadow buffer and wakes this task using FreeRTOS notifications (`vTaskNotifyGiveFromISR`).
4. **Task Processing:** The task wakes from `ulTaskNotifyTake()`, pops the raw data, processes it, and updates the Data Broker.

## Core Features
1. **Raw to Voltage Conversion:** 
   * Uses domain algorithms (`Algorithms_Sensors_ProcessVoltages`) to convert raw 12-bit ADC counts into accurate millivolt readings.
   * Leverages the factory-calibrated `VREFINT` to compensate for supply voltage fluctuations, ensuring high-precision readings.
2. **Voltage to Physical Units:**
   * Converts the calculated millivolts into usable physical units (e.g., suspension travel in millimeters, MCU temperature in degrees Celsius).
3. **Broker Synchronization:** 
   * Safely fetches the current ADC state from the Broker using a Mutex.
   * Updates only the necessary fields and writes the struct back to the Broker.
4. **Heartbeat Indication:**
   * Upon every successful processing cycle, it signals the `AMS_Led_Task` to blink the ORANGE LED, serving as a live heartbeat monitor for the ADC subsystem.
