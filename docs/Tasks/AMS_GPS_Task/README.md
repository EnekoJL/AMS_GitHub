# AMS_GPS_Task

## Overview
The `AMS_GPS_Task` manages the reception, buffering, and parsing of NMEA 0183 sentences coming from an external GPS module over a USART interface. It utilizes a DMA-based reception strategy coupled with idle-line detection to efficiently capture complete GPS sentences without dropping bytes.

## Execution Model: DMA Idle-Line to Queue
1. **Hardware DMA Setup:** The driver configures `HAL_UARTEx_ReceiveToIdle_DMA`, pointing the hardware to write incoming UART bytes directly into a buffer.
2. **Idle-Line Interrupt:** When the GPS module finishes transmitting a burst of NMEA sentences, the UART RX line goes idle. The STM32 hardware detects this and triggers `HAL_UARTEx_RxEventCallback` in ISR context.
3. **Queue Push:** The ISR immediately copies the buffered burst into a `GPS_NmeaPacket_t` and posts it to a FreeRTOS Queue. It then instantly re-arms the DMA to prevent dropped bytes.
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
   * It also updates `ui32_last_fix_tick_ms`, which signals the Data Calculator task that fresh telemetry is available.
