/**
 * @file    main.h  (TEST-ONLY STUB — not the real CubeMX-generated header)
 *
 * The real Core/Inc/main.h pulls in the entire STM32 HAL stack (stm32f4xx_hal.h
 * and every peripheral driver header transitively) — full of hardware register
 * definitions and ARM CMSIS core intrinsics that don't compile on a host x86
 * build. This stub declares only the specific types/macros/functions that
 * Middleware files under test actually reference, mirroring the real
 * definitions' shape exactly (same field names/types) so test code can
 * construct and pass them normally.
 *
 * Listed FIRST in project.yml's :include paths (same mechanism as
 * test/vendor_stubs/cmsis_os.h) so host test builds see this instead of the
 * real Core/Inc/main.h. TEST-ONLY — the real STM32CubeIDE target build never
 * sees this file.
 *
 * Add to this file (don't touch the real main.h) if a new file under test
 * needs another HAL type/macro/function — copy the real signature/shape
 * verbatim from the actual STM32 HAL headers.
 */
#ifndef MAIN_H_
#define MAIN_H_

#include <stdint.h>

typedef enum { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3 } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;

typedef struct { uint32_t dummy; } GPIO_TypeDef;
typedef struct { uint32_t dummy; } CAN_HandleTypeDef;
typedef struct { uint32_t StdId; uint32_t ExtId; uint32_t IDE; uint32_t RTR; uint32_t DLC; } CAN_RxHeaderTypeDef;

#define GPIO_PIN_0  ((uint16_t)0x0001)

/* Real main.h defines these as macros resolving to a real GPIOA register
 * block address + GPIO_PIN_0. Test code never dereferences GPIOA (HAL_GPIO_
 * ReadPin is mocked), so any fixed non-NULL pointer value works here. */
#define GPIOA ((GPIO_TypeDef *)0x40020000u)
#define Boton_Azul_Pin        GPIO_PIN_0
#define Boton_Azul_GPIO_Port  GPIOA

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
void Error_Handler(void);

#endif /* MAIN_H_ */
