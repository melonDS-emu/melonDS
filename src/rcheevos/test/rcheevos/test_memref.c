#include "rc_internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

#include <float.h>
#include <math.h> /* pow */

static void test_mask(char size, uint32_t expected)
{
  ASSERT_NUM_EQUALS(rc_memref_mask(size), expected);
}

static void test_shared_masks(void)
{
  TEST_PARAMS2(test_mask, RC_MEMSIZE_8_BITS, 0x000000ff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_0, 0x00000001);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_1, 0x00000002);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_2, 0x00000004);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_3, 0x00000008);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_4, 0x00000010);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_5, 0x00000020);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_6, 0x00000040);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BIT_7, 0x00000080);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_LOW, 0x0000000f);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_HIGH, 0x000000f0);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_BITCOUNT, 0x000000ff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_16_BITS, 0x0000ffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_16_BITS_BE, 0x0000ffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_24_BITS, 0x00ffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_24_BITS_BE, 0x00ffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_32_BITS, 0xffffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_32_BITS_BE, 0xffffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_FLOAT, 0xffffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_FLOAT_BE, 0xffffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_DOUBLE32, 0xffffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_DOUBLE32_BE, 0xffffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_MBF32, 0xffffffff);
  TEST_PARAMS2(test_mask, RC_MEMSIZE_VARIABLE, 0xffffffff);
}

static void test_shared_size(char size, char expected)
{
  ASSERT_NUM_EQUALS(rc_memref_shared_size(size), expected);
}

static void test_shared_sizes(void)
{
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_8_BITS, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_0, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_1, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_2, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_3, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_4, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_5, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_6, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BIT_7, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_LOW, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_HIGH, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_BITCOUNT, RC_MEMSIZE_8_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_16_BITS, RC_MEMSIZE_16_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_16_BITS_BE, RC_MEMSIZE_16_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_24_BITS, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_24_BITS_BE, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_32_BITS, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_32_BITS_BE, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_FLOAT, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_FLOAT_BE, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_DOUBLE32, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_DOUBLE32_BE, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_MBF32, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_MBF32_LE, RC_MEMSIZE_32_BITS);
  TEST_PARAMS2(test_shared_size, RC_MEMSIZE_VARIABLE, RC_MEMSIZE_32_BITS);
}

static void test_transform(uint32_t value, uint8_t size, uint32_t expected)
{
  rc_typed_value_t typed_value;
  typed_value.type = RC_VALUE_TYPE_UNSIGNED;
  typed_value.value.u32 = value;
  rc_transform_memref_value(&typed_value, size);
  ASSERT_NUM_EQUALS(typed_value.value.u32, expected);
}

static void test_transform_float(uint32_t value, uint8_t size, double expected)
{
  rc_typed_value_t typed_value;
  typed_value.type = RC_VALUE_TYPE_UNSIGNED;
  typed_value.value.u32 = value;
  rc_transform_memref_value(&typed_value, size);
  ASSERT_FLOAT_EQUALS(typed_value.value.f32, expected);
}

static double rc_round(double n)
{
  return floor(n + 0.5); /* no round() in c89 */
}

static void test_transform_double32(uint32_t value, uint8_t size, double expected)
{
  rc_typed_value_t typed_value;
  typed_value.type = RC_VALUE_TYPE_UNSIGNED;
  typed_value.value.u32 = value;
  rc_transform_memref_value(&typed_value, size);

  /* a 20-bit mantissa only has 6 digits of precision. round to 6 digits, then do a float comparison. */
  if (fabs(expected) != 0.0) {
    const double digits = floor(log10(fabs(expected))) + 1;
    const double expected_pow = pow(10, 6 - digits);
    expected = rc_round(expected * expected_pow) / expected_pow;
    typed_value.value.f32 = (float)(rc_round(typed_value.value.f32 * expected_pow) / expected_pow);
  }

  ASSERT_FLOAT_EQUALS(typed_value.value.f32, expected);
}

static void test_transform_float_inf(uint32_t value, uint8_t size)
{
  /* C89 does not provide defines for NAN and INFINITY, nor does it provide isnan() or isinf() functions */
  rc_typed_value_t typed_value;
  typed_value.type = RC_VALUE_TYPE_UNSIGNED;
  typed_value.value.u32 = value;
  rc_transform_memref_value(&typed_value, size);

  if (typed_value.value.f32 < FLT_MAX) {
    /* infinity will be greater than max float value */
    ASSERT_FAIL("result of transform is not infinity")
  }
}

static void test_transform_float_nan(uint32_t value, uint8_t size)
{
  /* C89 does not provide defines for NAN and INFINITY, nor does it provide isnan() or isinf() functions */
  rc_typed_value_t typed_value;
  typed_value.type = RC_VALUE_TYPE_UNSIGNED;
  typed_value.value.u32 = value;
  rc_transform_memref_value(&typed_value, size);

  if (typed_value.value.f32 == typed_value.value.f32) {
    /* NaN cannot be compared, will fail equality check with itself */
    ASSERT_FAIL("result of transform is not NaN")
  }
}

static void test_transforms(void)
{
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_8_BITS, 0x00000078);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_16_BITS, 0x00005678);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_24_BITS, 0x00345678);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_32_BITS, 0x12345678);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_LOW, 0x00000008);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_HIGH, 0x00000007);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_BITCOUNT, 0x00000004); /* only counts bits in lowest byte */
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_16_BITS_BE, 0x00007856);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_24_BITS_BE, 0x00785634);
  TEST_PARAMS3(test_transform, 0x12345678, RC_MEMSIZE_32_BITS_BE, 0x78563412);

  TEST_PARAMS3(test_transform, 0x00000001, RC_MEMSIZE_BIT_0, 0x00000001);
  TEST_PARAMS3(test_transform, 0x00000002, RC_MEMSIZE_BIT_1, 0x00000001);
  TEST_PARAMS3(test_transform, 0x00000004, RC_MEMSIZE_BIT_2, 0x00000001);
  TEST_PARAMS3(test_transform, 0x00000008, RC_MEMSIZE_BIT_3, 0x00000001);
  TEST_PARAMS3(test_transform, 0x00000010, RC_MEMSIZE_BIT_4, 0x00000001);
  TEST_PARAMS3(test_transform, 0x00000020, RC_MEMSIZE_BIT_5, 0x00000001);
  TEST_PARAMS3(test_transform, 0x00000040, RC_MEMSIZE_BIT_6, 0x00000001);
  TEST_PARAMS3(test_transform, 0x00000080, RC_MEMSIZE_BIT_7, 0x00000001);

  TEST_PARAMS3(test_transform, 0x000000FE, RC_MEMSIZE_BIT_0, 0x00000000);
  TEST_PARAMS3(test_transform, 0x000000FD, RC_MEMSIZE_BIT_1, 0x00000000);
  TEST_PARAMS3(test_transform, 0x000000FB, RC_MEMSIZE_BIT_2, 0x00000000);
  TEST_PARAMS3(test_transform, 0x000000F7, RC_MEMSIZE_BIT_3, 0x00000000);
  TEST_PARAMS3(test_transform, 0x000000EF, RC_MEMSIZE_BIT_4, 0x00000000);
  TEST_PARAMS3(test_transform, 0x000000DF, RC_MEMSIZE_BIT_5, 0x00000000);
  TEST_PARAMS3(test_transform, 0x000000BF, RC_MEMSIZE_BIT_6, 0x00000000);
  TEST_PARAMS3(test_transform, 0x0000007F, RC_MEMSIZE_BIT_7, 0x00000000);

  TEST_PARAMS3(test_transform_float, 0x3F800000, RC_MEMSIZE_FLOAT, 1.0);
  TEST_PARAMS3(test_transform_float, 0x41460000, RC_MEMSIZE_FLOAT, 12.375);
  TEST_PARAMS3(test_transform_float, 0x42883EFA, RC_MEMSIZE_FLOAT, 68.123);
  TEST_PARAMS3(test_transform_float, 0x00000000, RC_MEMSIZE_FLOAT, 0.0);
  TEST_PARAMS3(test_transform_float, 0x80000000, RC_MEMSIZE_FLOAT, -0.0);
  TEST_PARAMS3(test_transform_float, 0xC0000000, RC_MEMSIZE_FLOAT, -2.0);
  TEST_PARAMS3(test_transform_float, 0x40490FDB, RC_MEMSIZE_FLOAT, 3.14159274101257324);
  TEST_PARAMS3(test_transform_float, 0x3EAAAAAB, RC_MEMSIZE_FLOAT, 0.333333334326744076);
  TEST_PARAMS3(test_transform_float, 0x429A4492, RC_MEMSIZE_FLOAT, 77.133926);
  TEST_PARAMS3(test_transform_float, 0x4350370A, RC_MEMSIZE_FLOAT, 208.214996);
  TEST_PARAMS3(test_transform_float, 0x45AE36E9, RC_MEMSIZE_FLOAT, 5574.863770);
  TEST_PARAMS3(test_transform_float, 0x58635FA9, RC_MEMSIZE_FLOAT, 1000000000000000.0);
  TEST_PARAMS3(test_transform_float, 0x24E69595, RC_MEMSIZE_FLOAT, 0.0000000000000001);
  TEST_PARAMS3(test_transform_float, 0x000042B4, RC_MEMSIZE_FLOAT, 2.39286e-41);
  TEST_PARAMS2(test_transform_float_inf, 0x7F800000, RC_MEMSIZE_FLOAT);
  TEST_PARAMS2(test_transform_float_nan, 0x7FFFFFFF, RC_MEMSIZE_FLOAT);

  TEST_PARAMS3(test_transform_double32, 0x3FF00000, RC_MEMSIZE_DOUBLE32, 1.0);
  TEST_PARAMS3(test_transform_double32, 0x4028C000, RC_MEMSIZE_DOUBLE32, 12.375);
  TEST_PARAMS3(test_transform_double32, 0x405107DF, RC_MEMSIZE_DOUBLE32, 68.123);
  TEST_PARAMS3(test_transform_double32, 0x00000000, RC_MEMSIZE_DOUBLE32, 0.0);
  TEST_PARAMS3(test_transform_double32, 0x80000000, RC_MEMSIZE_DOUBLE32, -0.0);
  TEST_PARAMS3(test_transform_double32, 0xC0000000, RC_MEMSIZE_DOUBLE32, -2.0);
  TEST_PARAMS3(test_transform_double32, 0x400921FB, RC_MEMSIZE_DOUBLE32, 3.14159274101257324);
  TEST_PARAMS3(test_transform_double32, 0x3FD55555, RC_MEMSIZE_DOUBLE32, 0.333333334326744076);
  TEST_PARAMS3(test_transform_double32, 0x40534892, RC_MEMSIZE_DOUBLE32, 77.133926);
  TEST_PARAMS3(test_transform_double32, 0x406A06E1, RC_MEMSIZE_DOUBLE32, 208.214996);
  TEST_PARAMS3(test_transform_double32, 0x40B5C6DD, RC_MEMSIZE_DOUBLE32, 5574.863770);
  TEST_PARAMS3(test_transform_double32, 0x430C6BF5, RC_MEMSIZE_DOUBLE32, 1000000000000000.0);
  TEST_PARAMS3(test_transform_double32, 0x3C9CD2B2, RC_MEMSIZE_DOUBLE32, 0.0000000000000001);
  TEST_PARAMS3(test_transform_double32, 0x3780AD01, RC_MEMSIZE_DOUBLE32, 2.39286e-41);
  TEST_PARAMS3(test_transform_double32, 0x3FF3C0CA, RC_MEMSIZE_DOUBLE32, 1.234568);
  TEST_PARAMS2(test_transform_float_inf, 0x7FF00000, RC_MEMSIZE_DOUBLE32);
  TEST_PARAMS2(test_transform_float_nan, 0x7FFFFFFF, RC_MEMSIZE_DOUBLE32);

  TEST_PARAMS3(test_transform_double32, 0x000000C0, RC_MEMSIZE_DOUBLE32_BE, -2.0);
  TEST_PARAMS3(test_transform_double32, 0x00003840, RC_MEMSIZE_DOUBLE32_BE, 24.0);
  TEST_PARAMS3(test_transform_double32, 0xCAC0F33F, RC_MEMSIZE_DOUBLE32_BE, 1.234568);
  TEST_PARAMS3(test_transform_double32, 0xFB210940, RC_MEMSIZE_DOUBLE32_BE, 3.14159274101257324);

  TEST_PARAMS3(test_transform_float, 0x0000803F, RC_MEMSIZE_FLOAT_BE, 1.0);
  TEST_PARAMS3(test_transform_float, 0x00004641, RC_MEMSIZE_FLOAT_BE, 12.375);
  TEST_PARAMS3(test_transform_float, 0xFA3E8842, RC_MEMSIZE_FLOAT_BE, 68.123);
  TEST_PARAMS3(test_transform_float, 0x00000000, RC_MEMSIZE_FLOAT_BE, 0.0);
  TEST_PARAMS3(test_transform_float, 0x00000080, RC_MEMSIZE_FLOAT_BE, -0.0);
  TEST_PARAMS3(test_transform_float, 0x000000C0, RC_MEMSIZE_FLOAT_BE, -2.0);
  TEST_PARAMS3(test_transform_float, 0xDB0F4940, RC_MEMSIZE_FLOAT_BE, 3.14159274101257324);
  TEST_PARAMS3(test_transform_float, 0xABAAAA3E, RC_MEMSIZE_FLOAT_BE, 0.333333334326744076);
  TEST_PARAMS3(test_transform_float, 0x92449A42, RC_MEMSIZE_FLOAT_BE, 77.133926);
  TEST_PARAMS3(test_transform_float, 0x0A375043, RC_MEMSIZE_FLOAT_BE, 208.214996);
  TEST_PARAMS3(test_transform_float, 0xE936AE45, RC_MEMSIZE_FLOAT_BE, 5574.863770);
  TEST_PARAMS3(test_transform_float, 0xA95F6358, RC_MEMSIZE_FLOAT_BE, 1000000000000000.0);
  TEST_PARAMS3(test_transform_float, 0x9595E624, RC_MEMSIZE_FLOAT_BE, 0.0000000000000001);
  TEST_PARAMS3(test_transform_float, 0xB4420000, RC_MEMSIZE_FLOAT_BE, 2.39286e-41);
  TEST_PARAMS2(test_transform_float_inf, 0x0000807F, RC_MEMSIZE_FLOAT_BE);
  TEST_PARAMS2(test_transform_float_nan, 0xFFFFFF7F, RC_MEMSIZE_FLOAT_BE);

  /* MBF values are stored big endian (at least on Apple II), so will be byteswapped
   * when passed to rc_transform_memref_value. MBF doesn't support infinity or NaN. */
  TEST_PARAMS3(test_transform_float, 0x00000081, RC_MEMSIZE_MBF32, 1.0);        /* 81 00 00 00 */
  TEST_PARAMS3(test_transform_float, 0x00002084, RC_MEMSIZE_MBF32, 10.0);       /* 84 20 00 00 */
  TEST_PARAMS3(test_transform_float, 0x00004687, RC_MEMSIZE_MBF32, 99.0);       /* 87 46 00 00 */
  TEST_PARAMS3(test_transform_float, 0x00000000, RC_MEMSIZE_MBF32, 0.0);        /* 00 00 00 00 */
  TEST_PARAMS3(test_transform_float, 0x00000080, RC_MEMSIZE_MBF32, 0.5);        /* 80 00 00 00 */
  TEST_PARAMS3(test_transform_float, 0x00008082, RC_MEMSIZE_MBF32, -2.0);       /* 82 80 00 00 */
  TEST_PARAMS3(test_transform_float, 0xF3043581, RC_MEMSIZE_MBF32, 1.41421354); /* 81 34 04 F3 */
  TEST_PARAMS3(test_transform_float, 0xDA0F4982, RC_MEMSIZE_MBF32, 3.14159256); /* 82 49 0F DA */
  TEST_PARAMS3(test_transform_float, 0xDB0F4983, RC_MEMSIZE_MBF32, 6.28318548); /* 83 49 0F DB */

  /* Some flavors of BASIC (notably Locomotive BASIC on the Amstrad CPC) use the native endian-ness of
   * the system for their MBF values, so we support both MBF32 (big endian) and MBF32_LE (little endian).
   * Also note that Amstrad BASIC and Apple II BASIC both use MBF40, but since MBF40 just adds 8 extra bits
   * of significance as the end of the MBF32 value, we can discard those as we convert to a 32-bit float. */
  TEST_PARAMS3(test_transform_float, 0x81000000, RC_MEMSIZE_MBF32_LE, 1.0);        /* 00 00 00 81 */
  TEST_PARAMS3(test_transform_float, 0x84200000, RC_MEMSIZE_MBF32_LE, 10.0);       /* 00 00 20 84 */
  TEST_PARAMS3(test_transform_float, 0x87460000, RC_MEMSIZE_MBF32_LE, 99.0);       /* 00 00 46 87 */
  TEST_PARAMS3(test_transform_float, 0x00000000, RC_MEMSIZE_MBF32_LE, 0.0);        /* 00 00 00 00 */
  TEST_PARAMS3(test_transform_float, 0x80000000, RC_MEMSIZE_MBF32_LE, 0.5);        /* 00 00 00 80 */
  TEST_PARAMS3(test_transform_float, 0x82800000, RC_MEMSIZE_MBF32_LE, -2.0);       /* 00 00 80 82 */
  TEST_PARAMS3(test_transform_float, 0x813504F3, RC_MEMSIZE_MBF32_LE, 1.41421354); /* F3 04 34 81 */
  TEST_PARAMS3(test_transform_float, 0x82490FDA, RC_MEMSIZE_MBF32_LE, 3.14159256); /* DA 0F 49 82 */
  TEST_PARAMS3(test_transform_float, 0x83490FDB, RC_MEMSIZE_MBF32_LE, 6.28318548); /* DB 0F 49 83 */
}

static int get_memref_count(rc_parse_state_t* parse) {
  return rc_memrefs_count_memrefs(parse->memrefs) + rc_memrefs_count_modified_memrefs(parse->memrefs);
}

static void test_allocate_shared_address() {
  rc_parse_state_t parse;
  rc_memrefs_t memrefs;
  rc_init_parse_state(&parse, NULL);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  rc_alloc_memref(&parse, 1, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 1);

  rc_alloc_memref(&parse, 1, RC_MEMSIZE_16_BITS); /* differing size will not match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 2);

  rc_alloc_memref(&parse, 1, RC_MEMSIZE_LOW); /* differing size will not match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 3);

  rc_alloc_memref(&parse, 1, RC_MEMSIZE_BIT_2); /* differing size will not match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 4);

  rc_alloc_memref(&parse, 2, RC_MEMSIZE_8_BITS); /* differing address will not match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_alloc_memref(&parse, 1, RC_MEMSIZE_8_BITS); /* match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_alloc_memref(&parse, 1, RC_MEMSIZE_16_BITS); /* match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_alloc_memref(&parse, 1, RC_MEMSIZE_BIT_2); /* match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_alloc_memref(&parse, 2, RC_MEMSIZE_8_BITS); /* match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_destroy_parse_state(&parse);
}

static void test_allocate_shared_address2() {
  rc_parse_state_t parse;
  rc_memrefs_t memrefs;
  rc_memref_t* memref1;
  rc_memref_t* memref2;
  rc_memref_t* memref3;
  rc_memref_t* memref4;
  rc_memref_t* memref5;
  rc_memref_t* memrefX;
  rc_init_parse_state(&parse, NULL);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  memref1 = rc_alloc_memref(&parse, 1, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(memref1->address, 1);
  ASSERT_NUM_EQUALS(memref1->value.size, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(memref1->value.memref_type, RC_MEMREF_TYPE_MEMREF);
  ASSERT_NUM_EQUALS(memref1->value.type, RC_VALUE_TYPE_UNSIGNED);
  ASSERT_NUM_EQUALS(memref1->value.value, 0);
  ASSERT_NUM_EQUALS(memref1->value.changed, 0);
  ASSERT_NUM_EQUALS(memref1->value.prior, 0);
  
  memref2 = rc_alloc_memref(&parse, 1, RC_MEMSIZE_16_BITS); /* differing size will not match */
  memref3 = rc_alloc_memref(&parse, 1, RC_MEMSIZE_LOW); /* differing size will not match */
  memref4 = rc_alloc_memref(&parse, 1, RC_MEMSIZE_BIT_2); /* differing size will not match */
  memref5 = rc_alloc_memref(&parse, 2, RC_MEMSIZE_8_BITS); /* differing address will not match */

  memrefX = rc_alloc_memref(&parse, 1, RC_MEMSIZE_8_BITS); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref1);

  memrefX = rc_alloc_memref(&parse, 1, RC_MEMSIZE_16_BITS); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref2);

  memrefX = rc_alloc_memref(&parse, 1, RC_MEMSIZE_LOW); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref3);

  memrefX = rc_alloc_memref(&parse, 1, RC_MEMSIZE_BIT_2); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref4);

  memrefX = rc_alloc_memref(&parse, 2, RC_MEMSIZE_8_BITS); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref5);

  rc_destroy_parse_state(&parse);
}

static void test_allocate_shared_indirect_address() {
  rc_parse_state_t parse;
  rc_memrefs_t memrefs;
  rc_memref_t* parent_memref1, *parent_memref2;
  rc_operand_t parent1, parent2, delta1, intermediate2;
  rc_modified_memref_t* child1, *child2, *child3, *child4;
  rc_operand_t offset0, offset4;
  rc_operand_set_const(&offset0, 0);
  rc_operand_set_const(&offset4, 4);

  rc_init_parse_state(&parse, NULL);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  parent1.value.memref = parent_memref1 = rc_alloc_memref(&parse, 88, RC_MEMSIZE_16_BITS);
  parent1.type = RC_OPERAND_ADDRESS;
  parent1.size = RC_MEMSIZE_16_BITS;
  parent2.value.memref = parent_memref2 = rc_alloc_memref(&parse, 99, RC_MEMSIZE_16_BITS);
  parent2.type = RC_OPERAND_ADDRESS;
  parent2.size = RC_MEMSIZE_16_BITS;
  delta1.value.memref = parent_memref1;
  delta1.type = RC_OPERAND_DELTA;
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 2);

  child1 = rc_alloc_modified_memref(&parse, RC_MEMSIZE_8_BITS, &parent1, RC_OPERATOR_INDIRECT_READ, &offset0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 3);

  /* differing size will not match */
  child2 = rc_alloc_modified_memref(&parse, RC_MEMSIZE_16_BITS, &parent1, RC_OPERATOR_INDIRECT_READ, &offset0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 4);

  /* differing parent will not match */
  child3 = rc_alloc_modified_memref(&parse, RC_MEMSIZE_8_BITS, &parent2, RC_OPERATOR_INDIRECT_READ, &offset0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  /* differing parent type will not match */
  child4 = rc_alloc_modified_memref(&parse, RC_MEMSIZE_8_BITS, &delta1, RC_OPERATOR_INDIRECT_READ, &offset0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 6);

  /* differing offset will not match */
  child4 = rc_alloc_modified_memref(&parse, RC_MEMSIZE_8_BITS, &parent1, RC_OPERATOR_INDIRECT_READ, &offset4);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 7);

  /* exact match to first */
  ASSERT_PTR_EQUALS(rc_alloc_modified_memref(&parse, RC_MEMSIZE_8_BITS, &parent1, RC_OPERATOR_INDIRECT_READ, &offset0), child1);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 7);

  /* exact match to differing parent */
  ASSERT_PTR_EQUALS(rc_alloc_modified_memref(&parse, RC_MEMSIZE_8_BITS, &parent2, RC_OPERATOR_INDIRECT_READ, &offset0), child3);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 7);

  /* exact match to differing offset */
  ASSERT_PTR_EQUALS(rc_alloc_modified_memref(&parse, RC_MEMSIZE_8_BITS, &parent1, RC_OPERATOR_INDIRECT_READ, &offset4), child4);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 7);

  /* intermediate parent */
  intermediate2.value.memref = &child2->memref;
  intermediate2.type = RC_OPERAND_ADDRESS;
  intermediate2.size = RC_MEMSIZE_32_BITS;
  intermediate2.memref_access_type = RC_OPERAND_ADDRESS;
  child4 = rc_alloc_modified_memref(&parse, RC_MEMSIZE_8_BITS, &intermediate2, RC_OPERATOR_INDIRECT_READ, &offset0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 8);

  ASSERT_PTR_EQUALS(rc_alloc_modified_memref(&parse, RC_MEMSIZE_8_BITS, &intermediate2, RC_OPERATOR_INDIRECT_READ, &offset0), child4);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 8);

  rc_destroy_parse_state(&parse);
}

static void test_sizing_mode_grow_buffer() {
  int i;
  rc_parse_state_t parse;
  rc_memrefs_t memrefs;
  rc_init_parse_state(&parse, NULL);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  /* memrefs are allocated 16 at a time */
  for (i = 0; i < 100; i++) {
      rc_alloc_memref(&parse, i, RC_MEMSIZE_8_BITS);
  }
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  /* 100 have been allocated, make sure we can still access items at various addresses without allocating more */
  rc_alloc_memref(&parse, 1, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_alloc_memref(&parse, 25, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_alloc_memref(&parse, 50, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_alloc_memref(&parse, 75, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_alloc_memref(&parse, 99, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_destroy_parse_state(&parse);
}

static void test_update_memref_values() {
  rc_parse_state_t parse;
  rc_memrefs_t memrefs;
  rc_memref_t* memref1;
  rc_memref_t* memref2;

  uint8_t ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_init_parse_state(&parse, NULL);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  memref1 = rc_alloc_memref(&parse, 1, RC_MEMSIZE_8_BITS);
  memref2 = rc_alloc_memref(&parse, 2, RC_MEMSIZE_8_BITS);

  rc_update_memref_values(&memrefs, peek, &memory);

  ASSERT_NUM_EQUALS(memref1->value.value, 0x12);
  ASSERT_NUM_EQUALS(memref1->value.changed, 1);
  ASSERT_NUM_EQUALS(memref1->value.prior, 0);
  ASSERT_NUM_EQUALS(memref2->value.value, 0x34);
  ASSERT_NUM_EQUALS(memref2->value.changed, 1);
  ASSERT_NUM_EQUALS(memref2->value.prior, 0);

  ram[1] = 3;
  rc_update_memref_values(&memrefs, peek, &memory);

  ASSERT_NUM_EQUALS(memref1->value.value, 3);
  ASSERT_NUM_EQUALS(memref1->value.changed, 1);
  ASSERT_NUM_EQUALS(memref1->value.prior, 0x12);
  ASSERT_NUM_EQUALS(memref2->value.value, 0x34);
  ASSERT_NUM_EQUALS(memref2->value.changed, 0);
  ASSERT_NUM_EQUALS(memref2->value.prior, 0);

  ram[1] = 5;
  rc_update_memref_values(&memrefs, peek, &memory);

  ASSERT_NUM_EQUALS(memref1->value.value, 5);
  ASSERT_NUM_EQUALS(memref1->value.changed, 1);
  ASSERT_NUM_EQUALS(memref1->value.prior, 3);
  ASSERT_NUM_EQUALS(memref2->value.value, 0x34);
  ASSERT_NUM_EQUALS(memref2->value.changed, 0);
  ASSERT_NUM_EQUALS(memref2->value.prior, 0);

  ram[2] = 7;
  rc_update_memref_values(&memrefs, peek, &memory);

  ASSERT_NUM_EQUALS(memref1->value.value, 5);
  ASSERT_NUM_EQUALS(memref1->value.changed, 0);
  ASSERT_NUM_EQUALS(memref1->value.prior, 3);
  ASSERT_NUM_EQUALS(memref2->value.value, 7);
  ASSERT_NUM_EQUALS(memref2->value.changed, 1);
  ASSERT_NUM_EQUALS(memref2->value.prior, 0x34);

  rc_destroy_parse_state(&parse);
}

void test_memref(void) {
  TEST_SUITE_BEGIN();

  test_shared_masks();
  test_shared_sizes();
  test_transforms();

  TEST(test_allocate_shared_address);
  TEST(test_allocate_shared_address2);
  TEST(test_allocate_shared_indirect_address);

  TEST(test_sizing_mode_grow_buffer);
  TEST(test_update_memref_values);

  /* rc_parse_memref is thoroughly tested by rc_parse_operand tests */

  TEST_SUITE_END();
}
