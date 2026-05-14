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
 * @brief Hardware data structure (raw and calibrated ADC values).
 *
 * --- Raw fields (written by DMA callbacks, volatile) ---
 *   adc1_filtrado        : CH9  averaged count  (12V battery sense)
 *   ui16_temp_sensor_raw : CH18 averaged count  (MCU junction temperature)
 *   ui16_vrefint_raw     : CH17 averaged count  (internal bandgap reference)
 *   adc2_ch1_filtrado    : ADC2 CH12 averaged   (suspension sensor 1)
 *   adc2_ch2_filtrado    : ADC2 CH13 averaged   (suspension sensor 2)
 *
 * --- Computed fields (written by Algorithms layer, NOT volatile) ---
 *   ui32_vdda_actual_mV  : Actual board VDD in millivolts (derived from VREFINT)
 *   voltaje_adc1_mV      : Battery sense in millivolts (VDD-compensated)
 *   voltaje_adc2_ch1_mV  : Suspension 1 in millivolts  (VDD-compensated)
 *   voltaje_adc2_ch2_mV  : Suspension 2 in millivolts  (VDD-compensated)
 *   i32_mcu_temp_cC      : MCU temperature in centi-°C (e.g. 2547 = 25.47 °C)
 */
typedef struct {
    /* --- Raw ADC1 scan results (filled by DMA interrupt callback) --- */
    volatile uint16_t adc1_filtrado;          /* CH9  Rank1: 12V battery sense  */
    volatile uint16_t ui16_temp_sensor_raw;   /* CH18 Rank2: MCU temperature    */
    volatile uint16_t ui16_vrefint_raw;       /* CH17 Rank3: VREFINT bandgap    */

    /* --- Raw ADC2 scan results (filled by DMA interrupt callback) --- */
    volatile uint16_t adc2_ch1_filtrado;      /* CH12: suspension sensor 1      */
    volatile uint16_t adc2_ch2_filtrado;      /* CH13: suspension sensor 2      */

    /* --- Computed values (filled by Algorithms_Sensors_ProcessVoltages) --- */
    uint32_t ui32_vdda_actual_mV;   /* Actual VDD derived from VREFINT (mV)   */
    uint32_t voltaje_adc1_mV;       /* 12V sense voltage, VDD-compensated (mV) */
    uint32_t voltaje_adc2_ch1_mV;   /* Suspension 1 voltage, compensated (mV) */
    uint32_t voltaje_adc2_ch2_mV;   /* Suspension 2 voltage, compensated (mV) */
    int32_t  i32_mcu_temp_cC;       /* MCU junction temp in centi-°C           */
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
    bool     b_gps_is_connected;    /* true  = valid position fix (RMC status 'A')    */
    uint8_t  ui8_fix_quality;       /* 0=none, 1=GPS, 2=DGPS (from GGA)              */
    uint8_t  ui8_satellites;        /* Satellites tracked (from GGA)                  */

    /* Position — micro-degrees (integer, ×1 000 000) */
    int32_t  i32_latitude_udeg;     /* e.g.  43123456 =  43.123456° N (negative = S)  */
    int32_t  i32_longitude_udeg;    /* e.g.   2987654 =   2.987654° E (negative = W)  */

    /* Kinematics (from RMC) */
    int32_t  i32_vel_kmh_x1000;
    int32_t  i32_vel_knots_x1000;
    int32_t  i32_vel_ms_x1000;

    /* UTC timestamp (from RMC) */
    uint8_t  ui8_hour;
    uint8_t  ui8_minute;
    uint8_t  ui8_second;
    uint32_t ui32_utc_total_ms;     /* High-res time of day from GPS (includes fractional seconds) */

    /* Reception health */
    uint32_t ui32_last_fix_tick_ms; /* System tick when data was received             */
} GPS_Data_t;

/**
 * @brief Telemetry calculated from GPS data.
 *        Populated by AMS_Data_Calculator_Task.
 */
typedef struct {
    uint32_t ui32_total_distance_m;
    int32_t  i32_max_vel_kmh_x1000;
    int32_t  i32_avg_vel_kmh_x1000;
    int32_t  i32_max_accel_ms2_x1000;
    int32_t  i32_max_decel_ms2_x1000;
} AMS_Telemetry_Data_t;

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
