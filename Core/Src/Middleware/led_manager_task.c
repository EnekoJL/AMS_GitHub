/**
 * @file    led_manager_task.c
 * @brief   Implementación del Gestor de LEDs (Máquina de estados)
 * @date    11 de Marzo de 2026
 */

#include "Middleware/led_manager_task.h"
#include "Middleware/DataBroker.h"
#include "Drivers_Custom/AMS_led_driver.h"
#include "main.h" // HAL_GetTick

#define BLINK_DELAY_MS 500  // Velocidad de parpadeo por defecto (0.5 Hz)

static uint32_t s_last_blink_time = 0;
static AMS_LED_Mode_t s_last_known_mode = LED_MODE_ALL_OFF;

void vd_LED_Manager_Init(void) {
    vd_LED_Driver_Init();
    s_last_blink_time = HAL_GetTick();
    s_last_known_mode = LED_MODE_ALL_OFF;
}

void vd_LED_Manager_Process(void) {
    AMS_LED_Mode_t current_mode = e_Broker_Get_LEDMode();

    if (current_mode != s_last_known_mode) {
        vd_LED_Driver_SetAll(false); // Apagamos todo por seguridad al cambiar de estado
        s_last_known_mode = current_mode;
        
        // Efectos inmediatos (los que no parpadean)
        switch (current_mode) {
            case LED_MODE_ALL_OFF:   vd_LED_Driver_SetAll(false);  break;
            case LED_MODE_ALL_ON:    vd_LED_Driver_SetAll(true);   break;
            case LED_MODE_GREEN_ON:  vd_LED_Driver_SetGreen(true); break;
            case LED_MODE_RED_ON:    vd_LED_Driver_SetRed(true);   break;
            case LED_MODE_BLUE_ON:   vd_LED_Driver_SetBlue(true);  break;
            case LED_MODE_ORANGE_ON: vd_LED_Driver_SetOrange(true);break;
            default: break; // Los modos BLINK se manejan en la rutina de abajo
        }
    }

    // 2. Rutina de Parpadeo No Bloqueante (solo para modos BLINK)
    bool is_blink_mode = (
        current_mode == LED_MODE_ALL_BLINK   ||
        current_mode == LED_MODE_GREEN_BLINK ||
        current_mode == LED_MODE_RED_BLINK   ||
        current_mode == LED_MODE_BLUE_BLINK  ||
        current_mode == LED_MODE_ORANGE_BLINK
    );

    if (is_blink_mode && (HAL_GetTick() - s_last_blink_time >= BLINK_DELAY_MS)) {
        s_last_blink_time = HAL_GetTick();

        switch (current_mode) {
            case LED_MODE_ALL_BLINK:    vd_LED_Driver_ToggleAll();    break;
            case LED_MODE_GREEN_BLINK:  vd_LED_Driver_ToggleGreen();  break;
            case LED_MODE_RED_BLINK:    vd_LED_Driver_ToggleRed();    break;
            case LED_MODE_BLUE_BLINK:   vd_LED_Driver_ToggleBlue();   break;
            case LED_MODE_ORANGE_BLINK: vd_LED_Driver_ToggleOrange(); break;
            default: break;
        }
    }
}
