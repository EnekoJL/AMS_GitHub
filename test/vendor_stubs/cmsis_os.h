/**
 * @file    cmsis_os.h  (TEST-ONLY STUB — not the real vendor header)
 *
 * The real cmsis_os.h lives in
 * Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os.h and pulls
 * in FreeRTOS.h, task.h, portmacro.h, CMSIS core intrinsics, etc. It's full
 * of `#if (osCMSIS < 0x20000U)`-style version guards that CMock's header
 * parser does not evaluate the same way a real C preprocessor does — it
 * only picked up the deprecated v1 API and silently missed every v2
 * function this project actually calls (osMutexNew, osMutexAcquire,
 * osMutexRelease, osKernelGetTickCount), causing "undefined reference"
 * link errors with no compile-time warning.
 *
 * Fix: this directory (test/vendor_stubs) is listed FIRST in project.yml's
 * :include paths, so host test builds see this minimal, flat,
 * conditional-free stand-in instead of the real vendor header. It declares
 * exactly the subset of the CMSIS-RTOS v2 API this project's Middleware
 * layer uses. If a new file under test calls another cmsis_os function,
 * add its prototype here (copy the real signature verbatim from
 * cmsis_os2.h) — don't try to mock the real header directly.
 *
 * This file is TEST-ONLY. The real STM32CubeIDE target build never sees
 * it — it only exists under test/ and is not referenced by the .cproject.
 */
#ifndef CMSIS_OS_H_
#define CMSIS_OS_H_

#include <stdint.h>

typedef void *osMutexId_t;

typedef enum {
  osOK                      =  0,
  osError                   = -1,
  osErrorTimeout            = -2,
  osErrorResource           = -3,
  osErrorParameter          = -4,
  osErrorNoMemory           = -5,
  osErrorISR                = -6,
} osStatus_t;

#define osMutexRecursive    1U
#define osMutexPrioInherit  2U
#define osWaitForever       0xFFFFFFFFU

/* Real macro does tick-rate math (see FreeRTOS projdefs.h); tests always
 * ignore the exact timeout value via _IgnoreArg_timeout(), so identity is
 * sufficient here — only needs to compile. */
#define pdMS_TO_TICKS(ms)   (ms)

typedef struct {
  const char    *name;
  uint32_t       attr_bits;
  void          *cb_mem;
  uint32_t       cb_size;
} osMutexAttr_t;

osMutexId_t osMutexNew(const osMutexAttr_t *attr);
osStatus_t  osMutexAcquire(osMutexId_t mutex_id, uint32_t timeout);
osStatus_t  osMutexRelease(osMutexId_t mutex_id);

uint32_t    osKernelGetTickCount(void);
osStatus_t  osDelay(uint32_t ticks);

#endif /* CMSIS_OS_H_ */
