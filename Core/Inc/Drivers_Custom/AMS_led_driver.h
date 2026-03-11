/**
 * @file    AMS_led_driver.h
 * @brief   Driver Físico / HAL para los LEDs de la Discovery STM32F469
 * @date    11 de Marzo de 2026
 */
#ifndef LED_DRIVER_H_
#define LED_DRIVER_H_

#include <stdbool.h>

void vd_LED_Driver_Init(void);
void vd_LED_Driver_SetAll(bool state);
void vd_LED_Driver_ToggleAll(void);

void vd_LED_Driver_SetGreen(bool state);
void vd_LED_Driver_ToggleGreen(void);

void vd_LED_Driver_SetOrange(bool state);
void vd_LED_Driver_ToggleOrange(void);

void vd_LED_Driver_SetRed(bool state);
void vd_LED_Driver_ToggleRed(void);

void vd_LED_Driver_SetBlue(bool state);
void vd_LED_Driver_ToggleBlue(void);

#endif /* LED_DRIVER_H_ */
