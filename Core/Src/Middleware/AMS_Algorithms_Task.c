/**
 * @file    AMS_Algorithms_Task.c
 * @brief   Single task hosting every derived/calculated value: current,
 *          charge/SOC, telemetry, LED. Each section below reads its own
 *          Broker domain, calls its own single-responsibility Algorithms
 *          module, and writes its own output — they do not share state or
 *          call into each other. Deleting or reordering one section has no
 *          effect on the others.
 *
 *          Why one task instead of four small ones: none of these four
 *          things is expensive enough to justify its own stack/TCB, and
 *          keeping "where does derived data get (re)computed" in one place
 *          is easier for a rotating team to find than four scattered tasks.
 *          The actual math for each section still lives in its own
 *          Algorithms source file — this file is orchestration only (read
 *          Broker -> call algorithm -> write Broker), same as every other
 *          task in this codebase.
 *
 *          KNOWN TRADEOFF — read before touching the CURRENT/SOC sections:
 *          this task polls the Broker's latest AMS_BMS_Data_t.i32_pack_
 *          current_mA once per loop (currently 10ms). Coulomb counting
 *          (the SOC section) is only as accurate as this loop's period is
 *          fast relative to how quickly pack current actually changes —
 *          the Broker only holds the LATEST sample, so if a future BMS
 *          task ever samples current faster than this loop polls, current
 *          transients between polls are invisible to the Ah integral. If
 *          that turns out to matter once real BMS hardware exists, move
 *          just the CURRENT/SOC folds into the BMS task's own acquisition
 *          loop instead (it sees every sample with an exact dt) — see
 *          docs/Next_Steps.md. Telemetry has the same "sees the Broker's
 *          latest, not every sample" property, but GPS fixes arrive far
 *          slower than this loop polls, so it's a non-issue there.
 *
 *          THERMAL (4th section, below) reads AMS_BMS_Data_t.i16_cell_temp_cC[],
 *          written by AMS_BMS_Task every ~250ms — real as of that task
 *          existing, no longer a placeholder like CURRENT/SOC above.
 *
 *          Five sections total, in order: Current, Charge/SOC, Telemetry,
 *          Thermal, LED.
 */

#include "Middleware/AMS_Algorithms_Task.h"
#include "Middleware/AMS_DataBroker.h"
#include "Middleware/AMS_Led_Task.h"
#include "Algorithms/AMS_current_algorithms.h"
#include "Algorithms/AMS_charge_algorithms.h"
#include "Algorithms/AMS_telemetry_algorithms.h"
#include "Algorithms/AMS_thermal_algorithms.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <stdlib.h> /* abs() */

void vd_Algorithms_Task_Init(void)
{
}

void vd_Algorithms_Manager_TaskProcess(void)
{
    /* One accumulator per section, each owned only by that section — see
     * the file header. TODO: seed lifetime baselines (charge/current) once
     * AMS_Persistent_Config_t has a flash schema for historic stats (not
     * decided yet, see AMS_Telemetry_Data_t's TODO in AMS_DataStructs.h). */
    AMS_CurrentAccumulator_t   current_acc   = {0};
    AMS_ChargeAccumulator_t    charge_acc    = {0};
    AMS_TelemetryAccumulator_t telemetry_acc = {0};
    AMS_ThermalAccumulator_t   thermal_acc   = {0};

    for (;;) {
        /* -----------------------------------------------------------
         * 1. CURRENT — peak discharge/charge current, session + lifetime.
         *    Input: AMS_BMS_Data_t.i32_pack_current_mA (Broker; still a
         *    placeholder until a BMS task exists, so this folds zeros
         *    until then — harmless, matches the rest of the codebase's
         *    placeholder-domain convention).
         * ----------------------------------------------------------- */
        {
            AMS_BMS_Data_t bms = {0};
            if (b_Broker_Get_BMSData(&bms)) {
                AMS_CurrentStats_t current_stats = {0};
                if (b_CurrentCalc_Fold(&current_acc, bms.i32_pack_current_mA, &current_stats)) {
                    AMS_BatteryStats_Data_t stats = {0};
                    if (b_Broker_Get_BatteryStats(&stats)) {
                        stats.current = current_stats;
                        b_Broker_Update_BatteryStats(&stats);
                    }
                }
            }
        }

        /* -----------------------------------------------------------
         * 2. CHARGE / SOC — Ah discharged/charged (coulomb counting),
         *    session + lifetime. Same input as CURRENT above, different
         *    question ("how much moved total" vs "worst instant") — see
         *    AMS_current_algorithms.h's file header for why that's two
         *    modules, not one.
         * ----------------------------------------------------------- */
        {
            AMS_BMS_Data_t bms = {0};
            if (b_Broker_Get_BMSData(&bms)) {
                AMS_ChargeStats_t charge_stats = {0};
                if (b_ChargeCalc_Fold(&charge_acc, bms.i32_pack_current_mA,
                                       osKernelGetTickCount(), &charge_stats)) {
                    AMS_BatteryStats_Data_t stats = {0};
                    if (b_Broker_Get_BatteryStats(&stats)) {
                        stats.charge = charge_stats;
                        b_Broker_Update_BatteryStats(&stats);
                    }
                }
            }
        }

        /* -----------------------------------------------------------
         * 3. TELEMETRY — odometer, max/avg speed, max accel/decel
         *    (session + lifetime), derived from GPS fixes.
         * ----------------------------------------------------------- */
        {
            GPS_Data_t st_gps_data;
            if (b_Broker_Get_GPSData(&st_gps_data)) {
                AMS_Telemetry_Data_t st_telemetry;
                if (b_TelemetryCalc_ProcessFix(&telemetry_acc, &st_gps_data, &st_telemetry)) {
                    b_Broker_Update_TelemetryData(&st_telemetry);

                    /* Print using integer components to save stack space */
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
        }

        /* -----------------------------------------------------------
         * 4. THERMAL — max/min/avg/delta cell temperature, session-worst.
         *    Input: AMS_BMS_Data_t.i16_cell_temp_cC[] (Broker, real —
         *    written by AMS_BMS_Task from the LTC6813's NTC channels).
         * ----------------------------------------------------------- */
        {
            AMS_BMS_Data_t bms = {0};
            if (b_Broker_Get_BMSData(&bms)) {
                AMS_ThermalStats_t thermal_stats = {0};
                if (b_ThermalCalc_Fold(&thermal_acc, bms.i16_cell_temp_cC, BMS_TOTAL_TEMP_CH, &thermal_stats)) {
                    AMS_BatteryStats_Data_t stats = {0};
                    if (b_Broker_Get_BatteryStats(&stats)) {
                        stats.thermal = thermal_stats;
                        b_Broker_Update_BatteryStats(&stats);
                    }
                }
            }
        }

        /* -----------------------------------------------------------
         * 5. LED — drive the ON/OFF/TOGGLE/BLINK state machine. No Broker
         *    domain of its own; purely local state inside AMS_Led_Task.c.
         * ----------------------------------------------------------- */
        vd_LED_Manager_Process();

        osDelay(10);
    }
}
