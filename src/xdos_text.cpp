#include "xdos_text.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef __AVR__
#include <avr/pgmspace.h>
#endif

namespace xdos {

namespace {

bool append(char *out, size_t cap, size_t &pos, const char *text)
{
    size_t n = strlen(text);
    if (pos + n + 1 > cap) return false;
    memcpy(out + pos, text, n + 1);
    pos += n;
    return true;
}

bool appendChar(char *out, size_t cap, size_t &pos, char c)
{
    if (pos + 2 > cap) return false;
    out[pos++] = c;
    out[pos] = '\0';
    return true;
}

}  // namespace

bool formatFloat(char *out, size_t cap, float value, uint8_t significant)
{
    size_t pos = 0;
    if (cap == 0) return false;
    out[0] = '\0';

    if (isnan(value)) return append(out, cap, pos, "NaN");
    if (isinf(value)) return append(out, cap, pos, value > 0 ? "inf" : "-inf");
    if (value == 0.0f) return append(out, cap, pos, "0");
    if (significant < 1) significant = 1;
    if (significant > 9) significant = 9;

    double v = value;
    if (v < 0)
    {
        if (!appendChar(out, cap, pos, '-')) return false;
        v = -v;
    }

    // Normalize to [1, 10) and round to the significant digits.
    int exp10 = 0;
    while (v >= 10.0) { v /= 10.0; exp10++; }
    while (v < 1.0) { v *= 10.0; exp10--; }
    uint32_t scale = 1;
    for (uint8_t i = 1; i < significant; i++) scale *= 10;
    uint32_t digits = (uint32_t)(v * scale + 0.5);
    if (digits >= scale * 10)  // rounded up to the next power of ten
    {
        digits /= 10;
        exp10++;
    }

    // Digits as text, trailing zeros dropped.
    char d[10];
    for (int i = significant - 1; i >= 0; i--)
    {
        d[i] = (char)('0' + digits % 10);
        digits /= 10;
    }
    int nd = significant;
    while (nd > 1 && d[nd - 1] == '0') nd--;

    if (exp10 >= -4 && exp10 < significant)
    {
        // Fixed notation.
        if (exp10 < 0)
        {
            if (!append(out, cap, pos, "0.")) return false;
            for (int i = -1; i > exp10; i--)
            {
                if (!appendChar(out, cap, pos, '0')) return false;
            }
            for (int i = 0; i < nd; i++)
            {
                if (!appendChar(out, cap, pos, d[i])) return false;
            }
        }
        else
        {
            for (int i = 0; i <= exp10; i++)
            {
                if (!appendChar(out, cap, pos, i < nd ? d[i] : '0')) return false;
            }
            if (nd > exp10 + 1)
            {
                if (!appendChar(out, cap, pos, '.')) return false;
                for (int i = exp10 + 1; i < nd; i++)
                {
                    if (!appendChar(out, cap, pos, d[i])) return false;
                }
            }
        }
        return true;
    }

    // Scientific notation: d[.ddd]e+XX
    if (!appendChar(out, cap, pos, d[0])) return false;
    if (nd > 1)
    {
        if (!appendChar(out, cap, pos, '.')) return false;
        for (int i = 1; i < nd; i++)
        {
            if (!appendChar(out, cap, pos, d[i])) return false;
        }
    }
    char e[6];
    int ae = exp10 < 0 ? -exp10 : exp10;
    e[0] = 'e';
    e[1] = exp10 < 0 ? '-' : '+';
    if (ae >= 100)
    {
        e[2] = (char)('0' + ae / 100);
        e[3] = (char)('0' + (ae / 10) % 10);
        e[4] = (char)('0' + ae % 10);
        e[5] = '\0';
    }
    else
    {
        e[2] = (char)('0' + ae / 10);
        e[3] = (char)('0' + ae % 10);
        e[4] = '\0';
    }
    return append(out, cap, pos, e);
}

bool formatDeviceName(char *out, size_t cap, const DosimeterEeprom &record,
                      const char *fallback)
{
    const char *family = device_type_name(record.device_type);
    size_t pos = 0;
    if (cap == 0) return false;
    out[0] = '\0';
    if (family == 0) return append(out, cap, pos, fallback);

#ifdef __AVR__
    size_t n = strlen_P(family);
    if (n + 1 > cap) return false;
    strcpy_P(out, family);
#else
    size_t n = strlen(family);
    if (n + 1 > cap) return false;
    strcpy(out, family);
#endif
    pos = n;

    uint8_t version = record.hardware_version.device_version;
    if (!appendChar(out, cap, pos, (char)('0' + (version / 10) % 10))) return false;
    if (!appendChar(out, cap, pos, (char)('0' + version % 10))) return false;
    uint8_t rev = record.hardware_version.hardware_revision;
    if (rev >= 'A' && rev <= 'Z')
    {
        if (!appendChar(out, cap, pos, (char)rev)) return false;
    }
    return true;
}

void formatIdentifier(char *out, size_t cap, const DosimeterEeprom &record)
{
    size_t n = 0;
    for (size_t i = 0; i < sizeof(record.device_identifier) && n + 1 < cap; i++)
    {
        char c = record.device_identifier[i];
        if (c == '\0') break;
        out[n++] = (c >= 0x20 && c < 0x7F && c != ',') ? c : '_';
    }
    if (cap) out[n] = '\0';
}

}  // namespace xdos
