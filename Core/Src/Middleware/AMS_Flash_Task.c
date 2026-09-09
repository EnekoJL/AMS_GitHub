/**
 * @file    AMS_Flash_Task.c
 * @brief   Flash persistence middleware task.
 *
 *  CIRCULAR PING-PONG BUFFER STRATEGY:
 *  ──────────────────────────────────────────────────
 *  Two 128 KB sectors (S22 and S23) at the end of Bank 2 are used.
 *  Records (12 bytes each) are appended sequentially — no erase on write.
 *  The alternate sector is only erased when the active one is full, then
 *  writing continues there. Wear is split evenly (>2M total cycles).
 *
 *  MEMORY MAP:
 *    S22: 0x081C0000 → 0x081DFFFF  (128 KB = 10,922 records of 12 bytes)
 *    S23: 0x081E0000 → 0x081FFFFF  (128 KB = 10,922 records of 12 bytes)
 *
 *  RECORD FORMAT (12 bytes):
 *    [magic: 4B][soc_x10: 2B][cycle: 2B][crc: 4B]
 *
 * @author  Eneko Juanena
 * @date    11 de Marzo de 2026
 */

#include "Middleware/AMS_Flash_Task.h"
#include "Middleware/AMS_DataBroker.h"
#include "Drivers_Custom/AMS_flash_driver.h"
#include "AMS_task_config.h"   /* FEATURE_FLASH_WRITE_ENABLE */
#include "cmsis_os.h"          /* osDelay */
#include <stdio.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Constantes y macros locales
 * ----------------------------------------------------------------------- */
#define RECORDS_PER_SECTOR  (FLASH_EEPROM_SECTOR_SIZE / FLASH_EEPROM_RECORD_SIZE)

/* CRC32 casero (sin tabla, para no depender de HW CRC) */
static uint32_t prv_calc_crc32(const uint8_t *data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (uint8_t bit = 0; bit < 8u; bit++) {
            if (crc & 1u) { crc = (crc >> 1u) ^ 0xEDB88320u; }
            else           { crc >>= 1u; }
        }
    }
    return ~crc;
}

static bool prv_is_record_valid(const AMS_Flash_Record_t *p_rec) {
    if (p_rec->magic != AMS_FLASH_RECORD_MAGIC) return false;
    uint32_t expected_crc = prv_calc_crc32(
        (const uint8_t *)&p_rec->config,
        sizeof(AMS_Persistent_Config_t)
    );
    return (p_rec->crc == expected_crc);
}

/* -----------------------------------------------------------------------
 * Estado interno del Manager (privado, estático)
 * ----------------------------------------------------------------------- */
typedef struct {
    uint32_t active_sector_addr;   // Dirección base del sector activo actualmente
    uint8_t  active_sector_num;    // Número de sector activo (22 ó 23)
    uint32_t next_write_offset;    // Offset (en bytes) del próximo slot libre en el sector activo
    uint32_t last_slot_index;      // Índice global del último slot escrito (para debug)
    bool     is_initialized;
} PersistState_t;

static PersistState_t s_state = { 0 };

/* -----------------------------------------------------------------------
 * Función auxiliar: escanea un sector y devuelve el offset del siguiente slot libre
 * y un puntero al último registro válido encontrado (puede ser NULL).
 * ----------------------------------------------------------------------- */
static uint32_t prv_scan_sector(uint32_t base_addr, AMS_Flash_Record_t *p_last_valid_out) {
    AMS_Flash_Record_t rec;
    uint32_t offset = 0u;
    bool found_valid = false;

    while (offset + FLASH_EEPROM_RECORD_SIZE <= FLASH_EEPROM_SECTOR_SIZE) {
        uint32_t current_addr = base_addr + offset;

        // Si la primera palabra del slot está borrada, hemos llegado al final de los datos
        if (b_Flash_IsAddressErased(current_addr)) {
            break;
        }

        b_Flash_ReadRecord(current_addr, &rec);

        if (prv_is_record_valid(&rec)) {
            if (p_last_valid_out != NULL) {
                *p_last_valid_out = rec;
            }
            found_valid = true;
        }

        offset += FLASH_EEPROM_RECORD_SIZE;
    }

    (void)found_valid; // Silencia warning si no se usa directamente
    return offset;  // Este es el offset del siguiente slot libre
}

/* -----------------------------------------------------------------------
 * Implementación pública
 * ----------------------------------------------------------------------- */

static void vd_Persist_Init(void) {
    AMS_Flash_Record_t best_record = { 0 };
    bool found_any = false;

    // Escanear los dos sectores para encontrar el más reciente
    AMS_Flash_Record_t rec_a = { 0 }, rec_b = { 0 };
    bool valid_a = false, valid_b = false;

    uint32_t offset_a = prv_scan_sector(FLASH_EEPROM_SECTOR_A_ADDR, &rec_a);
    uint32_t offset_b = prv_scan_sector(FLASH_EEPROM_SECTOR_B_ADDR, &rec_b);

    if (rec_a.magic == AMS_FLASH_RECORD_MAGIC && prv_is_record_valid(&rec_a)) valid_a = true;
    if (rec_b.magic == AMS_FLASH_RECORD_MAGIC && prv_is_record_valid(&rec_b)) valid_b = true;

    // Elegir el sector activo: el que tiene datos más recientes (mayor cycle_count)
    // Si los dos tienen datos, el que tiene mayor cycle_count gana.
    // Si sólo uno tiene datos, ese es el activo.
    if (valid_a && valid_b) {
        if (rec_b.config.cycle_count > rec_a.config.cycle_count) {
            s_state.active_sector_addr = FLASH_EEPROM_SECTOR_B_ADDR;
            s_state.active_sector_num  = FLASH_EEPROM_SECTOR_B_NUM;
            s_state.next_write_offset  = offset_b;
            best_record = rec_b;
        } else {
            s_state.active_sector_addr = FLASH_EEPROM_SECTOR_A_ADDR;
            s_state.active_sector_num  = FLASH_EEPROM_SECTOR_A_NUM;
            s_state.next_write_offset  = offset_a;
            best_record = rec_a;
        }
        found_any = true;
    } else if (valid_a) {
        s_state.active_sector_addr = FLASH_EEPROM_SECTOR_A_ADDR;
        s_state.active_sector_num  = FLASH_EEPROM_SECTOR_A_NUM;
        s_state.next_write_offset  = offset_a;
        best_record = rec_a;
        found_any = true;
    } else if (valid_b) {
        s_state.active_sector_addr = FLASH_EEPROM_SECTOR_B_ADDR;
        s_state.active_sector_num  = FLASH_EEPROM_SECTOR_B_NUM;
        s_state.next_write_offset  = offset_b;
        best_record = rec_b;
        found_any = true;
    } else {
        // Flash virgen o corrupta: usamos sector A como activo por defecto
        s_state.active_sector_addr = FLASH_EEPROM_SECTOR_A_ADDR;
        s_state.active_sector_num  = FLASH_EEPROM_SECTOR_A_NUM;
        s_state.next_write_offset  = 0u;
    }

    s_state.is_initialized = true;

    // Cargar datos al DataBroker
    if (found_any) {
        AMS_Persistent_Config_t temp_cfg = best_record.config;
        b_Broker_Update_PersistentConfig(&temp_cfg);
    } else {
        // Valores por defecto si la flash está vacía
        AMS_Persistent_Config_t defaults = {
            .soc_percent_x10 = 1000u,  // 100.0% (asumimos cargado en el primer boot)
            .cycle_count     = 0u
        };
        b_Broker_Update_PersistentConfig(&defaults);
    }
}

bool b_Persist_SaveConfig(void) {
    if (!s_state.is_initialized) return false;

#if (FEATURE_FLASH_WRITE_ENABLE == 0)
    /* Flash writes disabled: return without touching flash (testing mode). */
    return true;
#endif

    // 1. Obtener datos actuales del DataBroker
    AMS_Persistent_Config_t current_cfg = { 0 };
    b_Broker_Get_PersistentConfig(&current_cfg);
    current_cfg.cycle_count++;  // Incrementar el contador de ciclo de vida

    // 2. Comprobar si el sector está lleno: si es así, saltar al alternativo
    if (s_state.next_write_offset + FLASH_EEPROM_RECORD_SIZE > FLASH_EEPROM_SECTOR_SIZE) {
        // Seleccionar el sector alternativo
        uint8_t  alt_num  = (s_state.active_sector_num == FLASH_EEPROM_SECTOR_A_NUM)
                            ? FLASH_EEPROM_SECTOR_B_NUM
                            : FLASH_EEPROM_SECTOR_A_NUM;
        uint32_t alt_addr = (alt_num == FLASH_EEPROM_SECTOR_A_NUM)
                            ? FLASH_EEPROM_SECTOR_A_ADDR
                            : FLASH_EEPROM_SECTOR_B_ADDR;

        // Borrar el sector alternativo antes de usarlo
        if (!b_Flash_EraseSector(alt_num)) return false;

        // Saltar al sector alternativo
        s_state.active_sector_addr = alt_addr;
        s_state.active_sector_num  = alt_num;
        s_state.next_write_offset  = 0u;
    }

    // 3. Ensamblar el registro con magic y CRC
    AMS_Flash_Record_t new_record;
    new_record.magic  = AMS_FLASH_RECORD_MAGIC;
    new_record.config = current_cfg;
    new_record.crc    = prv_calc_crc32(
        (const uint8_t *)&new_record.config,
        sizeof(AMS_Persistent_Config_t)
    );

    // 4. Escribir en el slot libre actual
    uint32_t write_addr = s_state.active_sector_addr + s_state.next_write_offset;
    if (!b_Flash_WriteRecord(write_addr, &new_record)) return false;

    // 5. Avanzar el puntero y actualizar datos en el Broker con el cycle_count correcto
    s_state.next_write_offset += FLASH_EEPROM_RECORD_SIZE;
    s_state.last_slot_index++;
    b_Broker_Update_PersistentConfig(&current_cfg);

    return true;
}

uint32_t u32_Persist_GetLastSlotIndex(void) {
    return s_state.last_slot_index;
}

/* -----------------------------------------------------------------------
 * Task entry points (called from Flash_Memory_Start in main.c)
 * ----------------------------------------------------------------------- */

/**
 * @brief Initializes the Flash persistence subsystem.
 *        Scans both Flash sectors, finds the most recent valid record,
 *        loads it into the DataBroker (or sets defaults on blank Flash),
 *        and prints a boot diagnostic over UART.
 */
void vd_Flash_Task_Init(void) {
    vd_Persist_Init();

    /* Boot diagnostic: show the SOC loaded from Flash */
    AMS_Persistent_Config_t init_cfg;
    b_Broker_Get_PersistentConfig(&init_cfg);
    printf("\r\n[BOOT] SOC loaded from Flash: %u.%u %% (cycles: %u)\r\n",
           init_cfg.soc_percent_x10 / 10,
           init_cfg.soc_percent_x10 % 10,
           init_cfg.cycle_count);
}

/**
 * @brief Infinite RTOS loop for the Flash persistence task.
 *        Placeholder for future scheduled save logic (e.g. periodic SOC save).
 */
void vd_Flash_Manager_TaskProcess(void) {
    for (;;) {
        /* TODO: add periodic b_Persist_SaveConfig() call here when needed */
        osDelay(1000);
    }
}
