# AMS_Led_Task

## Overview
The `AMS_Led_Task` serves as the central manager for all onboard status LEDs. Rather than letting various tasks toggle GPIO pins directly (which leads to uncoordinated flashing and delays), tasks ask the LED manager to change a specific LED's state.

## Execution Model: Periodic State Machine
The task is driven by the `vd_LED_Manager_Process()` function, which is designed to be called periodically (either in its own FreeRTOS thread or integrated into the main loop of another supervisory task). 

1. **Iteration:** It loops through every configured `AMS_LED_Color_t` channel.
2. **Evaluation:** It checks the current requested mode (`LED_PIN_ON`, `LED_PIN_OFF`, `LED_PIN_TOGGLE`, `LED_PIN_BLINK`) against the system tick counter (`HAL_GetTick()`).
3. **Execution:** It toggles or turns off the LED if an internal deadline has been reached.

## Core Features
1. **Independent Channels:** 
   * Each LED (Green, Red, Blue, Orange) maintains its own state, toggle timer, and blink deadline.
   * This allows the Blue LED to blink rapidly while the Green LED is held solid ON, with zero blocking code (`osDelay`) preventing the other from functioning.
2. **Modes of Operation:**
   * **ON / OFF:** Static state holding.
   * **TOGGLE:** Continuous blinking at a fixed frequency (e.g., 500ms).
   * **BLINK:** A one-shot flash. The LED is turned ON, and after a predefined timeout specific to that color, it automatically turns OFF. This is extensively used for "Heartbeat" confirmations in the ADC and CAN tasks.
3. **Decoupled Architecture:**
   * Other tasks only call `vd_LED_Manager_SetMode`. They never touch the HAL GPIO functions, ensuring that hardware changes only require updates in the LED driver, not across the entire application layer.
