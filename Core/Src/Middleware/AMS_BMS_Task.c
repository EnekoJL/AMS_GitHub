/**
 * @file    AMS_BMS_Task.c
 * @brief   RTOS task for the LTC6813 battery pack BMS. Single writer of
 *          AMS_BMS_Data_t.
 *
 * Every ~250ms (same cadence as Test_4_09_2025's bms_loop()):
 *   1. b_BMS_Driver_Measure()      -> raw per-cell voltage + per-channel
 *                                      temperature (Drivers_Custom, HAL only)
 *   2. b_BmsSafety_CheckCellVoltage() -> debounced UV/OV fault check
 *                                         (Algorithms, pure math)
 *   3. b_Broker_Update_BMSData()   -> publish the snapshot
 *
 * No printf, no direct HAL_GPIO_WritePin for LED indication — the fault
 * only sets BMS_FAULT_CELL_VOLTAGE in ui32_fault_flags. See this task's
 * README for why no LED color was claimed for it (all four are already
 * used elsewhere) and what's needed before this can drive one.
 */
#include "Middleware/AMS_BMS_Task.h"
#include "Middleware/AMS_DataBroker.h"
#include "Drivers_Custom/AMS_bms_driver.h"
#include "Algorithms/AMS_bms_safety_algorithms.h"
#include "cmsis_os.h"
#include <stdio.h>

#define BMS_TASK_PERIOD_MS  250u

void vd_BMS_Task_Init(SPI_HandleTypeDef *phspi)
{
    if (!b_BMS_Driver_Init(phspi)) {
        printf("[BMS] WARNING: init readback PEC error — check SPI2 wiring (PB14/PB15/PD3) and CS (PH6)\r\n");
    }
}

void vd_BMS_Manager_TaskProcess(void)
{
    AMS_CellVoltageFaultState_t fault_state = {0};

    for (;;) {
        AMS_BMS_Data_t snapshot = {0};

        if (b_BMS_Driver_Measure(&snapshot)) {
            bool fault = b_BmsSafety_CheckCellVoltage(&fault_state,
                                                        snapshot.ui16_cell_mV,
                                                        BMS_TOTAL_CELLS,
                                                        BMS_CELL_UNDERVOLTAGE_mV,
                                                        BMS_CELL_OVERVOLTAGE_mV);
            if (fault) {
                snapshot.ui32_fault_flags |= BMS_FAULT_CELL_VOLTAGE;
                printf("[BMS] FAULT: cell voltage out of range (cell %u = %umV or cell %u = %umV)\r\n",
                       snapshot.ui8_min_cell_id, snapshot.ui16_min_cell_mV,
                       snapshot.ui8_max_cell_id, snapshot.ui16_max_cell_mV);
            }

            /* i32_pack_current_mA and soc_percent_x10 stay 0 — no producer yet, see
             * AMS_BMS_Data_t's doc comment in AMS_DataStructs.h. */
            if (!b_Broker_Update_BMSData(&snapshot)) {
                printf("[BMS] WARNING: Broker write failed, snapshot dropped\r\n");
            }
        } else {
            printf("[BMS] WARNING: measurement PEC error, snapshot skipped this cycle\r\n");
        }

        osDelay(BMS_TASK_PERIOD_MS);
    }
}
