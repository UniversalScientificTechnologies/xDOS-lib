// Configuration bridge: forwards I2C transactions from a serial line, so that
// dosview can read and write the device EEPROM and RTC over the data cable.
//
// See docs/CONFIG_BRIDGE.md for the protocol and the rules for integrating it
// into a firmware.

#ifndef XDOS_CONFIG_BRIDGE_H
#define XDOS_CONFIG_BRIDGE_H

#include <Arduino.h>
#include <Wire.h>

#include "xdos_cmdline.h"

namespace xdos {

// Config mode ends by itself after this long without a valid command.
const uint32_t kConfigIdleTimeoutMs = 60000UL;

class ConfigBridge
{
public:
    ConfigBridge(Stream &port, TwoWire &wire);

    // Reads the bytes waiting on the port. Returns true once a valid
    // "!CONFIG,<token>" line has arrived. Everything else is dropped without
    // an answer. Call it only where a short delay does not matter (the
    // firmware's report window), never from the measurement loop.
    bool poll();

    // Runs config mode: answers bridge commands until "!EXIT" or the idle
    // timeout and then resets the MCU with the watchdog. Never returns.
    // The caller stops the measurement, closes its files and powers the I2C
    // devices before calling it.
    void run() __attribute__((noreturn));

private:
    void respond(const char *body);
    void execute(const Command &cmd);
    void resetDevice(const char *body) __attribute__((noreturn));

    Stream &port_;
    TwoWire &wire_;
    LineReader reader_;
};

}  // namespace xdos

#endif  // XDOS_CONFIG_BRIDGE_H
