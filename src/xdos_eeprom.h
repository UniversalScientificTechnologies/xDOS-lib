// EEPROM configuration record (DosimeterEeprom from xDOS-versions) - pure
// logic, no Arduino: CRC, validation, sync age.

#ifndef XDOS_EEPROM_H
#define XDOS_EEPROM_H

#include <stddef.h>
#include <stdint.h>

#include "eeprom_layout.h"

namespace xdos {

// CRC32 of a record the way dosview computes it: the reflected IEEE
// polynomial with the register starting at 0 and no final inversion
// (Python: zlib.crc32(data, 0xFFFFFFFF) ^ 0xFFFFFFFF), over len bytes with the
// four bytes at crcOffset taken as zero.
uint32_t recordCrc32(const uint8_t *data, size_t len, size_t crcOffset);

enum class RecordStatus : uint8_t
{
    Ok,             // latest format_version, CRC matches
    OldVersion,     // an older known format_version, CRC matches
    Blank,          // erased EEPROM (format_version 0xFFFF)
    UnknownVersion, // format_version this firmware does not know
    BadCrc,         // known format_version, CRC does not match
    NotRead,        // the EEPROM could not be read
};

// Checks a record read from the EEPROM. raw holds len bytes from address 0,
// at least sizeof(DosimeterEeprom). Fills record (fields the version does
// not have read as 0xFF) and version.
RecordStatus checkRecord(const uint8_t *raw, size_t len, DosimeterEeprom &record,
                         uint16_t &version);

// Fields of the record can be used (Ok or OldVersion).
inline bool recordTrusted(RecordStatus s)
{
    return s == RecordStatus::Ok || s == RecordStatus::OldVersion;
}

// Seconds since the clock was last set according to the sync record
// (rtc_history[0]); rtcSeconds is the current RTC counter (the Unix time for
// a calendar RTC). False if the record holds no sync or it lies in the future.
bool syncAge(const DosimeterEeprom &record, uint32_t rtcSeconds, uint32_t &age);

}  // namespace xdos

#endif  // XDOS_EEPROM_H
