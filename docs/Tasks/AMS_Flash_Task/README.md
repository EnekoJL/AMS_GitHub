# AMS_Flash_Task

## Overview
The `AMS_Flash_Task` is the persistent storage manager. It ensures critical data—such as the battery's State of Charge (SOC) and lifecycle metrics—survives power cycles. It employs a custom wear-leveling algorithm designed specifically for the STM32's internal flash memory structure.

## Execution Model: Boot Recovery & Periodic
1. **Boot Initialization (`vd_Persist_Task_Init`):** 
   * Before the main FreeRTOS scheduler even starts, the system scans both allocated flash sectors.
   * It determines the most recent valid record (verifying Magic Word and CRC) and loads the `AMS_Persistent_Config_t` into the Data Broker.
2. **Periodic Saving:** 
   * The task runs a slow, periodic loop (`osDelay(1000)`) which serves as a placeholder for periodic or event-driven saves (e.g., saving SOC only when it drops by 1% to save flash cycles).

## Core Features
1. **Circular Ping-Pong Buffer Strategy:** 
   * STM32 flash sectors are large (e.g., 128KB) and erasing them degrades hardware life.
   * The task uses two 128KB sectors (e.g., S22 and S23 on Bank 2).
   * Records are appended sequentially into empty space (without erasing).
   * Only when Sector A is completely full is Sector B erased, and writing jumps to the fresh sector. This splits wear evenly and allows for millions of save cycles.
2. **Data Integrity (CRC32):**
   * Every 12-byte record contains a Magic Word (`0xDEADBEEF`), the actual config data, and a custom CRC32 checksum.
   * If a power loss occurs mid-write and corrupts the last slot, the scanner simply ignores it because the CRC will fail, falling back to the previous valid slot.
3. **Flash Write Toggles:**
   * Write operations can be dynamically disabled via the `FEATURE_FLASH_WRITE_ENABLE` macro in `AMS_task_config.h` during testing to prevent unnecessary flash wear.
