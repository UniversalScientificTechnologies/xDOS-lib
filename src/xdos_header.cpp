#include "xdos_header.h"

#include <math.h>
#include <string.h>

#include "xdos_text.h"

// Text constants stay in flash on AVR (the header's literals would otherwise
// take a few hundred bytes of RAM).
#ifdef __AVR__
#include <avr/pgmspace.h>
#define XP(s) PSTR(s)
#else
#define XP(s) (s)
#define strlen_P strlen
#define memcpy_P memcpy
#endif

namespace xdos {

namespace {

const uint16_t kUnknownU16 = 0xFFFF;
const float kDefaultTickS = 128e-6f;

// Small bounded string builder for one header line.
class Line
{
public:
    Line() : len_(0) { buf_[0] = '\0'; }

    Line &add(const char *text)
    {
        size_t n = strlen(text);
        if (len_ + n >= sizeof(buf_)) n = sizeof(buf_) - 1 - len_;
        memcpy(buf_ + len_, text, n);
        len_ += n;
        buf_[len_] = '\0';
        return *this;
    }

    // text in flash (XP("..."))
    Line &addP(const char *text)
    {
        size_t n = strlen_P(text);
        if (len_ + n >= sizeof(buf_)) n = sizeof(buf_) - 1 - len_;
        memcpy_P(buf_ + len_, text, n);
        len_ += n;
        buf_[len_] = '\0';
        return *this;
    }

    Line &add(uint32_t value)
    {
        char tmp[11];
        int i = 10;
        tmp[i] = '\0';
        do
        {
            tmp[--i] = (char)('0' + value % 10);
            value /= 10;
        } while (value && i > 0);
        return add(tmp + i);
    }

    Line &addFloat(float value)
    {
        char tmp[20];
        formatFloat(tmp, sizeof(tmp), value);
        return add(tmp);
    }

    Line &addHexByte(uint8_t b)
    {
        const char *hex = "0123456789abcdef";
        char tmp[3] = {hex[b >> 4], hex[b & 0x0F], '\0'};
        return add(tmp);
    }

    const char *text() const { return buf_; }

private:
    char buf_[160];
    size_t len_;
};

}  // namespace

void writeHeader(LineSink &out, const HeaderInfo &info)
{
    const bool trusted = recordTrusted(info.status);
    const DosimeterEeprom &r = *info.record;

    {
        Line l;
        l.addP(XP("$DATAFORMAT,VERSION_2.1"));
        out.line(l.text());
    }

    char name[24];
    if (trusted)
    {
        formatDeviceName(name, sizeof(name), r, info.fallbackType);
    }
    else
    {
        strncpy(name, info.fallbackType, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }

    {
        Line l;
        l.addP(XP("$DOS,")).add(name).addP(XP(",")).add(info.fwVersion).addP(XP(",0,"))
            .add(info.gitHashAndBuild).addP(XP(",")).add(info.serial);
        out.line(l.text());
    }
    {
        // The analog board carries the device identity: the same name as
        // $DOS; the configuration field is the first two record bytes.
        Line l;
        l.addP(XP("$ADC,")).add(name).addP(XP(",")).add(info.serial).addP(XP(","))
            .addHexByte(info.rawFirstBytes[0]).addHexByte(info.rawFirstBytes[1]);
        out.line(l.text());
    }

    if (trusted)
    {
        char id[25];
        formatIdentifier(id, sizeof(id), r);
        Line l;
        l.addP(XP("$ADC_NAME,")).add(id);
        out.line(l.text());
    }

    if (info.hasBattery)
    {
        Line l;
        l.addP(XP("$BATP,")).addP(info.batteryPresent ? XP("1") : XP("0")).addP(XP(",")).add(info.batteryMv);
        out.line(l.text());
    }

    if (trusted)
    {
        Line l;
        l.addP(XP("$CALIB,")).addFloat(r.calibration_constants[0]).addP(XP(","))
            .addFloat(r.calibration_constants[1]).addP(XP(","))
            .addFloat(r.calibration_constants[2]).addP(XP(",")).add(r.calibration_version);
        out.line(l.text());
    }

    // Measurement metadata - format_version 2.
    const bool metadata = info.status == RecordStatus::Ok;
    if (metadata && r.num_noise_channels_default != kUnknownU16)
    {
        Line l;
        l.addP(XP("$CHAN,")).add(info.numChannels).addP(XP(",")).add(r.num_noise_channels_default);
        out.line(l.text());
    }
    if (metadata && !isnan(r.si_chip_area_cm2) && !isnan(r.si_chip_thickness_cm))
    {
        Line l;
        l.addP(XP("$DIODE,")).addFloat(r.si_chip_area_cm2).addP(XP(","))
            .addFloat(r.si_chip_thickness_cm);
        out.line(l.text());
    }
    if (metadata && (!isnan(r.energy_range_min_mev) || !isnan(r.energy_range_max_mev)))
    {
        // An unknown bound stays empty.
        Line l;
        l.addP(XP("$ERNG,"));
        if (!isnan(r.energy_range_min_mev)) l.addFloat(r.energy_range_min_mev);
        l.addP(XP(","));
        if (!isnan(r.energy_range_max_mev)) l.addFloat(r.energy_range_max_mev);
        out.line(l.text());
    }

    {
        Line l;
        l.addP(XP("$ITIME,")).addFloat(info.integrationPeriodS);
        out.line(l.text());
    }
    if (fabsf(info.tickS - kDefaultTickS) > kDefaultTickS * 1e-4f)
    {
        Line l;
        l.addP(XP("$TICK,")).addFloat(info.tickS);
        out.line(l.text());
    }

    // Why the record could not be used (fully).
    Line e;
    switch (info.status)
    {
    case RecordStatus::Ok:
        return;
    case RecordStatus::OldVersion:
        e.addP(XP("$ERROR,EEPROM record version ")).add(info.version)
            .addP(XP(", firmware expects ")).add((uint32_t)EEPROM_FORMAT_VERSION_LATEST)
            .addP(XP(" - measurement metadata not available"));
        break;
    case RecordStatus::Blank:
        e.addP(XP("$ERROR,EEPROM record missing - device not configured"));
        break;
    case RecordStatus::UnknownVersion:
        e.addP(XP("$ERROR,EEPROM record version ")).add(info.version)
            .addP(XP(" unknown, firmware expects ")).add((uint32_t)EEPROM_FORMAT_VERSION_LATEST)
            .addP(XP(" - record ignored"));
        break;
    case RecordStatus::BadCrc:
        e.addP(XP("$ERROR,EEPROM record CRC mismatch - record ignored"));
        break;
    case RecordStatus::NotRead:
        e.addP(XP("$ERROR,EEPROM not readable - record ignored"));
        break;
    }
    out.line(e.text());
}

}  // namespace xdos
