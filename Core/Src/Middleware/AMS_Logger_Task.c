/**
 * @file    AMS_Logger_Task.c
 * @brief   Middleware RTOS Task: SD-card logger + terminal broker data printer.
 *
 *          ANSI terminal colour macros are defined in AMS_ansi_colors.h.
 *
 *  Two independent sub-features run inside the same FreeRTOS task:
 *
 *    1. SD-CARD LOGGER  (TASK_SD_CARD_ENABLE in AMS_task_config.h)
 *       Reads a full AMS_Data_t snapshot from the Broker every
 *       LOGGER_SD_PERIOD_MS and appends one CSV row (vehicle, GPS,
 *       telemetry, BMS, plus a fresh/stale flag per domain) to the open
 *       log file on the SD card.
 *
 *    2. TERMINAL PRINTER  (FEATURE_LOGGER_PRINT_ENABLE in AMS_task_config.h)
 *       Reads ALL broker structs and pretty-prints them to the debug terminal
 *       with ANSI colour codes.  Completely independent of the SD card.
 *
 * @author  Eneko Juanena
 */

#include "Middleware/AMS_Logger_Task.h"
#include "Middleware/AMS_DataBroker.h"
#include "Middleware/AMS_Led_Task.h"
#include "AMS_task_config.h"
#include "AMS_ansi_colors.h"
#include "cmsis_os.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* =========================================================================
 * SD-card logger — only included when the SD task is enabled
 * ========================================================================= */
#if (TASK_SD_CARD_ENABLE == 1)
#include "Drivers_Custom/AMS_SD_driver.h"

static char  s_current_log_file[32] = {0};
static bool  b_logger_ready         = false;
#endif /* TASK_SD_CARD_ENABLE */

/* =========================================================================
 * PRIVATE HELPERS
 * ========================================================================= */

/** Print a full-width section banner with a colour and title. */
static void vd_print_banner(const char *p_colour, const char *p_title)
{
    printf("%s%s", ANSI_RESET, p_colour);
    printf("  +----------------------------------------------------------+\r\n");
    printf("  |  %-56s|\r\n", p_title);
    printf("  +----------------------------------------------------------+\r\n");
    printf(ANSI_RESET);
}

/** Print a single labelled value row: "  label ............. value\n" */
#define PRINT_ROW(label, fmt, ...)  \
    printf(COL_LABEL "    %-28s" ANSI_RESET COL_VALUE fmt ANSI_RESET "\r\n", \
           label, ##__VA_ARGS__)

/* =========================================================================
 * PUBLIC API
 * ========================================================================= */

/* ---------- vd_Logger_Init ----------------------------------------------- */
void vd_Logger_Init(void)
{
#if (TASK_SD_CARD_ENABLE == 1)
    if (b_SD_Card_Mount()) {
        if (b_SD_Card_FindNextFilename("gekko", "csv",
                                       s_current_log_file,
                                       sizeof(s_current_log_file))) {
            if (b_SD_Card_OpenLogFile(s_current_log_file)) {
                /* One row = one full AMS_Data_t snapshot. Slow-changing
                 * columns (e.g. BMS_*) repeat their last value between
                 * updates — check the matching _FRESH column (1 = new
                 * sample since last row, 0 = repeated/stale) before
                 * treating a value as a fresh reading. */
                const char *p_header =
                    "TICK_MS,"
                    "BAT_12V_MV,SUSP1_DMM,SUSP2_DMM,RPM,"
                    "GPS_FIX,SATS,LAT_UDEG,LON_UDEG,SPEED_KMH_X1000,"
                    "DIST_M,MAX_SPEED_KMH_X1000,AVG_SPEED_KMH_X1000,MAX_ACCEL_X1000,MAX_DECEL_X1000,"
                    "BMS_PACK_MV,BMS_PACK_MA,BMS_MIN_CELL_MV,BMS_MAX_CELL_MV,BMS_MAX_TEMP_CC,BMS_SOC_X10,BMS_FAULTS,"
                    "VEH_FRESH,ADC_FRESH,GPS_FRESH,BMS_FRESH,TELEM_FRESH\n";
                if (b_SD_Card_WriteSync(p_header)) {
                    b_logger_ready = true;
                    printf("[LOGGER] SD ready. Logging to: %s\r\n",
                           s_current_log_file);
                } else {
                    printf("[LOGGER] ERROR: header write failed (%s)\r\n",
                           s_current_log_file);
                }
            } else {
                printf("[LOGGER] ERROR: could not open %s\r\n",
                       s_current_log_file);
            }
        } else {
            printf("[LOGGER] ERROR: no free filename available.\r\n");
        }
    } else {
        printf("[LOGGER] WARNING: SD card not mounted (not inserted?).\r\n");
    }
#endif /* TASK_SD_CARD_ENABLE */
}

/* ---------- vd_Logger_PrintBrokerData ------------------------------------ */
#if (FEATURE_LOGGER_PRINT_ENABLE == 1)

void vd_Logger_PrintBrokerData(void)
{
    /* Local snapshot copies — never touch broker internals directly */
    AMS_ADC_Data_t          adc         = {0};
    Vehicle_Data_t          veh         = {0};
    GPS_Data_t              gps         = {0};
    AMS_Telemetry_Data_t    telem       = {0};
    AMS_Persistent_Config_t persist     = {0};

    bool b_adc_ok     = b_Broker_Get_ADCData(&adc);
    bool b_veh_ok     = b_Broker_Get_VehicleState(&veh);
    bool b_gps_ok     = b_Broker_Get_GPSData(&gps);
    bool b_telem_ok   = b_Broker_Get_TelemetryData(&telem);
    bool b_persist_ok = b_Broker_Get_PersistentConfig(&persist);

    /* ------------------------------------------------------------------ */
    /* Top banner                                                           */
    /* ------------------------------------------------------------------ */
    printf("\r\n");
    printf(COL_HEADER
           "  ╔══════════════════════════════════════════════════════════╗\r\n"
           "  ║            AMS DataBroker  —  Live Snapshot              ║\r\n"
           "  ║  tick: %10lu ms                                    ║\r\n"
           "  ╚══════════════════════════════════════════════════════════╝\r\n"
           ANSI_RESET,
           (unsigned long)HAL_GetTick());

    /* ------------------------------------------------------------------ */
    /* 1. ADC DATA                                                          */
    /* ------------------------------------------------------------------ */
    vd_print_banner(COL_ADC, "[ 1 ]  ADC DATA  (raw + computed)");

    if (!b_adc_ok) {
        printf(COL_WARN "    Broker read FAILED\r\n" ANSI_RESET);
    } else {
        /* Raw */
        PRINT_ROW("Battery raw (CH9):",       "%5u  counts",  (unsigned)adc.adc1_filtrado);
        PRINT_ROW("Temp-sensor raw (CH18):",  "%5u  counts",  (unsigned)adc.ui16_temp_sensor_raw);
        PRINT_ROW("VREFINT raw (CH17):",      "%5u  counts",  (unsigned)adc.ui16_vrefint_raw);
        PRINT_ROW("Susp-1 raw (ADC2-CH12):", "%5u  counts",  (unsigned)adc.adc2_ch1_filtrado);
        PRINT_ROW("Susp-2 raw (ADC2-CH13):", "%5u  counts",  (unsigned)adc.adc2_ch2_filtrado);

        printf(COL_ADC "    -- computed ---\r\n" ANSI_RESET);
        PRINT_ROW("VDDA actual:",             "%5lu mV",  (unsigned long)adc.ui32_vdda_actual_mV);
        PRINT_ROW("Battery voltage:",         "%5lu mV",  (unsigned long)adc.voltaje_adc1_mV);
        PRINT_ROW("Susp-1 voltage:",          "%5lu mV",  (unsigned long)adc.voltaje_adc2_ch1_mV);
        PRINT_ROW("Susp-2 voltage:",          "%5lu mV",  (unsigned long)adc.voltaje_adc2_ch2_mV);
        PRINT_ROW("MCU junction temp:",       "%4ld.%02ld °C",
                  (long)(adc.i32_mcu_temp_cC / 100),
                  (long)((adc.i32_mcu_temp_cC < 0 ?
                          -adc.i32_mcu_temp_cC : adc.i32_mcu_temp_cC) % 100));
    }

    /* ------------------------------------------------------------------ */
    /* 2. VEHICLE STATE                                                     */
    /* ------------------------------------------------------------------ */
    vd_print_banner(COL_VEHICLE, "[ 2 ]  VEHICLE STATE");

    if (!b_veh_ok) {
        printf(COL_WARN "    Broker read FAILED\r\n" ANSI_RESET);
    } else {
        PRINT_ROW("12 V battery:",     "%5lu mV  (%lu.%03lu V)",
                  (unsigned long)veh.bateria_12v_mV,
                  (unsigned long)(veh.bateria_12v_mV / 1000u),
                  (unsigned long)(veh.bateria_12v_mV % 1000u));
        PRINT_ROW("Suspension 1:",     "%5lu d-mm  (%lu.%01lu mm)",
                  (unsigned long)veh.recorrido_susp_1_dmm,
                  (unsigned long)(veh.recorrido_susp_1_dmm / 10u),
                  (unsigned long)(veh.recorrido_susp_1_dmm % 10u));
        PRINT_ROW("Suspension 2:",     "%5lu d-mm  (%lu.%01lu mm)",
                  (unsigned long)veh.recorrido_susp_2_dmm,
                  (unsigned long)(veh.recorrido_susp_2_dmm / 10u),
                  (unsigned long)(veh.recorrido_susp_2_dmm % 10u));
        PRINT_ROW("Inverter RPM:",     "%6d RPM",  (int)veh.inverter_rpm);
    }

    /* ------------------------------------------------------------------ */
    /* 3. GPS DATA                                                          */
    /* ------------------------------------------------------------------ */
    vd_print_banner(COL_GPS, "[ 3 ]  GPS DATA");

    if (!b_gps_ok) {
        printf(COL_WARN "    Broker read FAILED\r\n" ANSI_RESET);
    } else {
        const char *p_fix_str = gps.b_gps_is_connected
                                ? (COL_OK "VALID" ANSI_RESET)
                                : (COL_NA "NO FIX" ANSI_RESET);
        printf(COL_LABEL "    %-28s" ANSI_RESET "%s\r\n",
               "Fix status:", p_fix_str);

        PRINT_ROW("Fix quality:",      "%u  (0=none 1=GPS 2=DGPS)", (unsigned)gps.ui8_fix_quality);
        PRINT_ROW("Satellites:",       "%u", (unsigned)gps.ui8_satellites);

        /* Latitude: split micro-degrees into degrees + fractional */
        int32_t lat_deg  = gps.i32_latitude_udeg / 1000000;
        int32_t lat_frac = (gps.i32_latitude_udeg < 0 ?
                            -gps.i32_latitude_udeg : gps.i32_latitude_udeg) % 1000000;
        int32_t lon_deg  = gps.i32_longitude_udeg / 1000000;
        int32_t lon_frac = (gps.i32_longitude_udeg < 0 ?
                            -gps.i32_longitude_udeg : gps.i32_longitude_udeg) % 1000000;

        PRINT_ROW("Latitude:",         "%4ld.%06ld °", (long)lat_deg, (long)lat_frac);
        PRINT_ROW("Longitude:",        "%4ld.%06ld °", (long)lon_deg, (long)lon_frac);

        PRINT_ROW("Speed (km/h):",     "%4ld.%03ld",
                  (long)(gps.i32_vel_kmh_x1000 / 1000),
                  (long)((gps.i32_vel_kmh_x1000 < 0 ?
                          -gps.i32_vel_kmh_x1000 : gps.i32_vel_kmh_x1000) % 1000));
        PRINT_ROW("Speed (knots):",    "%4ld.%03ld",
                  (long)(gps.i32_vel_knots_x1000 / 1000),
                  (long)((gps.i32_vel_knots_x1000 < 0 ?
                          -gps.i32_vel_knots_x1000 : gps.i32_vel_knots_x1000) % 1000));
        PRINT_ROW("Speed (m/s):",      "%4ld.%03ld",
                  (long)(gps.i32_vel_ms_x1000 / 1000),
                  (long)((gps.i32_vel_ms_x1000 < 0 ?
                          -gps.i32_vel_ms_x1000 : gps.i32_vel_ms_x1000) % 1000));
        PRINT_ROW("UTC time:",         "%02u:%02u:%02u",
                  (unsigned)gps.ui8_hour,
                  (unsigned)gps.ui8_minute,
                  (unsigned)gps.ui8_second);
        PRINT_ROW("Last fix tick:",    "%lu ms", (unsigned long)gps.ui32_last_fix_tick_ms);
    }

    /* ------------------------------------------------------------------ */
    /* 4. TELEMETRY DATA                                                    */
    /* ------------------------------------------------------------------ */
    vd_print_banner(COL_TELEMETRY, "[ 4 ]  TELEMETRY  (calculated)");

    if (!b_telem_ok) {
        printf(COL_WARN "    Broker read FAILED\r\n" ANSI_RESET);
    } else {
        PRINT_ROW("Total distance:",   "%lu m",    (unsigned long)telem.ui32_total_distance_m);
        PRINT_ROW("Max speed:",        "%4ld.%03ld km/h",
                  (long)(telem.i32_max_vel_kmh_x1000 / 1000),
                  (long)((telem.i32_max_vel_kmh_x1000 < 0 ?
                          -telem.i32_max_vel_kmh_x1000 : telem.i32_max_vel_kmh_x1000) % 1000));
        PRINT_ROW("Avg speed:",        "%4ld.%03ld km/h",
                  (long)(telem.i32_avg_vel_kmh_x1000 / 1000),
                  (long)((telem.i32_avg_vel_kmh_x1000 < 0 ?
                          -telem.i32_avg_vel_kmh_x1000 : telem.i32_avg_vel_kmh_x1000) % 1000));
        PRINT_ROW("Max accel:",        "%4ld.%03ld m/s²",
                  (long)(telem.i32_max_accel_ms2_x1000 / 1000),
                  (long)((telem.i32_max_accel_ms2_x1000 < 0 ?
                          -telem.i32_max_accel_ms2_x1000 : telem.i32_max_accel_ms2_x1000) % 1000));
        PRINT_ROW("Max decel:",        "%4ld.%03ld m/s²",
                  (long)(telem.i32_max_decel_ms2_x1000 / 1000),
                  (long)((telem.i32_max_decel_ms2_x1000 < 0 ?
                          -telem.i32_max_decel_ms2_x1000 : telem.i32_max_decel_ms2_x1000) % 1000));
    }

    /* ------------------------------------------------------------------ */
    /* 5. PERSISTENT CONFIG                                                 */
    /* ------------------------------------------------------------------ */
    vd_print_banner(COL_PERSISTENT, "[ 5 ]  PERSISTENT CONFIG  (Flash)");

    if (!b_persist_ok) {
        printf(COL_WARN "    Broker read FAILED\r\n" ANSI_RESET);
    } else {
        PRINT_ROW("State of Charge:",  "%u.%01u %%",
                  (unsigned)(persist.soc_percent_x10 / 10u),
                  (unsigned)(persist.soc_percent_x10 % 10u));
        PRINT_ROW("Flash cycle count:", "%u writes", (unsigned)persist.cycle_count);
    }

    /* ------------------------------------------------------------------ */
    /* Footer                                                               */
    /* ------------------------------------------------------------------ */
    printf(ANSI_DIM ANSI_FG_WHITE
           "  ──────────────────────────────────────────────────────────\r\n"
           ANSI_RESET "\r\n");
}

#else  /* FEATURE_LOGGER_PRINT_ENABLE == 0 */

/* Stub: compiles to nothing, zero overhead. */
void vd_Logger_PrintBrokerData(void) { }

#endif /* FEATURE_LOGGER_PRINT_ENABLE */

/* ---------- vd_Logger_TaskProcess --------------------------------------- */
void vd_Logger_TaskProcess(void)
{
    /* Tracks when we last fired the terminal printer */
    uint32_t ui32_last_print_tick_ms = 0U;
    /* Tracks the last seen Broker fault count, to detect new faults */
    uint32_t ui32_last_broker_fault_count = 0U;
#if (TASK_SD_CARD_ENABLE == 1)
    /* Tracks when we last wrote an SD-card CSV row */
    uint32_t ui32_last_sd_write_tick_ms = 0U;
#endif

    for (;;)
    {
        uint32_t ui32_now_ms = HAL_GetTick();

        /* ----------------------------------------------------------------
         * Broker health check: a Get/Set that timed out means some task
         * held a mutex too long. Runs unconditionally, independent of
         * the SD/print feature flags below, so it always gets surfaced.
         * ------------------------------------------------------------- */
        uint32_t ui32_broker_faults = u32_Broker_GetFaultCount();
        if (ui32_broker_faults != ui32_last_broker_fault_count)
        {
            ui32_last_broker_fault_count = ui32_broker_faults;
            printf("[LOGGER] WARNING: DataBroker mutex timeout (total faults: %lu)\r\n",
                   (unsigned long)ui32_broker_faults);
            vd_LED_Manager_SetMode(LED_COLOR_RED, LED_PIN_BLINK);
        }

        /* ----------------------------------------------------------------
         * SUB-FEATURE A: SD-card CSV logger
         *
         * Gated by its own LOGGER_SD_PERIOD_MS, independent of the 100ms
         * task tick and of the terminal print period below — logging rate
         * and dashboard refresh rate are different concerns.
         * ------------------------------------------------------------- */
#if (TASK_SD_CARD_ENABLE == 1)
        if (b_logger_ready &&
            (ui32_now_ms - ui32_last_sd_write_tick_ms) >= LOGGER_SD_PERIOD_MS)
        {
            ui32_last_sd_write_tick_ms = ui32_now_ms;

            /* One call, one snapshot of every domain — see AMS_Data_t. */
            AMS_Data_t snap = {0};
            b_Broker_Get_AllData(&snap);

            char s_csv_row[400];
            snprintf(s_csv_row, sizeof(s_csv_row),
                     "%lu,"
                     "%lu,%lu,%lu,%d,"
                     "%u,%u,%ld,%ld,%ld,"
                     "%lu,%ld,%ld,%ld,%ld,"
                     "%lu,%ld,%u,%u,%d,%u,%lu,"
                     "%u,%u,%u,%u,%u\n",
                     (unsigned long)ui32_now_ms,
                     /* Vehicle */
                     (unsigned long)snap.vehicle.bateria_12v_mV,
                     (unsigned long)snap.vehicle.recorrido_susp_1_dmm,
                     (unsigned long)snap.vehicle.recorrido_susp_2_dmm,
                     (int)snap.vehicle.inverter_rpm,
                     /* GPS */
                     (unsigned)snap.gps.b_gps_is_connected,
                     (unsigned)snap.gps.ui8_satellites,
                     (long)snap.gps.i32_latitude_udeg,
                     (long)snap.gps.i32_longitude_udeg,
                     (long)snap.gps.i32_vel_kmh_x1000,
                     /* Telemetry */
                     (unsigned long)snap.telemetry.ui32_total_distance_m,
                     (long)snap.telemetry.i32_max_vel_kmh_x1000,
                     (long)snap.telemetry.i32_avg_vel_kmh_x1000,
                     (long)snap.telemetry.i32_max_accel_ms2_x1000,
                     (long)snap.telemetry.i32_max_decel_ms2_x1000,
                     /* BMS (placeholder — 0 until a BMS task exists) */
                     (unsigned long)snap.bms.ui32_pack_voltage_mV,
                     (long)snap.bms.i32_pack_current_mA,
                     (unsigned)snap.bms.ui16_min_cell_mV,
                     (unsigned)snap.bms.ui16_max_cell_mV,
                     (int)snap.bms.i16_max_cell_temp_cC,
                     (unsigned)snap.bms.soc_percent_x10,
                     (unsigned long)snap.bms.ui32_fault_flags,
                     /* Freshness — 1 = updated since last row, 0 = repeated/stale */
                     (unsigned)snap.safety.b_vehicle_data_fresh,
                     (unsigned)snap.safety.b_adc_data_fresh,
                     (unsigned)snap.safety.b_gps_data_fresh,
                     (unsigned)snap.safety.b_bms_data_fresh,
                     (unsigned)snap.safety.b_telemetry_data_fresh);

            if (b_SD_Card_WriteSync(s_csv_row))
            {
                vd_LED_Manager_SetMode(LED_COLOR_GREEN, LED_PIN_BLINK);
            }
            else
            {
                printf("[LOGGER] WARNING: SD write failed on %s\r\n",
                       s_current_log_file);
            }
        }
#endif /* TASK_SD_CARD_ENABLE */

        /* ----------------------------------------------------------------
         * SUB-FEATURE B: Terminal broker printer
         * ------------------------------------------------------------- */
#if (FEATURE_LOGGER_PRINT_ENABLE == 1)
        if ((ui32_now_ms - ui32_last_print_tick_ms) >= LOGGER_PRINT_PERIOD_MS)
        {
            ui32_last_print_tick_ms = ui32_now_ms;
            vd_Logger_PrintBrokerData();
        }
#endif /* FEATURE_LOGGER_PRINT_ENABLE */

        /* ----------------------------------------------------------------
         * Task sleep — wake every 100 ms for SD writes; the print timer
         * above gates the heavier terminal output independently.
         * ------------------------------------------------------------- */
        osDelay(100U);
    }
}
