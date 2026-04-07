/**
 * @file    AMS_CAN_Task.c
 * @brief   RTOS task for CAN2 RX/TX management.
 *
 * Init sequence (called once from CAN_Start in main.c):
 *   vd_CAN_Task_Init()  →  creates RX queue, binds driver, configures filter,
 *                           starts CAN peripheral with RX FIFO0 interrupt.
 *
 * Data flow (RX):
 *   ISR (HAL_CAN_RxFifo0MsgPendingCallback)
 *     → vd_CAN_RxQueue_PostFromISR()  [ISR-safe]
 *     → osMessageQueue
 *     → vd_CAN_Manager_TaskProcess()  [RTOS thread: parse & update Broker]
 */

#include "Middleware/AMS_CAN_Task.h"
#include "Middleware/AMS_DataBroker.h"
#include "Middleware/AMS_Led_Task.h"
#include "Drivers_Custom/AMS_can_driver.h"
#include "cmsis_os.h"
#include <stdio.h>

/* --------------------------------------------------------------------------
 * TIPOS PRIVADOS
 * -------------------------------------------------------------------------- */

/**
 * @brief Paquete RAW que viaja por la Queue.
 *        Contiene el StdId del mensaje y los 8 bytes de datos en bruto.
 *        Es lo mínimo necesario para identificar y parsear el mensaje.
 */
typedef struct {
    uint32_t std_id;       /* ID estándar del mensaje CAN (ej. 0x181) */
    uint8_t  data[8];      /* Payload crudo del mensaje                */
} CAN_RxPacket_t;

/* --------------------------------------------------------------------------
 * VARIABLES PRIVADAS
 * -------------------------------------------------------------------------- */

/* Queue de FreeRTOS con capacidad para 8 paquetes sin riesgo de pérdida */
static osMessageQueueId_t s_can_rx_queue = NULL;

/* --------------------------------------------------------------------------
 * IMPLEMENTACIÓN PÚBLICA
 * -------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
 * PRIVATE FUNCTIONS
 * -------------------------------------------------------------------------- */

/**
 * @brief Creates the internal FreeRTOS RX queue (capacity: 8 raw packets).
 *        Called internally by vd_CAN_Task_Init — not part of the public API.
 */
static void prv_can_rx_queue_create(void) {
    s_can_rx_queue = osMessageQueueNew(8u, sizeof(CAN_RxPacket_t), NULL);
}


/* --------------------------------------------------------------------------
 * PUBLIC IMPLEMENTATION
 * -------------------------------------------------------------------------- */

void vd_CAN_Task_Init(CAN_HandleTypeDef *phcan) {
    /* 1. Create the RX queue before enabling the interrupt */
    prv_can_rx_queue_create();

    /* 2. Bind the HAL handle to the driver */
    vd_AMS_CAN_Init(phcan);

    /* 3. Configure the Inverter Status filter (ID 0x181, exact match, bank 14) */
    b_AMS_CAN_ConfigureFilter(0x181, 0x181, 14);

    /* 4. Start the peripheral and enable the RX FIFO0 interrupt */
    if (b_AMS_CAN_Start() != HAL_OK) {
        Error_Handler();
    }
}

void vd_CAN_RxQueue_PostFromISR(CAN_RxHeaderTypeDef *rx_header, uint8_t rx_data[8]) {
    if (s_can_rx_queue == NULL) return;

    CAN_RxPacket_t pkt;
    pkt.std_id = rx_header->StdId;
    for (int i = 0; i < 8; i++) {
        pkt.data[i] = rx_data[i];
    }

    /* osMessageQueuePut con timeout=0 es seguro desde ISR */
    osMessageQueuePut(s_can_rx_queue, &pkt, 0, 0);
}

/* --------------------------------------------------------------------------
 * PARSERS PRIVADOS
 * -------------------------------------------------------------------------- */

/**
 * @brief Parsea un paquete del Inversor (ID 0x181) y escribe las RPM en el Broker.
 */
static void parse_inverter_status(const CAN_RxPacket_t *pkt) {
    /* El Inversor manda RPM en Bytes [0] y [1] (Little Endian con signo) */
    int16_t rpm = (int16_t)((pkt->data[1] << 8) | pkt->data[0]);

    /* Extraer el VehicleState actual del Broker, actualizar RPM, subir */
    Vehicle_Data_t veh;
    b_Broker_Get_VehicleState(&veh);
    veh.inverter_rpm = rpm;
    b_Broker_Update_VehicleState(&veh);

    printf("[CAN RX] 0x%03lX → RPM: %d\r\n", pkt->std_id, rpm);
}

/* --------------------------------------------------------------------------
 * TAREA RTOS PRINCIPAL
 * -------------------------------------------------------------------------- */

void vd_CAN_Manager_TaskProcess(void) {
    CAN_RxPacket_t pkt;

    for(;;) {
        /* --- BLOQUE 1: Procesar todos los paquetes RX que hayan llegado --- */
        /* osMessageQueueGet con timeout=0 → no bloquea, solo coge si hay algo */
        while (osMessageQueueGet(s_can_rx_queue, &pkt, NULL, 0) == osOK) {
            switch (pkt.std_id) {
                case 0x181:
                    parse_inverter_status(&pkt);
                    break;
                /* Aquí se añadirán futuros IDs del Inversor (temperaturas, errores...) */
                default:
                    printf("[CAN RX] ID desconocido: 0x%03lX\r\n", pkt.std_id);
                    break;
            }
        }

        /* --- BLOQUE 2: TX bajo demanda (Boton Azul) --- */
        if (HAL_GPIO_ReadPin(Boton_Azul_GPIO_Port, Boton_Azul_Pin) == GPIO_PIN_SET) {
            if (u32_AMS_CAN_GetTxFreeLevel() > 0) {
                uint8_t tx_data[8] = {0x0C, 0, 0, 0, 0, 0, 0, 0};
                if (b_AMS_CAN_Transmit(0x201, tx_data, 8) == HAL_OK) {
                    vd_LED_Manager_SetMode(LED_COLOR_BLUE, LED_PIN_BLINK);
                    printf("[CAN TX] StdId=0x201 sent via Boton_Azul\r\n");
                }
            }
            /* Anti-rebote no bloqueante */
            while (HAL_GPIO_ReadPin(Boton_Azul_GPIO_Port, Boton_Azul_Pin) == GPIO_PIN_SET) {
                osDelay(20);
            }
        }

        /* Ceder CPU: escaneo a 50 Hz */
        osDelay(20);
    }
}
