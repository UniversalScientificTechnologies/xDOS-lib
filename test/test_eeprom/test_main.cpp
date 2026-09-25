// EEPROM record, text formatting and the VERSION_2.1 header
// (src/xdos_eeprom.*, src/xdos_text.*, src/xdos_header.*).

#include <math.h>
#include <string.h>

#include <string>
#include <vector>

#include <unity.h>

#include "xdos_eeprom.h"
#include "xdos_header.h"
#include "xdos_text.h"

using namespace xdos;

void setUp() {}
void tearDown() {}

// Records packed by dosview (eeprom_schema.pack_record) - the firmware must
// accept exactly what dosview writes.
// format_version 2: SPACEDOS 04, "EM2", calibration 0.5/0.125/1.5e-5, sync at
// 1789732352, 4 noise channels, chip 0.25 cm2 x 0.03 cm, energy 0.05..? MeV.
static const char kV2Hex[] =
    "0200040027a1fbe50400454d3200000000000000000000000000000000000000"
    "00000000050026ad6a000000000026ad6a000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "000000003f0000003e82a87b37007fac6a04000000803e8fc2f53ccdcc4c3d00"
    "00c07f";
// format_version 1: AIRDOS 04C, identifier "OT,TER".
static const char kV1Hex[] =
    "010001002fb9710f04434f542c54455200000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000803f0000000007000000";

// Bytes of a hex record, padded with 0xFF (erased EEPROM) to the full struct.
static std::vector<uint8_t> eeprom(const char *hex)
{
    std::vector<uint8_t> out(sizeof(DosimeterEeprom), 0xFF);
    for (size_t i = 0; hex[2 * i] && i < out.size(); i++)
    {
        unsigned v;
        sscanf(hex + 2 * i, "%2x", &v);
        out[i] = (uint8_t)v;
    }
    return out;
}

// ---- CRC and record check ----

static void test_crc_matches_dosview()
{
    const uint8_t text[] = "123456789";
    // zlib.crc32(b"123456789", 0xFFFFFFFF) ^ 0xFFFFFFFF, the CRC field outside
    TEST_ASSERT_EQUAL_HEX32(0x2DFD2D88, recordCrc32(text, 9, 100));
}

static void test_check_v2_record_from_dosview()
{
    std::vector<uint8_t> raw = eeprom(kV2Hex);
    DosimeterEeprom r;
    uint16_t version;
    TEST_ASSERT_EQUAL(RecordStatus::Ok, checkRecord(raw.data(), raw.size(), r, version));
    TEST_ASSERT_EQUAL_UINT16(2, version);
    TEST_ASSERT_EQUAL_UINT16(4, r.num_noise_channels_default);
    TEST_ASSERT_EQUAL_FLOAT(0.25f, r.si_chip_area_cm2);
    TEST_ASSERT_TRUE(isnan(r.energy_range_max_mev));
}

static void test_check_v1_record_from_dosview()
{
    std::vector<uint8_t> raw = eeprom(kV1Hex);
    raw[120] = 0x12;  // garbage after the 113 B record must not leak in
    DosimeterEeprom r;
    uint16_t version;
    TEST_ASSERT_EQUAL(RecordStatus::OldVersion, checkRecord(raw.data(), raw.size(), r, version));
    TEST_ASSERT_EQUAL_UINT16(1, version);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, r.num_noise_channels_default);
    TEST_ASSERT_TRUE(isnan(r.si_chip_area_cm2));
}

static void test_check_bad_records()
{
    DosimeterEeprom r;
    uint16_t version;
    std::vector<uint8_t> blank(sizeof(DosimeterEeprom), 0xFF);
    TEST_ASSERT_EQUAL(RecordStatus::Blank, checkRecord(blank.data(), blank.size(), r, version));

    std::vector<uint8_t> bad = eeprom(kV2Hex);
    bad[10] ^= 1;
    TEST_ASSERT_EQUAL(RecordStatus::BadCrc, checkRecord(bad.data(), bad.size(), r, version));

    std::vector<uint8_t> future = eeprom(kV2Hex);
    future[0] = 3;
    TEST_ASSERT_EQUAL(RecordStatus::UnknownVersion,
                      checkRecord(future.data(), future.size(), r, version));
    TEST_ASSERT_EQUAL_UINT16(3, version);

    TEST_ASSERT_EQUAL(RecordStatus::NotRead, checkRecord(bad.data(), 50, r, version));
}

static void test_sync_age()
{
    std::vector<uint8_t> raw = eeprom(kV2Hex);
    DosimeterEeprom r;
    uint16_t version;
    checkRecord(raw.data(), raw.size(), r, version);
    uint32_t age = 0;
    TEST_ASSERT_TRUE(syncAge(r, 1789732352UL + 3600, age));
    TEST_ASSERT_EQUAL_UINT32(3600, age);
    TEST_ASSERT_FALSE(syncAge(r, 946685100UL, age));  // RTC lost its time
    r.rtc_history[0].rtc_value_at_reference_timestamp = 0;
    TEST_ASSERT_FALSE(syncAge(r, 1789732352UL, age));  // no sync record
}

// ---- text ----

static std::string f(float v)
{
    char buf[20];
    TEST_ASSERT_TRUE(formatFloat(buf, sizeof(buf), v));
    return buf;
}

static void test_format_float_like_printf_g()
{
    TEST_ASSERT_EQUAL_STRING("0", f(0.0f).c_str());
    TEST_ASSERT_EQUAL_STRING("0.25", f(0.25f).c_str());
    TEST_ASSERT_EQUAL_STRING("0.03", f(0.03f).c_str());
    TEST_ASSERT_EQUAL_STRING("20", f(20.0f).c_str());
    TEST_ASSERT_EQUAL_STRING("8", f(8.0f).c_str());
    TEST_ASSERT_EQUAL_STRING("-0.5", f(-0.5f).c_str());
    TEST_ASSERT_EQUAL_STRING("123456", f(123456.0f).c_str());
    TEST_ASSERT_EQUAL_STRING("1.23457e+06", f(1234567.0f).c_str());
    TEST_ASSERT_EQUAL_STRING("0.0001", f(0.0001f).c_str());
    TEST_ASSERT_EQUAL_STRING("1.5e-05", f(1.5e-5f).c_str());
    TEST_ASSERT_EQUAL_STRING("0.000128", f(128e-6f).c_str());
    TEST_ASSERT_EQUAL_STRING("NaN", f(NAN).c_str());
    char tiny[4];
    TEST_ASSERT_FALSE(formatFloat(tiny, sizeof(tiny), 123456.0f));
}

static void test_device_name_and_identifier()
{
    char name[24];
    DosimeterEeprom r;
    uint16_t version;
    std::vector<uint8_t> v2 = eeprom(kV2Hex);
    checkRecord(v2.data(), v2.size(), r, version);
    TEST_ASSERT_TRUE(formatDeviceName(name, sizeof(name), r, "FALLBACK"));
    TEST_ASSERT_EQUAL_STRING("SPACEDOS04", name);

    std::vector<uint8_t> v1 = eeprom(kV1Hex);
    checkRecord(v1.data(), v1.size(), r, version);
    formatDeviceName(name, sizeof(name), r, "FALLBACK");
    TEST_ASSERT_EQUAL_STRING("AIRDOS04C", name);
    char id[25];
    formatIdentifier(id, sizeof(id), r);
    TEST_ASSERT_EQUAL_STRING("OT_TER", id);  // no commas in a field

    r.device_type = 555;
    formatDeviceName(name, sizeof(name), r, "FALLBACK");
    TEST_ASSERT_EQUAL_STRING("FALLBACK", name);
}

// ---- header ----

class Collect : public LineSink
{
public:
    void line(const char *text) override { lines.push_back(text); }
    std::string all() const
    {
        std::string s;
        for (size_t i = 0; i < lines.size(); i++) s += lines[i] + "\n";
        return s;
    }
    std::vector<std::string> lines;
};

static std::string header(const char *hex, uint32_t numChannels = 65536,
                          float tick = 128e-6f)
{
    static DosimeterEeprom r;
    std::vector<uint8_t> raw = hex ? eeprom(hex)
                                   : std::vector<uint8_t>(sizeof(DosimeterEeprom), 0xFF);
    HeaderInfo info;
    info.record = &r;
    info.status = checkRecord(raw.data(), raw.size(), r, info.version);
    info.rawFirstBytes[0] = raw[0];
    info.rawFirstBytes[1] = raw[1];
    info.serial = "0910410874100851840BA080A08000E8";
    info.fallbackType = "SPACEDOS04";
    info.fwVersion = "2.0.0-0-User";
    info.gitHashAndBuild = "f16589a140b2ff2b34fcf55651f97c4bba5d0252-dirty,User";
    info.hasBattery = true;
    info.batteryPresent = true;
    info.batteryMv = 3311;
    info.numChannels = numChannels;
    info.integrationPeriodS = 8.0f;
    info.tickS = tick;
    Collect c;
    writeHeader(c, info);
    return c.all();
}

static void test_header_v2()
{
    TEST_ASSERT_EQUAL_STRING(
        "$DATAFORMAT,VERSION_2.1\n"
        "$DOS,SPACEDOS04,2.0.0-0-User,0,f16589a140b2ff2b34fcf55651f97c4bba5d0252-dirty,User,"
        "0910410874100851840BA080A08000E8\n"
        "$ADC,SPACEDOS04,0910410874100851840BA080A08000E8,0200\n"
        "$ADC_NAME,EM2\n"
        "$BATP,1,3311\n"
        "$CALIB,0.5,0.125,1.5e-05,1789689600\n"
        "$CHAN,65536,4\n"
        "$DIODE,0.25,0.03\n"
        "$ERNG,0.05,\n"
        "$ITIME,8\n",
        header(kV2Hex).c_str());
}

static void test_header_v1_has_no_metadata()
{
    TEST_ASSERT_EQUAL_STRING(
        "$DATAFORMAT,VERSION_2.1\n"
        "$DOS,AIRDOS04C,2.0.0-0-User,0,f16589a140b2ff2b34fcf55651f97c4bba5d0252-dirty,User,"
        "0910410874100851840BA080A08000E8\n"
        "$ADC,AIRDOS04C,0910410874100851840BA080A08000E8,0100\n"
        "$ADC_NAME,OT_TER\n"
        "$BATP,1,3311\n"
        "$CALIB,0,1,0,7\n"
        "$ITIME,8\n"
        "$ERROR,EEPROM record version 1, firmware expects 2 - measurement metadata not available\n",
        header(kV1Hex).c_str());
}

static void test_header_blank_and_broken()
{
    std::string blank = header(0);
    TEST_ASSERT_TRUE(blank.find("$DOS,SPACEDOS04,") != std::string::npos);
    TEST_ASSERT_TRUE(blank.find("$ADC,SPACEDOS04,0910410874100851840BA080A08000E8,ffff\n")
                     != std::string::npos);
    TEST_ASSERT_TRUE(blank.find("$CALIB") == std::string::npos);
    TEST_ASSERT_TRUE(blank.find("$ADC_NAME") == std::string::npos);
    TEST_ASSERT_TRUE(blank.find("$ERROR,EEPROM record missing - device not configured\n")
                     != std::string::npos);

    std::string hex(kV2Hex);
    hex[20] = hex[20] == '0' ? '1' : '0';  // corrupt the identifier
    std::string broken = header(hex.c_str());
    TEST_ASSERT_TRUE(broken.find("$CALIB") == std::string::npos);
    TEST_ASSERT_TRUE(broken.find("$ERROR,EEPROM record CRC mismatch - record ignored\n")
                     != std::string::npos);
}

static void test_header_tick_only_when_not_default()
{
    TEST_ASSERT_TRUE(header(kV2Hex).find("$TICK") == std::string::npos);
    TEST_ASSERT_TRUE(header(kV2Hex, 65536, 64e-6f).find("$TICK,6.4e-05\n") != std::string::npos);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_matches_dosview);
    RUN_TEST(test_check_v2_record_from_dosview);
    RUN_TEST(test_check_v1_record_from_dosview);
    RUN_TEST(test_check_bad_records);
    RUN_TEST(test_sync_age);
    RUN_TEST(test_format_float_like_printf_g);
    RUN_TEST(test_device_name_and_identifier);
    RUN_TEST(test_header_v2);
    RUN_TEST(test_header_v1_has_no_metadata);
    RUN_TEST(test_header_blank_and_broken);
    RUN_TEST(test_header_tick_only_when_not_default);
    return UNITY_END();
}
