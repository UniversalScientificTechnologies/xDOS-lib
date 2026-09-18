# xDOS-lib

Shared firmware code for the UST xDOS family of dosimeters (SPACEDOS, AIRDOS,
LABDOS, …) running on ATmega1284P with the Arduino core (MightyCore).

The library is meant to hold everything the device firmwares would otherwise
copy from each other:

| part | status |
|---|---|
| UART → I2C configuration bridge and config mode — lets dosview read and write the device EEPROM and RTC over the data serial line | planned |
| EEPROM configuration record — reading, CRC check, format version handling | planned |
| Header of the [xDOS data format v2.1](https://docs.dos.ust.cz/xdos_format) (`$DATAFORMAT`, `$DOS`, `$ADC`, `$CALIB`, `$CHAN`, …) | planned |

The EEPROM record layout itself is not defined here but in
[xDOS-versions](https://github.com/UniversalScientificTechnologies/xDOS-versions).

## Using it in a firmware project

Add both repositories as sibling submodules and let PlatformIO link them:

```bash
git submodule add ../xDOS-lib      lib/xDOS-lib
git submodule add ../xDOS-versions lib/xDOS-versions
```

```ini
lib_deps =
  symlink://lib/xDOS-lib
  symlink://lib/xDOS-versions
```

```cpp
#include <xdos.h>
```

The library does not declare xDOS-versions as a dependency — the firmware
project provides it, so both always come from the commits pinned by the
firmware.

## Tests

Pure logic is tested on the PC. xDOS-versions has to be checked out next to
this repository (`../xDOS-versions`):

```bash
pio test -e native
```

## License

GPL-3.0, see [LICENSE](LICENSE).
