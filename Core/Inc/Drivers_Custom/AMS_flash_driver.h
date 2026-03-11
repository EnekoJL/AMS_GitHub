/**
 * @file    AMS_flash_driver.h
 * @brief   Driver HAL puro para la Flash interna del STM32F469.
 *          Solo Lee / Escribe / Borra. Sin lógica de negocio.
 * @author  Eneko Juanena
 * @date    11 de Marzo de 2026
 *
 * Mapa de sectores de uso exclusivo EEPROM (Banco 2):
 *   Sector 22 -> 0x081C0000 (128 KB) -> Sector activo principal
 *   Sector 23 -> 0x081E0000 (128 KB) -> Sector alternativo (Ping-Pong)
 */
#ifndef AMS_FLASH_DRIVER_H_
#define AMS_FLASH_DRIVER_H_

#include "AMS_DataTypes.h"
#include <stdbool.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * CONFIGURACIÓN: Sectores de la Flash reservados para EEPROM
 * ----------------------------------------------------------------------- */
#define FLASH_EEPROM_SECTOR_A_NUM   22u
#define FLASH_EEPROM_SECTOR_B_NUM   23u
#define FLASH_EEPROM_SECTOR_A_ADDR  0x081C0000u
#define FLASH_EEPROM_SECTOR_B_ADDR  0x081E0000u
#define FLASH_EEPROM_SECTOR_SIZE    (128u * 1024u)  // 128 KB en bytes
#define FLASH_EEPROM_RECORD_SIZE    sizeof(AMS_Flash_Record_t)

/* -----------------------------------------------------------------------
 * API Pública del Driver
 * ----------------------------------------------------------------------- */

/**
 * @brief  Lee un registro de flash desde una dirección absoluta.
 * @param  address  Dirección absoluta de inicio del registro (debe estar alineada a 4 bytes).
 * @param  p_out    Puntero a la estructura destino.
 * @retval true  si la dirección es válida y la lectura es correcta.
 * @retval false si parámetros inválidos.
 */
bool b_Flash_ReadRecord(uint32_t address, AMS_Flash_Record_t *p_out);

/**
 * @brief  Escribe un registro en flash en una dirección absoluta.
 *         La flash en esa dirección DEBE estar borrada (=0xFFFFFFFF) antes de llamar.
 *         HAL_FLASH_Unlock() se gestiona internamente.
 * @param  address  Dirección absoluta destino (alineada a 4 bytes, sin borrar).
 * @param  p_record Puntero al registro a escribir.
 * @retval true  si la escritura fue exitosa.
 * @retval false si hubo error HAL.
 */
bool b_Flash_WriteRecord(uint32_t address, const AMS_Flash_Record_t *p_record);

/**
 * @brief  Borra un sector completo de la Flash (128 KB).
 * @param  sector_number  Número de sector (22 ó 23).
 * @retval true  si el borrado fue exitoso.
 * @retval false si hubo error HAL.
 */
bool b_Flash_EraseSector(uint8_t sector_number);

/**
 * @brief  Comprueba si una palabra de 32 bits en flash está borrada (=0xFFFFFFFF).
 * @param  address  Dirección absoluta del uint32 a inspeccionar.
 * @retval true  si la dirección contiene 0xFFFFFFFF (borrada).
 */
bool b_Flash_IsAddressErased(uint32_t address);

#endif /* AMS_FLASH_DRIVER_H_ */
