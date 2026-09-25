// Calendar -> Unix time (src/xdos_time.*).

#include <unity.h>

#include "xdos_time.h"

void setUp() {}
void tearDown() {}

static void test_epoch_and_known_dates()
{
    TEST_ASSERT_EQUAL_UINT32(0UL, xdos::unixTime(1970, 1, 1, 0, 0, 0));
    // PCF85263A reset default
    TEST_ASSERT_EQUAL_UINT32(946684800UL, xdos::unixTime(2000, 1, 1, 0, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(1789729200UL, xdos::unixTime(2026, 9, 18, 11, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(1789732352UL, xdos::unixTime(2026, 9, 18, 11, 52, 32));
}

static void test_leap_years()
{
    TEST_ASSERT_EQUAL_UINT32(1709164800UL, xdos::unixTime(2024, 2, 29, 0, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(1709251200UL, xdos::unixTime(2024, 3, 1, 0, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(951782400UL, xdos::unixTime(2000, 2, 29, 0, 0, 0));  // 2000 is leap
    TEST_ASSERT_EQUAL_UINT32(4107542400UL, xdos::unixTime(2100, 3, 1, 0, 0, 0));  // 2100 is not
}

static void test_end_of_day_and_year()
{
    TEST_ASSERT_EQUAL_UINT32(1798761599UL, xdos::unixTime(2026, 12, 31, 23, 59, 59));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_epoch_and_known_dates);
    RUN_TEST(test_leap_years);
    RUN_TEST(test_end_of_day_and_year);
    return UNITY_END();
}
