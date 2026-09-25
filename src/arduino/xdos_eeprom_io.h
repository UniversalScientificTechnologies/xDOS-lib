// Reading the EEPROM configuration record and the factory serial number over
// I2C (AT24CS64-style EEPROM: 2-byte addresses, serial number at 0x0800 of a
// separate I2C address).

#ifndef XDOS_EEPROM_IO_H
#define XDOS_EEPROM_IO_H

#include <Arduino.h>
#include <Wire.h>

#include "../xdos_eeprom.h"

namespace xdos {

// Reads len bytes from offset of the EEPROM at addr, in blocks the Wire
// buffer can hold, each with a repeated start between address and read.
bool readEeprom(TwoWire &wire, uint8_t addr, uint16_t offset, uint8_t *buf, size_t len);

// Reads and checks the configuration record at offset 0 (see checkRecord()).
// firstBytes gets the two raw bytes at offset 0 ($ADC configuration field),
// 0xFF 0xFF when the EEPROM does not answer (status NotRead).
RecordStatus readRecord(TwoWire &wire, uint8_t addr, DosimeterEeprom &record,
                        uint16_t &version, uint8_t firstBytes[2]);

// The 16-byte factory serial number as 32 lowercase hex digits (as the
// firmwares have always written it) into out (at least 33 bytes). False
// (out empty) when the EEPROM does not answer.
bool readSerialNumber(TwoWire &wire, uint8_t snAddr, char *out);

}  // namespace xdos

#endif  // XDOS_EEPROM_IO_H
