/**
 * @file    FreeRTOS.h  (TEST-ONLY STUB — not the real vendor header)
 *
 * The real FreeRTOS.h lives in
 * Middlewares/Third_Party/FreeRTOS/Source/include/FreeRTOS.h and pulls in
 * FreeRTOSConfig.h, portmacro.h, and a large amount of kernel-internal
 * configuration — not something a host test build should try to parse.
 *
 * Same convention as cmsis_os.h in this directory: this file is TEST-ONLY,
 * never seen by the real STM32CubeIDE target build (not referenced by
 * .cproject), and exists purely so a Middleware file that includes
 * "FreeRTOS.h" + "task.h" for taskENTER_CRITICAL()/taskEXIT_CRITICAL()
 * (see task.h in this same directory) can be host-tested without pulling
 * in the real kernel headers.
 */
#ifndef FREERTOS_H_
#define FREERTOS_H_
#endif /* FREERTOS_H_ */
