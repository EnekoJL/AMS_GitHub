/**
 * @file    AMS_Data_Calculator_Task.c
 * @brief   Telemetry calculator task that uses GPS DataBroker info.
 *
 *          The actual math (odometer, max/avg speed, max accel/decel) lives
 *          in Algorithms/AMS_telemetry_algorithms.c as a pure function —
 *          this file only does task orchestration: read Broker, call the
 *          algorithm, write Broker, print, sleep.
 */

#include "Middleware/AMS_Data_Calculator_Task.h"
#include "Middleware/AMS_DataBroker.h"
#include "Middleware/AMS_Led_Task.h"
#include "Algorithms/AMS_telemetry_algorithms.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <stdlib.h> // For abs()

void vd_Calculator_Task_Init(void) {

}

void vd_Calculator_Manager_TaskProcess(void) {
    AMS_TelemetryAccumulator_t acc = {0};

    /* TODO: seed lifetime baselines here once AMS_Persistent_Config_t has a
     * flash schema for historic distance/max-speed/max-accel (not decided
     * yet). Until then acc's baseline_* fields stay 0 (see the {0} init
     * above), which is a safe default: b_TelemetryCalc_ProcessFix() reports
     * lifetime == session on every boot rather than silently guessing.
     *   vd_TelemetryCalc_SeedLifetime(&acc, cfg.distance_m, cfg.max_vel, ...);
     */

    for (;;) {
        vd_LED_Manager_Process();

        GPS_Data_t st_gps_data;
        if (b_Broker_Get_GPSData(&st_gps_data)) {
            AMS_Telemetry_Data_t st_telemetry;

            if (b_TelemetryCalc_ProcessFix(&acc, &st_gps_data, &st_telemetry)) {
                b_Broker_Update_TelemetryData(&st_telemetry);

                // Print using integer components to save stack space
                int32_t dist_m = st_telemetry.ui32_total_distance_m;

                int32_t max_v_i = st_telemetry.i32_max_vel_kmh_x1000 / 1000;
                int32_t max_v_d = abs((int)(st_telemetry.i32_max_vel_kmh_x1000 % 1000)) / 100;

                int32_t avg_v_i = st_telemetry.i32_avg_vel_kmh_x1000 / 1000;
                int32_t avg_v_d = abs((int)(st_telemetry.i32_avg_vel_kmh_x1000 % 1000)) / 100;

                int32_t max_a_i = st_telemetry.i32_max_accel_ms2_x1000 / 1000;
                int32_t max_a_d = abs((int)(st_telemetry.i32_max_accel_ms2_x1000 % 1000)) / 10;

                int32_t max_d_i = st_telemetry.i32_max_decel_ms2_x1000 / 1000;
                int32_t max_d_d = abs((int)(st_telemetry.i32_max_decel_ms2_x1000 % 1000)) / 10;

                printf("[TELEMETRY] Dist: %ld m | MaxV: %ld.%01ld km/h | AvgV: %ld.%01ld km/h | MaxAcc: %ld.%02ld m/s2 | MaxDec: %ld.%02ld m/s2\r\n",
                       dist_m, max_v_i, max_v_d, avg_v_i, avg_v_d, max_a_i, max_a_d, max_d_i, max_d_d);
            }
        }

        osDelay(10);
    }
}
