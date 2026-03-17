/**
 * @file    AMS_DataTypes.h
 * @brief   Constantes, macros y defines del proyecto AMS.
 *          Para estructuras de datos (structs/enums), usar AMS_DataStructs.h.
 */

#ifndef AMS_DATATYPES_H_
#define AMS_DATATYPES_H_

#include <stdint.h>
#include "AMS_DataStructs.h" /* Incluye las estructuras para retrocompatibilidad */

/* =========== CONFIGTURACION DEL SISTEMA =========== */

#define NUM_MUESTRAS 10

/* --- Rangos de sensores fisicos --- */
#define VOLTAJE_ALIMENTACION_POT_MV 5000u // 5V en milivoltios
#define RANGO_POT_SUSP_1_MM         150u
#define RANGO_POT_SUSP_2_MM         50u

/* --- Duraciones de parpadeo de LED (ms) --- */
#define LED_BLINK_DURATION_GREEN_ms   200u
#define LED_BLINK_DURATION_RED_ms     200u
#define LED_BLINK_DURATION_BLUE_ms    200u
#define LED_BLINK_DURATION_ORANGE_ms  200u

/* --- Magic word Flash --- */
#define AMS_FLASH_RECORD_MAGIC  0xAEC01AD0u

#endif /* AMS_DATATYPES_H_ */
