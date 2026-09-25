#include "xdos_eeprom.h"

#include <string.h>

namespace xdos {

uint32_t recordCrc32(const uint8_t *data, size_t len, size_t crcOffset)
{
    uint32_t crc = 0;
    for (size_t i = 0; i < len; i++)
    {
        uint8_t b = (i >= crcOffset && i < crcOffset + 4) ? 0 : data[i];
        crc ^= b;
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
        }
    }
    return crc;
}

RecordStatus checkRecord(const uint8_t *raw, size_t len, DosimeterEeprom &record,
                         uint16_t &version)
{
    memset(&record, 0xFF, sizeof(record));
    version = 0xFFFF;
    if (len < sizeof(DosimeterEeprom)) return RecordStatus::NotRead;

    version = (uint16_t)(raw[0] | (raw[1] << 8));
    if (version == 0xFFFF) return RecordStatus::Blank;

    uint16_t size = eeprom_record_size(version);
    if (size == 0) return RecordStatus::UnknownVersion;

    // Only the bytes of the record's own version; the rest stays 0xFF.
    memcpy(&record, raw, size);

    const size_t crcOffset = offsetof(DosimeterEeprom, crc32);
    if (recordCrc32(raw, size, crcOffset) != record.crc32) return RecordStatus::BadCrc;

    return version == EEPROM_FORMAT_VERSION_LATEST ? RecordStatus::Ok
                                                   : RecordStatus::OldVersion;
}

bool syncAge(const DosimeterEeprom &record, uint32_t rtcSeconds, uint32_t &age)
{
    uint32_t value = record.rtc_history[0].rtc_value_at_reference_timestamp;
    if (value == 0 || value == 0xFFFFFFFFUL || value > rtcSeconds) return false;
    age = rtcSeconds - value;
    return true;
}

}  // namespace xdos
