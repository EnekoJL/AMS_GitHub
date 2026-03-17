/*
 * AMS_SD_driver.c
 * Modulo para la gestion de la tarjeta MicroSD
 */

#include "Drivers_Custom/AMS_SD_driver.h"
#include <stdio.h>
#include <string.h>

/* Private variables ---------------------------------------------------------*/
static FATFS s_fs;      // Objeto privado del sistema de archivos
static FIL s_fil;       // Objeto de archivo persistente para el Logger
static bool b_file_is_open = false;

/* Functions -----------------------------------------------------------------*/

bool b_SD_Card_Mount(void)
{
    FRESULT fres = f_mount(&s_fs, "", 1); // 1 = forzar montaje inmediato
    if (fres != FR_OK) {
        printf("\r\n[SD DRIVER] ERROR: No se pudo montar la tarjeta SD (%d)\r\n", fres);
        return false;
    }
    return true;
}

bool b_SD_Card_OpenLogFile(const char* s_filename)
{
    if (s_filename == NULL) return false;

    if (b_file_is_open) {
        f_close(&s_fil);
        b_file_is_open = false;
    }

    FRESULT fres = f_open(&s_fil, s_filename, FA_WRITE | FA_OPEN_APPEND);
    if (fres == FR_OK) {
        b_file_is_open = true;
        return true;
    } else {
        printf("\r\n[SD DRIVER] ERROR: Fallo al abrir/crear %s (%d)\r\n", s_filename, fres);
        if (fres == FR_NOT_READY || fres == FR_DISK_ERR) {
            f_mount(&s_fs, "", 1);
        }
        return false;
    }
}

bool b_SD_Card_WriteSync(const char* s_data)
{
    if (s_data == NULL || !b_file_is_open) return false;

    UINT bytesWrote;
    FRESULT fres = f_write(&s_fil, s_data, strlen(s_data), &bytesWrote);
    
    if (fres == FR_OK) {
        fres = f_sync(&s_fil);
        if (fres == FR_OK) {
            return true;
        } else {
             printf("\r\n[SD DRIVER] ERROR: Fallo al sincronizar (f_sync) código (%d)\r\n", fres);
             return false;
        }
    } else {
        printf("\r\n[SD DRIVER] ERROR: Fallo escritura (%d)\r\n", fres);
        return false;
    }
}

void vd_SD_Card_CloseLogFile(void)
{
    if (b_file_is_open) {
        f_close(&s_fil);
        b_file_is_open = false;
    }
}

bool b_SD_Card_FindNextFilename(const char* s_prefix, const char* s_extension, char* s_out_filename, uint8_t ui8_max_len)
{
    if (s_prefix == NULL || s_extension == NULL || s_out_filename == NULL) return false;

    FILINFO fno;
    for (uint16_t i = 0; i < 1000; i++) {
        snprintf(s_out_filename, ui8_max_len, "%s_%03u.%s", s_prefix, i, s_extension);
        
        FRESULT fres = f_stat(s_out_filename, &fno);
        if (fres == FR_NO_FILE) {
            return true;
        }
    }
    return false;
}
