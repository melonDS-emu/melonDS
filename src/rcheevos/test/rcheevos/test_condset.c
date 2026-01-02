#include "rc_internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

#include "../src/rc_compat.h"

static void _assert_parse_condset(rc_condset_t** condset, rc_memrefs_t* memrefs, void* buffer, const char* memaddr)
{
  rc_parse_state_t parse;
  int size;

  rc_init_parse_state(&parse, buffer);
  rc_init_parse_state_memrefs(&parse, memrefs);

  *condset = rc_parse_condset(&memaddr, &parse);
  size = parse.offset;
  rc_destroy_parse_state(&parse);

  ASSERT_NUM_GREATER(size, 0);
  ASSERT_PTR_NOT_NULL(*condset);
}
#define assert_parse_condset(condset, memrefs_out, buffer, memaddr) ASSERT_HELPER(_assert_parse_condset(condset, memrefs_out, buffer, memaddr), "assert_parse_condset")

static void _assert_evaluate_condset(rc_condset_t* condset, rc_memrefs_t* memrefs, memory_t* memory, int expected_result) {
  int result;
  rc_eval_state_t eval_state;

  rc_update_memref_values(memrefs, peek, memory);

  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = memory;

  result = rc_test_condset(condset, &eval_state);

  /* NOTE: reset normally handled by trigger since it's not group specific */
  if (eval_state.was_reset)
    rc_reset_condset(condset);

  ASSERT_NUM_EQUALS(result, expected_result);
}
#define assert_evaluate_condset(condset, memrefs, memory, expected_result) ASSERT_HELPER(_assert_evaluate_condset(condset, &memrefs, memory, expected_result), "assert_evaluate_condset")

static rc_condition_t* condset_get_cond(rc_condset_t* condset, int cond_index) {
  rc_condition_t* cond = condset->conditions;

  while (cond_index-- != 0) {
    if (cond == NULL)
      break;

    cond = cond->next;
  }

  return cond;
}

static void _assert_hit_count(rc_condset_t* condset, int cond_index, uint32_t expected_hit_count) {
  rc_condition_t* cond = condset_get_cond(condset, cond_index);
  ASSERT_PTR_NOT_NULL(cond);

  ASSERT_NUM_EQUALS(cond->current_hits, expected_hit_count);
}
#define assert_hit_count(condset, cond_index, expected_hit_count) ASSERT_HELPER(_assert_hit_count(condset, cond_index, expected_hit_count), "assert_hit_count")


static void test_hitcount_increment_when_true() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18"); /* one condition, true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1U);
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2U);
}

static void test_hitcount_does_not_increment_when_false() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001!=18"); /* one condition, false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0U);
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0U);
}

static void test_hitcount_target() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=20(2)_0xH0002=52");
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 2);

  /* hit target met, overall is true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 3);

  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2); /* hit target met, not incremented */
  assert_hit_count(condset, 1, 4);

  /* first condition no longer true, but hit count was met so it acts true */
  ram[1] = 18;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 5);
}

static void test_hitcount_two_conditions(const char* memaddr, int expected_result, uint32_t expected_hitcount1, uint32_t expected_hitcount2) {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, memaddr);
  assert_evaluate_condset(condset, memrefs, &memory, expected_result);
  assert_hit_count(condset, 0, expected_hitcount1);
  assert_hit_count(condset, 1, expected_hitcount2);
}

static void test_hitcount_three_conditions(const char* memaddr, int expected_result, uint32_t expected_hitcount1,
    uint32_t expected_hitcount2, uint32_t expected_hitcount3) {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, memaddr);
  assert_evaluate_condset(condset, memrefs, &memory, expected_result);
  assert_hit_count(condset, 0, expected_hitcount1);
  assert_hit_count(condset, 1, expected_hitcount2);
  assert_hit_count(condset, 2, expected_hitcount3);
}

static void test_pauseif() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18_P:0xH0002=52_P:0xL0x0004=6");

  /* first condition true, but ignored because both pause conditions are true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0); /* Also true, but processing stops on first PauseIf */

  /* first pause condition no longer true, but second still is */
  ram[2] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0); /* PauseIf goes to 0 when false */
  assert_hit_count(condset, 2, 1);

  /* both pause conditions not true, set will trigger */
  ram[4] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
}

static void test_pauseif_hitcount_one() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18_P:0xH0002=52.1.");

  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* pause condition no longer true, but hitcount prevents trigger */
  ram[2] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
}

static void test_pauseif_hitcount_two() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18_P:0xH0002=52.2.");

  /* pause hit target has not been met, group is true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);

  /* pause hit target has been met, group is false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 2);

  /* pause condition is no longer true, but hitcount prevents trigger */
  ram[2] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 2);
}

static void test_pauseif_hitcount_with_reset() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18_P:0xH0002=52.1._R:0xH0003=1");

  /* pauseif triggered, non-pauseif conditions ignored */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* pause condition is no longer true, but hitcount prevents trigger */
  ram[2] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* pause has precedence over reset, reset in group is ignored */
  ram[3] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
}

static void test_pauseif_resetnextif()
{
  uint8_t ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0002=0_0xH0001=18_Z:0xH0003=1_N:0xH0000=0_P:0xH0002=25.1.");

  /* accumulate hit counts */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1);
  assert_hit_count(condset, 4, 0);

  /* pauseif triggered, non-pauseif conditions ignored */
  ram[2] = 25;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 2);
  assert_hit_count(condset, 4, 1);

  /* pause condition is no longer true, but its hitcount prevents the non-pauseif conditions from being processed */
  ram[2] = 10;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 3);
  assert_hit_count(condset, 4, 1);

  /* reset next if clears hit on pause, but not on non-pauseif conditions */
  ram[3] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 1); /* resetnextif keeps its own hitcount */
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 0);

  /* trigger is true */
  ram[2] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 2); /* resetnextif keeps its own hitcount */
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 0);
}

static void test_pauseif_does_not_increment_hits() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18(2)_0xH0002=52_P:0xL0004=4");

  /* both conditions true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* pause condition is true, other conditions should not tally hits */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* pause condition not true, other conditions should tally hits */
  ram[4] = 0x56;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);

  /* pause condition is true, other conditions should not tally hits */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 1);

  /* pause condition not true, other conditions should tally hits */
  ram[4] = 0x56;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 0);
}

static void test_pauseif_delta_updated() {
  uint8_t ram[] = {0x00, 0x00, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "P:0xH0001=1_d0xH0002=60");

  /* upaused, delta = 0, current = 52 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* paused, delta = 52, current = 44 */
  ram[1] = 1;
  ram[2] = 44;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);

  /* paused, delta = 44, current = 60 */
  ram[2] = 60;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);

  /* unpaused, delta = 60, current = 97 */
  ram[1] = 0;
  ram[2] = 97;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
}

static void test_pauseif_indirect_delta_updated() {
  uint8_t ram[] = {0x00, 0x00, 0x34, 0x3C, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "P:0xH0001=1_I:0xH0000_d0xH0002=60");

  /* upaused, delta = 0, current = 52 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 2, 0);

  /* paused, delta = 52, current = 44 */
  ram[1] = 1;
  ram[2] = 44;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 2, 0);

  /* paused, delta = 44, current = 60 */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 2, 0);

  /* unpaused, delta = 60, current = 97 */
  ram[1] = 0;
  ram[3] = 97;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 2, 1);
}

static void test_pauseif_short_circuit() {
  uint8_t ram[] = {0x00, 0x00, 0x00, 0x00, 0x00};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* evaluation of an achievement stops at the first true pauseif condition
   *
   * this allows achievements to prevent accumulating hits on a pauselock farther down
   * in a group. a better solution would be to use an AndNext on the pauselock, but
   * there are achievements in the wild relying on this behavior.
   * see https://retroachievements.org/achievement/66804, which has a PauseIf 2040 frames
   * pass (condition 5), but don't tally those frames if the game is paused (condition 3).
   * similarly, https://retroachievements.org/achievement/154804 has a PauseIf 480 frames
   * (condition 5), but don't tally those frames if the map is visible (condition 4).
   */
  assert_parse_condset(&condset, &memrefs, buffer, "P:0xH0001=1_P:0xH0002=1.3._0xH0003=1.4.");

  /* nothing true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* non-pauseif true */
  ram[3] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);

  /* second pauseif tallies a hit, but it's not enough to pause the non-pauseif */
  ram[2] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 2);

  /* first pauseif is true, pauses the second pauseif and the non-pauseif */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 2);

  /* first pauseif is false, the second pauseif and the non-pauseif can update */
  ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 3);

  /* second pauseif reaches hitcount, non-pauseif does not update */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 3);

  /* pauseif hitcount still met, non-pauseif does not update */
  ram[2] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 3);
}

static void test_resetif() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18_R:0xH0002=50_R:0xL0x0004=4");

  /* first condition true, neither reset true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);

  /* first reset true */
  ram[2] = 50;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0); /* hitcount reset */

  /* both resets true */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);

  /* only second reset is true */
  ram[2] = 52;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);

  /* neither reset true */
  ram[4] = 0x56;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
}

static void test_resetif_cond_with_hittarget() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18(2)_0xH0002=52_R:0xL0004=4");

  /* both conditions true, reset not true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* hit target met */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);

  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 0);

  /* reset */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* reset no longer true, hit target not met */
  ram[4] = 0x56;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* hit target met */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);
}

static void test_resetif_hitcount() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18(2)_0xH0002=52_R:0xL0004=4.2.");

  /* hitcounts on conditions 1 and 2 are incremented */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* hitcounts on conditions 1 and 2 are incremented. cond 1 now true, so entire set is true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* hitcount on condition 2 is incremented, cond 1 already at its target */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 0);

  /* reset condition is true, but its hitcount is not met */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 4);
  assert_hit_count(condset, 2, 1);
  ASSERT_NUM_EQUALS(condset_get_cond(condset, 2)->is_true, 1);

  /* second hit on reset condition should reset everything */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  /* is_true=3 indicates the condition was true this frame, and its hit target was met before it got reset */
  ASSERT_NUM_EQUALS(condset_get_cond(condset, 2)->is_true, 3);
}

static void test_resetif_hitcount_one() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18(2)_0xH0002=52_R:0xL0004=4.1.");

  /* hitcounts on conditions 1 and 2 are incremented */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* hitcounts on conditions 1 and 2 are incremented. cond 1 now true, so entire set is true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* hitcount on condition 2 is incremented, cond 1 already at its target */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 0);

  /* reset condition is true, its hitcount is met, so all hitcounts (including the resetif) should be reset */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
}

static void test_resetif_hitcount_addhits() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* never(repeated(3, byte(1) == 18 || low(4) == 6)) */
  assert_parse_condset(&condset, &memrefs, buffer, "C:0xH0001=18_R:0xL0004=6(3)");

  /* result is true, no non-reset conditions */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  ASSERT_NUM_EQUALS(condset_get_cond(condset, 0)->is_true, 1);
  ASSERT_NUM_EQUALS(condset_get_cond(condset, 1)->is_true, 1);

  /* total hitcount is met (2 for first condition, 1 for second, need 3 total) , everything resets */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  ASSERT_NUM_EQUALS(condset_get_cond(condset, 0)->is_true, 1);
  /* is_true=2 indicates the condition was not true this frame, but its hit target was met before it got reset due to the AddHits */
  ASSERT_NUM_EQUALS(condset_get_cond(condset, 1)->is_true, 2);
}

static void test_pauseif_resetif_hitcounts() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18(2)_R:0xH0002=50_P:0xL0004=4");

  /* first condition is true, pauseif and resetif are not, so it gets a hit */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);

  /* pause is true, hit not incremented or reset */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);

  /* reset if true, but set is still paused */
  ram[2] = 50;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);

  /* set no longer paused, reset clears hitcount */
  ram[4] = 0x56;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);

  /* reset no longer true, hits increment again */
  ram[2] = 52;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);

  /* hitcount met, set is true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
}

static void test_resetnextif() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "Z:0xL0004=4_0xH0001=18(2)_0xH0002=52.4.");

  /* both conditions true, resetnextif not true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* hit target met */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 2);

  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 3);

  /* trigger resetnextif, last condition should not be reset */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 4);

  /* reset no longer true, hit target not met */
  ram[4] = 0x56;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 4);

  /* hit target met */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 4);
}

static void test_resetnextif_non_hitcount_condition() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* resetnextif for non-hitcount condition will still set the hitcount to 0 and make it false */
  assert_parse_condset(&condset, &memrefs, buffer, "Z:0xL0004=4_0xH0001=18_0xH0002=52.4.");

  /* both conditions true, resetnextif not true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* conditions continue to tally */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 2);

  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 3);

  /* target hitcount met, condset true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 4);
  assert_hit_count(condset, 2, 4);

  /* trigger resetnextif, last condition should not be reset */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 4);

  /* reset no longer true (hit count on reset kept), condset is true again */
  ram[4] = 0x56;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 4);
}

static void test_resetnextif_addhits() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ResetNextIf low4(0x0004)=4
   * AddHits     byte(0x0001)=18        <-- ResetNextIf resets hits on this condition before it's added to the accumulator
   *             upper4(0x0003)=10 (4)
   *             byte(0x0002)=52 (1)
   */
  assert_parse_condset(&condset, &memrefs, buffer, "Z:0xL0004=4_C:0xH0001=18_0xU0003=10(4)_0xH0002=52.1.");

  /* both conditions true, resetnextif not true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);

  /* total tallies match limit, trigger */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 2);
  assert_hit_count(condset, 3, 1);

  /* resetnextif resets hits on condition 2, but not condition 3 - total will be 3/4 - does not trigger */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 3);
  assert_hit_count(condset, 3, 1);
}

static void test_resetnextif_addhits_chain() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ResetNextIf low4(0x0004)=4
   * AddHits     byte(0x0001)=18
   * ResetNextIf low4(0x0004)=5
   * AddHits     byte(0x0000)=0
   *             upper4(0x0003)=10 (6)
   *             byte(0x0002)=52 (1)
   */
  assert_parse_condset(&condset, &memrefs, buffer, "Z:0xL0004=4_C:0xH0001=18_Z:0xL0004=5_C:0xH0000=0_0xU0003=10(6)_0xH0002=52.1.");

  /* resetnextifs not true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1);
  assert_hit_count(condset, 4, 1); /* <- total hits is 3/6 */
  assert_hit_count(condset, 5, 1);

  /* first resetnextif true, only affects first addhits condition */
  ram[4] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 2);
  assert_hit_count(condset, 4, 2); /* <- total hits is 4/6 */
  assert_hit_count(condset, 5, 1);

  /* second resetnextif true, only affects second addhits condition */
  ram[4] = 5;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 3); /* <- total hits is 4/6 */
  assert_hit_count(condset, 5, 1);

  /* total hits reaches hit target */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 2);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 4); /* <- total hits is 6/6 */
  assert_hit_count(condset, 5, 1);
}

static void test_resetnextif_addhits_chain_total() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* AddHits     byte(0x0001)=18
   * AddHits     byte(0x0000)=0
   * ResetNextIf low4(0x0004)=4
   *             upper4(0x0003)=10 (6)
   *             byte(0x0002)=52 (1)
   */
  assert_parse_condset(&condset, &memrefs, buffer, "C:0xH0001=18_C:0xH0000=0_Z:0xL0004=4_0xU0003=10(6)_0xH0002=52.1.");

  /* resetnextif not true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1); /* <- total hits is 3/6 */
  assert_hit_count(condset, 4, 1);

  /* resetnextif true, only affects that condition, not total */
  ram[4] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 0); /* <- total hits is 4/6 */
  assert_hit_count(condset, 4, 1);

  /* resetnextif still true, total matches target */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 2);
  assert_hit_count(condset, 3, 0); /* <- total hits is 6/6 */
  assert_hit_count(condset, 4, 1);
}

static void test_resetnextif_using_andnext() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* AndNext     byte(0x0001)=18
   * ResetNextIf low4(0x0004)=4
   *             upper4(0x0003)=10 (3)
   *             byte(0x0002)=52 (1)
   */
  assert_parse_condset(&condset, &memrefs, buffer, "N:0xH0001=18_Z:0xL0004=4_0xU0003=10(3)_0xH0002=52.1.");

  /* conditions 1, 3, and 4 true; resetnextif relies on conditions 1 and 2, so it not true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);

  /* both resetnextif conditions true */
  ram[4] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1);

  /* first part of resetnextif not true */
  ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);

  /* both resetnextif conditions true */
  ram[1] = 18;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1);
}

static void test_resetnextif_andnext() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ResetNextIf low4(0x0004)=4
   * AndNext     byte(0x0001)=18
   *             upper4(0x0003)=10 (3)
   *             byte(0x0002)=52 (1)
   */
  assert_parse_condset(&condset, &memrefs, buffer, "Z:0xL0004=4_N:0xH0001=18_0xU0003=10(3)_0xH0002=52.1.");

  /* both conditions true, resetnextif not true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);

  /* partial andnext not true */
  ram[3] = 0x86;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);

  /* andnext true again */
  ram[3] = 0xA0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 2);
  assert_hit_count(condset, 3, 1);

  /* resetnextif resets all hits in the andnext chain */
  ram[4] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1);
}

static void test_resetnextif_andnext_hitchain() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ResetNextIf low4(0x0004)=4
   * AndNext     byte(0x0001)=18 (2)
   *             upper4(0x0003)=10 (3)
   *             byte(0x0002)=52 (1)
   */
  assert_parse_condset(&condset, &memrefs, buffer, "Z:0xL0004=4_N:0xH0001=18.2._0xU0003=10(3)_0xH0002=52.1.");

  /* both conditions true, resetnextif not true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0); /* condition 2 must meet hitcount before condition three tallies a hit */
  assert_hit_count(condset, 3, 1);

  /* resetnextif true, should reset conditions 2 and 3 (3 was already 0) */
  ram[4] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1);

  /* resetnextif not true, condition 2 tallies a hit */
  ram[4] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1);

  /* resetnextif still not true, condition 2 tallies another hit, which allows condition 3 to tally a hit */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);

  /* resetnextif true, should reset both conditions 2 and 3 */
  ram[4] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1);
}

static void test_resetnextif_andnext_chain()
{
  uint8_t ram[] = { 0x00, 0x00, 0x00, 0x01, 0x00 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ResetNextIf byte(0x0001)!=0
   * AndNext     byte(0x0002)=0
   * ResetNextIf byte(0x0001)=0 (2)
   *             byte(0x0003)=1 (5)
   */
  assert_parse_condset(&condset, &memrefs, buffer, "Z:0xH0001!=0_N:0xH0002=0_Z:0xH0001=0.2._0xH0003=1.5.");

  /* first resetnextif not true, conditions 1, 2 and 3 are true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);

  /* first resetnextif true, conditions 1 and 2 should reset, but not 3 */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 2);

  /* first resetnextif not true, condition 1, 2 and 3 are true */
  ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 3);

  /* hitcount on condition 2 reached, condition 3 is reset */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 2);
  assert_hit_count(condset, 3, 0);
}

static void test_resetnextif_addaddress_andnext_chain()
{
  uint8_t ram[] = { 0x00, 0x00, 0x00, 0x01, 0x00 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* AddAddress  byte(0x0004)
   * ResetNextIf byte(0x0001)!=0
   * AndNext     byte(0x0002)=0
   * AddAddress  byte(0x0004)
   * ResetNextIf byte(0x0001)=0 (2)
   * AddAddress  byte(0x0004)
   *             byte(0x0003)=1 (5)
   * Trigger     byte(0x0003)=6
   */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0004_Z:0xH0001!=0_N:0xH0002=0_A:0xH0004_Z:0xH0001=0.2._A:0xH0004_0xH0003=1.5._T:0xH0003=6");

  /* first resetnextif not true, conditions 2, 4 and 6 are true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 4, 1);
  assert_hit_count(condset, 6, 1);

  /* first resetnextif true, conditions 2 and 4 should reset, but not 6 */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 4, 0);
  assert_hit_count(condset, 6, 2);

  /* first resetnextif not true, conditions 2, 4 and 6 are true */
  ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 4, 1);
  assert_hit_count(condset, 6, 3);

  /* hitcount on condition 4 reached, condition 6 is reset */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 2);
  assert_hit_count(condset, 4, 2);
  assert_hit_count(condset, 6, 0);

  /* allow last condition to reach hit target */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 6);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 4, 0);
  assert_hit_count(condset, 6, 5);

  /* first resetnextif not true, conditions 2, 4 and 6 are true */
  ram[1] = 0;
  ram[3] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 6);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 4, 1);
  assert_hit_count(condset, 6, 5);

  /* first resetnextif true, conditions 2 and 4 should reset, but not 6 */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 7);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 4, 0);
  assert_hit_count(condset, 6, 5);
}

static void test_resetnextif_addaddress() {
  uint8_t ram[] = {0x00, 0x00, 0x02, 0x03, 0x04};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000_Z:0xH0001=1_I:0xH0000_0xH0002=2(3)_0xH0004=4.8.");

  /* both conditions true, resetnextif not true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1);
  assert_hit_count(condset, 4, 1);

  /* resetnextif true */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 2);

  /* pointer changes. resetnextif not true, condition not true */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 3);

  /* condition true, resetnextif not true */
  ram[3] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1);
  assert_hit_count(condset, 4, 4);

  /* resetnextif true */
  ram[2] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 5);

  /* pointer changes, resetnextif and condition true */
  ram[0] = 0;
  ram[2] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 6);

  /* resetnextif not true */
  ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1);
  assert_hit_count(condset, 4, 7);
}

static void test_resetnextif_chain() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ResetNextIf low4(0x0004)=4
   * ResetNextIf byte(0x0001)=1
   *             upper4(0x0003)=10 (3)
   *             byte(0x0002)=52 (1)
   */
  assert_parse_condset(&condset, &memrefs, buffer, "Z:0xL0004=4_Z:0xH0001=1_0xU0003=10(3)_0xH0002=52.1.");

  /* both conditions true, resetnextifs not true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);

  /* second resetnextif true, resets first hit count */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1);

  /* first resetnextif true, disables second, allows hitcount */
  ram[4] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);

  /* second resetnextif no longer true, first still keeps it disabled */
  ram[1] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 2);
  assert_hit_count(condset, 3, 1);

  /* first resetnextif no longer true (hit count on resetnextif itself is not reset), second already false */
  ram[4] = 5;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 3);
  assert_hit_count(condset, 3, 1);
}

static void test_resetnextif_chain_andnext() {
  uint8_t ram[] = {0x00, 0x00, 0x01};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* AndNext     byte(0x0000)=0
   * ResetNextIf byte(0x0001)=1
   * AndNext     byte(0x0000)=1
   * ResetNextIf byte(0x0001)=1
   *             byte(0x0002)=1 (5)
   */
  assert_parse_condset(&condset, &memrefs, buffer, "N:0xH0000=0_Z:0xH0001=1_N:0xH0000=1_Z:0xH0001=1_0xH0002=1.5.");

  /* no ResetNextIf true. hit count incremented */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1); /* hit captured on byte(0) == 0 */
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 1);

  /* second ResetNextIf clause true */
  ram[0] = ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1); /* ResetNextIf on condition 3 doesn't affect condition 1 */
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);
  assert_hit_count(condset, 4, 0);

  /* no ResetNextIf true, hit count incremented */
  ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 2);
  assert_hit_count(condset, 3, 1);
  assert_hit_count(condset, 4, 1);

  /* first ResetNextIf clause true */
  ram[0] = 0;
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0); /* first ResetNextIf affects second ResetNextIf */
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 2); /* but not final clause */
}

static void test_resetnextif_chain_with_hits() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ResetNextIf low4(0x0004)=4
   * ResetNextIf byte(0x0001)=1 (2)
   *             upper4(0x0003)=10 (3)
   *             byte(0x0002)=52 (1)
   */
  assert_parse_condset(&condset, &memrefs, buffer, "Z:0xL0004=4_Z:0xH0001=1(2)_0xU0003=10(8)_0xH0002=52.1.");

  /* both conditions true, resetnextifs not true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);

  /* second resetnextif true, but hit target not met */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 2);
  assert_hit_count(condset, 3, 1);

  /* first resetnextif true, resets second, allows hitcount */
  ram[4] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 3);
  assert_hit_count(condset, 3, 1);

  /* second resetnextif no longer true, first still keeps it disabled */
  ram[1] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 4);
  assert_hit_count(condset, 3, 1);

  /* first resetnextif no longer true (hit count on resetnextif itself is not reset), second already false */
  ram[4] = 5;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 5);
  assert_hit_count(condset, 3, 1);

  /* second resetnextif true again, but hitcount not met */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 6);
  assert_hit_count(condset, 3, 1);

  /* second resetnextif hitcount met */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1);

  /* second resetnextif condition no longer true, but hitcount keeps it active */
  ram[1] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 1);

  /* first resetnextif true, resets second, allows hit on third condition */
  ram[4] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);
}

static void test_resetnextif_pause_lock() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ResetNextIf byte(0x0002)=1
   * PauseIf     byte(0x0001)=1 (1)
   */
  assert_parse_condset(&condset, &memrefs, buffer, "Z:0xH0002=1_P:0xH0001=1(1)");

  /* both conditions false */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* reset next true */
  ram[2] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);

  /* reset next and pause true */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);

  /* only pause true */
  ram[2] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 1);

  /* both conditions true */
  ram[2] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 0);

  /* only pause true */
  ram[2] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 1);

  /* both conditions false */
  ram[1] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 1);

  /* reset next true */
  ram[2] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 4);
  assert_hit_count(condset, 1, 0);
}

static void test_resetnextif_unfinished() {
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[512];

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=0_Z:0xH0002=22");

  ASSERT_NUM_EQUALS(condset->num_other_conditions, 2);
}

static void test_addsource() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) + byte(2) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001=0_0xH0002=22");

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* sum is correct */
  ram[2] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1); /* hit only tallied on final condition */

  /* first condition is true, but not sum */
  ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* first condition is true, sum is correct */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addsource_overflow() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* adding two bytes will result in a value larger than 256, don't truncate to a byte */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001=0_0xH0002=22");

  /* sum is 0x102 (0x12 + 0xF0) */
  ram[2] = 0xF0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* sum is 0x122 (0x32 + 0xF0) */
  ram[1] = 0x32;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_subsource() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* NOTE: SubSource subtracts the first item from the second! */
  /* byte(1) - byte(2) == 14 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0002=0_0xH0001=14");

  /* difference is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* difference is correct */
  ram[2] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1); /* hit only tallied on final condition */

  /* first condition is true, but not difference */
  ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* first condition is true, difference is negative inverse of expected value */
  ram[2] = 14;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is correct again */
  ram[1] = 28;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_subsource_legacy_garbage() {
  uint8_t ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) - byte(2) == 14 */
  /* old serializers would store the comparison and right-side value from the condition before it was converted to SubSource */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0002=0xH0000_0xH0001=14");

  /* difference is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* difference is correct */
  ram[2] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1); /* hit only tallied on final condition */

  /* first condition is true, but not difference */
  ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* first condition is true, difference is negative inverse of expected value */
  ram[2] = 14;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is correct again */
  ram[1] = 28;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_subsource_overflow() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* subtracting two bytes will result in a very large positive number, don't truncate to a byte */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0002=0_0xH0001=14");

  /* difference is -10 (8 - 18) */
  ram[2] = 8;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* difference is 0xFFFFFF0E (8 - 0xFA) */
  ram[1] = 0xFA;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_addsource_subsource() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) - low(2) + low(4) == 14 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001=0_B:0xL0002=0_0xL0004=14");

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* sum is correct */
  ram[1] = 12;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);

  /* first condition is true, but not sum */
  ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);

  /* byte(4) would make sum true, but not low(4) */
  ram[4] = 0x12;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);

  /* difference is correct again */
  ram[2] = 1;
  ram[4] = 15;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 2);
}

static void test_addsource_multiply() {
  uint8_t ram[] = {0x00, 0x06, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) * 3 + byte(2) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001*3_0xH0002=22");

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* sum is correct */
  ram[2] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is not correct */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is correct */
  ram[2] = 19;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_subsource_multiply() {
  uint8_t ram[] = {0x00, 0x06, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(2) - byte(1) * 3 == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0001*3_0xH0002=14");

  /* difference is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* difference is correct */
  ram[2] = 32;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is not correct */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is correct */
  ram[2] = 17;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addsource_multiply_fraction() {
  uint8_t ram[] = {0x00, 0x08, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) * 0.75 + byte(2) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001*f0.75_0xH0002=22");

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* sum is correct */
  ram[2] = 16;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is not correct */
  ram[1] = 15;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is correct */
  ram[2] = 11;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addsource_multiply_address() {
  uint8_t ram[] = {0x00, 0x06, 0x04, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) * byte(0) + byte(2) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001*0xH00000_0xH0002=22");

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* sum is correct */
  ram[0] = 3;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is not correct */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is correct */
  ram[2] = 19;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addsource_divide() {
  uint8_t ram[] = {0x00, 0x06, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) / 3 + byte(2) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001/3_0xH0002=22");

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* sum is correct */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is not correct */
  ram[1] = 14;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is correct */
  ram[2] = 18;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addsource_compare_percentage() {
  uint8_t ram[] = {0x00, 0x06, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* (byte(0)/byte(1) > 0.5) => (byte(1) * 0.5) < byte(0) */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001*f0.5_0<0xH0000");

  /* 0/6 > 50% = false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  ram[0] = 2; assert_evaluate_condset(condset, memrefs, &memory, 0); /* 2/6 > 50% = false */
  ram[0] = 4; assert_evaluate_condset(condset, memrefs, &memory, 1); /* 4/6 > 50% = true */
  ram[1] = 7; assert_evaluate_condset(condset, memrefs, &memory, 1); /* 4/7 > 50% = true */
  ram[0] = 3; assert_evaluate_condset(condset, memrefs, &memory, 0); /* 3/7 > 50% = false */
}

static void test_subsource_divide() {
  uint8_t ram[] = {0x00, 0x06, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(2) - byte(1) / 3 == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0001/3_0xH0002=14");

  /* difference is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* difference is correct */
  ram[2] = 16;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is not correct */
  ram[1] = 14;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is correct */
  ram[2] = 18;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addsource_divide_address() {
  uint8_t ram[] = {0x00, 0x06, 0x10, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) / byte(0) + byte(2) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001/0xH00000_0xH0002=22");

  /* sum is not correct (divide by zero) */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* sum is correct */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is not correct */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is correct */
  ram[2] = 21;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addsource_divide_self() {
  uint8_t ram[] = {0x00, 0x06, 0x10, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) / byte(1) + byte(2) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001/0xH00001_0xH0002=22");

  /* sum is not correct (1 + 16 != 22) */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* sum is correct (1 + 21 == 22) */
  ram[2] = 21;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is not correct (0 + 21 == 22) */
  ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is correct (0 + 22 == 22) */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addsource_mask() {
  uint8_t ram[] = {0x00, 0x06, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) & 0x07 + byte(2) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001&h7_0xH0002=22");

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* sum is correct */
  ram[2] = 16;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is not correct */
  ram[1] = 0x74;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is correct */
  ram[2] = 18;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addsource_xor() {
  uint8_t ram[] = {0x00, 0x06, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) ^ 0x05 + byte(2) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001^h5_0xH0002=22");

  /* sum (6 ^ 5 + 52 == 22) is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* sum ((6 ^ 5 = 3) + 19 = 22) is correct */
  ram[2] = 19;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum ((17 ^ 5 = 20) + 19 = 22) is not correct */
  ram[1] = 0x11;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum ((17 ^ 5 = 20) + 2 = 22) is correct */
  ram[2] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addsource_float_first() {
  uint8_t ram[] = {0x00, 0x06, 0x34, 0xAB, 0x00, 0x00, 0xC0, 0x3F}; /* fF0004 = 1.5 */
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* float(4) + float(4) + float(4) + 1 == 5.5 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:fF0004_A:fF0004_A:fF0004_1=f5.5");

  /* +1 (int) will convert the intermediate value to an integer
   *    (1.5 + 1.5 + 1.5) => 4.5 => 4 + 1 = 5
   * which will not equal 5.5, so logic will fail. */

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* float(4) + float(4) + float(4) + 1.0 == 5.5 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:fF0004_A:fF0004_A:fF0004_f1.0=f5.5");

  /* +1.0 (float) will not convert the intermediate value to an integer
   *    (1.5 + 1.5 + 1.5) => 4.5 + 1.0 = 5.5
   * which will equal 5.5, so logic will succeed. */

  /* sum is correct */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addsource_float_second() {
  uint8_t ram[] = {0x00, 0x06, 0x34, 0xAB, 0x00, 0x00, 0xC0, 0x3F}; /* fF0004 = 1.5 */
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* 1 + float(4) + float(4) + float(4) == 5.5 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:1_A:fF0004_A:fF0004_fF0004=f5.5");

  /* the first value defines the type of the accumulator. since it's an integer, each float will be
   * truncated when it is added to the accumulator, so 1 + (1.5) + (1.5) is 3. when the accumulator is
   * added to the final value, the accumulator is converted to match the final value, so 3.0 + 1.5 = 4.5. */

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 1 + float(4) + float(4) + float(4) == 4.5 (note: two intermediary floats are converted to int, but not last) */
  assert_parse_condset(&condset, &memrefs, buffer, "A:1_A:fF0004_A:fF0004_fF0004=f4.5");
  /* sum is correct */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 1 + float(4) + float(4) + float(4) + 0 == 4.0 (note: all intermediate floats are converted to int) */
  assert_parse_condset(&condset, &memrefs, buffer, "A:1_A:fF0004_A:fF0004_A:fF0004_0=f4.0");
  /* sum is correct */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_subsource_mask() {
  uint8_t ram[] = {0x00, 0x6C, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(2) - byte(1) & 0x06 == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0001&6_0xH0002=14");

  /* difference is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* difference is correct */
  ram[2] = 18;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is not correct */
  ram[1] = 10;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is correct */
  ram[2] = 16;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_subsource_overflow_comparison_equal() {
  uint8_t ram[] = {0x00, 0x6C, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ASSERT: "A==B" can be expressed as "-A+B==0" */

  /* - byte(0) + byte(1) = 0 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0000_0xH0001=0");

  /* 1 == 0 = false */
  ram[0] = 1; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 1 == 1 = true */
  ram[0] = 1; ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 0 == 0 = true */
  ram[0] = 0; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 0 == 1 = false */
  ram[0] = 0; ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 0 == 255 = false */
  ram[0] = 0; ram[1] = 255;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 255 == 255 = true */
  ram[0] = 255; ram[1] = 255;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 255 == 254 = false */
  ram[0] = 255; ram[1] = 254;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 255 == 0 = false */
  ram[0] = 255; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_subsource_overflow_comparison_greater() {
  uint8_t ram[] = {0x00, 0x6C, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ASSERT: "A>B" can be expressed as "-A+B>M" where M is the largest number that cannot be 
   * represented by A or B */

  /* - byte(0) + byte(1) > 256 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0000_0xH0001>256");

  /* 1 > 0 = true */
  ram[0] = 1; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 1 > 1 = false */
  ram[0] = 1; ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 0 > 0 = false */
  ram[0] = 0; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 0 > 1 = false */
  ram[0] = 0; ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 0 > 255 = false */
  ram[0] = 0; ram[1] = 255;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 255 > 255 = false */
  ram[0] = 255; ram[1] = 255;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 255 > 254 = true */
  ram[0] = 255; ram[1] = 254;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 255 > 0 = true */
  ram[0] = 255; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_subsource_overflow_comparison_greater_or_equal() {
  uint8_t ram[] = {0x00, 0x6C, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ASSERT: "A>=B" can be expressed as "-A-1+B>=M" where M is the largest number that cannot be 
   * represented by A or B */

  /* - byte(0) - 1 + byte(1) > 256 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0000_B:1_0xH0001>=256");

  /* 1 >= 0 = true */
  ram[0] = 1; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 1 >= 1 = true */
  ram[0] = 1; ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 0 >= 0 = true */
  ram[0] = 0; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 0 >= 1 = false */
  ram[0] = 0; ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 0 >= 255 = false */
  ram[0] = 0; ram[1] = 255;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 255 >= 255 = true */
  ram[0] = 255; ram[1] = 255;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 255 >= 254 = true */
  ram[0] = 255; ram[1] = 254;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 255 >= 0 = true */
  ram[0] = 255; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_subsource_overflow_comparison_lesser() {
  uint8_t ram[] = {0x00, 0x6C, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ASSERT: "A<B" can be expressed as "-A+B+M>M" where M is the largest number that cannot be 
   * represented by A or B */

  /* - byte(0) + byte(1) + 256 > 256 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0000_A:0xH0001_256>256");

  /* 1 < 0 = false */
  ram[0] = 1; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 1 < 1 = false */
  ram[0] = 1; ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 0 < 0 = false */
  ram[0] = 0; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 0 < 1 = true */
  ram[0] = 0; ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 0 < 255 = true */
  ram[0] = 0; ram[1] = 255;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 255 < 255 = false */
  ram[0] = 255; ram[1] = 255;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 254 < 255 = true */
  ram[0] = 254; ram[1] = 255;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 255 < 0 = false */
  ram[0] = 255; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_subsource_overflow_comparison_lesser_or_equal() {
  uint8_t ram[] = {0x00, 0x6C, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ASSERT: "A<B" can be expressed as "-A+B+M>M" where M is the largest number that cannot be 
   * represented by A or B */

  /* - byte(0) + byte(1) + 256 >= 256 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0000_A:0xH0001_256>=256");

  /* 1 <= 0 = false */
  ram[0] = 1; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 1 <= 1 = true */
  ram[0] = 1; ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 0 <= 0 = true */
  ram[0] = 0; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 0 <= 1 = true */
  ram[0] = 0; ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 0 <= 255 = true */
  ram[0] = 0; ram[1] = 255;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 255 <= 255 = true */
  ram[0] = 255; ram[1] = 255;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 254 <= 255 = true */
  ram[0] = 254; ram[1] = 255;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 255 <= 0 = false */
  ram[0] = 255; ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_subsource_float() {
  uint8_t ram[] = {0x06, 0x00, 0x00, 0x00, 0x92, 0x44, 0x9A, 0x42}; /* fF0004 = 77.133926 */
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* float(0x0004) - word(0) * 1.666667 > 65.8 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0x 0000*f1.666667_fF0004>f65.8");

  /* 77.133926 - (6 * 1.666667) = 77.133926 - 10 = 67.133926 */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 77.133926 - (7 * 1.666667) = 77.133926 - 11.6667 = 65.4672 */
  ram[0] = 7;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_addsource_bits_change() {
  uint8_t ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* bit0(0x01) + bit1(0x01) + bit2(0x01) == 3 && delta(bit0(0x01)) + delta(bit1(0x01)) + delta(bit2(0x01)) == 2 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xM0001_A:0xN0001_0xO0001=3_A:d0xM0001_A:d0xN0001_d0xO0001=2");

  /* bit sum = 1 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 5, 0);

  /* bit sum = 3 (delta = 1) */
  ram[1] = 0x17;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 5, 0);

  /* bit sum = 2 (delta = 3) */
  ram[1] = 0x16;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 5, 0);

  /* bit sum = 1 (delta = 2) */
  ram[1] = 0x14;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 5, 1);

  /* bit sum = 2 (delta = 1) */
  ram[1] = 0x16;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 5, 1);

  /* bit sum = 3 (delta = 2) */
  ram[1] = 0x17;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 2, 2);
  assert_hit_count(condset, 5, 2);
}

static void test_addsource_bcd() {
  uint8_t ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* bcd(byte(0)) * 100 + bcd(byte(1)) > 4990 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:b0xH0000*100_b0xH0001>4990");

  /* 0*100+12 = 12 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 49*100+89 = 4989 */
  ram[0] = 0x49;
  ram[1] = 0x89;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 49*100+99 = 4999 */
  ram[1] = 0x99;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 50*100+00 = 5000 */
  ram[0] = 0x50;
  ram[1] = 0x00;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addsource_invert() {
  uint8_t ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ~low4(0) + ~high4(0) > 20 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:~0xL0000_~0xU0000>20");

  /* ~0 (F) + ~0 (F) = 1E */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* ~4 (B) + ~9 (6) = 11 */
  ram[0] = 0x49;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* ~9 (6) + ~9 (6) = 0C */
  ram[1] = 0x99;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* ~5 (A) + ~1 (E) = 18 */
  ram[0] = 0x51;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_subsource_mem_delta() {
  uint8_t ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* -byte(0) + prior(byte(0)) = 10 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0000_d0xH0000=10");

  /* -0 + 0 = 10 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* -5 + 0 = 10 */
  ram[0] = 5;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* -15 + 5 = 10 */
  ram[0] = 15;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* -5 + 15 = 10 */
  ram[0] = 5;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* -5 + 5 = 10 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_subsource_mem_prior() {
  uint8_t ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* -byte(0) + prior(byte(0)) = 10 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0000_p0xH0000=10");

  /* -0 + 0 = 10 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* -5 + 0 = 10 */
  ram[0] = 5;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* -15 + 5 = 10 */
  ram[0] = 15;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* -5 + 15 = 10 */
  ram[0] = 5;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* -5 + 15 = 10 */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addsource_unfinished() {
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[512];

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=0_A:0xH0002=22");

  ASSERT_NUM_EQUALS(condset->num_other_conditions, 1);
  ASSERT_NUM_EQUALS(condset->num_indirect_conditions, 1);
}

static void test_addsource_float_division()
{
  uint8_t ram[] = { 0x00, 0x06, 0x34, 0xAB, 0x00, 0x00, 0x80, 0x40 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0) / remember(float(4)) > 0.5 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0000/f4.0_f0.0>f1.4");

  /* 0 / 4.0 > 1.4 = false (0.00) */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 6 / 4.0 > 1.4 = true (1.50) */
  ram[0] = 6;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 5 / 4.0 > 1.4 = false (1.25) */
  ram[0] = 5;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 7 / 4.0 > 1.4 = true (1.75) */
  ram[0] = 7;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addsource_recall_float_division() {
  uint8_t ram[] = {0x00, 0x06, 0x34, 0xAB, 0x00, 0x00, 0x80, 0x40}; /* fF0004 = 4.0 */
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0) / remember(float(4)) > 0.5 */
  assert_parse_condset(&condset, &memrefs, buffer, "K:fF0004_A:0xH0000/{recall}_f0.0>f1.4");

  /* 0 / 4.0 > 1.4 = false (0.00) */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 6 / 4.0 > 1.4 = true (1.50) */
  ram[0] = 6;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* 5 / 4.0 > 1.4 = false (1.25) */
  ram[0] = 5;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 7 / 4.0 > 1.4 = true (1.75) */
  ram[0] = 7;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addsource_long_chain() {
  /* assume 22 bytes per cond (10 for base, 11 for delta, 1 for separator)
   * and approximately 320 bytes per cond when convered to data structures.
   * 320+22 = 342. Round that up to 384 just to be safe.
   */
  const size_t cond_count = 1500;
  const size_t buffer_size = 384 * cond_count;
  char* buffer = (char*)malloc(buffer_size);
  char* ptr = buffer;
  const char* memaddr = buffer;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  rc_parse_state_t parse;
  clock_t start, end;
  size_t i, len, remaining;

  ASSERT_PTR_NOT_NULL(buffer);

  remaining = buffer_size;
  for (i = 0; i < cond_count; i++) {
    len = snprintf(ptr, remaining, "A:0xH%04x_", (uint32_t)i);
    remaining -= len;
    ptr += len;
  }
  len = snprintf(ptr, remaining, "0=500");
  remaining -= len;
  ptr += len;
  for (i = 0; i < cond_count; i++) {
    len = snprintf(ptr, remaining, "_A:d0xH%04x", (uint32_t)i);
    remaining -= len;
    ptr += len;
  }
  ptr += snprintf(ptr, remaining, "=499");

  ptr = (char*)RC_ALIGN((size_t)ptr);
  remaining = buffer_size - (ptr - buffer);

  rc_init_parse_state(&parse, ptr);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  start = clock();
  condset = rc_parse_condset(&memaddr, &parse);
  end = clock();

  rc_destroy_parse_state(&parse);

  ASSERT_NUM_GREATER(parse.offset, 0);
  ASSERT_NUM_LESS(parse.offset, remaining);
  ASSERT_PTR_NOT_NULL(condset);

  double elapsed = (double)(end - start) * 1000 / CLOCKS_PER_SEC;
  /* this shouldn't take more than 20-40ms (depending on the machine its running on).
   * allow up to 1 full second before this errors. it was taking over 10s when reported */
  ASSERT_NUM_LESS(elapsed, 1000);

  /* each condition and its delta should share a memref */
  ASSERT_NUM_EQUALS(rc_memrefs_count_memrefs(&memrefs), cond_count);
  /* one modified memref should be generated for each condition past the first
   * plus one for the final condition: cond_count - 1 + 1 */
  ASSERT_NUM_EQUALS(rc_memrefs_count_modified_memrefs(&memrefs), cond_count);

  free(buffer);
}

static void test_addhits() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /*  repeated(4, byte(1) == 18 || low(4) == 6) */
  assert_parse_condset(&condset, &memrefs, buffer, "C:0xH0001=18(2)_0xL0004=6(4)");

  /* both conditions true, total not met */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);

  /* total hits met (two for each condition) */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);

  /* target met for first, it stops incrementing, second continues */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 3);

  /* reset hit counts */
  rc_reset_condset(condset);

  /* both conditions true, total not met */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);

  /* first condition not true, total not met*/
  ram[1] = 16;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 2);

  /* total met */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 3);
}

static void test_addhits_multiple() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /*  repeated(4, byte(1) == 18 || low(4) == 6) */
  assert_parse_condset(&condset, &memrefs, buffer, "C:0xH0001=18(2)_0xL0004=6(4)_0xL0004=6(3)");

  /* both conditions true, total not met */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* total hits met (two for each condition), third condition not yet met */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 2);

  /* target met for first, it stops incrementing, second and third continue */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 3);
}

static void test_addhits_no_target() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* AddHits is not a substitution for OrNext */
  /* since the second condition doesn't have a target hit count, the hits tallied by the first condition are ignored */
  assert_parse_condset(&condset, &memrefs, buffer, "C:0xH0001=18_0xH0000=1");

  /* first condition true, but ignored */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);

  /* second condition true, overall is true */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 1);

  /* second condition no longer true, overall is not true */
  ram[0] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 1);
}

static void test_addhits_with_addsource() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* repeated(2, (byte(1) + byte(2) == 70) || byte(0) == 0) */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001_C:0xH0002=70_0xH0000=0(2)");

  /* addsource (conditions 1 and 2) is true, condition 3 is true, total of two hits, overall is true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* repeated(2, byte(0) == 0 || (byte(1) + byte(2) == 70)) */
  assert_parse_condset(&condset, &memrefs, buffer, "C:0xH0000=0_A:0xH0001=0_0xH0002=70(2)");

  /* condition 1 is true, addsource (conditions 2 and 3) is true, total of two hits, overall is true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);
}

static void test_subhits() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* repeated(4, low(4) == 6, deducting = repeated(2, byte(1) == 16)) */
  /* NOTE: cannot have SubHits without AddHits as there's no way to reach the final hit target 
   *       if hits are subtracted but not added */
  assert_parse_condset(&condset, &memrefs, buffer, "D:0xH0001=16(2)_C:0xL0004=6_0=1(4)");

  /* second condition true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);

  /* both conditions true 1+3 == 4, not -1+3 != 4, no trigger */
  ram[1] = 16;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 3);

  /* -2+4 != 4 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 4);

  /* first condition target met, -2+5 != 4 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 5);

  /* total met */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 6);
}

static void test_subhits_below_zero() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* repeated(4, low(4) == 6, deducting = repeated(2, byte(1) == 16)) */
  /* NOTE: cannot have SubHits without AddHits as there's no way to reach the final hit target 
   *       if hits are subtracted but not added */
  assert_parse_condset(&condset, &memrefs, buffer, "D:0xH0001=18(2)_C:0xL0002=6_0=1(4)");

  /* first condition true. -1 less than 0. target hit count is unsigned.
     make sure comparison doesn't treat -1 as unsigned */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);

  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);

  /* first condition target met */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);

  /* both conditions true. takes 6 counts on second condition to reach hit target because 
     first condition is currently -2 */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 1);

  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);

  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 3);

  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 4);

  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 5);

  /* total met */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 6);
}

static void test_addhits_unfinished() {
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[512];

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=0_C:0xH0002=22");

  ASSERT_NUM_EQUALS(condset->num_other_conditions, 2);
}

static void test_addhits_float_coercion() {
  uint8_t ram[] = { 0x00, 0x06, 0x34, 0xAB, 0x00, 0x00, 0xC0, 0x3F }; /* fF0004 = 1.5 */
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* 0 + float(4) * 10 - prev(float(4)) * 10 + 0 + 0 = 1 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0_A:fF0004*10_B:dfF0004*10_A:0_0=1");

  /* float(4) = 1.5, prev(float(4)) = 0.0. 0+15-0+0+0=1 = false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* float(4) = 2.0, prev(float(4)) = 1.5. 0+20-15+0+0=1 = false */
  ram[6] = 0x00;
  ram[7] = 0x40;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* float(4) = 2.1, prev(float(4)) = 2.0. 0+21-20+0+0=1 = true */
  ram[6] = 0x06;
  ram[5] = 0x66;
  ram[4] = 0x66;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_andnext() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* repeated(3, byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) */
  assert_parse_condset(&condset, &memrefs, buffer, "N:0xH0001=20_N:0xH0002=20_0xH0003=20.3.");

  /* all conditions are false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* final condition is not enough */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* first two are true, still not enough */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* all three are true, tally hits. hits are tallied for each true statement starting with the first */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* middle condition not true, only first tallies a hit */
  ram[2] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* all three conditions are true */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 2);

  /* third condition not true, first two tally hits */
  ram[3] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 4);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 2);

  /* all three conditions are true, hit target reached */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 5);
  assert_hit_count(condset, 1, 4);
  assert_hit_count(condset, 2, 3);

  /* hit target previously reached */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 6);
  assert_hit_count(condset, 1, 5);
  assert_hit_count(condset, 2, 3);

  /* second condition no longer true, only first condition tallied, hit target was previously met */
  ram[2] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 7);
  assert_hit_count(condset, 1, 5);
  assert_hit_count(condset, 2, 3);
}

static void test_andnext_boundaries() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0000) == 0 && once(byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) && byte(0x0000) == 0 */
  assert_parse_condset(&condset, &memrefs, buffer, "0xH0000=0_N:0xH0001=20_N:0xH0002=20_0xH0003=20.1._0xH0000=0");

  /* first and last condition are true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 1);

  /* final condition of AndNext chain is not enough */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 2);

  /* two conditions of AndNext chain are true, still not enough */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 3);

  /* whole AndNext chain is true */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 4);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);
  assert_hit_count(condset, 4, 4);
}

static void test_andnext_resetif() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0000) == 0 && never(byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) */
  assert_parse_condset(&condset, &memrefs, buffer, "0xH0000=0_N:0xH0001=20_N:0xH0002=20_R:0xH0003=20");

  /* tally a hit */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* final condition of AndNext chain is not enough */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* two conditions of AndNext chain are true, still not enough */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* whole AndNext chain is true */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* middle condition not true */
  ram[2] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* whole AndNext chain is true */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* third condition not true */
  ram[3] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 0);

  /* whole AndNext chain is true */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
}

static void test_andnext_pauseif() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0000) == 0 && unless(byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) */
  assert_parse_condset(&condset, &memrefs, buffer, "0xH0000=0_N:0xH0001=20_N:0xH0002=20_P:0xH0003=20");

  /* tally a hit */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* final condition of AndNext chain is not enough */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* two conditions of AndNext chain are true, still not enough */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* whole AndNext chain is true */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);

  /* middle condition not true */
  ram[2] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 4);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 0); /* pauseif goes to 0 when not true */

  /* whole AndNext chain is true */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 4);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 2);
  assert_hit_count(condset, 3, 1);

  /* third condition not true */
  ram[3] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 5);
  assert_hit_count(condset, 1, 4);
  assert_hit_count(condset, 2, 3);
  assert_hit_count(condset, 3, 0); /* pauseif goes to 0 when not true */

  /* whole AndNext chain is true */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 5);
  assert_hit_count(condset, 1, 5);
  assert_hit_count(condset, 2, 4);
  assert_hit_count(condset, 3, 1);
}

static void test_andnext_addsource() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* once(byte(0x0001) + byte(0x0002) == 20 && byte(0x0003) == 20) */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001=0_N:0xH0002=20_0xH0003=20.1.");

  /* nothing true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* final condition true */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* conditions 2 and 3 true, but AddSource in condition 1 makes condition 2 not true */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* AddSource condition true via sum, whole set is true */
  ram[1] = ram[2] = 10;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
}

static void test_andnext_addhits() {
  uint8_t ram[] = {0x00, 0x00, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* repeated(5, (byte(0) == 1 && byte(0x0001) > prev(byte(0x0001))) || byte(0) == 2 || 0 == 1) */
  assert_parse_condset(&condset, &memrefs, buffer, "N:0xH00=1_C:0xH01>d0xH01_N:0=1_0=1.2.");

  /* initialize delta */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* second part of AndNext is true, but first is still false */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* both parts of AndNext are true */
  ram[0] = 1;
  ram[1] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* And Next true again, hit count should match target */
  ram[1] = 3;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
}

static void test_andnext_between_addhits() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* AndNext has higher priority than AddHits
  *
  *   AddHits byte(0x0001) == 20 (2)
  *   AndNext byte(0x0002) == 20 (2)  <-- hit count only applies to line 2, AddHits on line 1 modifies line 3
  *           byte(0x0003) == 20 (4)
  *
  * The AndNext on line 2 will combine with line 3, not line 1, so the overall interpretation is:
  *
  *   repeated(4, repeated(2, byte(0x0001) == 20) || (byte(0x0002) == 20 && byte(0x0003) == 20)))
  */
  assert_parse_condset(&condset, &memrefs, buffer, "C:0xH0001=20.2._N:0xH0002=20.2._0xH0003=20.4.");

  /* nothing true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* final condition is not enough to trigger */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* second condition is true, but only has one hit, so won't increment third */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* first condition true, but not second, only first will increment */
  /* hits from first condition should not cause second condition to act true */
  ram[2] = 0;
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* all three conditions are true. the first has already hit its target hit count, the
   * second and third will increment. the total of the first and third is only 3, so no trigger */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 1);

  /* third clause will tally again and set will be true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 2);
}

static void test_andnext_with_hits_chain() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* AndNext has higher priority than AddHits
  *
  *   AndNext byte(0x0001) == 20 (1)
  *   AndNext byte(0x0002) == 20 (1)
  *           byte(0x0003) == 20 (1)
  *
  * Line 1 must be true before line 2 can be true, which has to be true before line 3
  *
  *   a = once(byte(0x0001) == 20)
  *   b = once(a && byte(0x0002) == 20)
  *   c = once(b && byte(0x0003) == 20)
  */
  assert_parse_condset(&condset, &memrefs, buffer, "N:0xH0001=20.1._N:0xH0002=20.1._0xH0003=20.1.");

  /* nothing true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* final condition is not enough to trigger */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* second condition is true, cut can't tally until the first is true */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* first condition is true, but not second, only first will increment */
  ram[2] = 0;
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* final condition cannot tally without the previous items in the chain */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* only second condition true. first is historically true, so second can tally */
  ram[3] = ram[1] = 0;
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* only final condition true, first two historically true, so can tally */
  ram[3] = 20;
  ram[2] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* nothing true, but all historically true, overall still true */
  ram[3] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
}

static void test_andnext_changes_to() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0001) ~> 18 */
  assert_parse_condset(&condset, &memrefs, buffer, "N:0xH0001=18_d0xH0001!=18");

  /* value already 18, initial delta value is 0, so its considered changed */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* value already 18 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* value no longer 18 */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* value changes to 18 */
  ram[1] = 18;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* value already 18 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_ornext() {
  uint8_t ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* repeated(5, byte(0x0001) == 20 || byte(0x0002) == 20 || byte(0x0003) == 20) */
  assert_parse_condset(&condset, &memrefs, buffer, "O:0xH0001=20_O:0xH0002=20_0xH0003=20.6.");

  /* first condition is true, which chains to make the second and third conditions true */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* first and second are true, all but third should update, but only 1 hit each */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 2);

  /* all three true, only increment each once */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 3);

  /* only middle is true, first won't be incremented */
  ram[1] = ram[3] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 4);
  assert_hit_count(condset, 2, 4);

  /* only last is true, only it will be incremented */
  ram[2] = 30; 
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 4);
  assert_hit_count(condset, 2, 5);

  /* none are true */
  ram[3] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 4);
  assert_hit_count(condset, 2, 5);

  /* first is true, hit target met, set is true */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 4);
  assert_hit_count(condset, 1, 5);
  assert_hit_count(condset, 2, 6);

  /* hit target met */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 5);
  assert_hit_count(condset, 1, 6);
  assert_hit_count(condset, 2, 6);
}

static void test_andnext_ornext_interaction() {
  uint8_t ram[] = {0, 0, 0, 0, 0};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* AndNext and OrNext are evaluated at each step: (((1 || 2) && 3) || 4) */
  assert_parse_condset(&condset, &memrefs, buffer, "O:0xH0001=1_N:0xH0002=1_O:0xH0003=1_0xH0004=1");

  ram[3] = 0; assert_evaluate_condset(condset, memrefs, &memory, 0); /* (((0 || 0) && 0) || 0) = 0 */
  ram[4] = 1; assert_evaluate_condset(condset, memrefs, &memory, 1); /* (((0 || 0) && 0) || 1) = 1 */
  ram[3] = 1; assert_evaluate_condset(condset, memrefs, &memory, 1); /* (((0 || 0) && 1) || 1) = 1 */
  ram[4] = 0; assert_evaluate_condset(condset, memrefs, &memory, 0); /* (((0 || 0) && 1) || 0) = 0 */
  ram[2] = 1; assert_evaluate_condset(condset, memrefs, &memory, 1); /* (((0 || 0) && 1) || 0) = 1 */
  ram[1] = 1; assert_evaluate_condset(condset, memrefs, &memory, 1); /* (((1 || 0) && 1) || 0) = 1 */
  ram[2] = 0; assert_evaluate_condset(condset, memrefs, &memory, 1); /* (((1 || 0) && 1) || 0) = 1 */
  ram[3] = 0; assert_evaluate_condset(condset, memrefs, &memory, 0); /* (((1 || 0) && 0) || 0) = 0 */
  ram[4] = 1; assert_evaluate_condset(condset, memrefs, &memory, 1); /* (((1 || 0) && 0) || 1) = 1 */
  ram[3] = 1; assert_evaluate_condset(condset, memrefs, &memory, 1); /* (((1 || 0) && 1) || 1) = 1 */
}

static void test_addaddress_direct_pointer() {
  uint8_t ram[] = {0x01, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0000 + byte(0xh0000)) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000=0_0xH0000=22");

  /* initially, byte(0x0000 + 1) == 22, false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* pointed-at value is correct */
  ram[1] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to new value */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to original value, still correct */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* original value no longer correct */
  ram[1] = 11;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_addaddress_direct_pointer_delta() {
  uint8_t ram[] = { 0x01, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* delta(byte(0x0000 + byte(0xh0000))) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000=0_d0xH0000=22");

  /* byte(0x0000 + 1); value=18, prev=0, prior=0 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 1); value=18, prev=18, prior=0 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 1); value=22, prev=18, prior=18 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  ram[1] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 1); value=22, prev=22, prior=18 */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to new value */
  /* byte(0x0000 + 2); value=52, prev=22, prior=22 */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* byte(0x0000 + 2); value=52, prev=52, prior=22 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  /* byte(0x0000 + 2); value=22, prev=52, prior=52 */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 2); value=22, prev=22, prior=52 */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to original value, still correct */
  /* byte(0x0000 + 1); value=22, prev=22, prior=52 */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* original value no longer correct */
  /* byte(0x0000 + 1); value=11, prev=22, prior=22 */
  ram[1] = 11;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* byte(0x0000 + 1); value=11, prev=11, prior=22 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* point to secondary value, which is correct */
  /* byte(0x0000 + 2); value=22, prev=11, prior=11 */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 2); value=22, prev=22, prior=11 */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_direct_pointer_prior() {
  uint8_t ram[] = { 0x01, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* delta(byte(0x0000 + byte(0xh0000))) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000=0_p0xH0000=22");

  /* byte(0x0000 + 1); value=18, prev=0, prior=0 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 1); value=18, prev=18, prior=0 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 1); value=22, prev=18, prior=18 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  ram[1] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 1); value=22, prev=22, prior=18 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* point to new value */
  /* byte(0x0000 + 2); value=52, prev=22, prior=22 */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* byte(0x0000 + 2); value=52, prev=52, prior=22 */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* new pointed-at value is correct */
  /* byte(0x0000 + 2); value=22, prev=52, prior=52 */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 2); value=22, prev=22, prior=52 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* point to original value, still correct */
  /* byte(0x0000 + 1); value=22, prev=22, prior=52 */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* original value no longer correct */
  /* byte(0x0000 + 1); value=11, prev=22, prior=22 */
  ram[1] = 11;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* byte(0x0000 + 1); value=11, prev=11, prior=22 */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to secondary value, which is correct */
  /* byte(0x0000 + 2); value=22, prev=11, prior=11 */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 2); value=22, prev=22, prior=11 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 2); value=32, prev=22, prior=22 */
  ram[2] = 32;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_direct_pointer_change() {
  uint8_t ram[] = { 0x01, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0000 + byte(0xh0000)) == 20 && delta(byte(0x0000 + byte(0xh0000))) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000=0_0xH0000=20_I:0xH0000=0_d0xH0000=22");

  /* byte(0x0000 + 1); value=18, prev=0, prior=0 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 1); value=18, prev=18, prior=0 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 1); value=22, prev=18, prior=18 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  ram[1] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 1); value=22, prev=22, prior=18 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 1); value=20, prev=22, prior=22 */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* byte(0x0000 + 1); value=20, prev=20, prior=22 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* point to new value */
  /* byte(0x0000 + 2); value=52, prev=20, prior=20 */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 2); value=52, prev=52, prior=20 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  /* byte(0x0000 + 2); value=22, prev=52, prior=52 */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 2); value=22, prev=22, prior=52 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* point to original value. looks correct! */
  /* byte(0x0000 + 1); value=20, prev=22, prior=22 */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to secondary value, which is not correct */
  /* byte(0x0000 + 2); value=22, prev=20, prior=20 */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 2); value=22, prev=22, prior=20 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0x0000 + 2); value=20, prev=22, prior=22 */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_indirect_pointer() {
  uint8_t ram[] = {0x01, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0002 + byte(0xh0000)) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000_0xH0002=22");

  /* initially, byte(0x0001 + 1) == 22, false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* non-offset value is correct */
  ram[1] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* pointed-at value is correct */
  ram[3] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to new value */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  ram[4] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to new value */
  ram[0] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_indirect_pointer_negative() {
  uint8_t ram[] = {0x02, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(byte(0xh0000) - 1) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000_0xHFFFFFFFF=22");

  /* initially, byte(0x0002 - 1) == 22, false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* non-offset value is correct */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* pointed-at value is correct */
  ram[1] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to new value */
  ram[0] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  ram[3] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to invalid address */
  ram[0] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* point to already correct value */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_indirect_pointer_out_of_range() {
  uint8_t ram[] = {0x01, 0x12, 0x34, 0xAB, 0x56, 0x16};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram) - 1; /* purposely hide ram[5] */

  /* byte(0x0002 + byte(0xh0000)) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000_0xH0002=22");

  /* pointed-at value is correct */
  ram[3] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* way out of bounds */
  ram[0] = 100;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* boundary condition - ram[5] value is correct, but should be unreachable */
  /* NOTE: address validation must be handled by the registered 'peek' callback */
  ram[0] = 3;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_addaddress_indirect_pointer_multiple() {
  uint8_t ram[] = {0x01, 0x02, 0x03, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* the expectation is that the AddAddress lines will share rc_memref_value_t's, but the following lines
     will generate their own rc_memref_value_t's for indirection. none of that is actually verified. */
  assert_parse_condset(&condset, &memrefs, buffer,
      "I:0xH0000=0_0xH0002=22_I:0xH0000=0_0xH0003=23_I:0xH0001=0_0xH0003=24");
  /*   $(0002 + $0000) == 22 && $(0003 + $0000) == 23 && $(0003 + $0001) == 24 */
  /*   $0003 (0x34)    == 22 && $0004 (0xAB)    == 23 && $0005 (0x56)    == 24 */

  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 5, 0);

  /* first condition is true */
  ram[3] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 5, 0);

  /* second condition is true */
  ram[4] = 23;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 3, 1);
  assert_hit_count(condset, 5, 0);

  /* third condition is true */
  ram[5] = 24;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 3, 2);
  assert_hit_count(condset, 5, 1);
}

static void test_addaddress_indirect_pointer_with_delta()
{
  uint8_t ram[] = { 0x01, 0x02, 0x03, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(byte(0)*2+2) == 22 && prev(byte(byte(0)*2+2) == 20 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000*2_0xH0002=22_I:0xH0000*2_d0xH0002=20");

  /* byte(0) = 1, byte(1*2+2 [4]) = 0xAB, delta = 0 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 3, 0);

  /* byte(0) = 1, byte(1*2+2 [4]) = 22, delta = 0xAB */
  ram[4] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 3, 0);

  /* byte(0) = 1, byte(1*2+2 [4]) = 20, delta = 22 */
  ram[4] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 3, 0);

  /* byte(0) = 0, byte(0*2+2 [2]) = 2, delta = 20 */
  ram[0] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 3, 1);

  /* byte(0) = 0, byte(0*2+2 [2]) = 20, delta = 2 */
  ram[2] = 20;
  ram[4] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 3, 1);

  /* byte(0) = 1, byte(1*2+2 [4]) = 22, delta = 20 */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 3, 2);
}

static void test_addaddress_indirect_pointer_with_subsource()
{
  uint8_t ram[] = { 0x01, 0x02, 0x03, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(byte(0)*2+2) == 22 - prev(byte(byte(0)*2+2) == 20 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000*2_B:d0xH0002_I:0xH0000*2_0xH0002=20");

  /* byte(0) = 1, byte(1*2+2 [4]) = 0xAB, delta = 0, diff = 0xAB */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0) = 1, byte(1*2+2 [4]) = 0x22, delta = 0xAB, diff = 0x77 */
  ram[4] = 0x22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0) = 1, byte(1*2+2 [4]) = 0x36, delta = 0x22, diff = 0x14 */
  ram[4] = 0x36;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* byte(0) = 0, byte(0*2+2 [2]) = 0x03, delta = 0x36, diff = 0xCD */
  ram[0] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* byte(0) = 0, byte(0*2+2 [2]) = 0x17, delta = 0x03, diff = 0x14 */
  ram[2] = 0x17;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* byte(0) = 1, byte(1*2+2 [4]) = 0x2B, delta = 0x17, diff = 0x14 */
  ram[0] = 1;
  ram[4] = 0x2B;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_indirect_pointer_with_subsource_float()
{
  uint8_t ram[] = { 0x02, 0x05, 0x00, 0x00, 0x42, 0x70, 0x00, 0x00, 0x41, 0xc4, 0x00, 0x00 };
  /*                                        ^ ------ 60.0 ------ ^  ^ ------ 24.5 ------ ^ */
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* float(byte(0) + 2) - float(byte(1) + 3) == 15.0 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0001_B:fB0003_I:0xH0000_fB0002=f15.0");

  /* float(4) = 60.0, float(8) = 24.5, 60.0 - 24.5 = 35.5 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* float(4) = 60.0, float(8) = 45.0, 60.0 - 45.0 = 15.0 */
  ram[8] = 0x42;
  ram[9] = 0x34;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* float(8) = 45.0, float(4) = 60.0, 45.0 - 60.0 = -15.0 */
  ram[0] = 0x06;
  ram[1] = 0x01;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* float(8) = 24.5, float(4) = 60.0, 24.5 - 60.0 = -35.5 */
  ram[8] = 0x41;
  ram[9] = 0xc4;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* float(8) = 24.5, float(4) = 9.5, 24.5 - 9.5 = 15.0 */
  ram[4] = 0x41;
  ram[5] = 0x18;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_indirect_pointer_from_memory() {
  uint8_t ram[] = { 0x01, 0x01, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(byte(0x0000) + byte(0x0001)) == 22 --- byte(0x0000) = pointer, byte(0x0001) = index */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000+0xH0001_0xH0000=22");

  /* initially, byte(1 + 1) == 22, false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* pointed-at value is correct */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to new value */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  ram[3] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to new value */
  ram[1] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  ram[4] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_indirect_constant() {
  uint8_t ram[] = { 0x01, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0002 + 1) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:1_0xH0002=22");

  /* initially, byte(0x0003) == 22, false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* memory at constant is correct */
  ram[1] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* memory at address is correct */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* memory at offset address is correct */
  ram[3] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_pointer_data_size_differs_from_pointer_size() {
  uint8_t ram[] = {0x01, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0002 + word(0xh0000)) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000_0x 0002=22");

  /* 8-bit pointed-at value is correct */
  ram[3] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 16-bit pointed-at value is correct */
  ram[4] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to new value */
  ram[0] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is only partially correct */
  ram[3] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_double_indirection() {
  uint8_t ram[] = {0x01, 0x02, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0000 + byte(0x0000 + byte(0x0000))) == 22 | $($($0000))) == 22*/
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000=0_I:0xH0000=0_0xH0000=22");

  /* value is correct: $0000=1, $0001=2, $0002 = 22 */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* second pointer in chain causes final pointer to point at address 3 */
  ram[1] = 3;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  ram[3] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* first pointer points at address 2, which is 22, so out-of-bounds */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* second pointer points at address 3, which is correct */
  ram[2] = 3;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* first pointer is out of range, so returns 0 for the second pointer, $0 contains the correct value */
  ram[0] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_double_indirection_with_delta() {
  uint8_t ram[] = { 0, 2, 4 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* prev(byte(0x0000 + byte(0x0000 + byte(0x0000)))) == 22 | prev($($($0000)))) == 22*/
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000=0_I:0xH0000=0_d0xH0000=4");

  /* NOTE: indirectly calculated memrefs keep their own delta, not the delta of the newly pointed to 
   *       value. using the intermediate deltas to calculate an address for the current frame will
   *       generate incorrect values. Only the final item in the chain should have the delta. */

  /* 1st frame: A = Mem[0] = 0 (delta[0] = 0), B = Mem[A] = Mem[0] = 0 (delta B = 0), C = Mem[B] = Mem[0] = 0 (delta C = 0) */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 2nd frame: A = Mem[0] = 1 (delta[0] = 0), B = Mem[A] = Mem[1] = 2 (delta B = 0), C = Mem[B] = Mem[2] = 4 (delta C = 0) */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 3nd frame: A = Mem[0] = 1 (delta[0] = 1), B = Mem[A] = Mem[1] = 2 (delta B = 2), C = Mem[B] = Mem[2] = 4 (delta C = 4) */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_double_indirection_with_delta_incorrect() {
  uint8_t ram[] = { 0, 2, 4 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* prev(byte(0x0000 + prev(byte(0x0000 + prev(byte(0x0000)))))) == 22 | prev($($($0000)))) == 22*/
  assert_parse_condset(&condset, &memrefs, buffer, "I:d0xH0000=0_I:d0xH0000=0_d0xH0000=4");

  /* putting prevs on each step of the chain results in using old pointers to get to data that may not exist yet,
   * but this validates that incorrect behavior */

  /* 1st frame: Mem[0] = 0 (delta[0] = 0), B = Mem[deltaA] = Mem[0] = 0 (delta B = 0), C = Mem[deltaB] = Mem[0] = 0 (delta C = 0) */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 2nd frame: Mem[0] = 1 (delta[0] = 0), B = Mem[deltaA] = Mem[0] = 1 (delta B = 0), C = Mem[deltaB] = Mem[0] = 1 (delta C = 0) */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 3rd frame: Mem[0] = 1 (delta[0] = 1), B = Mem[deltaA] = Mem[1] = 2 (delta B = 1), C = Mem[deltaB] = Mem[1] = 2 (delta C = 1) */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 4th frame: Mem[0] = 1 (delta[0] = 1), B = Mem[deltaA] = Mem[1] = 2 (delta B = 2), C = Mem[deltaB] = Mem[2] = 4 (delta C = 2) */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 5th frame: Mem[0] = 1 (delta[0] = 1), B = Mem[deltaA] = Mem[1] = 2 (delta B = 2), C = Mem[deltaB] = Mem[2] = 4 (delta C = 4) */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_adjust_both_sides() {
  uint8_t ram[] = {0x02, 0x11, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* $($0) > delta $($0) */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000=0_0xH0000>d0xH0000");

  /* initial delta will be 0, so 2 will be greater */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* delta should be the same as current */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* value increased */
  ram[2]++;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* value decreased */
  ram[2]--;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* this is a small hiccup in the AddAddress behavior. when the pointer changes, we 
   * can't reasonably know the previous value, so delta will be 0 for the first frame. 
   * 52 is greater than 0 (even though it didn't change), so set will be true. */
  ram[0] = 3;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_adjust_both_sides_different_bases() {
  uint8_t ram[] = {0x02, 0x11, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* $($0) == $($0 + 1) */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000=0_0xH0000=0xH0001");
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* values are the same */
  ram[2] = ram[3];
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* adjust pointer */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* values are the same */
  ram[1] = ram[2];
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_scaled() {
  uint8_t ram[] = {0x01, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* $($0 * 2) */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000*2_0xH0000=22");
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* value is correct */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* adjust pointer */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new value is correct */
  ram[4] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to original value */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* original value no longer correct */
  ram[2] = 11;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_addaddress_scaled_negative() {
  uint8_t ram[] = {0x01, 0x12, 0x34, 0xAB, 0x01};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* $($4 * -1 + 2) */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0004*4294967295_0xH0002=22"); /* 4294967295 = 0xFFFFFFFF = -1 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* value is correct: $(1 * -1 + 2) = $(1) */
  ram[1] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* adjust pointer: $(2 * -1 + 2) = $(0) */
  ram[4] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new value is correct */
  ram[0] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to original value */
  ram[4] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* original value no longer correct */
  ram[1] = 11;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_addaddress_double_read() {
  uint8_t ram[] = {0x01, 0x02, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* $($0 + $1) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000+0xH0001_0xH0000=22");
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* value is correct */
  ram[3] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* adjust first address */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new value is correct */
  ram[4] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to original value */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* original value no longer correct */
  ram[3] = 11;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_addaddress_shared_size() {
  uint8_t ram[] = { 0x01, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* bit2(byte(0) * 2) == 1 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000*2_0xO0000=1");

  /* bit2( [0x34] ) = 1 */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* bit2( [0x01] ) = 0 */
  ram[2] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* adjust pointer. bit2( [0x56] ) = 1 */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* new value is correct */
  ram[4] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_prior_sequence() {
  uint8_t ram[] = {0x00};
  memory_t memory;
  rc_condset_t* condset;
  rc_memrefs_t memrefs;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* prior(bit0(0))==1 && prior(bit1(0))==1 && prior(bit2(0))==1 */
  assert_parse_condset(&condset, &memrefs, buffer, "p0xM0000=1_p0xN0000=1_p0xO0000=1");
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* value ~> 1 [0001], all priors still 0 */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* value ~> 2 [0010], prior(bit0(0)) = 1, prior(bit1(0)) = 0, prior(bit2(0)) = 0 */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* value ~> 3 [0011], prior(bit0(0)) = 0, prior(bit1(0)) = 0, prior(bit2(0)) = 0 */
  ram[0] = 3;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* value ~> 4 [0100], prior(bit0(0)) = 1, prior(bit1(0)) = 1, prior(bit2(0)) = 0 */
  ram[0] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* value ~> 5 [0101], prior(bit0(0)) = 0, prior(bit1(0)) = 1, prior(bit2(0)) = 0 */
  ram[0] = 5;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);

  /* value ~> 6 [0110], prior(bit0(0)) = 1, prior(bit1(0)) = 0, prior(bit2(0)) = 0 */
  ram[0] = 6;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);

  /* value ~> 7 [0111], prior(bit0(0)) = 0, prior(bit1(0)) = 0, prior(bit2(0)) = 0 */
  ram[0] = 7;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);

  /* value ~> 8 [1000], prior(bit0(0)) = 1, prior(bit1(0)) = 1, prior(bit2(0)) = 1 */
  ram[0] = 8;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 4);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 1);
}

static void test_get_real_operand1(void)
{
  rc_condset_t* condset;
  rc_condition_t* condition;
  const rc_operand_t* operand;
  rc_memrefs_t memrefs;
  char buffer[2048];

  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH1234*100_A:0xH1235*10_M:0xH1236>850");

  condition = condset->conditions;
  ASSERT_NUM_EQUALS(condition->operand1.is_combining, 0);
  operand = rc_condition_get_real_operand1(condition);
  ASSERT_NUM_EQUALS(operand->type, RC_OPERAND_ADDRESS);
  ASSERT_NUM_EQUALS(operand->size, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(operand->value.memref->address, 0x1234);
  ASSERT_NUM_EQUALS(operand->is_combining, 0);

  condition = condition->next;
  ASSERT_NUM_EQUALS(condition->operand1.is_combining, 0);
  operand = rc_condition_get_real_operand1(condition);
  ASSERT_NUM_EQUALS(operand->type, RC_OPERAND_ADDRESS);
  ASSERT_NUM_EQUALS(operand->size, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(operand->value.memref->address, 0x1235);
  ASSERT_NUM_EQUALS(operand->is_combining, 0);

  condition = condition->next;
  ASSERT_NUM_EQUALS(condition->operand1.is_combining, 1);
  operand = rc_condition_get_real_operand1(condition);
  ASSERT_NUM_EQUALS(operand->type, RC_OPERAND_ADDRESS);
  ASSERT_NUM_EQUALS(operand->size, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(operand->value.memref->address, 0x1236);
  ASSERT_NUM_EQUALS(operand->is_combining, 0);
}

static void test_get_real_operand1_recall(void)
{
  rc_condset_t* condset;
  rc_condition_t* condition;
  const rc_operand_t* operand;
  rc_memrefs_t memrefs;
  char buffer[2048];

  assert_parse_condset(&condset, &memrefs, buffer, "A:1_K:0xH0001_I:{recall}_I:0xH0002_0xH0003=4");

  condition = condset->conditions;
  ASSERT_NUM_EQUALS(condition->operand1.is_combining, 0);
  operand = rc_condition_get_real_operand1(condition);
  ASSERT_NUM_EQUALS(operand->type, RC_OPERAND_CONST);
  ASSERT_NUM_EQUALS(operand->size, RC_MEMSIZE_32_BITS);
  ASSERT_NUM_EQUALS(operand->value.num, 1);
  ASSERT_NUM_EQUALS(operand->is_combining, 0);

  condition = condition->next;
  ASSERT_NUM_EQUALS(condition->operand1.is_combining, 1);
  operand = rc_condition_get_real_operand1(condition);
  ASSERT_NUM_EQUALS(operand->type, RC_OPERAND_ADDRESS);
  ASSERT_NUM_EQUALS(operand->size, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(operand->value.memref->address, 1);
  ASSERT_NUM_EQUALS(operand->is_combining, 0);

  condition = condition->next;
  ASSERT_NUM_EQUALS(condition->operand1.is_combining, 0);
  operand = rc_condition_get_real_operand1(condition);
  ASSERT_NUM_EQUALS(operand->type, RC_OPERAND_RECALL);
  ASSERT_NUM_EQUALS(operand->size, RC_MEMSIZE_32_BITS);
  ASSERT_NUM_EQUALS(operand->is_combining, 0);

  condition = condition->next;
  ASSERT_NUM_EQUALS(condition->operand1.is_combining, 0);
  operand = rc_condition_get_real_operand1(condition);
  ASSERT_NUM_EQUALS(operand->type, RC_OPERAND_ADDRESS);
  ASSERT_NUM_EQUALS(operand->size, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(operand->value.memref->address, 2);
  ASSERT_NUM_EQUALS(operand->is_combining, 0);

  condition = condition->next;
  ASSERT_NUM_EQUALS(condition->operand1.is_combining, 0);
  operand = rc_condition_get_real_operand1(condition);
  ASSERT_NUM_EQUALS(operand->type, RC_OPERAND_ADDRESS);
  ASSERT_NUM_EQUALS(operand->size, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(operand->value.memref->address, 3);
  ASSERT_NUM_EQUALS(operand->is_combining, 0);
}

static void test_get_real_operand1_indirect_static()
{
  rc_condset_t* condset;
  rc_condition_t* condition;
  const rc_operand_t* operand;
  rc_memrefs_t memrefs;
  char buffer[2048];

  assert_parse_condset(&condset, &memrefs, buffer, "I:8_I:0xH0010_0xH0020=2");

  condition = condset->conditions;
  ASSERT_NUM_EQUALS(condition->operand1.is_combining, 0);
  operand = rc_condition_get_real_operand1(condition);
  ASSERT_NUM_EQUALS(operand->type, RC_OPERAND_CONST);
  ASSERT_NUM_EQUALS(operand->size, RC_MEMSIZE_32_BITS);
  ASSERT_NUM_EQUALS(operand->value.num, 8);
  ASSERT_NUM_EQUALS(operand->is_combining, 0);

  condition = condition->next;
  ASSERT_NUM_EQUALS(condition->operand1.is_combining, 0);
  operand = rc_condition_get_real_operand1(condition);
  ASSERT_NUM_EQUALS(operand->type, RC_OPERAND_ADDRESS);
  ASSERT_NUM_EQUALS(operand->size, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(operand->value.memref->address, 0x0010);
  ASSERT_NUM_EQUALS(operand->is_combining, 0);

  condition = condition->next;
  ASSERT_NUM_EQUALS(condition->operand1.is_combining, 0);
  operand = rc_condition_get_real_operand1(condition);
  ASSERT_NUM_EQUALS(operand->type, RC_OPERAND_ADDRESS);
  ASSERT_NUM_EQUALS(operand->size, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(operand->value.memref->address, 0x0020);
  ASSERT_NUM_EQUALS(operand->is_combining, 0);
}

static void test_ignore_parse_errors(const char* memaddr, uint8_t is_value, int32_t expected_error)
{
  const char* memaddr_test;

  rc_preparse_state_t preparse;
  rc_init_preparse_state(&preparse);
  preparse.parse.is_value = is_value;

  memaddr_test = memaddr;
  rc_parse_condset(&memaddr_test, &preparse.parse);
  ASSERT_NUM_EQUALS(preparse.parse.offset, expected_error);
  rc_destroy_preparse_state(&preparse);

  rc_init_preparse_state(&preparse);
  preparse.parse.is_value = is_value;
  preparse.parse.ignore_non_parse_errors = 1;

  memaddr_test = memaddr;
  rc_parse_condset(&memaddr_test, &preparse.parse);
  ASSERT_NUM_GREATER(preparse.parse.offset, 0);
  rc_destroy_preparse_state(&preparse);
}

void test_condset(void) {
  TEST_SUITE_BEGIN();

  /* hit counts */
  TEST(test_hitcount_increment_when_true);
  TEST(test_hitcount_does_not_increment_when_false);
  TEST(test_hitcount_target);

  /* two conditions */
  TEST_PARAMS4(test_hitcount_two_conditions, "0xH0001=18_0xH0002=52", 1, 1, 1);
  TEST_PARAMS4(test_hitcount_two_conditions, "0xH0001=18_0xH0002!=52", 0, 1, 0);
  TEST_PARAMS4(test_hitcount_two_conditions, "0xH0001>18_0xH0002=52", 0, 0, 1);
  TEST_PARAMS4(test_hitcount_two_conditions, "0xH0001<18_0xH0002>52", 0, 0, 0);

  /* three conditions */
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001=18_0xH0002=52_0xL0004=6", 1, 1, 1, 1);
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001=18_0xH0002=52_0xL0004>6", 0, 1, 1, 0);
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001=18_0xH0002<52_0xL0004=6", 0, 1, 0, 1);
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001=18_0xH0002<52_0xL0004>6", 0, 1, 0, 0);
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001>18_0xH0002=52_0xL0004=6", 0, 0, 1, 1);
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001>18_0xH0002=52_0xL0004>6", 0, 0, 1, 0);
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001>18_0xH0002<52_0xL0004=6", 0, 0, 0, 1);
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001>18_0xH0002<52_0xL0004>6", 0, 0, 0, 0);

  /* pauseif */
  TEST(test_pauseif);
  TEST(test_pauseif_hitcount_one);
  TEST(test_pauseif_hitcount_two);
  TEST(test_pauseif_hitcount_with_reset);
  TEST(test_pauseif_resetnextif);
  TEST(test_pauseif_does_not_increment_hits);
  TEST(test_pauseif_delta_updated);
  TEST(test_pauseif_indirect_delta_updated);
  TEST(test_pauseif_short_circuit);

  /* resetif */
  TEST(test_resetif);
  TEST(test_resetif_cond_with_hittarget);
  TEST(test_resetif_hitcount);
  TEST(test_resetif_hitcount_one);
  TEST(test_resetif_hitcount_addhits);

  TEST(test_pauseif_resetif_hitcounts);

  /* resetnextif */
  TEST(test_resetnextif);
  TEST(test_resetnextif_non_hitcount_condition);
  TEST(test_resetnextif_addhits);
  TEST(test_resetnextif_addhits_chain);
  TEST(test_resetnextif_addhits_chain_total);
  TEST(test_resetnextif_using_andnext);
  TEST(test_resetnextif_andnext);
  TEST(test_resetnextif_andnext_chain);
  TEST(test_resetnextif_andnext_hitchain);
  TEST(test_resetnextif_addaddress);
  TEST(test_resetnextif_addaddress_andnext_chain);
  TEST(test_resetnextif_chain);
  TEST(test_resetnextif_chain_andnext);
  TEST(test_resetnextif_chain_with_hits);
  TEST(test_resetnextif_pause_lock);
  TEST(test_resetnextif_unfinished);

  /* addsource/subsource */
  TEST(test_addsource);
  TEST(test_addsource_overflow);
  TEST(test_subsource);
  TEST(test_subsource_legacy_garbage);
  TEST(test_subsource_overflow);
  TEST(test_addsource_subsource);
  TEST(test_addsource_multiply);
  TEST(test_subsource_multiply);
  TEST(test_addsource_multiply_fraction);
  TEST(test_addsource_multiply_address);
  TEST(test_addsource_divide);
  TEST(test_addsource_divide_address);
  TEST(test_addsource_divide_self);
  TEST(test_subsource_divide);
  TEST(test_addsource_compare_percentage);
  TEST(test_addsource_mask);
  TEST(test_addsource_xor);
  TEST(test_addsource_float_first);
  TEST(test_addsource_float_second);
  TEST(test_subsource_mask);
  TEST(test_subsource_overflow_comparison_equal);
  TEST(test_subsource_overflow_comparison_greater);
  TEST(test_subsource_overflow_comparison_greater_or_equal);
  TEST(test_subsource_overflow_comparison_lesser);
  TEST(test_subsource_overflow_comparison_lesser_or_equal);
  TEST(test_subsource_float);
  TEST(test_addsource_bits_change);
  TEST(test_addsource_bcd);
  TEST(test_addsource_invert);
  TEST(test_subsource_mem_delta);
  TEST(test_subsource_mem_prior);
  TEST(test_addsource_unfinished);
  TEST(test_addsource_float_division);
  TEST(test_addsource_recall_float_division);
  TEST(test_addsource_long_chain);

  /* addhits/subhits */
  TEST(test_addhits);
  TEST(test_addhits_no_target);
  TEST(test_addhits_with_addsource);
  TEST(test_addhits_multiple);
  TEST(test_subhits);
  TEST(test_subhits_below_zero);
  TEST(test_addhits_unfinished);
  TEST(test_addhits_float_coercion)

  /* andnext */
  TEST(test_andnext);
  TEST(test_andnext_boundaries);
  TEST(test_andnext_resetif);
  TEST(test_andnext_pauseif);
  TEST(test_andnext_addsource);
  TEST(test_andnext_addhits);
  TEST(test_andnext_between_addhits);
  TEST(test_andnext_with_hits_chain);
  TEST(test_andnext_changes_to);

  /* ornext */
  TEST(test_ornext);
  TEST(test_andnext_ornext_interaction);

  /* addaddress */
  TEST(test_addaddress_direct_pointer);
  TEST(test_addaddress_direct_pointer_delta);
  TEST(test_addaddress_direct_pointer_prior);
  TEST(test_addaddress_direct_pointer_change);
  TEST(test_addaddress_indirect_pointer);
  TEST(test_addaddress_indirect_pointer_negative);
  TEST(test_addaddress_indirect_pointer_out_of_range);
  TEST(test_addaddress_indirect_pointer_multiple);
  TEST(test_addaddress_indirect_pointer_with_delta);
  TEST(test_addaddress_indirect_pointer_with_subsource);
  TEST(test_addaddress_indirect_pointer_with_subsource_float);
  TEST(test_addaddress_indirect_pointer_from_memory);
  TEST(test_addaddress_indirect_constant);
  TEST(test_addaddress_pointer_data_size_differs_from_pointer_size);
  TEST(test_addaddress_double_indirection);
  TEST(test_addaddress_double_indirection_with_delta);
  TEST(test_addaddress_double_indirection_with_delta_incorrect);
  TEST(test_addaddress_adjust_both_sides);
  TEST(test_addaddress_adjust_both_sides_different_bases);
  TEST(test_addaddress_scaled);
  TEST(test_addaddress_scaled_negative);
  TEST(test_addaddress_double_read);
  TEST(test_addaddress_shared_size);

  /* prior */
  TEST(test_prior_sequence);

  /* get_real_operand1 */
  TEST(test_get_real_operand1);
  TEST(test_get_real_operand1_recall);
  TEST(test_get_real_operand1_indirect_static);

  /* ignore parse errors */
  TEST_PARAMS3(test_ignore_parse_errors, "M:0x1234=5_M:0x1234=6", 0, RC_MULTIPLE_MEASURED);
  TEST_PARAMS3(test_ignore_parse_errors, "M:0x1234", 0, RC_INVALID_OPERATOR); /* right side required for Measured non-value */
  TEST_PARAMS3(test_ignore_parse_errors, "M:0x1234=0x2345", 0, RC_INVALID_MEASURED_TARGET);
  TEST_PARAMS3(test_ignore_parse_errors, "0x1234=6", 1, RC_INVALID_VALUE_FLAG);
  TEST_PARAMS3(test_ignore_parse_errors, "T:0x1234=6", 1, RC_INVALID_VALUE_FLAG);
  TEST_PARAMS3(test_ignore_parse_errors, "R:0x1234", 0, RC_INVALID_OPERATOR);
  TEST_PARAMS3(test_ignore_parse_errors, "K:0x1234<6", 0, RC_INVALID_OPERATOR);

  TEST_SUITE_END();
}
