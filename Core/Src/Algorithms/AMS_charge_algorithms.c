/**
 * @file    AMS_charge_algorithms.c
 * @brief   See AMS_charge_algorithms.h.
 */

#include "Algorithms/AMS_charge_algorithms.h"
#include <stddef.h>

void vd_ChargeCalc_SeedLifetime(AMS_ChargeAccumulator_t *p_acc,
                                 uint32_t ui32_discharged_mAh,
                                 uint32_t ui32_charged_mAh)
{
    if (p_acc == NULL) {
        return;
    }
    p_acc->ui32_baseline_discharged_mAh = ui32_discharged_mAh;
    p_acc->ui32_baseline_charged_mAh    = ui32_charged_mAh;
}

bool b_ChargeCalc_Fold(AMS_ChargeAccumulator_t *p_acc,
                        int32_t  i32_pack_current_mA,
                        uint32_t ui32_now_ms,
                        AMS_ChargeStats_t *p_out)
{
    if (p_acc == NULL || p_out == NULL) {
        return false;
    }

    if (!p_acc->b_primed) {
        /* First-ever sample: nothing to integrate against yet (no dt). */
        p_acc->b_primed = true;
        p_acc->ui32_last_sample_tick_ms = ui32_now_ms;
        return false;
    }

    uint32_t ui32_dt_ms = ui32_now_ms - p_acc->ui32_last_sample_tick_ms;
    if (ui32_dt_ms == 0U) {
        return false; /* duplicate tick, nothing new to integrate */
    }
    p_acc->ui32_last_sample_tick_ms = ui32_now_ms;

    /* Rectangular integration in mA*ms, accumulated as uint64 so nothing
     * truncates per sample (a single 10ms/200A sample is 0.55 mAh — if we
     * converted to mAh before summing, integer truncation would throw away
     * ~90% of the actual charge). Convert to mAh only once, at output. */
    if (i32_pack_current_mA < 0) {
        p_acc->ui64_discharge_mA_ms += (uint64_t)(-(int64_t)i32_pack_current_mA) * ui32_dt_ms;
    } else {
        p_acc->ui64_charge_mA_ms += (uint64_t)(int64_t)i32_pack_current_mA * ui32_dt_ms;
    }

    p_out->ui32_session_discharged_mAh = (uint32_t)(p_acc->ui64_discharge_mA_ms / 3600000ULL);
    p_out->ui32_session_charged_mAh    = (uint32_t)(p_acc->ui64_charge_mA_ms    / 3600000ULL);

    p_out->ui32_lifetime_discharged_mAh = p_acc->ui32_baseline_discharged_mAh
                                         + p_out->ui32_session_discharged_mAh;
    p_out->ui32_lifetime_charged_mAh    = p_acc->ui32_baseline_charged_mAh
                                         + p_out->ui32_session_charged_mAh;

    return true;
}
