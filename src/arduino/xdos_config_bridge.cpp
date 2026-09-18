#include "xdos_config_bridge.h"

#include <stdio.h>
#include <string.h>

#include <avr/wdt.h>

namespace {

// Config mode ends with a watchdog reset, and the watchdog stays enabled
// across it. Switch it off right after reset, before main() - otherwise the
// firmware would reset again during its (seconds long) setup(). Linked in
// only together with the bridge.
void xdosDisableWatchdog() __attribute__((naked, used, section(".init3")));
void xdosDisableWatchdog()
{
    MCUSR = 0;
    wdt_disable();
}

}  // namespace

namespace xdos {

ConfigBridge::ConfigBridge(Stream &port, TwoWire &wire)
    : port_(port), wire_(wire)
{
}

bool ConfigBridge::poll()
{
    while (port_.available() > 0)
    {
        if (!reader_.feed((char)port_.read())) continue;

        Command cmd;
        if (parseCommand(reader_.line(), cmd) == CmdError::Ok &&
            cmd.type == CmdType::Config)
        {
            reader_.reset();
            return true;
        }
    }
    return false;
}

void ConfigBridge::run()
{
    // Guards against a hung I2C bus: the loop below resets the watchdog, a
    // transaction stuck for 8 s resets the MCU.
    wdt_enable(WDTO_8S);

    char body[24];
    snprintf(body, sizeof(body), "CONFIG_MODE,%lu",
             (unsigned long)(kConfigIdleTimeoutMs / 1000UL));
    respond(body);

    reader_.reset();
    uint32_t lastCommand = millis();

    for (;;)
    {
        wdt_reset();

        if (millis() - lastCommand >= kConfigIdleTimeoutMs)
        {
            resetDevice("CONFIG_TIMEOUT");
        }

        while (port_.available() > 0)
        {
            if (!reader_.feed((char)port_.read())) continue;

            Command cmd;
            CmdError err = parseCommand(reader_.line(), cmd);
            if (err == CmdError::NotCommand) continue;

            lastCommand = millis();

            if (err != CmdError::Ok)
            {
                char errBody[24];
                snprintf(errBody, sizeof(errBody), "CMD_ERR,%s", cmdErrorName(err));
                respond(errBody);
                continue;
            }

            if (cmd.type == CmdType::Exit) resetDevice("CONFIG_EXIT");
            if (cmd.type == CmdType::Config)
            {
                respond(body);
                continue;
            }
            execute(cmd);
        }
    }
}

void ConfigBridge::execute(const Command &cmd)
{
    uint8_t code = 0;

    if (cmd.type == CmdType::I2cWrite || cmd.type == CmdType::I2cWriteRead)
    {
        wire_.beginTransmission(cmd.addr);
        wire_.write(cmd.data, cmd.dataLen);
        // Write-then-read keeps the bus with a repeated start, so no other
        // master can move the device's register pointer in between.
        code = wire_.endTransmission(cmd.type == CmdType::I2cWrite);
    }

    char reply[8 + 2 * kI2cMaxBytes + 1];

    if (code != 0)
    {
        snprintf(reply, sizeof(reply), "I2C_ERR,%s", i2cErrorName(code));
        respond(reply);
        return;
    }

    if (cmd.type == CmdType::I2cWrite)
    {
        respond("I2C_OK");
        return;
    }

    uint8_t got = wire_.requestFrom(cmd.addr, cmd.readLen);
    uint8_t data[kI2cMaxBytes];
    for (uint8_t i = 0; i < got && i < kI2cMaxBytes; i++) data[i] = (uint8_t)wire_.read();

    if (got != cmd.readLen)
    {
        respond(got == 0 ? "I2C_ERR,NACK_ADDR" : "I2C_ERR,SHORT");
        return;
    }

    strcpy(reply, "I2C_OK,");
    formatHex(reply + 7, sizeof(reply) - 7, data, got);
    respond(reply);
}

void ConfigBridge::respond(const char *body)
{
    char line[kCmdLineMax + 8];
    if (frameResponse(line, sizeof(line), body) > 0) port_.print(line);
}

void ConfigBridge::resetDevice(const char *body)
{
    respond(body);
    port_.flush();
    wdt_enable(WDTO_15MS);
    for (;;)
    {
    }
}

}  // namespace xdos
