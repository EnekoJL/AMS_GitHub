/**
 * @file    AMS_DataTypes.h
 * @brief   Variables globales proyecto
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#ifndef AMS_DATATYPES_H_
#define AMS_DATATYPES_H_

#include <stdint.h>

#define NUM_MUESTRAS 10

#define VOLTAJE_ALIMENTACION_POT_MV 5000u // 5V en milivoltios
#define RANGO_POT_SUSP_1_MM         150u
#define RANGO_POT_SUSP_2_MM         50u

/**
 * @brief Estructura de hardware (Datos crudos ADC)
 */
typedef struct {
    volatile uint16_t adc1_filtrado;
    uint32_t voltaje_adc1_mV;
    volatile uint16_t adc2_ch1_filtrado;
    volatile uint16_t adc2_ch2_filtrado;
    uint32_t voltaje_adc2_ch1_mV;
    uint32_t voltaje_adc2_ch2_mV;
} AMS_ADC_Data_t;

/**
 * @brief Estructura de aplicación (Magnitudes físicas en enteros)
 */
typedef struct {
    uint32_t bateria_12v_mV;       // Tensión en milivoltios (ej. 12500 -> 12.5V)
    uint32_t recorrido_susp_1_dmm; // Recorrido en décimas de mm (ej. 1452 -> 145.2 mm)
    uint32_t recorrido_susp_2_dmm; // Recorrido en décimas de mm (ej. 485 -> 48.5 mm)
} Vehicle_Data_t;

/**
 * @brief Modos de parpadeo y estado visual para la placa
 */
typedef enum {
    LED_MODE_ALL_OFF = 0,
    LED_MODE_ALL_ON,
    LED_MODE_ALL_BLINK,
    LED_MODE_GREEN_ON,
    LED_MODE_GREEN_BLINK,
    LED_MODE_RED_ON,
    LED_MODE_RED_BLINK,
    LED_MODE_BLUE_ON,
    LED_MODE_BLUE_BLINK,
    LED_MODE_ORANGE_ON,
    LED_MODE_ORANGE_BLINK
} AMS_LED_Mode_t;

/* ================= VARIABLES (TYPEDEFS) ========================= */

/**
 * @brief Datos persistentes del sistema (guardados en Flash interna).
 *        Cualquier campo aqui sobrevive a cortes de tensión y reinicios.
 */
typedef struct {
    uint16_t soc_percent_x10;  // Estado de carga en décimas de % (ej. 952 = 95.2%)
    uint16_t cycle_count;      // Número de escrituras (debug de desgaste flash)
} AMS_Persistent_Config_t;

/**
 * @brief Registro físico tal y como se almacena en la Flash interna.
 *        Magic + datos + CRC para detección de corrupción.
 *        TAMAÑO: 12 bytes (múltiplo de 4, requerimiento del STM32 Flash HAL)
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;                  // Siempre 0xAEC01AD0 si el registro es válido
    AMS_Persistent_Config_t config;  // Los datos a persistir (4 bytes)
    uint32_t crc;                    // CRC32 simple del bloque anterior
} AMS_Flash_Record_t;                // Total: 12 bytes

#define AMS_FLASH_RECORD_MAGIC  0xAEC01AD0u

#endif /* AMS_DATATYPES_H_ */
