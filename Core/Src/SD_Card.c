/*
 * SD_Card.c
 * Modulo para la gestion de la tarjeta MicroSD
 */

#include "SD_Card.h"
#include <stdio.h>
#include <string.h>

/* Private variables ---------------------------------------------------------*/
FATFS fs;      // Objeto del sistema de archivos
FIL fil;       // Objeto del archivo
FRESULT fres;  // Variable para guardar los resultados de las operaciones
UINT bytesWrote;

/* Functions -----------------------------------------------------------------*/

/**
 * @brief  Realiza un test básico de montaje, escritura y cierre en la SD.
 * @retval None
 */
void SD_Card_Test(void)
{
    printf("\r\n--- Iniciando Test de Tarjeta SD ---\r\n");

    // 1. Montar el sistema de archivos (1 = forzar montaje inmediato)
    fres = f_mount(&fs, "", 1);
    if (fres != FR_OK) {
        printf("ERROR: No se pudo montar la tarjeta SD. Codigo de error: %d\r\n", fres);
        return;
    }
    printf("EXITO: Tarjeta SD montada correctamente.\r\n");

    // 2. Crear y abrir el archivo "hello.txt"
    fres = f_open(&fil, "hello.txt", FA_WRITE | FA_CREATE_ALWAYS);
    if (fres != FR_OK) {
        printf("ERROR: No se pudo crear/abrir el archivo. Codigo de error: %d\r\n", fres);
        return;
    }
    printf("EXITO: Archivo 'hello.txt' creado.\r\n");

    // 3. Escribir datos
    char myData[] = "¡Hola mundo desde tu STM32F469-DISCO!\nEste es el primer test del Datalogger.\n";
    fres = f_write(&fil, myData, strlen(myData), &bytesWrote);

    if (fres == FR_OK) {
        printf("EXITO: Se escribieron %u bytes correctamente.\r\n", bytesWrote);
    } else {
        printf("ERROR: Fallo al escribir en el archivo. Codigo de error: %d\r\n", fres);
    }

    // 4. Cerrar el archivo de forma segura
    fres = f_close(&fil);
    if (fres == FR_OK) {
        printf("EXITO: Archivo cerrado de forma segura.\r\n");
    }

    printf("--- Test Finalizado ---\r\n");
}

/**
 * @brief  Guarda los datos del vehículo en un archivo "datalog.txt" abriéndolo en modo Append.
 * @param  p_veh_data: Puntero a la estructura con los datos físicos convertidos.
 * @retval None
 */
void SD_Card_Log_Gekko(Vehicle_Data_t *p_veh_data)
{
    if (p_veh_data == NULL) return;

    // Abrir o crear el archivo y colocar el puntero al final (Append)
    fres = f_open(&fil, "datalog.txt", FA_WRITE | FA_OPEN_APPEND);
    if (fres == FR_OK) {
        char buffer[100];
        
        // Formatear el string sin usar float
        snprintf(buffer, sizeof(buffer), "BAT: %lu.%02luV, S1: %lu.%lumm, S2: %lu.%lumm\n",
                 p_veh_data->bateria_12v_mV / 1000, (p_veh_data->bateria_12v_mV % 1000) / 10,
                 p_veh_data->recorrido_susp_1_dmm / 10, p_veh_data->recorrido_susp_1_dmm % 10,
                 p_veh_data->recorrido_susp_2_dmm / 10, p_veh_data->recorrido_susp_2_dmm % 10);
        
        // Escribir en la tarjeta SD
        f_write(&fil, buffer, strlen(buffer), &bytesWrote);
        
        // Cerrar el archivo (esto hace el sync físico a la SD)
        f_close(&fil);

        // Opcional: imprimir por serie si hubo algun error de escritura (fres != FR_OK no está checkeado en el write para no saturar)
    } else {
        printf("ERROR SD: Fallo al abrir datalog.txt (%d)\r\n", fres);
        // Intentar remontar el sistema de archivos si hay error (por si se sacó la tarjeta)
        if (fres == FR_NOT_READY || fres == FR_DISK_ERR) {
            f_mount(&fs, "", 1);
        }
    }
}
