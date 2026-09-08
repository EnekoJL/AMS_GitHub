/**
 * @file    AMS_current_algorithms.c
 * @brief   See AMS_current_algorithms.h.
 */

#include "Algorithms/AMS_current_algorithms.h"
#include <stddef.h>

void vd_CurrentCalc_SeedLifetime(AMS_CurrentAccumulator_t *p_acc,
                                  uint32_t ui32_max_discharge_mA)
{
    if (p_acc == NULL) {
        return;
    }
    p_acc->ui32_baseline_max_discharge_mA = ui32_max_discharge_mA;
}

bool b_CurrentCalc_Fold(AMS_CurrentAccumulator_t *p_acc,
                         int32_t i32_pack_current_mA,
                         AMS_CurrentStats_t *p_out)
{
    if (p_acc == NULL || p_out == NULL) {
        return false;
    }

    if (i32_pack_current_mA < 0) {
        uint32_t ui32_discharge_mA = (uint32_t)(-(int64_t)i32_pack_current_mA);
        if (ui32_discharge_mA > p_acc->ui32_session_max_discharge_mA) {
            p_acc->ui32_session_max_discharge_mA = ui32_discharge_mA;
        }
    } else {
        uint32_t ui32_charge_mA = (uint32_t)i32_pack_current_mA;
        if (ui32_charge_mA > p_acc->ui32_session_max_charge_mA) {
            p_acc->ui32_session_max_charge_mA = ui32_charge_mA;
        }
    }

    p_out->ui32_session_max_discharge_mA = p_acc->ui32_session_max_discharge_mA;
    p_out->ui32_session_max_charge_mA    = p_acc->ui32_session_max_charge_mA;

    p_out->ui32_lifetime_max_discharge_mA =
        (p_acc->ui32_baseline_max_discharge_mA > p_acc->ui32_session_max_discharge_mA)
            ? p_acc->ui32_baseline_max_discharge_mA
            : p_acc->ui32_session_max_discharge_mA;

    return true;
}
