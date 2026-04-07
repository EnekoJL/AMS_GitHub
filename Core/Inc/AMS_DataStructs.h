/**
 * @file    AMS_DataStructs.h
 * @brief   Estructuras de datos del proyecto AMS.
 *          Contiene todos los typedef struct y typedef enum tipicos de la
 *          capa de aplicacion (Vehicle_Data_t, AMS_ADC_Data_t, etc.).
 *
 *          Para constantes y macros, usar AMS_DataTypes.h.
 */

#ifndef AMS_DATASTRUCTS_H_
#define AMS_DATASTRUCTS_H_

#include <stdint.h>
#include <stdbool.h>

/* =========== ESTRUCTURAS DE DATOS DEL SISTEMA =========== */

/**
 * @brief Estructura de hardware (Datos crudos ADC).
 *        Llenada periodicamente por los callbacks DMA de los ADC1/ADC2.
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
 * @brief Estructura de aplicacion (Magnitudes fisicas en enteros).
 *        Representa el estado fisico actual de la moto.
 */
typedef struct {
    uint32_t bateria_12v_mV;       // Battery voltage in millivolts (e.g. 12500 = 12.5 V)
    uint32_t recorrido_susp_1_dmm; // Suspension travel in tenths of mm (e.g. 1452 = 145.2 mm)
    uint32_t recorrido_susp_2_dmm; // Suspension travel in tenths of mm (e.g. 485 = 48.5 mm)
    int16_t  inverter_rpm;         // Motor RPM received from Inverter via CAN
} Vehicle_Data_t;

/**
 * @brief GPS data parsed from NMEA sentences (USART6, 115200 baud, DMA idle-line).
 *        Populated by AMS_GPS_Task from $xxRMC and $xxGGA sentences.
 *
 *        Coordinates are stored as integer micro-degrees (×1,000,000) to avoid
 *        floating-point partial-read races when copying through the Broker mutex.
 *        The GPS task converts to float internally (minmea) then back to int32.
 */
typedef struct {
    /* Fix status */
    bool     b_fix_valid;           /* true  = valid position fix (RMC status 'A')    */
    uint8_t  ui8_fix_quality;       /* 0=none, 1=GPS, 2=DGPS (from GGA)              */
    uint8_t  ui8_satellites;        /* Satellites tracked (from GGA)                  */

    /* Position — micro-degrees (integer, ×1 000 000) */
    int32_t  i32_latitude_udeg;     /* e.g.  43123456 =  43.123456° N (negative = S)  */
    int32_t  i32_longitude_udeg;    /* e.g.   2987654 =   2.987654° E (negative = W)  */

    /* Kinematics (from RMC) */
    float    f_speed_kph;           /* Speed over ground in km/h                       */
    float    f_course_deg;          /* True course over ground, 0–360°                 */

    /* UTC timestamp (from RMC) */
    uint8_t  ui8_hour;
    uint8_t  ui8_minute;
    uint8_t  ui8_second;

    /* Reception health */
    uint32_t ui32_last_fix_tick_ms; /* HAL_GetTick() at last valid RMC sentence        */
} GPS_Data_t;

/**
 * @brief Datos persistentes del sistema (guardados en Flash interna).
 *        Cualquier campo aqui sobrevive a cortes de tension y reinicios.
 */
typedef struct {
    uint16_t soc_percent_x10;  // Estado de carga en decimas de % (ej. 952 = 95.2%)
    uint16_t cycle_count;      // Numero de escrituras (debug de desgaste flash)
} AMS_Persistent_Config_t;

/**
 * @brief Registro fisico tal y como se almacena en la Flash interna.
 *        Magic + datos + CRC para deteccion de corrupcion.
 *        TAMANO: 12 bytes (multiplo de 4, requerimiento del STM32 Flash HAL)
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;                  // Siempre 0xAEC01AD0 si el registro es valido
    AMS_Persistent_Config_t config;  // Los datos a persistir (4 bytes)
    uint32_t crc;                    // CRC32 simple del bloque anterior
} AMS_Flash_Record_t;                // Total: 12 bytes

/* =========== ENUMS DE ESTADO / MODO =========== */

/**
 * @brief Which LED to control.
 */
typedef enum {
    LED_COLOR_GREEN  = 0,
    LED_COLOR_RED    = 1,
    LED_COLOR_BLUE   = 2,
    LED_COLOR_ORANGE = 3,
    LED_COLOR_COUNT  = 4
} AMS_LED_Color_t;

/**
 * @brief What the LED should do.
 *  OFF    - LED off.
 *  ON     - LED permanently on.
 *  TOGGLE - LED toggles continuously every 500 ms.
 *  BLINK  - LED lights for LED_BLINK_DURATION_*_ms then turns off.
 */
typedef enum {
    LED_PIN_OFF    = 0,
    LED_PIN_ON     = 1,
    LED_PIN_TOGGLE = 2,
    LED_PIN_BLINK  = 3,
} AMS_LED_PinMode_t;

#endif /* AMS_DATASTRUCTS_H_ */
