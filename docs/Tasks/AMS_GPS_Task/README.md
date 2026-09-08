# AMS_GPS_Task

## Overview
The `AMS_GPS_Task` manages the reception, buffering, and parsing of NMEA 0183 sentences coming from an external GPS module over a USART interface. It utilizes an idle-line detection strategy to efficiently capture complete GPS sentences without dropping bytes — DMA-based when the bound UART has a DMA stream wired, interrupt-based otherwise.

**Which UART:** bound at init via `vd_GPS_Task_Init(&huartX)`, called from `main.c`'s `GPS_Start_Task()`. Currently wired to **USART3** for bench testing (NMEA sentences fed over the ST-LINK USB/VCP from a PC — USART3 has no DMA stream configured, so the driver falls back to interrupt-mode reception automatically). **Production on the bike is USART6** (has DMA wired) — swap the call site before flashing onto the vehicle. See `AMS_gps_driver.c`'s file header for the full explanation.

## Execution Model: Idle-Line to Queue
1. **Hardware Reception Setup:** The driver arms `HAL_UARTEx_ReceiveToIdle_DMA` (USART6) or `HAL_UARTEx_ReceiveToIdle_IT` (USART3), pointing the hardware to write incoming UART bytes directly into a buffer.
2. **Idle-Line Interrupt:** When the GPS module finishes transmitting a burst of NMEA sentences, the UART RX line goes idle. The STM32 hardware detects this and triggers `HAL_UARTEx_RxEventCallback` in ISR context.
3. **Queue Push:** The ISR immediately copies the buffered burst into a `GPS_NmeaPacket_t` and posts it to a FreeRTOS Queue. It then instantly re-arms reception to prevent dropped bytes.
4. **Task Processing:** The task wakes from `osMessageQueueGet`, splits the burst into individual sentences, and runs them through the parser.

## Core Features
1. **NMEA Parsing (`minmea` library):** 
   * Extracts data from standard sentences like `$GPRMC` (Recommended Minimum Data) and `$GPGGA` (Fix Data).
   * Parses valid coordinates, UTC time, ground speed, fix quality, and number of satellites tracked.
2. **Integer Arithmetic Conversions:**
   * Floating point math is computationally expensive. The task converts NMEA fractional coordinates directly into micro-degrees (`i32_latitude_udeg`) using integer scaling.
   * Speed is converted from Knots to km/h and m/s, strictly using integer arithmetic (scaled by 1000).
3. **Broker Synchronization:**
   * After successfully parsing and converting a sentence, the task updates the `GPS_Data_t` struct inside the Data Broker.
   * It also updates `ui32_last_fix_tick_ms`, which signals `AMS_Algorithms_Task` that fresh telemetry is available.
