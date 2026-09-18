// Checks that the library builds on the PC and sees the EEPROM layout from
// xDOS-versions with the same byte offsets the devices and dosview use.

#include <stddef.h>
#include <string.h>

#include <unity.h>

#include "xdos.h"

void setUp() {}
void tearDown() {}

static void test_library_version_is_set()
{
    TEST_ASSERT_EQUAL_STRING("0.1.0", xdos::libraryVersion());
}

static void test_eeprom_record_size()
{
    TEST_ASSERT_EQUAL_UINT(113, xdos::kEepromRecordSize);
}

static void test_eeprom_record_is_packed()
{
    TEST_ASSERT_EQUAL_UINT(0, offsetof(DosimeterEeprom, format_version));
    TEST_ASSERT_EQUAL_UINT(4, offsetof(DosimeterEeprom, crc32));
    TEST_ASSERT_EQUAL_UINT(10, offsetof(DosimeterEeprom, device_identifier));
    TEST_ASSERT_EQUAL_UINT(37, offsetof(DosimeterEeprom, rtc_history));
    TEST_ASSERT_EQUAL_UINT(97, offsetof(DosimeterEeprom, calibration_constants));
    TEST_ASSERT_EQUAL_UINT(109, offsetof(DosimeterEeprom, calibration_version));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_library_version_is_set);
    RUN_TEST(test_eeprom_record_size);
    RUN_TEST(test_eeprom_record_is_packed);
    return UNITY_END();
}
