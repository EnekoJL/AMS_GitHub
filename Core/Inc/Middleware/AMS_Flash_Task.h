/**
 * @file    AMS_persistence_manager.h
 * @brief   Gestor de Persistencia (Middleware).
 *          Abstrae la lógica de búsqueda/escritura circular de registros en Flash.
 *          Habla con el DataBroker para cargar y guardar la configuración.
 * @author  Eneko Juanena
 * @date    11 de Marzo de 2026
 */
#ifndef AMS_FLASH_TASK_H_
#define AMS_FLASH_TASK_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  Inicializa el gestor de persistencia.
 *         Escanea la Flash para encontrar el último registro válido
 *         y carga los datos en el DataBroker.
 *         Si no hay datos previos, carga valores por defecto.
 *
 * @note   *** COMENTAR ESTA LLAMADA en main.c para desactivar la persistencia ***
 */
void vd_Persist_Init(void);

/**
 * @brief  Guarda la configuración actual del DataBroker en Flash.
 *         Escribe en el siguiente slot libre (circular).
 *         Si el sector actual está lleno, borra el alternativo y salta a él.
 *
 * @retval true  si la escritura fue exitosa.
 * @retval false si hubo error o el sistema no está inicializado.
 */
bool b_Persist_SaveConfig(void);

/**
 * @brief  Devuelve el número de registro (slot) donde se ha escrito por última vez.
 *         Útil para debug desde la consola.
 * @retval Índice del último slot escrito (0 = ninguno aún).
 */
uint32_t u32_Persist_GetLastSlotIndex(void);

#endif /* AMS_FLASH_TASK_H_ */
