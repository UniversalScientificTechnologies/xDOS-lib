#include "xdos_cmdline.h"

#include <string.h>

namespace xdos {

namespace {

const char kHexDigits[] = "0123456789ABCDEF";

// Value of a hex digit, or -1.
int hexValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Parses 1 or 2 hex digits in [begin, end) into value.
bool parseHexByte(const char *begin, const char *end, uint8_t &value)
{
    size_t n = (size_t)(end - begin);
    if (n < 1 || n > 2) return false;
    int v = 0;
    for (const char *p = begin; p < end; p++)
    {
        int d = hexValue(*p);
        if (d < 0) return false;
        v = v * 16 + d;
    }
    value = (uint8_t)v;
    return true;
}

// Parses an even number of hex digits in [begin, end) into bytes.
// Returns false on a bad digit or odd length; sets tooLong if the bytes do
// not fit into cap (the output is then incomplete).
bool parseHexBytes(const char *begin, const char *end, uint8_t *out,
                   size_t cap, uint8_t &len, bool &tooLong)
{
    size_t n = (size_t)(end - begin);
    tooLong = false;
    if (n % 2 != 0) return false;
    for (const char *p = begin; p < end; p++)
    {
        if (hexValue(*p) < 0) return false;
    }
    if (n / 2 > cap)
    {
        tooLong = true;
        return true;
    }
    for (size_t i = 0; i < n / 2; i++)
    {
        out[i] = (uint8_t)(hexValue(begin[2 * i]) * 16 + hexValue(begin[2 * i + 1]));
    }
    len = (uint8_t)(n / 2);
    return true;
}

bool fieldIs(const char *begin, const char *end, const char *name)
{
    size_t n = (size_t)(end - begin);
    return strlen(name) == n && memcmp(begin, name, n) == 0;
}

}  // namespace

uint8_t nmeaChecksum(const char *begin, const char *end)
{
    uint8_t sum = 0;
    for (const char *p = begin; p < end; p++) sum ^= (uint8_t)*p;
    return sum;
}

LineReader::LineReader()
{
    reset();
}

void LineReader::reset()
{
    len_ = 0;
    overflow_ = false;
    complete_ = false;
    buf_[0] = '\0';
}

bool LineReader::feed(char c)
{
    if (complete_)
    {
        // The previous line was handed out; start a new one.
        len_ = 0;
        complete_ = false;
        buf_[0] = '\0';
    }

    if (c == '\r') return false;

    if (c == '\n')
    {
        if (overflow_)
        {
            reset();
            return false;
        }
        buf_[len_] = '\0';
        complete_ = true;
        return true;
    }

    if (overflow_) return false;

    if (len_ >= kCmdLineMax)
    {
        overflow_ = true;
        return false;
    }
    buf_[len_++] = c;
    return false;
}

CmdError parseCommand(const char *line, Command &out)
{
    if (line[0] != '!') return CmdError::NotCommand;

    // "!<body>*hh" - the checksum must be the last three characters.
    size_t n = strlen(line);
    if (n < 4 || line[n - 3] != '*') return CmdError::Checksum;
    const char *body = line + 1;
    const char *bodyEnd = line + n - 3;
    uint8_t expected;
    if (!parseHexByte(bodyEnd + 1, line + n, expected)) return CmdError::Checksum;
    if (nmeaChecksum(body, bodyEnd) != expected) return CmdError::Checksum;

    // Split the body into fields.
    const size_t kMaxFields = 5;
    const char *fieldBegin[kMaxFields];
    const char *fieldEnd[kMaxFields];
    size_t fields = 0;
    const char *p = body;
    for (;;)
    {
        if (fields == kMaxFields) return CmdError::Syntax;
        fieldBegin[fields] = p;
        while (p < bodyEnd && *p != ',') p++;
        fieldEnd[fields] = p;
        fields++;
        if (p == bodyEnd) break;
        p++;  // skip the comma
    }

    const char *name = fieldBegin[0];
    const char *nameEnd = fieldEnd[0];
    memset(&out, 0, sizeof(out));
    bool tooLong = false;

    if (fieldIs(name, nameEnd, "CONFIG"))
    {
        if (fields != 2) return CmdError::Syntax;
        if (!fieldIs(fieldBegin[1], fieldEnd[1], kConfigToken)) return CmdError::Token;
        out.type = CmdType::Config;
        return CmdError::Ok;
    }

    if (fieldIs(name, nameEnd, "EXIT"))
    {
        if (fields != 1) return CmdError::Syntax;
        out.type = CmdType::Exit;
        return CmdError::Ok;
    }

    bool write = fieldIs(name, nameEnd, "I2CW");
    bool read = fieldIs(name, nameEnd, "I2CR");
    bool writeRead = fieldIs(name, nameEnd, "I2CWR");
    if (!write && !read && !writeRead) return CmdError::Unknown;

    size_t wantFields = writeRead ? 4 : 3;
    if (fields != wantFields) return CmdError::Syntax;

    if (!parseHexByte(fieldBegin[1], fieldEnd[1], out.addr) || out.addr > 0x7F)
    {
        return CmdError::Syntax;
    }

    if (write || writeRead)
    {
        if (!parseHexBytes(fieldBegin[2], fieldEnd[2], out.data, kI2cMaxBytes,
                           out.dataLen, tooLong))
        {
            return CmdError::Syntax;
        }
        if (tooLong) return CmdError::Length;
        if (writeRead && out.dataLen == 0) return CmdError::Length;
    }

    if (read || writeRead)
    {
        size_t lenField = read ? 2 : 3;
        if (!parseHexByte(fieldBegin[lenField], fieldEnd[lenField], out.readLen))
        {
            return CmdError::Syntax;
        }
        if (out.readLen == 0 || out.readLen > kI2cMaxBytes) return CmdError::Length;
    }

    out.type = write ? CmdType::I2cWrite : (read ? CmdType::I2cRead : CmdType::I2cWriteRead);
    return CmdError::Ok;
}

const char *cmdErrorName(CmdError e)
{
    switch (e)
    {
    case CmdError::Ok: return "OK";
    case CmdError::NotCommand: return "NOT_COMMAND";
    case CmdError::Checksum: return "CHECKSUM";
    case CmdError::Syntax: return "SYNTAX";
    case CmdError::Unknown: return "UNKNOWN";
    case CmdError::Length: return "LENGTH";
    case CmdError::Token: return "TOKEN";
    }
    return "UNKNOWN";
}

const char *i2cErrorName(uint8_t wireCode)
{
    switch (wireCode)
    {
    case 0: return "OK";
    case 1: return "TOO_LONG";
    case 2: return "NACK_ADDR";
    case 3: return "NACK_DATA";
    case 4: return "BUS";
    case 5: return "TIMEOUT";
    }
    return "BUS";
}

bool formatHex(char *out, size_t cap, const uint8_t *data, size_t n)
{
    if (cap < 2 * n + 1) return false;
    for (size_t i = 0; i < n; i++)
    {
        out[2 * i] = kHexDigits[data[i] >> 4];
        out[2 * i + 1] = kHexDigits[data[i] & 0x0F];
    }
    out[2 * n] = '\0';
    return true;
}

size_t frameResponse(char *out, size_t cap, const char *body)
{
    size_t n = strlen(body);
    // '#' + body + '*' + 2 digits + CR + LF + NUL
    if (cap < n + 7) return 0;
    out[0] = '#';
    memcpy(out + 1, body, n);
    uint8_t sum = nmeaChecksum(body, body + n);
    out[n + 1] = '*';
    out[n + 2] = kHexDigits[sum >> 4];
    out[n + 3] = kHexDigits[sum & 0x0F];
    out[n + 4] = '\r';
    out[n + 5] = '\n';
    out[n + 6] = '\0';
    return n + 6;
}

}  // namespace xdos
