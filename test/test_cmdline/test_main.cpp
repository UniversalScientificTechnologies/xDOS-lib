// Command line protocol of the configuration bridge (src/xdos_cmdline.*).

#include <ctype.h>
#include <string.h>

#include <string>

#include <unity.h>

#include "xdos_cmdline.h"

using namespace xdos;

void setUp() {}
void tearDown() {}

// Frames a command body the way a host does: "!<body>*hh".
static std::string cmd(const char *body)
{
    char sum[3];
    uint8_t c = nmeaChecksum(body, body + strlen(body));
    const char *hex = "0123456789ABCDEF";
    sum[0] = hex[c >> 4];
    sum[1] = hex[c & 0x0F];
    sum[2] = '\0';
    return std::string("!") + body + "*" + sum;
}

static CmdError parse(const std::string &line, Command &out)
{
    return parseCommand(line.c_str(), out);
}

// ---- checksum ----

static void test_checksum_matches_nmea_reference()
{
    // Classic NMEA 0183 example sentence with checksum 27.
    const char *s = "GPGLL,5057.970,N,00146.110,E,142451,A";
    TEST_ASSERT_EQUAL_HEX8(0x27, nmeaChecksum(s, s + strlen(s)));
}

// ---- LineReader ----

static bool feedAll(LineReader &r, const char *s)
{
    bool done = false;
    for (const char *p = s; *p; p++) done = r.feed(*p);
    return done;
}

static void test_reader_lf_and_crlf()
{
    LineReader r;
    TEST_ASSERT_TRUE(feedAll(r, "abc\n"));
    TEST_ASSERT_EQUAL_STRING("abc", r.line());
    TEST_ASSERT_TRUE(feedAll(r, "de\r\n"));
    TEST_ASSERT_EQUAL_STRING("de", r.line());
}

static void test_reader_partial_line_waits()
{
    LineReader r;
    TEST_ASSERT_FALSE(feedAll(r, "!I2CR,5B"));
    TEST_ASSERT_TRUE(feedAll(r, ",10\n"));
    TEST_ASSERT_EQUAL_STRING("!I2CR,5B,10", r.line());
}

static void test_reader_drops_overlong_line_whole()
{
    LineReader r;
    std::string longLine(kCmdLineMax + 10, 'x');
    TEST_ASSERT_FALSE(feedAll(r, longLine.c_str()));
    TEST_ASSERT_FALSE(r.feed('\n'));  // the overlong line is not handed out
    TEST_ASSERT_TRUE(feedAll(r, "ok\n"));
    TEST_ASSERT_EQUAL_STRING("ok", r.line());
}

static void test_reader_accepts_max_length()
{
    LineReader r;
    std::string line(kCmdLineMax, 'y');
    TEST_ASSERT_TRUE(feedAll(r, (line + "\r\n").c_str()));
    TEST_ASSERT_EQUAL_UINT(kCmdLineMax, strlen(r.line()));
}

// ---- parseCommand ----

static void test_parse_config()
{
    Command c;
    TEST_ASSERT_EQUAL(CmdError::Ok, parse(cmd("CONFIG,XDOS"), c));
    TEST_ASSERT_EQUAL(CmdType::Config, c.type);
    TEST_ASSERT_EQUAL(CmdError::Token, parse(cmd("CONFIG,xdos"), c));
    TEST_ASSERT_EQUAL(CmdError::Syntax, parse(cmd("CONFIG"), c));
}

static void test_parse_exit()
{
    Command c;
    TEST_ASSERT_EQUAL(CmdError::Ok, parse(cmd("EXIT"), c));
    TEST_ASSERT_EQUAL(CmdType::Exit, c.type);
    TEST_ASSERT_EQUAL(CmdError::Syntax, parse(cmd("EXIT,1"), c));
}

static void test_parse_not_a_command()
{
    Command c;
    TEST_ASSERT_EQUAL(CmdError::NotCommand, parseCommand("$STOP,1,2,3", c));
    TEST_ASSERT_EQUAL(CmdError::NotCommand, parseCommand("", c));
    TEST_ASSERT_EQUAL(CmdError::NotCommand, parseCommand("\x01\xff!", c));
}

static void test_parse_checksum_errors()
{
    Command c;
    TEST_ASSERT_EQUAL(CmdError::Checksum, parseCommand("!EXIT", c));
    // XOR of "EXIT" happens to be 0x00, so *00 would be valid.
    TEST_ASSERT_EQUAL(CmdError::Checksum, parseCommand("!EXIT*01", c));
    TEST_ASSERT_EQUAL(CmdError::Checksum, parseCommand("!EXIT*G1", c));
    TEST_ASSERT_EQUAL(CmdError::Checksum, parseCommand("!", c));
}

static void test_parse_checksum_lower_case()
{
    Command c;
    std::string line = cmd("I2CR,5B,10");  // upper case digits
    for (size_t i = line.size() - 2; i < line.size(); i++)
    {
        line[i] = (char)tolower(line[i]);
    }
    TEST_ASSERT_EQUAL(CmdError::Ok, parse(line, c));
}

static void test_parse_i2c_write()
{
    Command c;
    TEST_ASSERT_EQUAL(CmdError::Ok, parse(cmd("I2CW,53,0000AaBb"), c));
    TEST_ASSERT_EQUAL(CmdType::I2cWrite, c.type);
    TEST_ASSERT_EQUAL_HEX8(0x53, c.addr);
    TEST_ASSERT_EQUAL_UINT8(4, c.dataLen);
    const uint8_t expected[] = {0x00, 0x00, 0xAA, 0xBB};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, c.data, 4);
}

static void test_parse_i2c_write_empty_is_probe()
{
    Command c;
    TEST_ASSERT_EQUAL(CmdError::Ok, parse(cmd("I2CW,51,"), c));
    TEST_ASSERT_EQUAL_UINT8(0, c.dataLen);
}

static void test_parse_i2c_read()
{
    Command c;
    TEST_ASSERT_EQUAL(CmdError::Ok, parse(cmd("I2CR,5B,10"), c));
    TEST_ASSERT_EQUAL(CmdType::I2cRead, c.type);
    TEST_ASSERT_EQUAL_HEX8(0x5B, c.addr);
    TEST_ASSERT_EQUAL_UINT8(16, c.readLen);  // lengths are hex
}

static void test_parse_i2c_write_read()
{
    Command c;
    TEST_ASSERT_EQUAL(CmdError::Ok, parse(cmd("I2CWR,5B,0800,10"), c));
    TEST_ASSERT_EQUAL(CmdType::I2cWriteRead, c.type);
    TEST_ASSERT_EQUAL_UINT8(2, c.dataLen);
    TEST_ASSERT_EQUAL_HEX8(0x08, c.data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, c.data[1]);
    TEST_ASSERT_EQUAL_UINT8(16, c.readLen);
    TEST_ASSERT_EQUAL(CmdError::Length, parse(cmd("I2CWR,5B,,10"), c));
}

static void test_parse_limits()
{
    Command c;
    std::string max(2 * kI2cMaxBytes, 'A');
    std::string over(2 * kI2cMaxBytes + 2, 'A');
    TEST_ASSERT_EQUAL(CmdError::Ok, parse(cmd(("I2CW,50," + max).c_str()), c));
    TEST_ASSERT_EQUAL_UINT8(kI2cMaxBytes, c.dataLen);
    TEST_ASSERT_EQUAL(CmdError::Length, parse(cmd(("I2CW,50," + over).c_str()), c));
    TEST_ASSERT_EQUAL(CmdError::Ok, parse(cmd("I2CR,50,20"), c));
    TEST_ASSERT_EQUAL(CmdError::Length, parse(cmd("I2CR,50,21"), c));
    TEST_ASSERT_EQUAL(CmdError::Length, parse(cmd("I2CR,50,0"), c));
    TEST_ASSERT_EQUAL(CmdError::Ok, parse(cmd("I2CR,7F,1"), c));
    TEST_ASSERT_EQUAL(CmdError::Syntax, parse(cmd("I2CR,80,1"), c));
}

static void test_parse_syntax_errors()
{
    Command c;
    TEST_ASSERT_EQUAL(CmdError::Syntax, parse(cmd("I2CW,53,ABC"), c));   // odd digits
    TEST_ASSERT_EQUAL(CmdError::Syntax, parse(cmd("I2CW,53,00XY"), c));  // not hex
    TEST_ASSERT_EQUAL(CmdError::Syntax, parse(cmd("I2CW,153,00"), c));   // 3 digit addr
    TEST_ASSERT_EQUAL(CmdError::Syntax, parse(cmd("I2CR,5B"), c));
    TEST_ASSERT_EQUAL(CmdError::Syntax, parse(cmd("I2CR,5B,10,1"), c));
    TEST_ASSERT_EQUAL(CmdError::Syntax, parse(cmd("I2CR,5B,1,2,3,4"), c));
    TEST_ASSERT_EQUAL(CmdError::Unknown, parse(cmd("RESET"), c));
    TEST_ASSERT_EQUAL(CmdError::Unknown, parse(cmd(""), c));
}

// ---- output helpers ----

static void test_format_hex()
{
    const uint8_t data[] = {0x09, 0x10, 0xAB, 0xFF};
    char out[9];
    TEST_ASSERT_TRUE(formatHex(out, sizeof(out), data, 4));
    TEST_ASSERT_EQUAL_STRING("0910ABFF", out);
    TEST_ASSERT_FALSE(formatHex(out, 8, data, 4));
}

static void test_frame_response()
{
    char out[32];
    size_t n = frameResponse(out, sizeof(out), "I2C_OK");
    TEST_ASSERT_EQUAL_UINT(strlen(out), n);
    std::string expected = "#I2C_OK*" + cmd("I2C_OK").substr(8) + "\r\n";
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), out);
    TEST_ASSERT_EQUAL_UINT(0, frameResponse(out, 12, "I2C_OK"));
}

static void test_error_names()
{
    TEST_ASSERT_EQUAL_STRING("CHECKSUM", cmdErrorName(CmdError::Checksum));
    TEST_ASSERT_EQUAL_STRING("NACK_ADDR", i2cErrorName(2));
    TEST_ASSERT_EQUAL_STRING("NACK_DATA", i2cErrorName(3));
    TEST_ASSERT_EQUAL_STRING("BUS", i2cErrorName(4));
    TEST_ASSERT_EQUAL_STRING("BUS", i2cErrorName(99));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_checksum_matches_nmea_reference);
    RUN_TEST(test_reader_lf_and_crlf);
    RUN_TEST(test_reader_partial_line_waits);
    RUN_TEST(test_reader_drops_overlong_line_whole);
    RUN_TEST(test_reader_accepts_max_length);
    RUN_TEST(test_parse_config);
    RUN_TEST(test_parse_exit);
    RUN_TEST(test_parse_not_a_command);
    RUN_TEST(test_parse_checksum_errors);
    RUN_TEST(test_parse_checksum_lower_case);
    RUN_TEST(test_parse_i2c_write);
    RUN_TEST(test_parse_i2c_write_empty_is_probe);
    RUN_TEST(test_parse_i2c_read);
    RUN_TEST(test_parse_i2c_write_read);
    RUN_TEST(test_parse_limits);
    RUN_TEST(test_parse_syntax_errors);
    RUN_TEST(test_format_hex);
    RUN_TEST(test_frame_response);
    RUN_TEST(test_error_names);
    return UNITY_END();
}
