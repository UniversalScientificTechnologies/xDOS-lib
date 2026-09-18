# Configuration bridge

The configuration bridge lets a host (dosview, `tools/xdos_bridge.py`) run
I2C transactions on the device's internal bus over the data serial line — to
read and write the EEPROM configuration record and to set the RTC — without
any extra hardware. AIRDOS04/05 do the same with an FT260 and an I2C switch
on the board; devices without them use this bridge.

Code: `src/xdos_cmdline.*` (protocol, pure logic, tested on the PC) and
`src/arduino/xdos_config_bridge.*` (Stream, Wire, watchdog).

## Config mode

The bridge only works in **config mode**, during which the device does not
measure.

1. Outside config mode the firmware calls `ConfigBridge::poll()` from its
   report window only — never from the measurement loop. Everything except
   a valid `!CONFIG,<token>` line is dropped without an answer, so random
   bytes on the line neither add dead time nor start the bridge.
2. On `!CONFIG` the firmware stops measuring, writes `#CONFIG_MODE` into its
   log, closes its files, makes sure the I2C devices are powered and calls
   `ConfigBridge::run()`.
3. `run()` answers bridge commands. It ends on `!EXIT` or after 60 s without
   a command, always with a **watchdog reset**: the device boots cleanly and
   starts a new log file whose header already shows the new EEPROM content
   and RTC time.

`run()` keeps the watchdog enabled (8 s) while it works, so an I2C
transaction stuck on a hung bus resets the device instead of blocking it
forever. Linking the bridge also adds a function in `.init3` that switches
the watchdog off right after any reset, before `setup()`.

## Line format

Both directions use NMEA 0183 style lines:

```
!<body>*hh\r\n      host → device (command)
#<body>*hh\r\n      device → host (response)
```

`hh` is the XOR of all characters of `<body>` as two hex digits (either case
is accepted from the host; the device sends upper case). The device ignores
CR, so `\n` alone also ends a line. Lines are at most 96 characters without
CR/LF; a longer line is dropped as a whole.

All numbers in commands — I2C addresses, data bytes and lengths — are
**hexadecimal**. Data bytes are written as one run of hex digit pairs without
separators. An I2C address is 7-bit (`00`–`7F`).

At most **32 bytes** go into one transaction in each direction (the Wire
buffer on AVR). For an EEPROM write this includes the two memory address
bytes, so at most 30 data bytes per `!I2CW`.

## Commands

| command | meaning | response |
|---|---|---|
| `!CONFIG,XDOS` | enter config mode; inside config mode just answers again | `#CONFIG_MODE,<timeout_s>` |
| `!I2CW,<addr>,<data>` | write bytes, then STOP; empty `<data>` only probes the address | `#I2C_OK` |
| `!I2CR,<addr>,<len>` | read `<len>` bytes | `#I2C_OK,<data>` |
| `!I2CWR,<addr>,<data>,<len>` | write bytes, **repeated start**, read `<len>` bytes — e.g. set a register or memory pointer and read from it | `#I2C_OK,<data>` |
| `!EXIT` | leave config mode | `#CONFIG_EXIT`, then reset |

After 60 s without a valid command the device answers `#CONFIG_TIMEOUT` and
resets. Lines that do not start with `!` are ignored in config mode, too.

The token `XDOS` only protects against random bytes; it is not a password.
The protection of flight hardware is that the bridge is compiled only into
development builds.

### Errors

| response | cause |
|---|---|
| `#CMD_ERR,CHECKSUM` | missing or wrong `*hh` |
| `#CMD_ERR,SYNTAX` | wrong number or format of fields |
| `#CMD_ERR,UNKNOWN` | unknown command |
| `#CMD_ERR,LENGTH` | data or read length outside 1–32 bytes |
| `#CMD_ERR,TOKEN` | `!CONFIG` with a wrong token |
| `#I2C_ERR,NACK_ADDR` | no device answered at the address |
| `#I2C_ERR,NACK_DATA` | the device refused a data byte (e.g. EEPROM still busy with a write) |
| `#I2C_ERR,TOO_LONG` | more data than the Wire buffer holds |
| `#I2C_ERR,BUS` | other bus error, e.g. lost arbitration |
| `#I2C_ERR,TIMEOUT` | bus timeout (only if the Wire library is built with timeouts) |
| `#I2C_ERR,SHORT` | the device returned fewer bytes than requested |

### Example

Reading the 16-byte factory serial number of an AT24CS64 whose serial number
area is at `0x5B` (memory address `0x0800`):

```
!CONFIG,XDOS*26
#CONFIG_MODE,60*7C
!I2CWR,5B,0800,10*6F
#I2C_OK,0910410874100851640BA080A08000AB*37
!EXIT*00
#CONFIG_EXIT*55
```

An EEPROM write is page-limited and takes a few milliseconds; while it runs
the chip does not acknowledge its address, so the host polls with an empty
`!I2CW,<addr>,` until it answers `#I2C_OK`.

## Integration into a firmware

- Create the bridge with the serial port the host talks to and the I2C bus:
  `xdos::ConfigBridge configBridge(Serial1, Wire);`
- Call `poll()` once per report window. When it returns `true`, stop the
  measurement, log `#CONFIG_MODE`, close the log file, power the I2C devices
  and call `run()`.
- Firmwares that must not contain the bridge (flight builds) compile it out,
  e.g. with a build flag around all of the above.
- The firmware's own I2C reads should use a repeated start
  (`endTransmission(false)`) between setting a register pointer and reading,
  and check the return codes.
