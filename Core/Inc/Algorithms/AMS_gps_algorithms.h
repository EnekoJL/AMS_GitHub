/**
 * @file    AMS_gps_algorithms.h
 * @brief   Domain-layer algorithms for turning parsed NMEA frames into a
 *          GPS_Data_t snapshot. Pure functions: given the current snapshot
 *          and a parsed frame, return the updated snapshot. No hardware,
 *          no FreeRTOS, no queue/DMA handling — that stays in
 *          AMS_GPS_Task.c, which calls these after minmea_parse_rmc()/
 *          minmea_parse_gga() succeed.
 *
 *          Extracted out of AMS_GPS_Task.c's task loop so this conversion
 *          logic can be unit-tested directly (a `for(;;)` task loop that
 *          blocks on a queue can't be called from a host test).
 */
#ifndef ALGORITHMS_GPS_ALGORITHMS_H_
#define ALGORITHMS_GPS_ALGORITHMS_H_

#include "AMS_DataTypes.h"
#include "Middleware/minmea.h"

/**
 * @brief  Applies a parsed $xxRMC frame to a GPS_Data_t snapshot.
 *         If the frame is not valid (no fix), only b_gps_is_connected is
 *         updated — position/speed/time fields are left untouched, matching
 *         the original task-loop behaviour (never overwrite a good fix with
 *         garbage from an invalid sentence).
 * @param  current      The GPS_Data_t snapshot before this frame.
 * @param  p_frame      Parsed RMC frame (from minmea_parse_rmc()).
 * @param  ui32_now_ms  Current system tick — caller reads this (HAL_GetTick()
 *                       in the real task), passed in so this function has no
 *                       hardware dependency of its own.
 * @retval Updated GPS_Data_t snapshot.
 */
GPS_Data_t st_GPS_ApplyRmcFrame(GPS_Data_t current,
                                const struct minmea_sentence_rmc *p_frame,
                                uint32_t ui32_now_ms);

/**
 * @brief  Applies a parsed $xxGGA frame to a GPS_Data_t snapshot
 *         (fix quality + satellite count only).
 * @param  current  The GPS_Data_t snapshot before this frame.
 * @param  p_frame  Parsed GGA frame (from minmea_parse_gga()).
 * @retval Updated GPS_Data_t snapshot.
 */
GPS_Data_t st_GPS_ApplyGgaFrame(GPS_Data_t current,
                                const struct minmea_sentence_gga *p_frame);

#endif /* ALGORITHMS_GPS_ALGORITHMS_H_ */
