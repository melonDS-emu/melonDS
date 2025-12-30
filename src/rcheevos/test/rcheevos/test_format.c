#include "rc_internal.h"

#include "../test_framework.h"

static void test_format_value(int format, int value, const char* expected) {
  char buffer[64];
  int result;

  result = rc_format_value(buffer, sizeof(buffer), value, format);
  ASSERT_STR_EQUALS(buffer, expected);
  ASSERT_NUM_EQUALS(result, strlen(expected));
}

static void test_parse_format(const char* format, int expected) {
  ASSERT_NUM_EQUALS(rc_parse_format(format), expected);
}

void test_format(void) {
  TEST_SUITE_BEGIN();

  /* rc_format_value */
  TEST_PARAMS3(test_format_value, RC_FORMAT_VALUE, 12345, "12,345");
  TEST_PARAMS3(test_format_value, RC_FORMAT_VALUE, -12345, "-12,345");
  TEST_PARAMS3(test_format_value, RC_FORMAT_VALUE, 0xFFFFFFFF, "-1");
  TEST_PARAMS3(test_format_value, RC_FORMAT_UNSIGNED_VALUE, 0xFFFFFFFF, "4,294,967,295");
  TEST_PARAMS3(test_format_value, RC_FORMAT_UNFORMATTED, 0xFFFFFFFF, "4294967295"); /* UNFORMATTED does not add commas */
  TEST_PARAMS3(test_format_value, RC_FORMAT_SCORE, 12345, "012345"); /* SCORE does not add commas */
  TEST_PARAMS3(test_format_value, RC_FORMAT_SECONDS, 45, "0:45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_SECONDS, 345, "5:45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_SECONDS, 12345, "3h25:45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_CENTISECS, 345, "0:03.45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_CENTISECS, 12345, "2:03.45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_CENTISECS, 1234567, "3h25:45.67");
  TEST_PARAMS3(test_format_value, RC_FORMAT_SECONDS_AS_MINUTES, 45, "0h00");
  TEST_PARAMS3(test_format_value, RC_FORMAT_SECONDS_AS_MINUTES, 345, "0h05");
  TEST_PARAMS3(test_format_value, RC_FORMAT_SECONDS_AS_MINUTES, 12345, "3h25");
  TEST_PARAMS3(test_format_value, RC_FORMAT_MINUTES, 45, "0h45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_MINUTES, 345, "5h45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_MINUTES, 12345, "205h45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FRAMES, 345, "0:05.75");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FRAMES, 12345, "3:25.75");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FRAMES, 1234567, "5h42:56.11");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FIXED1, 0, "0.0");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FIXED1, 1, "0.1");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FIXED1, 1234, "123.4");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FIXED1, -1234, "-123.4");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FIXED2, 0, "0.00");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FIXED2, 1, "0.01");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FIXED2, 1234, "12.34");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FIXED2, -1234, "-12.34");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FIXED3, 0, "0.000");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FIXED3, 1, "0.001");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FIXED3, 1234, "1.234");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FIXED3, -1234, "-1.234");
  TEST_PARAMS3(test_format_value, RC_FORMAT_TENS, 0, "0");
  TEST_PARAMS3(test_format_value, RC_FORMAT_TENS, 1, "10");
  TEST_PARAMS3(test_format_value, RC_FORMAT_TENS, 1234, "12,340");
  TEST_PARAMS3(test_format_value, RC_FORMAT_TENS, -1234, "-12,340");
  TEST_PARAMS3(test_format_value, RC_FORMAT_HUNDREDS, 0, "0");
  TEST_PARAMS3(test_format_value, RC_FORMAT_HUNDREDS, 1, "100");
  TEST_PARAMS3(test_format_value, RC_FORMAT_HUNDREDS, 1234, "123,400");
  TEST_PARAMS3(test_format_value, RC_FORMAT_HUNDREDS, -1234, "-123,400");
  TEST_PARAMS3(test_format_value, RC_FORMAT_THOUSANDS, 0, "0");
  TEST_PARAMS3(test_format_value, RC_FORMAT_THOUSANDS, 1, "1,000");
  TEST_PARAMS3(test_format_value, RC_FORMAT_THOUSANDS, 1234, "1,234,000");
  TEST_PARAMS3(test_format_value, RC_FORMAT_THOUSANDS, -1234, "-1,234,000");

  /* because of the internal conversion to centiseconds, anything above MAX_INT / 10 could overflow */
  TEST_PARAMS3(test_format_value, RC_FORMAT_FRAMES, 0x19999999, "1,988h24:38.81");

  /* rc_parse_format */
  TEST_PARAMS2(test_parse_format, "VALUE", RC_FORMAT_VALUE);
  TEST_PARAMS2(test_parse_format, "SECS", RC_FORMAT_SECONDS);
  TEST_PARAMS2(test_parse_format, "TIMESECS", RC_FORMAT_SECONDS);
  TEST_PARAMS2(test_parse_format, "TIME", RC_FORMAT_FRAMES);
  TEST_PARAMS2(test_parse_format, "MINUTES", RC_FORMAT_MINUTES);
  TEST_PARAMS2(test_parse_format, "SECS_AS_MINS", RC_FORMAT_SECONDS_AS_MINUTES);
  TEST_PARAMS2(test_parse_format, "FRAMES", RC_FORMAT_FRAMES);
  TEST_PARAMS2(test_parse_format, "SCORE", RC_FORMAT_SCORE);
  TEST_PARAMS2(test_parse_format, "POINTS", RC_FORMAT_SCORE);
  TEST_PARAMS2(test_parse_format, "MILLISECS", RC_FORMAT_CENTISECS);
  TEST_PARAMS2(test_parse_format, "TENS", RC_FORMAT_TENS);
  TEST_PARAMS2(test_parse_format, "HUNDREDS", RC_FORMAT_HUNDREDS);
  TEST_PARAMS2(test_parse_format, "THOUSANDS", RC_FORMAT_THOUSANDS);
  TEST_PARAMS2(test_parse_format, "UNSIGNED", RC_FORMAT_UNSIGNED_VALUE);
  TEST_PARAMS2(test_parse_format, "OTHER", RC_FORMAT_SCORE);
  TEST_PARAMS2(test_parse_format, "INVALID", RC_FORMAT_VALUE);

  TEST_PARAMS2(test_parse_format, "FLOAT", RC_FORMAT_VALUE);
  TEST_PARAMS2(test_parse_format, "FLOAT0", RC_FORMAT_VALUE);
  TEST_PARAMS2(test_parse_format, "FLOAT1", RC_FORMAT_FLOAT1);
  TEST_PARAMS2(test_parse_format, "FLOAT2", RC_FORMAT_FLOAT2);
  TEST_PARAMS2(test_parse_format, "FLOAT3", RC_FORMAT_FLOAT3);
  TEST_PARAMS2(test_parse_format, "FLOAT4", RC_FORMAT_FLOAT4);
  TEST_PARAMS2(test_parse_format, "FLOAT5", RC_FORMAT_FLOAT5);
  TEST_PARAMS2(test_parse_format, "FLOAT6", RC_FORMAT_FLOAT6);
  TEST_PARAMS2(test_parse_format, "FLOAT7", RC_FORMAT_VALUE);
  TEST_PARAMS2(test_parse_format, "FLOAT10", RC_FORMAT_VALUE);

  TEST_PARAMS2(test_parse_format, "FIXED", RC_FORMAT_VALUE);
  TEST_PARAMS2(test_parse_format, "FIXED0", RC_FORMAT_VALUE);
  TEST_PARAMS2(test_parse_format, "FIXED1", RC_FORMAT_FIXED1);
  TEST_PARAMS2(test_parse_format, "FIXED2", RC_FORMAT_FIXED2);
  TEST_PARAMS2(test_parse_format, "FIXED3", RC_FORMAT_FIXED3);
  TEST_PARAMS2(test_parse_format, "FIXED4", RC_FORMAT_VALUE);
  TEST_PARAMS2(test_parse_format, "FIXED5", RC_FORMAT_VALUE);
  TEST_PARAMS2(test_parse_format, "FIXED6", RC_FORMAT_VALUE);
  TEST_PARAMS2(test_parse_format, "FIXED7", RC_FORMAT_VALUE);
  TEST_PARAMS2(test_parse_format, "FIXED10", RC_FORMAT_VALUE);

  TEST_SUITE_END();
}
