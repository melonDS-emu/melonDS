#include "rc_internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

static void test_evaluate_value(const char* memaddr, int expected_value) {
  rc_value_t* self;
  /* bytes 5-8 are the float value for pi */
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56, 0xDB, 0x0F, 0x49, 0x40};
  memory_t memory;
  char buffer[2048];
  unsigned* overflow;
  int ret;

  memory.ram = ram;
  memory.size = sizeof(ram);

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  overflow = (unsigned*)(((char*)buffer) + ret);
  *overflow = 0xCDCDCDCD;

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);
  if (*overflow != 0xCDCDCDCD) {
    ASSERT_FAIL("write past end of buffer");
  }

  ret = rc_evaluate_value(self, peek, &memory, NULL);
  ASSERT_NUM_EQUALS(ret, expected_value);
}

static void test_invalid_value(const char* memaddr, int expected_error) {
  int ret = rc_value_size(memaddr);
  ASSERT_NUM_EQUALS(ret, expected_error);
}

static void test_measured_value_target(const char* memaddr, int expected_target) {
  rc_value_t* self;
  char buffer[2048];
  unsigned* overflow;
  int ret;

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  overflow = (unsigned*)(((char*)buffer) + ret);
  *overflow = 0xCDCDCDCD;

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);
  if (*overflow != 0xCDCDCDCD) {
    ASSERT_FAIL("write past end of buffer");
  }

  ASSERT_NUM_EQUALS(self->conditions->conditions->required_hits, expected_target);
}

static void test_evaluate_measured_value_with_pause() {
  rc_value_t* self;
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  char buffer[2048];
  const char* memaddr = "P:0xH0003=hAB_M:0xH0002!=d0xH0002";
  int ret;

  memory.ram = ram;
  memory.size = sizeof(ram);

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);

  /* should initially be paused, no hits captured */
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* pause should prevent hitcount */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* unpause should not report the change that occurred while paused */
  ram[3] = 0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* hitcount should be captured */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* pause should return current hitcount */
  ram[3] = 0xAB;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* pause should prevent hitcount */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* unpause should not report the change that occurred while paused */
  ram[3] = 0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* additional hitcount should be captured */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 2);
}

static void test_evaluated_and_next_measured_if_value() {
  rc_value_t* self;
  const rc_condition_t* cond2;
  const rc_condition_t* cond4;
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  char buffer[2048];
  const char* memaddr = "R:0xH0004=1_N:0xH0000=1.1._N:d0xH0000=9_Q:0xH0000=0.1._M:100";
  int ret;

  memory.ram = ram;
  memory.size = sizeof(ram);

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);

  cond2 = self->conditions->conditions->next;
  cond4 = cond2->next->next;

  /* measured if cannot be true */
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);
  ASSERT_NUM_EQUALS(cond2->current_hits, 0);
  ASSERT_NUM_EQUALS(cond4->current_hits, 0);

  /* capture first hit, measured_if still not true */
  ram[0] = 1;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);
  ASSERT_NUM_EQUALS(cond2->current_hits, 1);
  ASSERT_NUM_EQUALS(cond4->current_hits, 0);

  /* reset */
  ram[4] = 1;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);
  ASSERT_NUM_EQUALS(cond2->current_hits, 0);
  ASSERT_NUM_EQUALS(cond4->current_hits, 0);

  /* clear reset */
  ram[4] = 0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);
  ASSERT_NUM_EQUALS(cond2->current_hits, 1);
  ASSERT_NUM_EQUALS(cond4->current_hits, 0);

  /* prime measured_if */
  ram[0] = 9;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);
  ASSERT_NUM_EQUALS(cond2->current_hits, 1);
  ASSERT_NUM_EQUALS(cond4->current_hits, 0);

  /* trigger measured if */
  ram[0] = 0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 100);
  ASSERT_NUM_EQUALS(cond2->current_hits, 1);
  ASSERT_NUM_EQUALS(cond4->current_hits, 1);

  /* measured if should remain triggered */
  ram[0] = 1;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 100);
  ASSERT_NUM_EQUALS(cond2->current_hits, 1);
  ASSERT_NUM_EQUALS(cond4->current_hits, 1);

  /* reset */
  ram[4] = 1;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);
  ASSERT_NUM_EQUALS(cond2->current_hits, 0);
  ASSERT_NUM_EQUALS(cond4->current_hits, 0);
}

static void test_evaluate_measured_value_with_reset() {
  rc_value_t* self;
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  char buffer[2048];
  const char* memaddr = "R:0xH0003=hAB_M:0xH0002!=d0xH0002";
  int ret;

  memory.ram = ram;
  memory.size = sizeof(ram);

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);

  /* reset should initially be true, no hits captured */
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* reset should prevent hitcount */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* reset no longer true, change while reset shouldn't be captured */
  ram[3] = 0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* additional hitcount should be captured */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* reset should clear hit count */
  ram[3] = 0xAB;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* reset should prevent hitcount */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* reset no longer true, change while reset shouldn't be captured */
  ram[3] = 0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* additional hitcount should be captured */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);
}

static void init_typed_value(rc_typed_value_t* value, uint8_t type, uint32_t u32, double f32) {
  value->type = type;

  switch (type) {
    case RC_VALUE_TYPE_UNSIGNED:
      value->value.u32 = u32;
      break;

    case RC_VALUE_TYPE_SIGNED:
      value->value.i32 = (int)u32;
      break;

    case RC_VALUE_TYPE_FLOAT:
      value->value.f32 = (float)f32;
      break;

    case RC_VALUE_TYPE_NONE:
      value->value.u32 = 0xCDCDCDCD; /* force uninitialized value */
      break;

    default:
      break;
  }
}

static void _assert_typed_value(const rc_typed_value_t* value, uint8_t type, uint32_t u32, double f32) {
  ASSERT_NUM_EQUALS(value->type, type);

  switch (type) {
    case RC_VALUE_TYPE_UNSIGNED:
      ASSERT_NUM_EQUALS(value->value.u32, u32);
      break;

    case RC_VALUE_TYPE_SIGNED:
      ASSERT_NUM_EQUALS(value->value.i32, (int)u32);
      break;

    case RC_VALUE_TYPE_FLOAT:
      ASSERT_FLOAT_EQUALS(value->value.f32, (float)f32);
      break;

    default:
      break;
  }
}
#define assert_typed_value(value, type, u32, f32) ASSERT_HELPER(_assert_typed_value(value, type, u32, f32), "assert_typed_value")

static void test_typed_value_convert(uint8_t type, uint32_t u32, double f32, uint8_t new_type, uint32_t new_u32, double new_f32) {
  rc_typed_value_t value;
  init_typed_value(&value, type, u32, f32);

  rc_typed_value_convert(&value, new_type);

  assert_typed_value(&value, new_type, new_u32, new_f32);
}

static void test_typed_value_conversion() {
  /* unsigned source */
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0, 0.0, RC_VALUE_TYPE_UNSIGNED, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 12, 0.0, RC_VALUE_TYPE_UNSIGNED, 12, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0, 0.0, RC_VALUE_TYPE_SIGNED, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 12, 0.0, RC_VALUE_TYPE_SIGNED, 12, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-1, 0.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0, 0.0, RC_VALUE_TYPE_FLOAT, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 12, 0.0, RC_VALUE_TYPE_FLOAT, 0, 12.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0, RC_VALUE_TYPE_FLOAT, 0, 4294967295.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 12, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* signed source */
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 0, 0.0, RC_VALUE_TYPE_UNSIGNED, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 12, 0.0, RC_VALUE_TYPE_UNSIGNED, 12, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, (unsigned)-1, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 0, 0.0, RC_VALUE_TYPE_SIGNED, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 12, 0.0, RC_VALUE_TYPE_SIGNED, 12, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, (unsigned)-1, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-1, 0.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 0, 0.0, RC_VALUE_TYPE_FLOAT, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 12, 0.0, RC_VALUE_TYPE_FLOAT, 0, 12.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, (unsigned)-1, 0.0, RC_VALUE_TYPE_FLOAT, 0, -1.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, 12, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_SIGNED, (unsigned)-1, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* float source (whole numbers) */
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 0.0, RC_VALUE_TYPE_UNSIGNED, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 12.0, RC_VALUE_TYPE_UNSIGNED, 12, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -1.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 0.0, RC_VALUE_TYPE_SIGNED, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 12.0, RC_VALUE_TYPE_SIGNED, 12, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -1.0, RC_VALUE_TYPE_SIGNED, (unsigned)-1, 0.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 0.0, RC_VALUE_TYPE_FLOAT, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 12.0, RC_VALUE_TYPE_FLOAT, 0, 12.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -1.0, RC_VALUE_TYPE_FLOAT, 0, -1.0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 12.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -1.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* float source (non-whole numbers) */
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 3.14159);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_UNSIGNED, 3, 0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_SIGNED, 3, 0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_NONE, 0, 0);

  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -3.14159, RC_VALUE_TYPE_FLOAT, 0, -3.14159);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -3.14159, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFD, 0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -3.14159, RC_VALUE_TYPE_SIGNED, (unsigned)-3, 0);
  TEST_PARAMS6(test_typed_value_convert, RC_VALUE_TYPE_FLOAT, 0, -3.14159, RC_VALUE_TYPE_NONE, 0, 0);
}

static void test_typed_value_add(uint8_t type, uint32_t u32, double f32,
    uint8_t amount_type, uint32_t amount_u32, double amount_f32, uint32_t result_u32, double result_f32) {
  rc_typed_value_t value, amount;

  init_typed_value(&value, type, u32, f32);
  init_typed_value(&amount, amount_type, amount_u32, amount_f32);

  rc_typed_value_add(&value, &amount);

  if (result_f32 != 0.0) {
    assert_typed_value(&value, RC_VALUE_TYPE_FLOAT, result_u32, result_f32);
  }
  else if (type == RC_VALUE_TYPE_NONE) {
    assert_typed_value(&value, amount_type, result_u32, result_f32);
  }
  else {
    assert_typed_value(&value, type, result_u32, result_f32);
  }
}

static void test_typed_value_addition() {
  /* no source */
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_UNSIGNED, 8, 0.0, 8, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_FLOAT, 0, 8.0, 0, 8.0);

  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, 0xFFFFFFFE, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, (unsigned)-2, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_FLOAT, 0, -2.0, 0, -2.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, 0, 0.0);

  /* unsigned source */
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 8, 0.0, 14, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 8.0, 0, 14.0);

  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, 4, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, 4, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, -2.0, 0, 4.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, 6, 0.0);

  /* signed source */
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, (unsigned)-4, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, 0, -4.0);

  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_UNSIGNED, 8, 0.0, 2, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, (unsigned)-8, 0.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, 0, -4.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, (unsigned)-6, 0.0);

  /* float source (whole numbers) */
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, 0, 8.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, 0, 8.0);

  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, 0, 4294967300.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, 0, 4.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, -8.0, 0, -2.0);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_NONE, 0, 0.0, 0, 6.0);

  /* float source (non-whole numbers) */
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, 0, 5.14159);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, 0, 1.14159);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 2.0, 0, 5.14159);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 6.023, 0, 9.16459);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, -8.0, 0, -4.85841);
  TEST_PARAMS8(test_typed_value_add, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_NONE, 0, 0.0, 0, 3.14159);
}

static void test_typed_value_multiply(char type, uint32_t u32, double f32,
    uint8_t amount_type, uint32_t amount_u32, double amount_f32,
    uint8_t result_type, uint32_t result_u32, double result_f32) {
  rc_typed_value_t value, amount;

  init_typed_value(&value, type, u32, f32);
  init_typed_value(&amount, amount_type, amount_u32, amount_f32);

  rc_typed_value_multiply(&value, &amount);

  assert_typed_value(&value, result_type, result_u32, result_f32);
}

static void test_typed_value_multiplication() {
  /* unsigned source */
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 8, 0.0, RC_VALUE_TYPE_UNSIGNED, 48, 0.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 8.0, RC_VALUE_TYPE_FLOAT, 0, 48.0);

  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFF4, 0.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFF4, 0.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, -2.0, RC_VALUE_TYPE_FLOAT, 0, -12.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* signed source */
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-12, 0.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, -12.0);

  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, RC_VALUE_TYPE_SIGNED, 12, 0.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_SIGNED, 12, 0.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, -12.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* float source (whole numbers) */
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_FLOAT, 0, 12.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, 12.0);

  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, RC_VALUE_TYPE_FLOAT, 0, 25769803764.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_FLOAT, 0, -12.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, -8.0, RC_VALUE_TYPE_FLOAT, 0, -48.0);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* float source (non-whole numbers) */
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_FLOAT, 0, 6.28318);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_FLOAT, 0, -6.28318);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, 6.28318);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 6.023, RC_VALUE_TYPE_FLOAT, 0, 18.92179657);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, -8.0, RC_VALUE_TYPE_FLOAT, 0, -25.13272);
  TEST_PARAMS9(test_typed_value_multiply, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
}

static void test_typed_value_divide(uint8_t type, uint32_t u32, double f32,
    uint8_t amount_type, uint32_t amount_u32, double amount_f32,
    uint8_t result_type, uint32_t result_u32, double result_f32) {
  rc_typed_value_t value, amount;

  init_typed_value(&value, type, u32, f32);
  init_typed_value(&amount, amount_type, amount_u32, amount_f32);

  rc_typed_value_divide(&value, &amount);

  assert_typed_value(&value, result_type, result_u32, result_f32);
}

static void test_typed_value_division() {
  /* division by zero */
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_SIGNED, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* unsigned source */
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_UNSIGNED, 3, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, 3.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.5, RC_VALUE_TYPE_FLOAT, 0, 2.4);

  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, RC_VALUE_TYPE_UNSIGNED, 0, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_UNSIGNED, 0, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, -2.0, RC_VALUE_TYPE_FLOAT, 0, -3.0);

  /* signed source */
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-3, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, -3.0);

  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, RC_VALUE_TYPE_SIGNED, 3, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_SIGNED, 3, 0.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_FLOAT, 0, -2.0, RC_VALUE_TYPE_FLOAT, 0, 3.0);

  /* float source (whole numbers) */
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_FLOAT, 0, 3.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, 3.0);

  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, RC_VALUE_TYPE_FLOAT, 0, 0.00000000139698386);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_FLOAT, 0, -3.0);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, -8.0, RC_VALUE_TYPE_FLOAT, 0, -0.75);

  /* float source (non-whole numbers) */
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_FLOAT, 0, 1.570795);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_FLOAT, 0, -1.570795);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, 1.570795);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 6.023, RC_VALUE_TYPE_FLOAT, 0, 0.52159887099);
  TEST_PARAMS9(test_typed_value_divide, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, -8.0, RC_VALUE_TYPE_FLOAT, 0, -0.39269875);
}

static void test_typed_value_mod(uint8_t type, uint32_t u32, double f32,
  uint8_t amount_type, uint32_t amount_u32, double amount_f32,
  uint8_t result_type, uint32_t result_u32, double result_f32) {
  rc_typed_value_t value, amount;

  init_typed_value(&value, type, u32, f32);
  init_typed_value(&amount, amount_type, amount_u32, amount_f32);

  rc_typed_value_modulus(&value, &amount);

  assert_typed_value(&value, result_type, result_u32, result_f32);
}

static void test_typed_value_modulus() {
  /* modulus with zero divisor */
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_SIGNED, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, 0.0, RC_VALUE_TYPE_NONE, 0, 0.0);

  /* unsigned source */
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 4, 0.0, RC_VALUE_TYPE_UNSIGNED, 2, 0.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 4.0, RC_VALUE_TYPE_FLOAT, 0, 2.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.25, RC_VALUE_TYPE_FLOAT, 0, 1.5);

  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFC, 0.0, RC_VALUE_TYPE_UNSIGNED, 6, 0.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-4, 0.0, RC_VALUE_TYPE_UNSIGNED, 6, 0.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_UNSIGNED, 6, 0.0, RC_VALUE_TYPE_FLOAT, 0, -4.0, RC_VALUE_TYPE_FLOAT, 0, 2.0);

  /* signed source */
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_UNSIGNED, 4, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_FLOAT, 0, 4.0, RC_VALUE_TYPE_FLOAT, 0, -2.0);

  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFC, 0.0, RC_VALUE_TYPE_SIGNED, -2, 0.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_SIGNED, (unsigned)-4, 0.0, RC_VALUE_TYPE_SIGNED, -2, 0.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_SIGNED, (unsigned)-6, 0.0, RC_VALUE_TYPE_FLOAT, 0, -4.0, RC_VALUE_TYPE_FLOAT, 0, -2.0);

  /* float source (whole numbers) */
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_UNSIGNED, 4, 0.0, RC_VALUE_TYPE_FLOAT, 0, 2.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, 4.0, RC_VALUE_TYPE_FLOAT, 0, 2.0);

  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFE, 0.0, RC_VALUE_TYPE_FLOAT, 0, 6.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_SIGNED, (unsigned)-2, 0.0, RC_VALUE_TYPE_FLOAT, 0, 0.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_FLOAT, 0, 6.0, RC_VALUE_TYPE_FLOAT, 0, -5.0, RC_VALUE_TYPE_FLOAT, 0, 1.0);

  /* float source (non-whole numbers) */
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_UNSIGNED, 2, 0.0, RC_VALUE_TYPE_FLOAT, 0, 1.141590);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_SIGNED, (unsigned)-4, 0.0, RC_VALUE_TYPE_FLOAT, 0, 3.141590);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 2.0, RC_VALUE_TYPE_FLOAT, 0, 1.141590);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 1.570795, RC_VALUE_TYPE_FLOAT, 0, 0.0);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, 6.023, RC_VALUE_TYPE_FLOAT, 0, 3.141590);
  TEST_PARAMS9(test_typed_value_mod, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, -2.0, RC_VALUE_TYPE_FLOAT, 0, 1.141590);
}

static void test_typed_value_negate(char type, int i32, double f32, char expected_type, signed result_i32, double result_f32) {
  rc_typed_value_t value;

  init_typed_value(&value, type, (unsigned)i32, f32);

  rc_typed_value_negate(&value);

  assert_typed_value(&value, expected_type, (unsigned)result_i32, result_f32);
}

static void test_typed_value_negation() {
  /* unsigned source */
  TEST_PARAMS6(test_typed_value_negate, RC_VALUE_TYPE_UNSIGNED, 0, 0.0, RC_VALUE_TYPE_SIGNED, 0, 0.0);
  TEST_PARAMS6(test_typed_value_negate, RC_VALUE_TYPE_UNSIGNED, 99, 0.0, RC_VALUE_TYPE_SIGNED, -99, 0.0);
  TEST_PARAMS6(test_typed_value_negate, RC_VALUE_TYPE_UNSIGNED, 0xFFFFFFFF, 0.0, RC_VALUE_TYPE_SIGNED, 1, 0.0);

  /* signed source */
  TEST_PARAMS6(test_typed_value_negate, RC_VALUE_TYPE_SIGNED, 0, 0.0, RC_VALUE_TYPE_SIGNED, 0, 0.0);
  TEST_PARAMS6(test_typed_value_negate, RC_VALUE_TYPE_SIGNED, 99, 0.0, RC_VALUE_TYPE_SIGNED, -99, 0.0);
  TEST_PARAMS6(test_typed_value_negate, RC_VALUE_TYPE_SIGNED, -1, 0.0, RC_VALUE_TYPE_SIGNED, 1, 0.0);

  /* float source (whole numbers) */
  TEST_PARAMS6(test_typed_value_negate, RC_VALUE_TYPE_FLOAT, 0, 0.0, RC_VALUE_TYPE_FLOAT, 0, 0.0);
  TEST_PARAMS6(test_typed_value_negate, RC_VALUE_TYPE_FLOAT, 0, 99.0, RC_VALUE_TYPE_FLOAT, 0, -99.0);
  TEST_PARAMS6(test_typed_value_negate, RC_VALUE_TYPE_FLOAT, 0, -1.0, RC_VALUE_TYPE_FLOAT, 0, 1.0);

  /* float source (non-whole numbers) */
  TEST_PARAMS6(test_typed_value_negate, RC_VALUE_TYPE_FLOAT, 0, 0.1, RC_VALUE_TYPE_FLOAT, 0, -0.1);
  TEST_PARAMS6(test_typed_value_negate, RC_VALUE_TYPE_FLOAT, 0, 3.14159, RC_VALUE_TYPE_FLOAT, 0, -3.14159);
  TEST_PARAMS6(test_typed_value_negate, RC_VALUE_TYPE_FLOAT, 0, -2.7, RC_VALUE_TYPE_FLOAT, 0, 2.7);
}

static void test_addhits_float_coercion() {
  rc_value_t* self;
  uint8_t ram[] = { 0x00, 0x06, 0x34, 0xAB, 0x00, 0x00, 0xC0, 0x3F }; /* fF0004 = 1.5 */
  memory_t memory;
  char buffer[2048];
  /* measured(tally(0, (0 + float(4) * 10 - prev(float(4)) * 10) == 1)) */
  const char* memaddr = "A:0_A:fF0004*10_B:dfF0004*10_C:0=1_M:0=1";
  int ret;

  memory.ram = ram;
  memory.size = sizeof(ram);

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);

  /* The 0+ at the start of the expression changes the accumulator to an integer,
   * so when float(4)*10 is added and prev(float(4))*10 is subtracted, they'll also
   * be converted to integers before they're combined.
   */

  /* float(4) = 1.5, prev(float(4)) = 0.0. 0+15-0=1 is false => 0 */
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* float(4) = 1.75, prev(float(4)) = 1.5. 0+17-15 => 2 => 2=1 is false => 0 */
  ram[7] = 0x3f; ram[6] = 0xe0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* float(4) = 1.82, prev(float(4)) = 1.75. 0+18-17 => 1 => 1=1 is true => 1 */
  ram[6] = 0xe8; ram[5] = 0xf5; ram[4] = 0xc3;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* float(4) = 2.06, prev(float(4)) = 1.82. 0+20-18 => 2 => 2=1 is false => 1 */
  ram[7] = 0x40; ram[6] = 0x03; ram[5] = 0xd7; ram[4] = 0x0a;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);
}

static void test_addhits_float_coercion_remembered() {
  rc_value_t* self;
  uint8_t ram[] = { 0x00, 0x06, 0x34, 0xAB, 0x00, 0x00, 0xC0, 0x3F }; /* fF0004 = 1.5 */
  memory_t memory;
  char buffer[2048];
  /* measured(tally(0, remembered(0 - prev(float(4)) * 10) + (0 + float(4) * 10) == 1)) */
  const char* memaddr = "A:0_B:dfF0004*10_K:0_A:0_A:fF0004*10_C:{recall}=1_M:0=1";
  int ret;

  memory.ram = ram;
  memory.size = sizeof(ram);

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);

  /* using remember allows for both sides to explicitly be cast to integer before
   * performing the subtraction. */

  /* float(4) = 1.5, prev(float(4)) = 0.0. 15-0 => 15=1 is false => 0 */
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* float(4) = 1.75, prev(float(4)) = 1.5. 17-15 => 2=1 is false => 0 */
  ram[7] = 0x3f; ram[6] = 0xe0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* float(4) = 1.82, prev(float(4)) = 1.75. 18-17 => 1=1 is true => 1 */
  ram[6] = 0xe8; ram[5] = 0xf5; ram[4] = 0xc3;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* float(4) = 2.06, prev(float(4)) = 1.82. 20-18 => 2=1 is false => 1 */
  ram[7] = 0x40; ram[6] = 0x03; ram[5] = 0xd7; ram[4] = 0x0a;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);
}

void test_value(void) {
  TEST_SUITE_BEGIN();

  /* ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56}; */

  /* classic format - supports multipliers, max, inversion */
  TEST_PARAMS2(test_evaluate_value, "V6", 6);
  TEST_PARAMS2(test_evaluate_value, "V6*2", 12);
  TEST_PARAMS2(test_evaluate_value, "V6*0.5", 3);
  TEST_PARAMS2(test_evaluate_value, "V-6", -6);
  TEST_PARAMS2(test_evaluate_value, "V-6*2", -12);

  TEST_PARAMS2(test_evaluate_value, "0xH0001_0xH0002", 0x12 + 0x34);
  TEST_PARAMS2(test_evaluate_value, "0xH0001*100_0xH0002*0.5_0xL0003", 0x12 * 100 + 0x34 / 2 + 0x0B);
  TEST_PARAMS2(test_evaluate_value, "0xH0001$0xH0002", 0x34);
  TEST_PARAMS2(test_evaluate_value, "0xH0001_0xH0004*3$0xH0002*0xL0003", 0x34 * 0x0B);
  TEST_PARAMS2(test_evaluate_value, "0xH0001_V-20", 0x12 - 20);
  TEST_PARAMS2(test_evaluate_value, "0xH0001_H10", 0x12 + 0x10);
  TEST_PARAMS2(test_evaluate_value, "100-0xH0002", 100 - 0x34);
  TEST_PARAMS2(test_evaluate_value, "0xh0000*-1_99_0xh0001*-100_5900", 4199);
  TEST_PARAMS2(test_evaluate_value, "v5900_0xh0000*-1.0_0xh0001*-100.0", 4100);
  TEST_PARAMS2(test_evaluate_value, "v5900_0xh0000*v-1_0xh0001*v-100", 4100);

  TEST_PARAMS2(test_evaluate_value, "0xH01*4", 0x12 * 4); /* multiply by constant */
  TEST_PARAMS2(test_evaluate_value, "0xH01*0.5", 0x12 / 2); /* multiply by fraction */
  TEST_PARAMS2(test_evaluate_value, "0xH01/2", 0x12 / 2); /* divide by constant */
  TEST_PARAMS2(test_evaluate_value, "0xH01*0xH02", 0x12 * 0x34); /* multiply by second address */
  TEST_PARAMS2(test_evaluate_value, "0xH01*0xT02", 0); /* multiply by bit */
  TEST_PARAMS2(test_evaluate_value, "0xH01*~0xT02", 0x12); /* multiply by inverse bit */
  TEST_PARAMS2(test_evaluate_value, "0xH01*~0xH02", 0x12 * (0x34 ^ 0xff)); /* multiply by inverse byte */

  TEST_PARAMS2(test_evaluate_value, "B0xH01", 12);
  TEST_PARAMS2(test_evaluate_value, "B0x00001", 3412);
  TEST_PARAMS2(test_evaluate_value, "B0xH03", 111); /* 0xAB not really BCD */
  TEST_PARAMS2(test_evaluate_value, "B0xW00", 341200);

  /* non-comparison measured values just return the value at the address and have no target */
  TEST_PARAMS2(test_measured_value_target, "M:0xH0002", 0);

  /* hitcount based measured values always have unbounded targets, even if one is specified */
  TEST_PARAMS2(test_measured_value_target, "M:0xH0002!=d0xH0002", (unsigned)-1);
  TEST_PARAMS2(test_measured_value_target, "M:0xH0002!=d0xH0002.99.", (unsigned)-1);
  /* measured values always assumed to be hitcount based - they do not stop/trigger when the condition is met */
  TEST_PARAMS2(test_measured_value_target, "M:0xH0002<100", (unsigned)-1);

  /* measured format - supports hit counts and combining flags
   * (AddSource, SubSource, AddHits, SubHits, AndNext, OrNext, and AddAddress) */
  TEST_PARAMS2(test_evaluate_value, "M:0xH0002", 0x34);
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001_M:0xH0002", 0x12 + 0x34);
  TEST_PARAMS2(test_evaluate_value, "B:0xH0001_M:0xH0002", 0x34 - 0x12);
  TEST_PARAMS2(test_evaluate_value, "C:0xH0000=0_M:0xH0002=52", 2);
  TEST_PARAMS2(test_evaluate_value, "C:0xH0000=0_D:0xH0001=18_M:0xH0002=52", 1);
  TEST_PARAMS2(test_evaluate_value, "N:0xH0000=0_M:0xH0002=52", 1);
  TEST_PARAMS2(test_evaluate_value, "O:0xH0000=0_M:0xH0002=0", 1);
  TEST_PARAMS2(test_evaluate_value, "I:0xH0000_M:0xH0002", 0x34);

  TEST_PARAMS2(test_evaluate_value, "M:0xH0002*2", 0x34 * 2);
  TEST_PARAMS2(test_evaluate_value, "M:0xH0002/2", 0x34 / 2);
  TEST_PARAMS2(test_evaluate_value, "M:0xH0002%2", 0x34 % 2);
  TEST_PARAMS2(test_evaluate_value, "M:0xH0001%3", 0x12 % 3);
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001*2_A:0xH0002*2_M:0", 0x12 * 2 + 0x34 * 2);
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001%3_A:0xH0002%4_M:0", 0x12 % 3 + 0x34 % 4);
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001*2_M:0xH0002*2", 0x12 * 2 + 0x34 * 2); /* multiplier in final condition */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001/2_M:0xH0002/2", 0x12 / 2 + 0x34 / 2);
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001%3_M:0xH0002%4", 0x12 % 3 + 0x34 % 4);
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001&15_M:0xH0002&15", (0x12 & 15) + (0x34 & 15));

  /* measured format does not support alt groups */
  TEST_PARAMS2(test_invalid_value, "M:0xH0002=6SM:0xH0003=6", RC_INVALID_VALUE_FLAG);
  /* does not start with X:, so legacy parser says it's an invalid memory accessor */
  TEST_PARAMS2(test_invalid_value, "SM:0xH0002=6SM:0xH0003=6", RC_INVALID_MEMORY_OPERAND);

  /* measured format does not support trigger flag */
  TEST_PARAMS2(test_invalid_value, "T:0xH0002=6", RC_INVALID_VALUE_FLAG);

  /* measured format requires a measured condition */
  TEST_PARAMS2(test_invalid_value, "A:0xH0002_0xH0003>10.99.", RC_INVALID_VALUE_FLAG); /* no flag on condition 2 */
  TEST_PARAMS2(test_invalid_value, "A:0xH0002_A:0xH0003", RC_MISSING_VALUE_MEASURED);

  /* measured value with float data */
  TEST_PARAMS2(test_evaluate_value, "M:fF0005", 3);               /* 3.141592 -> 3 */
  TEST_PARAMS2(test_evaluate_value, "A:fF0005*10_M:0", 31);       /* 3.141592 x 10 -> 31.415 -> 31 */
  TEST_PARAMS2(test_evaluate_value, "A:fF0005*f11.2_M:f6.9", 42); /* 3.141592 x 11.2 -> 35.185 + 6.9 ->  -> 42.085 -> 42 */
  TEST_PARAMS2(test_evaluate_value, "A:fF0005*f5.555555555555555555555555555555555555555555555556_M:f6.9", 24); /* 3.141592 x 5.555556 -> 17.4532902 + 6.9 ->  -> 24.353290 -> 24 */

  /* delta should initially be 0, so a hit will be tallied */
  TEST_PARAMS2(test_evaluate_value, "M:0xH0002!=d0xH0002", 1);

  /* division */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001/2_M:0", 9);       /* 18/2 = 9 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001/5_M:0", 3);       /* 18/5 = 3 */
  TEST_PARAMS2(test_evaluate_value, "M:0xH0001/5", 3);           /* 18/5 = 3 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0002/0xH0001_M:0", 2); /* 52/18 = 2 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001/0xH0002_M:0", 0); /* 18/52 = 0 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001/0xH0001_M:0", 1); /* 18/18 = 1 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001/0xH0000_M:0", 0); /* 18/0 = 0 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0000/0xH0000_M:0", 0); /* 0/0 = 0 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0000/0xH0001_M:0", 0); /* 0/18 = 0 */

  /* modulus */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001%3_M:0", 0);       /* 18%3 = 0 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001%5_M:0", 3);       /* 18%5 = 3 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001%7_M:0", 4);       /* 18%7 = 4 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0002%0xH0001_M:0", 16); /* 52%18 = 16 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001%0xH0002_M:0", 18); /* 18%52 = 18 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001%0xH0001_M:0", 0); /* 18%18 = 0 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001%0xH0000_M:0", 0); /* 18%0 = 0 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0000%0xH0000_M:0", 0); /* 0%0 = 0 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0000%0xH0001_M:0", 0); /* 0%18 = 0 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0000%0xH0001_M:0", 0); /* 0%18 = 0 */
  TEST_PARAMS2(test_evaluate_value, "A:f5.5%f2.0_M:0", 1) /* 5.5 % 2.0 = 1.5 -> 1 */
  TEST_PARAMS2(test_evaluate_value, "A:f123.7%f5.65_M:0", 5) /* 123.7 % 5.65 = 5.05 -> 5 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH01%f7.5_A:0xH02%f5.5_M:0", 5); /* 18%7.3 = 3.4, 52%5.5=2.5, becomes 3+2=5*/

  /* addition */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001+3_M:0", 21);       /* 18+3 = 21 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0002+0xH0001_M:0", 70); /* 52+18 = 70 */
  TEST_PARAMS2(test_evaluate_value, "A:fF5+f2_M:0", 5)            /* PI + 2.0 = 5.141592 -> 5 */
  TEST_PARAMS2(test_evaluate_value, "A:f5.5+f2.0_M:0", 7)         /* 5.5 + 2.0 = 7.5 -> 7 */
  TEST_PARAMS2(test_evaluate_value, "A:f5.5+f2.7_M:0", 8)         /* 5.5 + 2.7 = 8.2 -> 8 */
  TEST_PARAMS2(test_evaluate_value, "B:0xH0001+3_M:100", 79)      /* 100 - (18+3) = 79 */ 
  TEST_PARAMS2(test_evaluate_value, "I:0xH0000+3_M:0x0", 0x56AB)  /* Add Address (0+3) -> Offset 0. 16-Bit Read @ Byte 3 = 0x56AB */

  /* subtraction */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001-3_M:0", 15);       /* 18-3 = 15 */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0002-0xH0001_M:0", 34); /* 52-18 = 34 */
  TEST_PARAMS2(test_evaluate_value, "A:fF5-f2_M:0", 1)            /* PI - 2.0 = 1.141592 -> 1 */
  TEST_PARAMS2(test_evaluate_value, "A:f5.5-f2.0_M:0", 3)         /* 5.5 - 2.0 = 2.5 -> 3 */
  TEST_PARAMS2(test_evaluate_value, "A:f5.5-f2.7_M:0", 2)         /* 5.5 - 2.7 = 2.8 -> 2 */
  TEST_PARAMS2(test_evaluate_value, "B:0xH0001-3_M:100",85)       /* 100 - (18-3) = 85 */

  /* rounding */
  TEST_PARAMS2(test_evaluate_value, "0xH03/2_0xH03/2", 0xAA); /* integer division results in rounding */
  TEST_PARAMS2(test_evaluate_value, "0xH03/f2.0_0xH03/f2.0", 0xAB); /* float division does not result in rounding */
  TEST_PARAMS2(test_evaluate_value, "0xH03*0.5_0xH03*0.5", 0xAB); /* float multiplication does not result in rounding */
  TEST_PARAMS2(test_evaluate_value, "A:0xH03/2_A:0xH03/2_M:0", 0xAA); /* integer division results in rounding */
  TEST_PARAMS2(test_evaluate_value, "A:0xH03/f2.0_A:0xH03/f2.0_M:0", 0xAB); /* float division does not result in rounding */
  TEST_PARAMS2(test_evaluate_value, "I:0xH0001-17_M:0x0", 0x3412)  /* Add Address (18-17) -> Offset 0. 16-Bit Read @ Byte 1 = 0x3412 */

  /* using measured_if */
  TEST_PARAMS2(test_evaluate_value, "Q:0xH0001!=0_M:0xH0002", 0x34);
  TEST_PARAMS2(test_evaluate_value, "Q:0xH0001=0_M:0xH0002", 0);
  TEST_PARAMS2(test_evaluate_value, "Q:0xH0001!=0_M:1", 1);
  TEST(test_evaluated_and_next_measured_if_value);

  /* using accumulator */
  TEST_PARAMS2(test_evaluate_value, "K:0xH01_M:{recall}", 0x12); /* 18-> recall accumulator, Measurement = 18 */
  TEST_PARAMS2(test_evaluate_value, "K:0xH01_K:{recall}*2_M:{recall}", 0x24); /* 18-> recall accumulator, recall accumulator*2 -> recall accumulator, Measurement 18*2 = 36 */
  TEST_PARAMS2(test_evaluate_value, "K:0xH01*0xH02_M:{recall}", 0x3A8); /* 18*52-> recall accumulator, Measurement = 936 */
  TEST_PARAMS2(test_evaluate_value, "A:4_K:0xH01_K:{recall}*2_M:{recall}", 44); /* Chain Addsource into Remember (4 + 18) * 2 = 44 */
  TEST_PARAMS2(test_evaluate_value, "A:4_K:2*8_M:{recall}", 20); /* Chain Addsource into Remember 4 + (2 * 8) = 20 */
  TEST_PARAMS2(test_evaluate_value, "A:4_K:2*8_A:{recall}*2_M:4*{recall}", 120); /* Use remembered value multiple times */
  TEST_PARAMS2(test_evaluate_value, "K:0xH01*2_Q:{recall}<40_P:{recall}=36_M:{recall}", 36); /* Pause happens before recall accumulator is set because remember not part of pause chain. */
  TEST_PARAMS2(test_evaluate_value, "K:0xH01*2_P:{recall}=18_M:{recall}", 36); /* Measures the accumulated value, which was set in the pause pass. */
  TEST_PARAMS2(test_evaluate_value, "K:1_I:{recall}_M:0x02", 0x56AB); /* using recall accumulator as pointer */
  TEST_PARAMS2(test_evaluate_value, "K:1_I:{recall}_K:0x02_M:{recall}", 0x56AB); /* Use recall accumulator as pointer, then store pointed-to data in recall accumulator and measure that */
  TEST_PARAMS2(test_evaluate_value, "K:5_C:{recall}>3_C:{recall}<7_C:{recall}>5_M:0=1", 2); /* with addhits, reusing the recall accumulator in each.  */
  TEST_PARAMS2(test_evaluate_value, "K:5_A:1_M:{recall}", 6); /* Add Source onto a read of the recall accumulator as the value */
  TEST_PARAMS2(test_evaluate_value, "K:5_A:1_C:{recall}=6_B:1_C:{recall}=4_M:0=1", 2);
  TEST_PARAMS2(test_evaluate_value, "A:{recall}_M:1", 1); /* Using recall without remember. 0 + 1 = 1*/
  TEST_PARAMS2(test_evaluate_value, "K:{recall}^255_M:{recall}", 255); /* Using recall in first remember. 0x00^0xFF = 0xFF  */
  TEST_PARAMS2(test_evaluate_value, "A:1_K:2_M:{recall}", 3); /* Remembered value includes AddSource chain  */

  /* pause and reset affect hit count */
  TEST(test_evaluate_measured_value_with_pause);
  TEST(test_evaluate_measured_value_with_reset);

  /* overflow - 145406052 * 86 = 125049208332 -> 0x1D1D837E0C, leading 0x1D is truncated off */
  TEST_PARAMS2(test_evaluate_value, "0xX0001*0xH0004", 0x1D837E0C);

  test_typed_value_conversion();
  test_typed_value_addition();
  test_typed_value_multiplication();
  test_typed_value_division();
  test_typed_value_modulus();
  test_typed_value_negation();

  test_addhits_float_coercion();
  test_addhits_float_coercion_remembered();

  TEST_SUITE_END();
}
