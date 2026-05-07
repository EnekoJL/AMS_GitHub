/**
 * @file    AMS_Data_Calculator_Task.h
 * @brief   Telemetry calculator task that uses GPS DataBroker info.
 */

#ifndef MIDDLEWARE_AMS_DATA_CALCULATOR_TASK_H_
#define MIDDLEWARE_AMS_DATA_CALCULATOR_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the calculator task.
 */
void vd_Calculator_Task_Init(void);

/**
 * @brief Infinite RTOS loop: processes telemetry from GPS data.
 */
void vd_Calculator_Manager_TaskProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_AMS_DATA_CALCULATOR_TASK_H_ */
