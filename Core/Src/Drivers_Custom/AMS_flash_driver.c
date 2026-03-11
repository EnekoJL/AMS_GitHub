/**
 * @file    AMS_flash_driver.c
 * @brief   Implementación del driver HAL para la Flash interna del STM32F469.
 * @author  Eneko Juanena
 * @date    11 de Marzo de 2026
 */

#include "Drivers_Custom/AMS_flash_driver.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * Implementación de funciones
 * ----------------------------------------------------------------------- */

bool b_Flash_ReadRecord(uint32_t address, AMS_Flash_Record_t *p_out) {
    if (p_out == NULL) {
        return false;
    }
    // La flash interna es memory-mapped: se puede leer con memcpy directamente
    memcpy(p_out, (const void *)address, sizeof(AMS_Flash_Record_t));
    return true;
}

bool b_Flash_WriteRecord(uint32_t address, const AMS_Flash_Record_t *p_record) {
    if (p_record == NULL) {
        return false;
    }

    HAL_StatusTypeDef status;

    // 1. Desbloquear la Flash para escritura
    if (HAL_FLASH_Unlock() != HAL_OK) {
        return false;
    }

    // 2. Escribir el registro palabra a palabra (32 bits = TYPEPROGRAM_WORD)
    //    STM32F469 requiere escritura en múltiplos de 4 bytes (word).
    const uint32_t *p_src  = (const uint32_t *)p_record;
    uint32_t        n_words = sizeof(AMS_Flash_Record_t) / sizeof(uint32_t);

    for (uint32_t i = 0; i < n_words; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   address + (i * 4u),
                                   (uint64_t)p_src[i]);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }

    // 3. Bloquear de nuevo la Flash tras la escritura
    HAL_FLASH_Lock();
    return true;
}

bool b_Flash_EraseSector(uint8_t sector_number) {
    if (sector_number != FLASH_EEPROM_SECTOR_A_NUM &&
        sector_number != FLASH_EEPROM_SECTOR_B_NUM) {
        // Protección: solo permitimos borrar los sectores designados para EEPROM
        return false;
    }

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return false;
    }

    FLASH_EraseInitTypeDef erase_cfg = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .Sector       = sector_number,
        .NbSectors    = 1u,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3  // 2.7V-3.6V: escritura de 32 bits
    };

    uint32_t sector_error = 0u;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase_cfg, &sector_error);

    HAL_FLASH_Lock();
    return (status == HAL_OK);
}

bool b_Flash_IsAddressErased(uint32_t address) {
    // Un byte/word borrado en flash NOR vale 0xFF / 0xFFFFFFFF
    return (*(volatile uint32_t *)address == 0xFFFFFFFFu);
}
