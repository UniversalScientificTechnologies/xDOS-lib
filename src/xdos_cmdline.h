// Command line protocol of the configuration bridge - pure logic, no Arduino.
//
// See docs/CONFIG_BRIDGE.md for the protocol itself.

#ifndef XDOS_CMDLINE_H
#define XDOS_CMDLINE_H

#include <stddef.h>
#include <stdint.h>

namespace xdos {

// Longest command or response line without the CR/LF terminator.
const size_t kCmdLineMax = 96;

// Most bytes moved in one I2C transaction - the Wire buffer on AVR is 32 B.
const uint8_t kI2cMaxBytes = 32;

// Token of the !CONFIG command. It only guards against random bytes, it is
// not a password.
const char kConfigToken[] = "XDOS";

// XOR of all characters in [begin, end) - the NMEA 0183 checksum.
uint8_t nmeaChecksum(const char *begin, const char *end);

// Collects characters into lines. A line ends with LF; CR is ignored. A line
// longer than kCmdLineMax is discarded as a whole, up to and including its LF.
class LineReader
{
public:
    LineReader();

    // Feeds one character. Returns true when it completed a line; line()
    // then holds it until the next call of feed().
    bool feed(char c);

    const char *line() const { return buf_; }

    void reset();

private:
    char buf_[kCmdLineMax + 1];
    size_t len_;
    bool overflow_;
    bool complete_;
};

enum class CmdType : uint8_t
{
    Config,       // !CONFIG,<token>
    I2cWrite,     // !I2CW,<addr>,<data>
    I2cRead,      // !I2CR,<addr>,<len>
    I2cWriteRead, // !I2CWR,<addr>,<data>,<len>
    Exit,         // !EXIT
};

enum class CmdError : uint8_t
{
    Ok,
    NotCommand, // the line does not start with '!' - not meant for the bridge
    Checksum,   // missing or wrong *hh
    Syntax,     // wrong number or format of fields
    Unknown,    // unknown command name
    Length,     // data or read length out of 1..kI2cMaxBytes (0 allowed for !I2CW)
    Token,      // !CONFIG with a wrong token
};

struct Command
{
    CmdType type;
    uint8_t addr;                // 7-bit I2C address
    uint8_t data[kI2cMaxBytes];  // bytes to write
    uint8_t dataLen;
    uint8_t readLen;             // bytes to read
};

// Parses and validates one line (without CR/LF).
CmdError parseCommand(const char *line, Command &out);

// Name of the error as used in "#CMD_ERR,<name>" responses.
const char *cmdErrorName(CmdError e);

// Name of a Wire.endTransmission() result code as used in "#I2C_ERR,<name>".
// Code 0 (success) gives "OK".
const char *i2cErrorName(uint8_t wireCode);

// Writes bytes as upper case hex digits plus a terminating NUL. Returns false
// if they do not fit into cap.
bool formatHex(char *out, size_t cap, const uint8_t *data, size_t n);

// Builds a response line "#<body>*hh\r\n" plus a terminating NUL. Returns its
// length without the NUL, or 0 if it does not fit into cap.
size_t frameResponse(char *out, size_t cap, const char *body);

}  // namespace xdos

#endif  // XDOS_CMDLINE_H
