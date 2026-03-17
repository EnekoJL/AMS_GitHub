# Araba Motorsport (AMS) - STM32 Firmware

Este repositorio contiene el firmware principal del vehículo, basado en un microcontrolador STM32F469I-Discovery.
A continuación se detalla la configuración de Hardware, periféricos y pines utilizados en la aplicación.

Para acceder a los Datasheets y esquemas completos, consulta el [Drive de Hardware](./docs/hardware_links.md).

---

## 1. Periféricos y Configuración de Pines

### ADC1 (Batería 12V)
Se encarga de leer la tensión de entrada del sistema LV (por defecto, la batería de 12V del prototipo).

*   **Pines definidos:** `PB1`
*   **Características:**
    *   Uso de **DMA** con **Timer 2**.
    *   DMA en modo Circular con un buffer de tamaño `[10]`.
    *   Timer dispara cada 25 ms.
    *   Callback `ConversionCompleted` al llenar el buffer cada 250 ms.
    *   Se hace un promedio de las 10 lecturas para mitigar el ruido.
    *   Se actualiza el dato de 12V en la estructura `Vehicle_Data_t` cada 250 ms.
*   **Información crítica:** (Reservado para notas de validación en pista)


### ADC2 (Sensores de Suspensión)
Se encarga de leer la señal tanto del sensor de horquilla delantero (150 mm) como del sensor de horquilla trasero (50 mm).

*   **Pines definidos:**
    *   `PC2` (Sensor delantero)
    *   `PC3` (Sensor trasero)
*   **Características:**
    *   Uso de **DMA** con **Timer 3**.
    *   DMA en modo Circular con un buffer de tamaño `[20]`.
    *   Timer dispara cada 200 µs. Se miden en cascada: 1 disparo → 2 lecturas.
    *   Callback `ConversionCompleted` al llenar el buffer cada 20 ms.
    *   Se hace un promedio de las 10 lecturas por canal para quitar ruidos.
    *   Se actualiza el dato de potenciómetros 1 y 2 en estructura cada 20 ms.
*   **Información crítica:** (Reservado para curvas de calibración)


### Timers (Disparos DMA)
Timers dedicados a disparar las lecturas de ADC. Actualmente **solo se usan para DMA**.

*   **Timers en uso:** `TIM2`, `TIM3`
*   **Clock Base:** Velocidad base de 45 MHz del bus APB1 (núcleo a 180 MHz > HCLK ÷ 4). STM32 multiplica x2 el reloj de los Timers si el divisor es distinto de 1, por lo que el reloj de entrada de `TIM2` y `TIM3` es de **90 MHz**.

#### Timer 2 (Asignado a ADC1)
*   **Prescaler (PSC):** `90 - 1` → Divide los 90 MHz entre 90. El timer cuenta a **1 MHz** (1 tick = 1 µs).
*   **Period / Autoreload (ARR):** `25000 - 1` → El Timer cuenta hasta 25000 y se reinicia, disparando el evento de lectura del ADC1 (`TRGO`).
*   **Frecuencia de Lectura:** `1 MHz / 25000 = 40 Hz` (Se lee el ADC cada 25 ms).

#### Timer 3 (Asignado a ADC2)
*   **Prescaler (PSC):** `90 - 1` → El timer cuenta a **1 MHz**.
*   **Period / Autoreload (ARR):** `200 - 1` → El Timer cuenta hasta 200 y dispara el ADC2 en cascada.
*   **Frecuencia de Lectura:** `1 MHz / 200 = 5.000 Hz` (Secuencia de lectura de los 2 canales de suspensión cada 200 µs).
*   Para las suspensiones, la DMA tiene que llenarse con 20 posiciones (`NUM_MUESTRAS * 2`). Como en cada "scan" se miden 2 canales, se necesitan 10 escaneos para llenar el buffer. El tiempo de refresco en la base de datos es: `10 escaneos × 200 µs = 2000 µs = 2 milisegundos (500 Hz)`.


### CAN2 (Comunicaciones Inversor)
El CAN2 está configurado para leer las variables del inversor y para enviar comandos bajo demanda. Dado que en los STM32F4 el CAN1 y CAN2 comparten recursos de hardware (filtros), el banco de filtros para CAN2 se asigna a partir del banco 14.

*   **Pines definidos:**
    *   `PB12` (CAN2_RX)
    *   `PB13` (CAN2_TX)
*   **Características:**
    *   **Baudrate:** 500 kbps.
    *   Uso de **Interrupción RX** (`CAN2_RX0_IRQn`) para recepción asíncrona mediante el callback `HAL_CAN_RxFifo0MsgPendingCallback`.
    *   Configurado Filtro en Banco 14 en modo Máscara (apuntando al `StdId = 0x181` para recibir telemetría del inversor, rpms de momento).
*   **Integración en Código:**
    *   **RX:** Al recibirse un mensaje por hardware FIFO0, salta la interrupción, se extraen los 2 bytes de RPMs, se procesan y se dispara un toggle visual (Led verde).
    *   **TX:** Transmisión mediante *Polling* (sin interrupción). Al presionar el Botón Azul (`PA0`), se encola el mensaje en un Mailbox (`HAL_CAN_AddTxMessage`) con `StdId = 0x201` y se manda una petición al Inversor, disparando un toggle visual del Led Azul.
*   **Información crítica:** → Es vital llamar a `HAL_CAN_Start()` y `HAL_CAN_ActivateNotification()` (para `CAN_IT_RX_FIFO0_MSG_PENDING`) después del Init para que las interrupciones entren correctamente.


### Tarjeta SD / SDIO (Data Logging)
El almacenamiento de datos en memoria no volátil se resuelve mediante el periférico SD/SDIO interactuando con una tarjeta MicroSD (Formato FAT32 o FAT).

*   **Pines definidos:** Definidos por defecto en el slot SD_Card de la Discovery.
    *    `PG2` como GPIO Input para detección de tarjeta (`microsd_detect`).
*   **Características:**
    *   Velocidad base de 45 MHz, ClockDivider a 4 → **7,5 MHz**.
    *   Interfaz SDIO a 1-bit con reloj de bus a 7,5 MHz *(Nota: Mirar si probar a 4-bit para mejorar ancho de banda, a 22,5 MHz fallaba).*
    *   Middleware **FATFS (FAT32)**, proporciona abstracciones estándar ANSI C.
    *   Tarea de logging actualmente a baja frecuencia (2 Hz para tests).
*   **Integridad de Datos:** Arquitectura de streaming de archivos abiertos de larga duración con llamadas periódicas explícitas de sincronización de caché (File Syncing). Esto garantiza la integridad atómica de los logs ante paradas de emergencia o desconexiones súbitas del sistema de Alta Tensión (HV).


### Puerto SERIE (Debug)
Puerto serial por el puerto Micro-USB (ST-LINK) para hacer debuging (impresión de telemetría/mensajes).

*   **Pines definidos:** `USART3` (Pines por defecto de la placa).
*   **Características:**
    *   Baud-rate: `115200 bit/s`.
    *   Modo: Asíncrono.
*   **Información crítica:** → **No tocar nada más de la configuración USB/UART.**
*   *Snippet obligatorio en main.c para* `printf`:
    ```c
    /* USER CODE BEGIN 4 */ 
    // Este código redirige los printfs por el puerto usart3.
    int __io_putchar(char ch) {
      HAL_UART_Transmit(&huart3, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
      return ch;
    }
    /* USER CODE END 4 */
    ```


### LEDs (Indicación Visual)
LEDs integrados en la placa base para indicación de estado, errores y feedback de la aplicación gobernados por el `led_manager_task.c`.

*   **Pines definidos:**
    *   `LED 1 (Verde)`: Pin `PG6` (`LED1_GPIO_Port`, `LED1_Pin`)
    *   `LED 2 (Naranja)`: Pin `PD4` (`LED2_GPIO_Port`, `LED2_Pin`)
    *   `LED 3 (Rojo)`: Pin `PD5` (`LED3_GPIO_Port`, `LED3_Pin`)
    *   `LED 4 (Azul)`: Pin `PK3` (`LED4_GPIO_Port`, `LED4_Pin`)
*   **Características:**
    *   Modo: `Output Push-Pull`.
    *   Lógica: Positiva (Activo en nivel ALTO).
*   **Información crítica:** → Asegurarse de que en CubeMX el modo sea **estrictamente Output Push Pull** y nunca Open Drain.

---
*Araba Motorsport - Área de Electrónica y Control.*
