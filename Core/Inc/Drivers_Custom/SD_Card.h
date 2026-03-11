/*
 * SD_Card.h
 * Modulo para la gestion de la tarjeta MicroSD
 */

#ifndef SD_CARD_H_
#define SD_CARD_H_

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ff.h"      // Librería FATFS
#include <stdbool.h>

/* Function prototypes -------------------------------------------------------*/
/**
 * @brief  Monta el sistema de archivos FATFS en la MicroSD.
 * @retval bool: true si el montaje es exitoso, false en caso contrario.
 */
bool b_SD_Card_Mount(void);

/**
 * @brief  Abre (o crea) un archivo para escritura continua.
 * @param  s_filename: Nombre del archivo de destino.
 * @retval bool: true si se abre con éxito.
 */
bool b_SD_Card_OpenLogFile(const char* s_filename);

/**
 * @brief  Escribe una cadena de texto en el archivo previamente abierto y
 *         fuerza el guardado físico con f_sync.
 * @param  s_data: Cadena de texto a escribir.
 * @retval bool: true si la escritura y el sync son exitosos.
 */
bool b_SD_Card_WriteSync(const char* s_data);

/**
 * @brief  Cierra explícitamente el archivo abierto.
 */
void vd_SD_Card_CloseLogFile(void);

/**
 * @brief  Itera buscando el siguiente número de archivo disponible (000 a 999).
 *         Ej: Prefix "log_", Ext "csv" -> busca log_000.csv, log_001.csv...
 * @param  s_prefix: Prefijo base del nombre del archivo.
 * @param  s_extension: Extensión sin punto.
 * @param  s_out_filename: Buffer donde se devolverá el nombre resultante.
 * @param  ui8_max_len: Longitud máxima del buffer de salida.
 * @retval bool: true si encuentra un hueco, false si llega al límite (999).
 */
bool b_SD_Card_FindNextFilename(const char* s_prefix, const char* s_extension, char* s_out_filename, uint8_t ui8_max_len);

#endif /* SD_CARD_H_ */
