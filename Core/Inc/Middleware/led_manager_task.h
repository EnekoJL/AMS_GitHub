/**
 * @file    led_manager_task.h
 * @brief   LED Manager - independent per-LED control (ON / TOGGLE / BLINK).
 * @date    17 de Marzo de 2026
 *
 * Each LED (green, red, blue, orange) is controlled independently.
 * Multiple LEDs can be active simultaneously in different modes.
 *
 * Usage:
 *   vd_LED_Manager_SetMode(LED_COLOR_BLUE,  LED_PIN_BLINK);   // 200 ms flash
 *   vd_LED_Manager_SetMode(LED_COLOR_GREEN, LED_PIN_ON);      // stays on
 *   vd_LED_Manager_SetMode(LED_COLOR_RED,   LED_PIN_TOGGLE);  // toggles 500 ms
 *   vd_LED_Manager_SetMode(LED_COLOR_BLUE,  LED_PIN_OFF);     // stop blue
 */
#ifndef LED_MANAGER_TASK_H_
#define LED_MANAGER_TASK_H_

#include "AMS_DataTypes.h"

/** Initialise internal state and hardware. Call once at boot. */
void vd_LED_Manager_Init(void);

/** Set the mode of one individual LED. Can be called at any time. */
void vd_LED_Manager_SetMode(AMS_LED_Color_t color, AMS_LED_PinMode_t mode);

/** Drive the LED state machine. Must be called every iteration of the main loop. */
void vd_LED_Manager_Process(void);

#endif /* LED_MANAGER_TASK_H_ */
