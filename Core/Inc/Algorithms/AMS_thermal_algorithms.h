/**
 * @file    AMS_thermal_algorithms.h
 * @brief   Battery pack thermal stats: max/min/avg/delta cell temperature,
 *          this-sample and session-worst.
 *
 *          Pure domain logic — zero Broker/RTOS/hardware dependency, same
 *          shape as AMS_telemetry_algorithms.h. A future BMS task owns one
 *          AMS_ThermalAccumulator_t and folds one cell-temperature reading
 *          into it per iteration.
 *
 *          Single responsibility: this file only answers "how hot is the
 *          pack, and how uneven." It knows nothing about current or
 *          charge — see AMS_charge_algorithms.h and AMS_current_algorithms.h
 *          for those.
 */
#ifndef ALGORITHMS_THERMAL_ALGORITHMS_H_
#define ALGORITHMS_THERMAL_ALGORITHMS_H_

#include "AMS_DataTypes.h"

/**
 * @brief Running thermal accumulator. One instance lives for the whole task
 *        lifetime and is folded into on every cell-temperature reading.
 */
typedef struct {
    int16_t  i16_session_max_temp_cC;    /* hottest any cell has been seen, this boot */
    int16_t  i16_session_max_delta_cC;   /* worst max-min spread seen, this boot */
    int64_t  i64_sum_avg_temp_cC;        /* running sum of each sample's spatial mean, for time-avg */
    uint32_t ui32_samples;
    bool     b_primed;
} AMS_ThermalAccumulator_t;

/* AMS_ThermalStats_t (the derived output this module publishes) is defined
 * in AMS_DataStructs.h, not here — see the note in AMS_charge_algorithms.h. */

/**
 * @brief Folds one set of per-cell temperature readings into the
 *        accumulator and derives the current thermal snapshot from it.
 *
 * @param  p_acc              Accumulator to read and update in place.
 * @param  p_cell_temps_cC    Array of per-cell temperatures, centi-°C.
 * @param  ui8_cell_count     Number of entries in p_cell_temps_cC. Must be
 *                             >= 1.
 * @param  p_out              Filled with the derived snapshot on success.
 * @retval true   a valid reading was folded in, *p_out is valid.
 * @retval false  NULL argument or ui8_cell_count == 0. *p_out and *p_acc
 *                are untouched on false.
 */
bool b_ThermalCalc_Fold(AMS_ThermalAccumulator_t *p_acc,
                         const int16_t *p_cell_temps_cC,
                         uint8_t        ui8_cell_count,
                         AMS_ThermalStats_t *p_out);

#endif /* ALGORITHMS_THERMAL_ALGORITHMS_H_ */
