# System Architecture Overview

This project follows a strict layered architecture pattern based on SOLID principles, utilizing FreeRTOS for task management and a Publisher/Subscriber model for inter-task communication.

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

3. **Data Broker (Pub/Sub)**:
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

## 3. General Execution Models

The tasks in this project generally follow one of two execution models:

* **Event-Driven (Interrupt -> Queue/Notify -> Task):**
  - Tasks like **ADC**, **CAN**, and **GPS** spend most of their time asleep.
  - When hardware receives data, an Interrupt Service Routine (ISR) fires.
  - The ISR safely wakes the task using `vTaskNotifyGiveFromISR` or posts data to a FreeRTOS Queue using `osMessageQueuePut`.
  - The task wakes up, processes the data, updates the Broker, and goes back to sleep.
  - This ensures maximum CPU efficiency and zero polling.

* **Periodic Polling:**
  - Tasks like **LED**, **Logger**, and **Data Calculator** wake up at fixed intervals using `osDelay()`.
  - They check the latest state from the Broker and perform their operations (e.g., blinking an LED, saving a file to the SD card).
