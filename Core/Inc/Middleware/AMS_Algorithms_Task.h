/**
 * @file    AMS_Algorithms_Task.h
 * @brief   Single home for every derived/calculated value in the system.
 *
 *          Raw acquisition tasks (ADC, CAN, GPS, a future BMS task) only
 *          ever write raw Broker domains. This task is where raw data
 *          becomes derived data: peak current, charge/SOC (Ah in-out),
 *          GPS telemetry (odometer/speed/accel), and the LED state machine
 *          tick. Replaces the old AMS_Data_Calculator_Task (GPS-telemetry
 *          only) — see docs/Next_Steps.md for why it was folded in here
 *          instead of staying split across multiple small tasks.
 */
#ifndef MIDDLEWARE_AMS_ALGORITHMS_TASK_H_
#define MIDDLEWARE_AMS_ALGORITHMS_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the algorithms task. Call once before the RTOS loop.
 */
void vd_Algorithms_Task_Init(void);

/**
 * @brief Infinite RTOS loop: current, charge/SOC, telemetry, LED — see the
 *        file header of AMS_Algorithms_Task.c for why these four live
 *        together and how they stay independent of each other.
 */
void vd_Algorithms_Manager_TaskProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_AMS_ALGORITHMS_TASK_H_ */
