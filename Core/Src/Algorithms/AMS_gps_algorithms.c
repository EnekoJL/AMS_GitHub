/**
 * @file    AMS_gps_algorithms.c
 * @brief   See AMS_gps_algorithms.h. Math moved here verbatim from
 *          AMS_GPS_Task.c's vd_GPS_Manager_TaskProcess() — behaviour is
 *          unchanged, only the location and the explicit ui32_now_ms
 *          parameter (was HAL_GetTick() called directly) are new.
 */

#include "Algorithms/AMS_gps_algorithms.h"

GPS_Data_t st_GPS_ApplyRmcFrame(GPS_Data_t current,
                                const struct minmea_sentence_rmc *p_frame,
                                uint32_t ui32_now_ms)
{
    GPS_Data_t out = current;

    out.b_gps_is_connected = p_frame->valid;

    if (p_frame->valid) {
        /* Convert to micro-degrees (x1,000,000) using purely integer arithmetic */
        if (p_frame->latitude.scale != 0) {
            int32_t lat_degrees = p_frame->latitude.value / (p_frame->latitude.scale * 100);
            int32_t lat_minutes = p_frame->latitude.value % (p_frame->latitude.scale * 100);
            out.i32_latitude_udeg = lat_degrees * 1000000 +
                (int32_t)(((int64_t)lat_minutes * 1000000) / (60 * p_frame->latitude.scale));
        }

        if (p_frame->longitude.scale != 0) {
            int32_t lon_degrees = p_frame->longitude.value / (p_frame->longitude.scale * 100);
            int32_t lon_minutes = p_frame->longitude.value % (p_frame->longitude.scale * 100);
            out.i32_longitude_udeg = lon_degrees * 1000000 +
                (int32_t)(((int64_t)lon_minutes * 1000000) / (60 * p_frame->longitude.scale));
        }

        /* Speed: knots -> km/h -> m/s (all integer x1000) */
        out.i32_vel_knots_x1000 = minmea_rescale(&p_frame->speed, 1000);
        out.i32_vel_kmh_x1000   = (out.i32_vel_knots_x1000 * 1852) / 1000;
        out.i32_vel_ms_x1000    = (out.i32_vel_kmh_x1000 * 1000) / 3600;

        /* UTC time */
        out.ui8_hour   = (uint8_t)p_frame->time.hours;
        out.ui8_minute = (uint8_t)p_frame->time.minutes;
        out.ui8_second = (uint8_t)p_frame->time.seconds;

        out.ui32_last_fix_tick_ms = ui32_now_ms;
    }

    return out;
}

GPS_Data_t st_GPS_ApplyGgaFrame(GPS_Data_t current,
                                const struct minmea_sentence_gga *p_frame)
{
    GPS_Data_t out = current;

    out.ui8_fix_quality = (uint8_t)p_frame->fix_quality;
    out.ui8_satellites  = (uint8_t)p_frame->satellites_tracked;

    return out;
}
