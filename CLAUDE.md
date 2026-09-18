# CLAUDE.md

Guidance for agents working in this repository. Everything here is in English —
code, comments and documentation.

## What this is

Shared firmware code for UST xDOS dosimeters (ATmega1284P, Arduino/MightyCore,
avr-gcc with C++11). Consumed as a git submodule by the device firmwares
(first SPACEDOS04, then AIRDOS03 and LABDOS01). See [README.md](README.md) for
what the library is going to contain.

The work is planned in the ROADMAP of the SPACEDOS04 firmware
(`SPACEDOS04/fw/ROADMAP.md`), which is the master plan for the xDOS v2.1
migration across repositories.

## Layout

```
library.json      PlatformIO library manifest (the one firmware projects use)
platformio.ini    test project of the library only, env "native"
src/              pure logic, no Arduino headers — built and tested on the PC
src/arduino/      Arduino glue (Stream, Wire, watchdog) — excluded from native
test/             Unity tests, one folder per test suite
tools/            host side tools (Python) and their unittest tests
docs/             design documents, one per topic (CONFIG_BRIDGE.md)
```

## Build and test

```bash
pio test -e native                      # C++ pure logic
python3 -m unittest discover -s tools   # host tools
```

Needs xDOS-versions checked out next to this repository (`../xDOS-versions`);
it is linked with `lib_deps = symlink://../xDOS-versions`. The library itself
does not declare it as a dependency — the firmware project provides it.

## Conventions

- Keep pure logic (parsing, checksums, CRC, record validation, text
  formatting) free of Arduino headers so it runs in `pio test -e native`.
  Hardware access goes to `src/arduino/`.
- Output goes through Arduino's `Print&`, never through `String` — the target
  has 16 kB of RAM and the firmwares already use a large part of it for event
  buffers. Tests replace `Print` with a small stub collecting the text.
- C++11 only (`-std=gnu++11` in the native env mirrors avr-gcc); no exceptions,
  no RTTI, no dynamic allocation.
- Do not use `KNOWN_DEVICES` from xDOS-versions in device code: the table is
  not in PROGMEM and would cost RAM.
- The EEPROM layout lives in xDOS-versions; never redefine it here.
