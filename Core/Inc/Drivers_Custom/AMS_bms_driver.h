/**
 * @file    AMS_bms_driver.h
 * @brief   Custom LTC6813 BMS driver for AMS project.
 *          Thin wrapper over the vendor LTC6813/LTC681x library
 *          (Drivers_Vendor/LTC681x_LTC6813/, untouched Analog Devices code)
 *          plus the SPI/CS/delay glue it needs. No business logic (safety
 *          thresholds, NTC math) and no printf — see
 *          Algorithms/AMS_bms_safety_algorithms.c for the math and
 *          Middleware/AMS_BMS_Task.c for where they're combined.
 *
 *          Ported from Test_4_09_2025's bms_hardware.c + spi_stm32f4.c,
 *          which mixed this HAL glue directly with business logic and
 *          terminal printing in one file.
 *
 * Hardware wiring (bench + production, same board per team decision):
 *   SPI2, PB14 = MISO, PB15 = MOSI, PD3 = SCK (AF5, all three configured by
 *   MX_SPI2_Init()/HAL_SPI_MspInit() in main.c / stm32f4xx_hal_msp.c).
 *   PH6 = chip-select, plain GPIO output, software-driven (SPI2 NSS=SOFT).
 *
 *   NOTE ON PH6: Test_4_09_2025's original project used PD3 for BOTH its
 *   "SS_BMS" GPIO label AND the real SPI2_SCK alternate function — its
 *   cs_low()/cs_high() toggled a pin that was actually driven by the SPI
 *   peripheral as a clock line, so chip-select never functioned as a real,
 *   separate signal there. PH6 here is a genuine, dedicated GPIO output,
 *   confirmed against the real board wiring — do not reuse that PD3
 *   double-booking.
 *
 * Usage (called from AMS_BMS_Task):
 *   vd_BMS_Driver_Init(&hspi2);
 *   AMS_BMS_Data_t snap = {0};
 *   if (b_BMS_Driver_Measure(&snap)) { ... }   // fills cell/temp arrays only
 */

#ifndef DRIVERS_CUSTOM_AMS_BMS_DRIVER_H_
#define DRIVERS_CUSTOM_AMS_BMS_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "AMS_DataStructs.h"
#include <stdbool.h>

/**
 * @brief Binds the driver to a HAL SPI handle, configures both LTC6813 ICs'
 *        registers, writes them, and reads back to verify. Must be called
 *        once before b_BMS_Driver_Measure().
 * @param phspi  Pointer to the HAL SPI handle (SPI2).
 * @retval true   config written and read back matches.
 * @retval false  PEC error on readback — see driver .c for detail.
 */
bool b_BMS_Driver_Init(SPI_HandleTypeDef *phspi);

/**
 * @brief Runs one full measurement cycle: wakes both ICs, triggers and
 *        polls cell/aux/stat ADC conversions, reads them back, and fills
 *        p_out's per-cell voltage and per-channel temperature arrays
 *        (ui16_cell_mV[BMS_TOTAL_CELLS], i16_cell_temp_cC[BMS_TOTAL_TEMP_CH])
 *        plus ui32_pack_voltage_mV / ui16_min_cell_mV / ui16_max_cell_mV /
 *        ui8_min_cell_id / ui8_max_cell_id / i16_max_cell_temp_cC.
 *
 *        Does NOT touch i32_pack_current_mA, soc_percent_x10, or
 *        ui32_fault_flags — those are not this driver's concern (current
 *        needs a separate sensor; fault flags are
 *        Algorithms/AMS_bms_safety_algorithms.c's job, run by the task).
 *
 * @param p_out  Snapshot to fill. Only the fields listed above are written.
 * @retval true   read succeeded (no PEC error).
 * @retval false  NULL p_out, or a PEC error was detected on readback.
 */
bool b_BMS_Driver_Measure(AMS_BMS_Data_t *p_out);

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_CUSTOM_AMS_BMS_DRIVER_H_ */
