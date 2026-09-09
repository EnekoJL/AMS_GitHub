/**
 * @file    AMS_bms_safety_algorithms.h
 * @brief   Pure battery-pack safety math: NTC voltage->temperature lookup,
 *          and per-cell under/over-voltage detection with debounce.
 *
 *          Pure domain logic — zero Broker/RTOS/hardware dependency, same
 *          shape as the other Algorithms modules. Ported from
 *          Test_4_09_2025/.../spi_stm32f4.c (calculate_temperature_lookup(),
 *          safety_BMS()), which mixed this math directly into the task loop
 *          together with HAL_GPIO_WritePin() calls and printf() — split out
 *          here so it's testable on host and reusable without either.
 *
 *          Single responsibility: this file only answers "is a cell voltage
 *          out of range" and "what temperature does this NTC reading mean."
 *          It knows nothing about SPI, the LTC6813, or the Broker — see
 *          Drivers_Custom/AMS_bms_driver.c for the hardware side and
 *          Middleware/AMS_BMS_Task.c for where these two meet.
 */
#ifndef ALGORITHMS_BMS_SAFETY_ALGORITHMS_H_
#define ALGORITHMS_BMS_SAFETY_ALGORITHMS_H_

#include <stdint.h>
#include <stdbool.h>

/** @brief Default per-cell voltage safety thresholds, millivolts.
 *         Same values as Test_4_09_2025's safety_BMS() (2.800V / 4.300V). */
#define BMS_CELL_UNDERVOLTAGE_mV  2800u
#define BMS_CELL_OVERVOLTAGE_mV   4300u

/** @brief Consecutive out-of-range samples required before a fault latches.
 *         Same debounce depth as the ported original (4 samples). */
#define BMS_FAULT_DEBOUNCE_SAMPLES  4u

/**
 * @brief Debounce state for the cell-voltage safety check. One instance
 *        lives for the whole task lifetime and is passed in every cycle.
 */
typedef struct {
    uint8_t ui8_out_of_range_count;
} AMS_CellVoltageFaultState_t;

/**
 * @brief Checks every cell voltage against [uv_threshold, ov_threshold] and
 *        debounces the result over BMS_FAULT_DEBOUNCE_SAMPLES consecutive
 *        calls before reporting a fault — a single glitchy sample does not
 *        latch a fault, matching the ported original's behaviour.
 *
 * @param  p_state          Debounce state to read and update in place.
 * @param  p_cell_mV         Array of per-cell voltages, millivolts.
 * @param  ui16_cell_count   Number of entries in p_cell_mV. Must be >= 1.
 * @param  ui16_uv_threshold_mV  Under-voltage threshold, millivolts.
 * @param  ui16_ov_threshold_mV  Over-voltage threshold, millivolts.
 * @retval true   fault confirmed (>= BMS_FAULT_DEBOUNCE_SAMPLES consecutive
 *                out-of-range readings).
 * @retval false  no fault (either all cells in range, or not enough
 *                consecutive bad samples yet).
 */
bool b_BmsSafety_CheckCellVoltage(AMS_CellVoltageFaultState_t *p_state,
                                   const uint16_t *p_cell_mV,
                                   uint16_t        ui16_cell_count,
                                   uint16_t        ui16_uv_threshold_mV,
                                   uint16_t        ui16_ov_threshold_mV);

/**
 * @brief Converts one NTC voltage-divider reading to a temperature, via
 *        linear interpolation over a fixed resistance/temperature table
 *        (same table and pull-up/reference values as the ported original).
 *
 * @param  f_voltage_reading  Measured NTC node voltage, volts.
 * @retval Temperature in degrees Celsius, or -273.15f (absolute zero, used
 *         as an explicit error sentinel) if the reading is out of the
 *         valid voltage range for the divider.
 */
float f_BmsSafety_NtcVoltageToTempC(float f_voltage_reading);

#endif /* ALGORITHMS_BMS_SAFETY_ALGORITHMS_H_ */
