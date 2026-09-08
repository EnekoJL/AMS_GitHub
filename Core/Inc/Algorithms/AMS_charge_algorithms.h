/**
 * @file    AMS_charge_algorithms.h
 * @brief   Coulomb counting: turns successive pack-current samples into
 *          running Ah discharged/charged, session and lifetime.
 *
 *          Pure domain logic — zero Broker/RTOS/hardware dependency, same
 *          shape as AMS_telemetry_algorithms.h. A future BMS task owns one
 *          AMS_ChargeAccumulator_t (a local variable, outside its for(;;)
 *          loop) and folds one current sample into it per iteration.
 *
 *          Single responsibility: this file only answers "how much charge
 *          has moved through the pack." It knows nothing about temperature
 *          or peak current — see AMS_thermal_algorithms.h and
 *          AMS_current_algorithms.h for those.
 */
#ifndef ALGORITHMS_CHARGE_ALGORITHMS_H_
#define ALGORITHMS_CHARGE_ALGORITHMS_H_

#include "AMS_DataTypes.h"

/**
 * @brief Running coulomb-counting accumulator. One instance lives for the
 *        whole task lifetime and is folded into on every current sample.
 */
typedef struct {
    uint64_t ui64_discharge_mA_ms;  /* session integral, mA*ms (no per-sample truncation) */
    uint64_t ui64_charge_mA_ms;
    uint32_t ui32_last_sample_tick_ms;
    bool     b_primed;              /* false until the first sample has been seen */

    /* Lifetime baseline, seeded once at boot — see SeedLifetime below. */
    uint32_t ui32_baseline_discharged_mAh;
    uint32_t ui32_baseline_charged_mAh;
} AMS_ChargeAccumulator_t;

/* AMS_ChargeStats_t (the derived output this module publishes) is defined
 * in AMS_DataStructs.h, not here — it's a Broker domain type (nested inside
 * AMS_BatteryStats_Data_t), and AMS_DataStructs.h must not depend on the
 * Algorithms layer. Pulled in transitively via AMS_DataTypes.h above, same
 * as GPS_Data_t/AMS_Telemetry_Data_t in AMS_telemetry_algorithms.h. */

/**
 * @brief Seeds the lifetime baseline. Call once, before the first Fold(),
 *        from whatever loads historic totals (not wired up yet — see the
 *        TODO on AMS_Persistent_Config_t in AMS_DataStructs.h). Safe to
 *        skip: an un-seeded accumulator just reports lifetime == session.
 */
void vd_ChargeCalc_SeedLifetime(AMS_ChargeAccumulator_t *p_acc,
                                 uint32_t ui32_discharged_mAh,
                                 uint32_t ui32_charged_mAh);

/**
 * @brief Folds one pack-current sample into the accumulator and derives the
 *        current charge snapshot from it.
 *
 * @param  p_acc              Accumulator to read and update in place.
 * @param  i32_pack_current_mA Instantaneous pack current, mA. Signed:
 *                             negative = discharging, positive = charging
 *                             (matches AMS_BMS_Data_t.i32_pack_current_mA).
 * @param  ui32_now_ms        Current tick, for computing dt against the
 *                             last sample.
 * @param  p_out              Filled with the derived snapshot on success.
 * @retval true   a sample was integrated, *p_out is valid.
 * @retval false  nothing to integrate yet — first-ever sample (no dt),
 *                dt == 0 (duplicate tick), or a NULL argument. *p_out and
 *                *p_acc are untouched on false.
 */
bool b_ChargeCalc_Fold(AMS_ChargeAccumulator_t *p_acc,
                        int32_t  i32_pack_current_mA,
                        uint32_t ui32_now_ms,
                        AMS_ChargeStats_t *p_out);

#endif /* ALGORITHMS_CHARGE_ALGORITHMS_H_ */
