# xDOS-lib

Shared firmware code for the UST xDOS family of dosimeters (SPACEDOS, AIRDOS,
LABDOS, …) running on ATmega1284P with the Arduino core (MightyCore).

The library is meant to hold everything the device firmwares would otherwise
copy from each other:

| part | status |
|---|---|
| UART → I2C configuration bridge and config mode — lets dosview read and write the device EEPROM and RTC over the data serial line, see [docs/CONFIG_BRIDGE.md](docs/CONFIG_BRIDGE.md) | done |
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

## Command line client

`tools/xdos_bridge.py` (needs pyserial) switches a device into config mode
and runs bridge commands, e.g. reading the EEPROM serial number at `0x5B`:

```bash
tools/xdos_bridge.py /dev/ttyACM0 I2CWR,5B,0800,10
```

## Tests

Pure logic is tested on the PC. xDOS-versions has to be checked out next to
this repository (`../xDOS-versions`):

```bash
pio test -e native
python3 -m unittest discover -s tools
```

## License

GPL-3.0, see [LICENSE](LICENSE).
