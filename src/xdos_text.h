// Text formatting for the xDOS data format - pure logic, no Arduino, the same
// output on the PC and on AVR (whose printf cannot print floats).

#ifndef XDOS_TEXT_H
#define XDOS_TEXT_H

#include <stddef.h>
#include <stdint.h>

#include "eeprom_layout.h"

namespace xdos {

// Like printf("%.<significant>g"): 0.25, 20, 1.5e-05, -3.2e+07. NaN gives
// "NaN". Returns false if the text does not fit into cap.
bool formatFloat(char *out, size_t cap, float value, uint8_t significant = 6);

// Device name of a record, e.g. "SPACEDOS04" or "AIRDOS04C": family name of
// device_type + two-digit device_version + revision letter. Copies fallback
// if device_type is unknown. Returns false if it does not fit into cap.
bool formatDeviceName(char *out, size_t cap, const DosimeterEeprom &record,
                      const char *fallback);

// device_identifier of the record as printable ASCII without commas (other
// characters become '_'), at most 24 characters.
void formatIdentifier(char *out, size_t cap, const DosimeterEeprom &record);

}  // namespace xdos

#endif  // XDOS_TEXT_H
