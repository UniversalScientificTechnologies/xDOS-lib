// Header of an xDOS VERSION_2.1 log file - pure logic, no Arduino.
//
// writeHeader() emits the device part of the header, line by line, into a
// LineSink; the firmware adds its own lines (e.g. $RTCCHK, $TIME) after it.

#ifndef XDOS_HEADER_H
#define XDOS_HEADER_H

#include <stdint.h>

#include "xdos_eeprom.h"

namespace xdos {

// Receives the header line by line (without CR/LF).
class LineSink
{
public:
    virtual void line(const char *text) = 0;

protected:
    ~LineSink() {}
};

struct HeaderInfo
{
    // Main EEPROM (analog board, or the only one): the record as read, its
    // status from checkRecord() and the first two raw bytes (the configuration
    // field of $ADC). serial = its factory serial number as hex text.
    const DosimeterEeprom *record;
    RecordStatus status;
    uint16_t version;
    uint8_t rawFirstBytes[2];
    const char *serial;

    const char *fallbackType;     // device name for an unusable record
    const char *fwVersion;        // e.g. "2.0.0-0-User"
    const char *gitHashAndBuild;  // "<git hash>,<build type>"

    bool hasBattery;              // $BATP only for devices with a battery
    bool batteryPresent;
    uint16_t batteryMv;

    uint32_t numChannels;         // ADC channel range for $CHAN, e.g. 65536
    float integrationPeriodS;     // $ITIME
    float tickS;                  // timer tick; $TICK only if not 128 us
};

// Writes, in this order: $DATAFORMAT, $DOS, $ADC, $ADC_NAME, $BATP, $CALIB,
// $CHAN, $DIODE, $ERNG, $ITIME, $TICK and $ERROR lines explaining an unusable
// or incomplete EEPROM record. Lines with unknown values are left out.
void writeHeader(LineSink &out, const HeaderInfo &info);

}  // namespace xdos

#endif  // XDOS_HEADER_H
