/**
 * @file    led_driver.c
 * @brief   Implementación del Driver Físico para LEDs
 * @date    11 de Marzo de 2026
 */

#include "Drivers_Custom/AMS_led_driver.h"
#include "main.h" // Para HAL_GPIO

/*
 * Mapeo de la Discovery STM32F469I:
 * GREEN  (LED1) -> PG6
 * ORANGE (LED2) -> PD4
 * RED    (LED3) -> PD5
 * BLUE   (LED4) -> PK3
 * Configuración -> PUSH-PULL (Activo BAJO / RESET = ON)
 */

void vd_LED_Driver_Init(void) {
    vd_LED_Driver_SetAll(false);
}

void vd_LED_Driver_SetAll(bool state) {
    vd_LED_Driver_SetGreen(state);
    vd_LED_Driver_SetOrange(state);
    vd_LED_Driver_SetRed(state);
    vd_LED_Driver_SetBlue(state);
}

void vd_LED_Driver_ToggleAll(void) {
    vd_LED_Driver_ToggleGreen();
    vd_LED_Driver_ToggleOrange();
    vd_LED_Driver_ToggleRed();
    vd_LED_Driver_ToggleBlue();
}

void vd_LED_Driver_SetGreen(bool state) {
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
void vd_LED_Driver_ToggleGreen(void) {
    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
}

void vd_LED_Driver_SetOrange(bool state) {
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
void vd_LED_Driver_ToggleOrange(void) {
    HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
}

void vd_LED_Driver_SetRed(bool state) {
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
void vd_LED_Driver_ToggleRed(void) {
    HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);
}

void vd_LED_Driver_SetBlue(bool state) {
    HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
void vd_LED_Driver_ToggleBlue(void) {
    HAL_GPIO_TogglePin(LED4_GPIO_Port, LED4_Pin);
}
