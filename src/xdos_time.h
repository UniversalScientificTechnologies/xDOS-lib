// Calendar time -> Unix time - pure logic, no Arduino.

#ifndef XDOS_TIME_H
#define XDOS_TIME_H

#include <stdint.h>

namespace xdos {

// Unix time (seconds since 1970-01-01 00:00:00 UTC) of a UTC calendar date
// and time. Valid for 1970..2105 (fits uint32_t); month 1..12, day 1..31.
uint32_t unixTime(uint16_t year, uint8_t month, uint8_t day,
                  uint8_t hour, uint8_t minute, uint8_t second);

}  // namespace xdos

#endif  // XDOS_TIME_H
