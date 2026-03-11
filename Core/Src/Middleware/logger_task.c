/**
 * @file    logger_task.c
 * @brief   Middleware (Pseudo-Task) para gestionar el guardado de datos.
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#include "Middleware/logger_task.h"
#include "Middleware/DataBroker.h"
#include "Drivers_Custom/AMS_SD_driver.h"
#include "main.h" // Para HAL_GetTick()
#include <stdio.h>
#include <string.h>

/* --- VARIABLES PRIVADAS --- */
static char s_current_log_file[32] = {0};
static bool b_logger_ready = false;

/* --- IMPLEMENTACIÓN --- */

void vd_Logger_Init(void) {
    // 1. Intentamos montar el sistema de archivos
    if (b_SD_Card_Mount()) {
        
        // 2. Buscamos el siguiente nombre libre (ej. gekko_000.csv)
        if (b_SD_Card_FindNextFilename("gekko", "csv", s_current_log_file, sizeof(s_current_log_file))) {
            
            // 3. Abrimos el archivo una única vez y lo mantenemos abierto
            if (b_SD_Card_OpenLogFile(s_current_log_file)) {
                // 4. Escribimos la cabecera (Header) del CSV y sincronizamos
                const char* s_csv_header = "TICK_MS,BAT_12V_MV,SUSP1_DMM,SUSP2_DMM\n";
                if (b_SD_Card_WriteSync(s_csv_header)) {
                    b_logger_ready = true;
                    printf("\r\n[LOGGER] Sistema SD listo. Guardando y sincronizando en: %s\r\n", s_current_log_file);
                } else {
                    printf("\r\n[LOGGER] Error escribiendo cabecera en %s\r\n", s_current_log_file);
                }
            } else {
                printf("\r\n[LOGGER] Error al abrir de forma continua el archivo %s\r\n", s_current_log_file);
            }

        } else {
            printf("\r\n[LOGGER] Error: No se encontraron nombres de archivo disponibles.\r\n");
        }
    } else {
         printf("\r\n[LOGGER] Error FATAL: No se pudo montar la SD.\r\n");
    }
}

void vd_Logger_Process(void) {
    if (!b_logger_ready) return;

    // 1. Obtenemos una copia segura desde el DataBroker
    Vehicle_Data_t local_veh;
    if (b_Broker_Get_VehicleState(&local_veh)) {
        
        // 2. Elegimos QUÉ información nos interesa loguear y la formateamos a texto
        char s_buffer[128];
        snprintf(s_buffer, sizeof(s_buffer), "%lu,%lu,%lu,%lu\n",
                 HAL_GetTick(),
                 local_veh.bateria_12v_mV,
                 local_veh.recorrido_susp_1_dmm,
                 local_veh.recorrido_susp_2_dmm);
                 
        // 3. Enviamos la cadena formateada al driver SD usando Sync en lugar de Abrir/Cerrar
        if(!b_SD_Card_WriteSync(s_buffer)) {
             printf("[LOGGER] Warning: Escritura y Sincronizacion fallida en %s\r\n", s_current_log_file);
        }
    }
}
