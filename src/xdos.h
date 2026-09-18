// xDOS-lib - shared firmware code for UST xDOS dosimeters.
//
// Main header. The EEPROM record layout comes from xDOS-versions
// (generated/eeprom_layout.h), which the firmware project provides as a
// sibling library.

#ifndef XDOS_H
#define XDOS_H

#include <stddef.h>

#include "eeprom_layout.h"

namespace xdos {

// Library version, the same as in library.json.
const char *libraryVersion();

// Size of the EEPROM configuration record as defined by xDOS-versions.
const size_t kEepromRecordSize = sizeof(DosimeterEeprom);

}  // namespace xdos

#endif  // XDOS_H
