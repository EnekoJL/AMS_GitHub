/**
 * @file    AMS_current_algorithms.h
 * @brief   Peak pack-current tracking: max discharge/charge current, session
 *          and lifetime.
 *
 *          Pure domain logic — zero Broker/RTOS/hardware dependency, same
 *          shape as AMS_telemetry_algorithms.h. A future BMS task owns one
 *          AMS_CurrentAccumulator_t and folds one current sample into it
 *          per iteration — same input as AMS_charge_algorithms.h, different
 *          question ("what was the worst instant?" vs "how much moved
 *          total?"), which is why it's a separate module rather than a
 *          field bolted onto AMS_ChargeStats_t.
 */
#ifndef ALGORITHMS_CURRENT_ALGORITHMS_H_
#define ALGORITHMS_CURRENT_ALGORITHMS_H_

#include "AMS_DataTypes.h"

/**
 * @brief Running peak-current accumulator. One instance lives for the whole
 *        task lifetime and is folded into on every current sample.
 */
typedef struct {
    uint32_t ui32_session_max_discharge_mA;
    uint32_t ui32_session_max_charge_mA;

    /* Lifetime baseline, seeded once at boot — see SeedLifetime below. */
    uint32_t ui32_baseline_max_discharge_mA;
} AMS_CurrentAccumulator_t;

/* AMS_CurrentStats_t (the derived output this module publishes) is defined
 * in AMS_DataStructs.h, not here — see the note in AMS_charge_algorithms.h. */

/**
 * @brief Seeds the lifetime baseline. Call once, before the first Fold(),
 *        from whatever loads historic totals (not wired up yet — see the
 *        TODO on AMS_Persistent_Config_t in AMS_DataStructs.h). Safe to
 *        skip: an un-seeded accumulator just reports lifetime == session.
 */
void vd_CurrentCalc_SeedLifetime(AMS_CurrentAccumulator_t *p_acc,
                                  uint32_t ui32_max_discharge_mA);

/**
 * @brief Folds one pack-current sample into the accumulator and derives the
 *        current peak-current snapshot from it.
 *
 * @param  p_acc              Accumulator to read and update in place.
 * @param  i32_pack_current_mA Instantaneous pack current, mA. Signed:
 *                             negative = discharging, positive = charging.
 * @param  p_out              Filled with the derived snapshot.
 * @retval true   always, unless a NULL argument was passed.
 * @retval false  p_acc or p_out was NULL.
 */
bool b_CurrentCalc_Fold(AMS_CurrentAccumulator_t *p_acc,
                         int32_t i32_pack_current_mA,
                         AMS_CurrentStats_t *p_out);

#endif /* ALGORITHMS_CURRENT_ALGORITHMS_H_ */
