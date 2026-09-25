#include "xdos_eeprom_io.h"

#include <string.h>

namespace xdos {

namespace {

// Wire's buffer is 32 bytes on AVR.
const uint8_t kBlock = 32;

}  // namespace

bool readEeprom(TwoWire &wire, uint8_t addr, uint16_t offset, uint8_t *buf, size_t len)
{
    while (len)
    {
        uint8_t n = len > kBlock ? kBlock : (uint8_t)len;
        wire.beginTransmission(addr);
        wire.write((uint8_t)(offset >> 8));
        wire.write((uint8_t)offset);
        if (wire.endTransmission(false) != 0) return false;
        if (wire.requestFrom(addr, n) != n) return false;
        for (uint8_t i = 0; i < n; i++) buf[i] = (uint8_t)wire.read();
        buf += n;
        offset += n;
        len -= n;
    }
    return true;
}

RecordStatus readRecord(TwoWire &wire, uint8_t addr, DosimeterEeprom &record,
                        uint16_t &version, uint8_t firstBytes[2])
{
    uint8_t raw[sizeof(DosimeterEeprom)];
    if (!readEeprom(wire, addr, 0, raw, sizeof(raw)))
    {
        memset(&record, 0xFF, sizeof(record));
        version = 0xFFFF;
        firstBytes[0] = firstBytes[1] = 0xFF;
        return RecordStatus::NotRead;
    }
    firstBytes[0] = raw[0];
    firstBytes[1] = raw[1];
    return checkRecord(raw, sizeof(raw), record, version);
}

bool readSerialNumber(TwoWire &wire, uint8_t snAddr, char *out)
{
    uint8_t sn[16];
    out[0] = '\0';
    if (!readEeprom(wire, snAddr, 0x0800, sn, sizeof(sn))) return false;
    const char *hex = "0123456789abcdef";
    for (uint8_t i = 0; i < sizeof(sn); i++)
    {
        out[2 * i] = hex[sn[i] >> 4];
        out[2 * i + 1] = hex[sn[i] & 0x0F];
    }
    out[2 * sizeof(sn)] = '\0';
    return true;
}

}  // namespace xdos
