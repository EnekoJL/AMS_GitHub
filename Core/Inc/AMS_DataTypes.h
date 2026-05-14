/**
 * @file    AMS_DataTypes.h
 * @brief   Constantes, macros y defines del proyecto AMS.
 *          Para estructuras de datos (structs/enums), usar AMS_DataStructs.h.
 */

#ifndef AMS_DATATYPES_H_
#define AMS_DATATYPES_H_

#include <stdint.h>
#include "AMS_DataStructs.h" /* Incluye las estructuras para retrocompatibilidad */

/* =========== CONFIGURACION DEL SISTEMA =========== */

#define NUM_MUESTRAS 10

/* --- ADC1 DMA scan layout ---
 * ADC1 scan order (set by CubeMX):
 *   Rank 1: CH9          (PB1,  12V battery sense)
 *   Rank 2: CH18         (internal Temperature Sensor)
 *   Rank 3: CH17         (internal VREFINT)
 * DMA buffer is interleaved: [CH9, TEMP, VREF, CH9, TEMP, VREF, ...]
 */
#define ADC1_NUM_CHANNELS       3u   /**< Number of ranks in ADC1 scan sequence */
#define ADC1_IDX_BATTERY        0u   /**< Buffer offset for CH9  (battery)      */
#define ADC1_IDX_TEMP_SENSOR    1u   /**< Buffer offset for CH18 (temperature)  */
#define ADC1_IDX_VREFINT        2u   /**< Buffer offset for CH17 (VREFINT)      */

/* --- VREFINT Factory Calibration (STM32F469, VDDA = 3.3 V @ factory) ---
 * ROM address: 0x1FFF7A2A-0x1FFF7A2B (16-bit, little-endian)
 * Stores the 12-bit ADC count of VREFINT measured at VDDA = 3.3 V.
 * Used to back-calculate the actual board supply voltage.
 */
#define AMS_VREFINT_CAL_ADDR        ((volatile uint16_t *)0x1FFF7A2AU)
#define VREFINT_CAL_MV          3300u  /**< VDDA at which factory cal was done (mV) */
#define ADC_MAX_COUNT           4095u  /**< 12-bit ADC full-scale count             */

/* --- Temperature Sensor Factory Calibration (STM32F469) ---
 * TS_CAL1: 12-bit count at 30 °C,  VDDA = 3.3 V  (ROM: 0x1FFF7A2C)
 * TS_CAL2: 12-bit count at 110 °C, VDDA = 3.3 V  (ROM: 0x1FFF7A2E)
 * Formula (RM0386 §13.10):
 *   T_cC = ((TS_scaled - TS_CAL1) * (110-30)*100) / (TS_CAL2 - TS_CAL1) + 30*100
 * where TS_scaled = TS_raw * VREFINT_CAL_MV / VDDA_actual_mV
 * Result is in centi-degrees Celsius (e.g. 2547 = 25.47 °C).
 */
#define TS_CAL1_ADDR            ((volatile uint16_t *)0x1FFF7A2CU)
#define TS_CAL2_ADDR            ((volatile uint16_t *)0x1FFF7A2EU)
#define TS_CAL1_TEMP_C          30     /**< Temperature (°C) when TS_CAL1 was sampled */
#define TS_CAL2_TEMP_C          110    /**< Temperature (°C) when TS_CAL2 was sampled */

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
