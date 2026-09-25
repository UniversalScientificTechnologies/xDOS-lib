#include "xdos_time.h"

namespace xdos {

uint32_t unixTime(uint16_t year, uint8_t month, uint8_t day,
                  uint8_t hour, uint8_t minute, uint8_t second)
{
    // Days since 1970-01-01 of a proleptic Gregorian date (H. Hinnant's
    // days_from_civil), with the year starting in March so that the leap
    // day is the last day of the year.
    int32_t y = (int32_t)year - (month <= 2 ? 1 : 0);
    int32_t era = y / 400;
    int32_t yoe = y - era * 400;                                   // [0, 399]
    int32_t mp = month > 2 ? month - 3 : month + 9;                // March = 0
    int32_t doy = (153 * mp + 2) / 5 + day - 1;                    // [0, 365]
    int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;           // [0, 146096]
    int32_t days = era * 146097 + doe - 719468;

    return (uint32_t)days * 86400UL + (uint32_t)hour * 3600UL
         + (uint32_t)minute * 60UL + second;
}

}  // namespace xdos
