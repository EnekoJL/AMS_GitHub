/**
 * @file    AMS_Data_Calculator_Task.c
 * @brief   Telemetry calculator task that uses GPS DataBroker info.
 */

#include "Middleware/AMS_Data_Calculator_Task.h"
#include "Middleware/AMS_DataBroker.h"
#include "Middleware/AMS_Led_Task.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <stdlib.h> // For abs()

static uint32_t ui32_last_processed_tick = 0;
static uint32_t ui32_total_distance_mm = 0;
static int32_t  i32_max_vel_kmh_x1000 = 0;
static int64_t  i64_sum_vel_kmh_x1000 = 0;
static uint32_t ui32_vel_samples = 0;
static int32_t  i32_max_accel_ms2_x1000 = 0;
static int32_t  i32_max_decel_ms2_x1000 = 0;
static int32_t  i32_last_vel_ms_x1000 = 0;

void vd_Calculator_Task_Init(void) {
    
}

void vd_Calculator_Manager_TaskProcess(void) {
    for (;;) {
        vd_LED_Manager_Process();

        GPS_Data_t st_gps_data;
        if (b_Broker_Get_GPSData(&st_gps_data)) {
            if (st_gps_data.b_gps_is_connected && st_gps_data.ui32_last_fix_tick_ms != ui32_last_processed_tick) {
                
                int32_t i32_vel_kmh = st_gps_data.i32_vel_kmh_x1000;
                int32_t i32_vel_ms = st_gps_data.i32_vel_ms_x1000;

                if (ui32_last_processed_tick != 0) {
                    uint32_t ui32_dt_ms = st_gps_data.ui32_last_fix_tick_ms - ui32_last_processed_tick;
                    
                    if (ui32_dt_ms > 0) {
                        /* Odometer (mm = (mm/s * ms) / 1000) */
                        uint64_t ui64_dist_step_mm = ((uint64_t)i32_vel_ms * ui32_dt_ms) / 1000ULL;
                        ui32_total_distance_mm += (uint32_t)ui64_dist_step_mm;

                        /* Acceleration (mm/s^2 = (delta_mm/s * 1000) / dt_ms) */
                        int32_t i32_delta_v_ms = i32_vel_ms - i32_last_vel_ms_x1000;
                        int32_t i32_accel_ms2 = (i32_delta_v_ms * 1000) / (int32_t)ui32_dt_ms;
                        
                        if (i32_accel_ms2 > i32_max_accel_ms2_x1000) {
                            i32_max_accel_ms2_x1000 = i32_accel_ms2;
                        }
                        if (i32_accel_ms2 < i32_max_decel_ms2_x1000) {
                            i32_max_decel_ms2_x1000 = i32_accel_ms2;
                        }
                    }
                }

                /* Max Velocity */
                if (i32_vel_kmh > i32_max_vel_kmh_x1000) {
                    i32_max_vel_kmh_x1000 = i32_vel_kmh;
                }

                /* Average Velocity */
                i64_sum_vel_kmh_x1000 += i32_vel_kmh;
                ui32_vel_samples++;
                int32_t i32_avg_vel_kmh_x1000 = (int32_t)(i64_sum_vel_kmh_x1000 / ui32_vel_samples);

                AMS_Telemetry_Data_t st_telemetry;
                st_telemetry.ui32_total_distance_m = ui32_total_distance_mm / 1000;
                st_telemetry.i32_max_vel_kmh_x1000 = i32_max_vel_kmh_x1000;
                st_telemetry.i32_avg_vel_kmh_x1000 = i32_avg_vel_kmh_x1000;
                st_telemetry.i32_max_accel_ms2_x1000 = i32_max_accel_ms2_x1000;
                st_telemetry.i32_max_decel_ms2_x1000 = i32_max_decel_ms2_x1000;
                
                b_Broker_Update_TelemetryData(&st_telemetry);

                // Print using integer components to save stack space
                int32_t dist_m = st_telemetry.ui32_total_distance_m;
                
                int32_t max_v_i = i32_max_vel_kmh_x1000 / 1000;
                int32_t max_v_d = abs((int)(i32_max_vel_kmh_x1000 % 1000)) / 100;
                
                int32_t avg_v_i = i32_avg_vel_kmh_x1000 / 1000;
                int32_t avg_v_d = abs((int)(i32_avg_vel_kmh_x1000 % 1000)) / 100;
                
                int32_t max_a_i = i32_max_accel_ms2_x1000 / 1000;
                int32_t max_a_d = abs((int)(i32_max_accel_ms2_x1000 % 1000)) / 10;
                
                int32_t max_d_i = i32_max_decel_ms2_x1000 / 1000;
                int32_t max_d_d = abs((int)(i32_max_decel_ms2_x1000 % 1000)) / 10;

                printf("[TELEMETRY] Dist: %ld m | MaxV: %ld.%01ld km/h | AvgV: %ld.%01ld km/h | MaxAcc: %ld.%02ld m/s2 | MaxDec: %ld.%02ld m/s2\r\n",
                       dist_m, max_v_i, max_v_d, avg_v_i, avg_v_d, max_a_i, max_a_d, max_d_i, max_d_d);

                ui32_last_processed_tick = st_gps_data.ui32_last_fix_tick_ms;
                i32_last_vel_ms_x1000 = i32_vel_ms;
            }
        }

        osDelay(10);
    }
}
