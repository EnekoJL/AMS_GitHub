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
} Vehicle_Data_t;

/**
 * @brief Powertrain data received from the Inverter via CAN.
 *        Split out from Vehicle_Data_t so it has its own single writer
 *        (AMS_CAN_Task) — see Architecture_Overview.md's one-writer-per-struct
 *        rule. Previously inverter_rpm lived inside Vehicle_Data_t, which also
 *        has ADC as a writer; a CAN write landing between ADC's read and
 *        write-back could get silently discarded (lost-update race).
 */
typedef struct {
    int16_t  inverter_rpm;         // Motor RPM received from Inverter via CAN
} AMS_Powertrain_Data_t;

/**
 * @brief GPS data parsed from NMEA sentences (USART6, 115200 baud, DMA idle-line).
 *        Populated by AMS_GPS_Task from $xxRMC and $xxGGA sentences.
 *
 *        Coordinates are stored as integer micro-degrees (×1,000,000) to avoid
 *        floating-point partial-read races when copying through the Broker mutex.
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
 *        Populated by AMS_Algorithms_Task.
 */
typedef struct {
    /* --- Session (since boot) --- */
    uint32_t ui32_total_distance_m;
    int32_t  i32_max_vel_kmh_x1000;
    int32_t  i32_avg_vel_kmh_x1000;
    int32_t  i32_max_accel_ms2_x1000;
    int32_t  i32_max_decel_ms2_x1000;

    /* --- Lifetime (survives power cycles) ---
     * baseline (loaded from flash at boot) folded with the session value
     * above. TODO: not yet seeded from flash — AMS_Persistent_Config_t
     * doesn't carry these fields yet (flash schema for historic stats is
     * still undecided). Until that's wired up, baseline is always 0, so
     * lifetime == session on every boot. See vd_TelemetryCalc_SeedLifetime()
     * in Algorithms/AMS_telemetry_algorithms.h for where seeding will plug
     * in once the schema exists. */
    uint32_t ui32_lifetime_distance_m;
    int32_t  i32_lifetime_max_vel_kmh_x1000;
    int32_t  i32_lifetime_max_accel_ms2_x1000;
    int32_t  i32_lifetime_max_decel_ms2_x1000;
} AMS_Telemetry_Data_t;

/**
 * @brief Derived battery pack statistics — charge moved, thermal extremes,
 *        peak current. Each sub-struct is the published output of exactly
 *        one Algorithms module (AMS_charge_algorithms.c,
 *        AMS_thermal_algorithms.c, AMS_current_algorithms.c respectively —
 *        single responsibility per module, see those files for the math).
 *        This struct only composes their outputs; it contains no math of
 *        its own.
 *
 *        Session fields reset on boot. Lifetime fields are meant to survive
 *        power cycles via AMS_Persistent_Config_t, but that flash schema
 *        isn't defined yet (see the TODO there) — until it is, lifetime ==
 *        session on every boot, which is a safe default, not a bug.
 *
 *        Written by AMS_Algorithms_Task (current/charge/thermal sections —
 *        see that file's header).
 *          - charge/current: still fold zero in practice — their input,
 *            AMS_BMS_Data_t.i32_pack_current_mA, is still a placeholder
 *            (needs a pack current shunt/Hall sensor, not chosen yet).
 *          - thermal: real as of AMS_BMS_Task existing — folds
 *            AMS_BMS_Data_t.i16_cell_temp_cC[], written every ~250ms from
 *            the LTC6813's NTC channels.
 */
typedef struct {
    uint32_t ui32_session_discharged_mAh;
    uint32_t ui32_session_charged_mAh;
    uint32_t ui32_lifetime_discharged_mAh;  /* baseline + session */
    uint32_t ui32_lifetime_charged_mAh;
} AMS_ChargeStats_t;

typedef struct {
    /* This sample */
    int16_t i16_max_cell_temp_cC;      /* hottest cell right now */
    int16_t i16_min_cell_temp_cC;      /* coldest cell right now */
    int16_t i16_avg_cell_temp_cC;      /* spatial mean across cells right now */
    int16_t i16_delta_temp_cC;         /* max - min across cells right now */
    uint8_t ui8_hottest_cell_id;
    uint8_t ui8_coldest_cell_id;

    /* Session (since boot) */
    int16_t i16_session_max_temp_cC;   /* worst single-cell temp seen */
    int16_t i16_session_max_delta_cC;  /* worst spread seen */
    int16_t i16_session_avg_temp_cC;   /* TIME-average of the spatial mean —
                                         * NOT the same number as
                                         * i16_avg_cell_temp_cC above, which
                                         * is a snapshot. Name them apart in
                                         * any CSV/dashboard. */
} AMS_ThermalStats_t;

typedef struct {
    uint32_t ui32_session_max_discharge_mA;
    uint32_t ui32_session_max_charge_mA;
    uint32_t ui32_lifetime_max_discharge_mA;  /* MAX(baseline, session) */
} AMS_CurrentStats_t;

typedef struct {
    AMS_ChargeStats_t   charge;
    AMS_ThermalStats_t  thermal;
    AMS_CurrentStats_t  current;
} AMS_BatteryStats_Data_t;

/* LTC6813 pack geometry — 2 ICs daisy-chained on SPI2, 18 cell channels and
 * 8 usable NTC channels each (GPIO5/channel index 5 is Vref2, not a sensor).
 * Ported from Test_4_09_2025's spi_stm32f4.c (TOTAL_IC=2). Revise if the
 * real pack uses a different IC count/wiring. */
#define BMS_TOTAL_IC        2
#define BMS_CELLS_PER_IC    18
#define BMS_TEMP_CH_PER_IC  8
#define BMS_TOTAL_CELLS     (BMS_TOTAL_IC * BMS_CELLS_PER_IC)
#define BMS_TOTAL_TEMP_CH   (BMS_TOTAL_IC * BMS_TEMP_CH_PER_IC)

/** @brief Bit assignments for AMS_BMS_Data_t.ui32_fault_flags. */
#define BMS_FAULT_CELL_VOLTAGE  (1u << 0)  /**< A cell is <=2.8V or >=4.3V,
                                             *   confirmed over 4 consecutive
                                             *   samples — see
                                             *   Algorithms/AMS_bms_safety_algorithms.c */

/**
 * @brief Battery Management System snapshot (pack-level safety data).
 *        Written by AMS_BMS_Task (LTC6813 over SPI2) — see that task's
 *        README. Not every field has a producer yet:
 *          - Per-cell voltages/temps, min/max cell+id, fault flags: real,
 *            written every BMS_Task cycle (~250ms).
 *          - i32_pack_current_mA / soc_percent_x10: still placeholders.
 *            The LTC6813 measures cell voltage and GPIO/NTC temperature,
 *            not pack current — pack current is delivered over CAN (from
 *            wherever the current sensor lives on the bus), not by this
 *            task. AMS_CAN_Task doesn't parse a current frame yet.
 *
 *            WARNING for whoever wires that CAN parser in: it must NOT
 *            write into THIS struct. AMS_BMS_Task already owns
 *            AMS_BMS_Data_t as sole writer (one-writer-per-struct, see
 *            Architecture_Overview.md) and overwrites the whole snapshot,
 *            i32_pack_current_mA included, every ~250ms — a second writer
 *            here reproduces the exact Vehicle_Data_t lost-update race
 *            that struct was split to fix. Give pack current its own
 *            domain/mutex written only by AMS_CAN_Task (or fold it into
 *            AMS_Powertrain_Data_t if it comes from the same CAN node as
 *            inverter RPM), then have AMS_Algorithms_Task's Current/Charge
 *            sections read from there instead of AMS_BMS_Data_t. See
 *            docs/Next_Steps.md.
 */
typedef struct {
    uint32_t ui32_pack_voltage_mV;    /* sum of all cells, real */
    int32_t  i32_pack_current_mA;     /* signed: negative = discharging. STILL A PLACEHOLDER. */
    uint16_t ui16_min_cell_mV;
    uint16_t ui16_max_cell_mV;
    uint8_t  ui8_min_cell_id;
    uint8_t  ui8_max_cell_id;
    int16_t  i16_max_cell_temp_cC;
    uint16_t soc_percent_x10;         /* STILL A PLACEHOLDER — needs real pack current first */
    uint32_t ui32_fault_flags;        /* see BMS_FAULT_* above */

    uint16_t ui16_cell_mV[BMS_TOTAL_CELLS];         /* per-cell voltage, index = physical position */
    int16_t  i16_cell_temp_cC[BMS_TOTAL_TEMP_CH];   /* per-channel NTC temp, feeds b_ThermalCalc_Fold() */
} AMS_BMS_Data_t;

/**
 * @brief Freshness/validity of each Broker data domain.
 *
 *        "Flag only" model: the Broker marks a domain stale when it hasn't
 *        been updated within its allowed max-age window — it does NOT take
 *        any corrective action itself (no forced shutdown, no overriding
 *        other tasks). Each consuming task decides what a stale/invalid
 *        flag means for it (log a warning, hold last value, refuse to act
 *        on it, etc). See b_Broker_Get_SafetyFlags().
 */
typedef struct {
    bool b_vehicle_data_fresh;
    bool b_adc_data_fresh;
    bool b_gps_data_fresh;
    bool b_bms_data_fresh;
    bool b_telemetry_data_fresh;
    bool b_powertrain_data_fresh;
    bool b_battery_stats_fresh;
} AMS_Safety_Flags_t;

/**
 * @brief Full snapshot of every Broker data domain, in one struct.
 *
 *        Convenience aggregate ONLY — the Broker still stores and locks
 *        each domain independently (own mutex, own memcpy, per struct).
 *        This type exists so:
 *          1. A consumer (e.g. the dashboard printer) can fetch "everything"
 *             in one call — see b_Broker_Get_AllData() — instead of one call
 *             per struct.
 *          2. There is one place in the codebase that names every domain
 *             that exists, useful for onboarding new developers.
 *        It does NOT introduce a single global mutex — concurrency is
 *        unchanged from the per-domain locking already in the Broker.
 */
typedef struct {
    Vehicle_Data_t          vehicle;
    AMS_BMS_Data_t          bms;
    AMS_ADC_Data_t          sensors;
    GPS_Data_t              gps;
    AMS_Telemetry_Data_t    telemetry;
    AMS_Powertrain_Data_t   powertrain;
    AMS_BatteryStats_Data_t battery_stats;
    AMS_Safety_Flags_t      safety;
} AMS_Data_t;

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
