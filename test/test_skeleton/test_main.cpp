// Checks that the library builds on the PC and sees the EEPROM layout from
// xDOS-versions with the same byte offsets and record sizes the devices and
// dosview use.

#include <stddef.h>
#include <string.h>

#include <unity.h>

#include "xdos.h"

void setUp() {}
void tearDown() {}

static void test_library_version_is_set()
{
    TEST_ASSERT_EQUAL_STRING("0.3.0", xdos::libraryVersion());
}

static void test_eeprom_record_size()
{
    // The struct is the latest format_version; each version has its own size.
    TEST_ASSERT_EQUAL_UINT(EEPROM_RECORD_SIZE_V2, xdos::kEepromRecordSize);
    TEST_ASSERT_EQUAL_UINT(113, EEPROM_RECORD_SIZE_V1);
    TEST_ASSERT_EQUAL_UINT(131, EEPROM_RECORD_SIZE_V2);
    TEST_ASSERT_EQUAL_UINT(113, eeprom_record_size(0));
    TEST_ASSERT_EQUAL_UINT(0, eeprom_record_size(0xFFFF));
}

static void test_eeprom_record_is_packed()
{
    TEST_ASSERT_EQUAL_UINT(0, offsetof(DosimeterEeprom, format_version));
    TEST_ASSERT_EQUAL_UINT(4, offsetof(DosimeterEeprom, crc32));
    TEST_ASSERT_EQUAL_UINT(10, offsetof(DosimeterEeprom, device_identifier));
    TEST_ASSERT_EQUAL_UINT(37, offsetof(DosimeterEeprom, rtc_history));
    TEST_ASSERT_EQUAL_UINT(97, offsetof(DosimeterEeprom, calibration_constants));
    TEST_ASSERT_EQUAL_UINT(109, offsetof(DosimeterEeprom, calibration_version));
    TEST_ASSERT_EQUAL_UINT(113, offsetof(DosimeterEeprom, num_noise_channels_default));
    TEST_ASSERT_EQUAL_UINT(127, offsetof(DosimeterEeprom, energy_range_max_mev));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_library_version_is_set);
    RUN_TEST(test_eeprom_record_size);
    RUN_TEST(test_eeprom_record_is_packed);
    return UNITY_END();
}
