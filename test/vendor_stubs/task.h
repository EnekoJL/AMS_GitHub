/**
 * @file    task.h  (TEST-ONLY STUB — not the real vendor header)
 *
 * The real task.h (Middlewares/Third_Party/FreeRTOS/Source/include/task.h)
 * declares the full FreeRTOS task API. This project's Middleware layer
 * only ever uses two macros from it directly (everywhere else goes through
 * the CMSIS-RTOS v2 wrapper, see cmsis_os.h in this directory):
 * taskENTER_CRITICAL()/taskEXIT_CRITICAL(), used for short read-modify-
 * write sections shared between tasks (e.g. AMS_Led_Task.c's per-channel
 * state, AMS_adc_driver.c's shadow buffer).
 *
 * On host, there is no real interrupt controller to mask — these are
 * no-ops here. That's correct for what's being tested: the logic inside
 * the critical section, not the actual interrupt-masking mechanism (which
 * only exists on target hardware).
 */
#ifndef TASK_H_
#define TASK_H_

#define taskENTER_CRITICAL()  ((void)0)
#define taskEXIT_CRITICAL()   ((void)0)

#endif /* TASK_H_ */
