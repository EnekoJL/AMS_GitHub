/**
 * @file    AMS_Logger_Task.c
 * @brief   Middleware RTOS Task para gestionar el guardado de datos en la SD.
 */

#include "Middleware/AMS_Logger_Task.h"
#include "Middleware/AMS_DataBroker.h"
#include "Middleware/AMS_Led_Task.h"
#include "Drivers_Custom/AMS_SD_driver.h"
#include "cmsis_os.h" /* Para osDelay */
#include "main.h"     /* Para HAL_GetTick() */
#include <stdio.h>
#include <string.h>

/* --- VARIABLES PRIVADAS --- */
static char s_current_log_file[32] = {0};
static bool b_logger_ready = false;

/* --- IMPLEMENTACIÓN --- */

void vd_Logger_Init(void) {
    /* 1. Intentamos montar el sistema de archivos */
    if (b_SD_Card_Mount()) {
        /* 2. Buscamos el siguiente nombre libre (ej. gekko_000.csv) */
        if (b_SD_Card_FindNextFilename("gekko", "csv", s_current_log_file, sizeof(s_current_log_file))) {
            /* 3. Abrimos el archivo una única vez y lo mantenemos abierto */
            if (b_SD_Card_OpenLogFile(s_current_log_file)) {
                /* 4. Escribimos la cabecera (Header) del CSV y sincronizamos */
                const char* s_csv_header = "TICK_MS,BAT_12V_MV,SUSP1_DMM,SUSP2_DMM,RPM\n";
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
         printf("\r\n[LOGGER] Warning: No se pudo montar la SD (puede no estar insertada).\r\n");
    }
}

void vd_Logger_TaskProcess(void) {
    Vehicle_Data_t local_veh;
    char s_buffer[128];

    /* Bucle infinito de la tarea RTOS */
    for(;;) {
        if (b_logger_ready) {
            /* 1. Obtenemos una copia segura desde el DataBroker */
            if (b_Broker_Get_VehicleState(&local_veh)) {
                
                /* 2. Formateamos a CSV */
                snprintf(s_buffer, sizeof(s_buffer), "%lu,%lu,%lu,%lu,%d\n",
                         HAL_GetTick(),
                         local_veh.bateria_12v_mV,
                         local_veh.recorrido_susp_1_dmm,
                         local_veh.recorrido_susp_2_dmm,
                         local_veh.inverter_rpm);
                         
                /* 3. Guardamos y hacemos Sync de los datos en la tarjeta */
                if(b_SD_Card_WriteSync(s_buffer)) {
                     /* Green LED feedback on successful write to SD */
                     vd_LED_Manager_SetMode(LED_COLOR_GREEN, LED_PIN_BLINK);
                } else {
                     printf("[LOGGER] Warning: Fallo de escritura SD en %s\r\n", s_current_log_file);
                }
            }
        }
        
        /* 
         * Pausar tarea 500 ms (2 Hz). 
         * Al usar osDelay, cede el control a las tareas CAN y ADC mientras 
         * espera o si la escritura FATFS fuera larga.
         */
        osDelay(500);
    }
}
