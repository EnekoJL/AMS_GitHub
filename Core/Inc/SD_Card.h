/*
 * SD_Card.h
 * Modulo para la gestion de la tarjeta MicroSD
 */

#ifndef SD_CARD_H_
#define SD_CARD_H_

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ff.h"      // Librería FATFS
#include "AMS_DataTypes.h" // Necesario para Vehicle_Data_t

/* Function prototypes -------------------------------------------------------*/
void SD_Card_Test(void);
void SD_Card_Log_Gekko(Vehicle_Data_t *p_veh_data);

#endif /* SD_CARD_H_ */
