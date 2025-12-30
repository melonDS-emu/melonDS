#include "rc_runtime.h"
#include "rc_internal.h"

#include "mock_memory.h"

#include "../test_framework.h"

static rc_runtime_t runtime;
static int trigger_count[128];

static void event_handler(const rc_runtime_event_t* e)
{
  if (e->type == RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED)
  {
    rc_trigger_t* trigger = rc_runtime_get_achievement(&runtime, e->id);
    trigger_count[e->id]++;
    
    rc_reset_trigger(trigger);
    trigger->state = RC_TRIGGER_STATE_ACTIVE;
  }
}

static void _assert_activate_achievement(rc_runtime_t* runtime, uint32_t id, const char* memaddr)
{
  int result = rc_runtime_activate_achievement(runtime, id, memaddr, NULL, 0);
  ASSERT_NUM_EQUALS(result, RC_OK);
}
#define assert_activate_achievement(runtime, id, memaddr) ASSERT_HELPER(_assert_activate_achievement(runtime, id, memaddr), "assert_activate_achievement")

static void do_timing(void)
{
  uint8_t ram[256], last, next, bitcount;
  memory_t memory;
  int i, j, mask;
  clock_t total_clocks = 0, start, end;
  double elapsed, average;

  memory.ram = ram;
  memory.size = sizeof(ram);
  memset(&ram[0], 0, sizeof(ram));
  for (i = 0; i < 0x3F; i++)
    ram[0xC0 + i] = i;

  memset(&trigger_count, 0, sizeof(trigger_count));

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0000=1");
  assert_activate_achievement(&runtime, 2, "0xH0001=1");
  assert_activate_achievement(&runtime, 3, "0xH0001=0");
  assert_activate_achievement(&runtime, 4, "0xH0002!=d0xH0002");
  assert_activate_achievement(&runtime, 5,
      "A:0xH0000_A:0xH0001_A:0xH0002_A:0xH0003_A:0xH0004_A:0xH0005_A:0xH0006_A:0xH0007_"
      "A:0xH0008_A:0xH0009_A:0xH000A_A:0xH000B_A:0xH000C_A:0xH000D_A:0xH000E_0xH000F=0xH0084");
  assert_activate_achievement(&runtime, 6,
      "A:0xH0000_A:0xH0001_A:0xH0002_A:0xH0003_A:0xH0004_A:0xH0005_A:0xH0006_0xH0007=0xK0080_"
      "A:0xH0008_A:0xH0009_A:0xH000A_A:0xH000B_A:0xH000C_A:0xH000D_A:0xH000E_0xH000F=0xK0081");
  assert_activate_achievement(&runtime, 7, "I:0xH0024_0xL00C0>8");
  assert_activate_achievement(&runtime, 8, "O:0xH0000=1_O:0xH0001=1_O:0xH0002=1_O:0xH0003=1_O:0xH0004=1_O:0xH0005=1_O:0xH0006=1_0xH0007=1");
  assert_activate_achievement(&runtime, 9, "N:0xH0000=1_N:0xH0001=1_N:0xH0002=1_N:0xH0003=1_N:0xH0004=1_N:0xH0005=1_N:0xH0006=1_0xH0007=1");
  assert_activate_achievement(&runtime, 10, "0xH008A>0xH008B_0xH008A<0xH0089");
  assert_activate_achievement(&runtime, 11,
      "0xH0040=0xH0060_0xH0041=0xH0061_0xH0042=0xH0062_0xH0043=0xH0063_"
      "0xH0044=0xH0064_0xH0045=0xH0065_0xH0046=0xH0066_0xH0047=0xH0067_"
      "0xH0048=0xH0068_0xH0049=0xH0069_0xH004A=0xH006A_0xH004B=0xH006B_"
      "0xH004C=0xH006C_0xH004D=0xH006D_0xH004E=0xH006E_0xH004F=0xH006F");
  assert_activate_achievement(&runtime, 12, "0xH0003=1.3._R:0xH0004=1_P:0xH0005=1");
  assert_activate_achievement(&runtime, 13, "0xH0003=1.3.SR:0xH0004=1_P:0xH0005=1");
  assert_activate_achievement(&runtime, 14, "I:0xH0024_0xH0040>d0xH0040");
  assert_activate_achievement(&runtime, 15, "b0xH0085=0xH0080");
  assert_activate_achievement(&runtime, 16, "0xH0026=0xH0028S0xH0023=0xH0027S0xH0025=0xH0022");
  assert_activate_achievement(&runtime, 17,
      "C:0xH0000!=0_C:0xH0001!=0_C:0xH0002!=0_C:0xH0003!=0_C:0xH0004!=0_C:0xH0005!=0_C:0xH0006!=0_C:0xH0007!=0_"
      "C:0xH0008!=0_C:0xH0009!=0_C:0xH000A!=0_C:0xH000B!=0_C:0xH000C!=0_C:0xH000D!=0_C:0xH000E!=0_0xH000F!=0.20.");
  assert_activate_achievement(&runtime, 18, "A:0xL0087*10000_A:0xU0086*1000_A:0xL0086*100_A:0xU0085*10_0xL0x0085=0x 0080");

  /* we expect these to only be true initially, so forcibly change them from WAITING to ACTIVE */
  rc_runtime_get_achievement(&runtime, 5)->state = RC_TRIGGER_STATE_ACTIVE;
  rc_runtime_get_achievement(&runtime, 6)->state = RC_TRIGGER_STATE_ACTIVE;
  rc_runtime_get_achievement(&runtime, 15)->state = RC_TRIGGER_STATE_ACTIVE;
  rc_runtime_get_achievement(&runtime, 18)->state = RC_TRIGGER_STATE_ACTIVE;

  for (i = 0; i < 65536; i++)
  {
    /*
     * $00-$1F = individual bits of i
     * $20-$3F = number of times individual bits changed
     * $40-$5F = number of times individual bits were 0
     * $60-$7F = number of times individual bits were 1
     * $80-$83 = i
     * $84     = bitcount i
     * $85-$87 = bcd i (LE)
     * $88-$8C = ASCII i
     * $C0-$FF = 0-63
     */
    mask = 1;
    bitcount = 0;
    for (j = 0; j < 32; j++)
    {
      next = ((i & mask) != 0) * 1;
      mask <<= 1;
      last = ram[j];

      ram[j] = next;
      ram[j + 0x60] += next;
      bitcount += next;
      ram[j + 0x40] += (1 - next);
      ram[j + 0x20] += (next != last) * 1;
    }
    ram[0x80] = i & 0xFF;
    ram[0x81] = (i >> 8) & 0xFF;
    ram[0x82] = (i >> 16) & 0xFF;
    ram[0x83] = (i >> 24) & 0xFF;
    ram[0x84] = bitcount;
    ram[0x85] = (i % 10) | ((i / 10) % 10) << 4;
    ram[0x86] = ((i / 100) % 10) | ((i / 1000) % 10) << 4;
    ram[0x87] = ((i / 10000) % 10);
    ram[0x88] = ((i / 10000) % 10) + '0';
    ram[0x89] = ((i / 1000) % 10) + '0';
    ram[0x8A] = ((i / 100) % 10) + '0';
    ram[0x8B] = ((i / 10) % 10) + '0';
    ram[0x8C] = (i % 10) + '0';

    start = clock();
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    end = clock();

    total_clocks += (end - start);
  }

  elapsed = (double)total_clocks * 1000 / CLOCKS_PER_SEC;
  average = elapsed * 1000 / i;
  printf("\n%0.6fms elapsed, %0.6fus average", elapsed, average);

  ASSERT_NUM_EQUALS(trigger_count[ 0],     0); /* no achievement 0 */
  ASSERT_NUM_EQUALS(trigger_count[ 1], 32768); /* 0xH0000 = 1 every other frame */
  ASSERT_NUM_EQUALS(trigger_count[ 2], 32768); /* 0xH0001 = 1 2 / 4 frames */
  ASSERT_NUM_EQUALS(trigger_count[ 3], 32766); /* 0xH0001 = 0 2 / 4 frames (won't trigger until after first time it's 1) */
  ASSERT_NUM_EQUALS(trigger_count[ 4], 16383); /* 0xH0002!=d0xH0002 every two frames (no delta for first frame) */
  ASSERT_NUM_EQUALS(trigger_count[ 5], 65536); /* sum of bits = bitcount (16-bit) */
  ASSERT_NUM_EQUALS(trigger_count[ 6], 65536); /* sum of bits = bitcount (8-bit) */
  ASSERT_NUM_EQUALS(trigger_count[ 7],  6912); /* I:0xH0024_0xL00C0>8 pointer advances every eight frames */
  ASSERT_NUM_EQUALS(trigger_count[ 8], 65280); /* number of times any bit is set in lower byte */
  ASSERT_NUM_EQUALS(trigger_count[ 9],   256); /* number of times all bits are set in lower byte */
  ASSERT_NUM_EQUALS(trigger_count[10],  7400); /* hundreds digit less than thousands, but greater than tens */
  ASSERT_NUM_EQUALS(trigger_count[11],   256); /* number of times individual bits were 0 or 1 is equal */
  ASSERT_NUM_EQUALS(trigger_count[12],  2048); /* number of times bit3 was set without bit 4 or 5 */
  ASSERT_NUM_EQUALS(trigger_count[13],  3071); /* number of times bit3 was set without bit 4 xor 5 */
  ASSERT_NUM_EQUALS(trigger_count[14], 10350); /* I:0xH0024_0xH0040>d0xH0040 - delta increase across moving target */
  ASSERT_NUM_EQUALS(trigger_count[15],  1100); /* BCD check (only valid for two digits) */
  ASSERT_NUM_EQUALS(trigger_count[16],    24); /* random collection of bit changes being equal */
  ASSERT_NUM_EQUALS(trigger_count[17], 22187); /* number of times 20 bits or more are set across frames */
  ASSERT_NUM_EQUALS(trigger_count[18], 65536); /* BCD reconstruction */

  rc_runtime_destroy(&runtime);
}

void test_timing(void) {
  TEST_SUITE_BEGIN();
  TEST(do_timing);
  TEST(do_timing);
  TEST(do_timing);
  TEST(do_timing);
  TEST(do_timing);
  TEST_SUITE_END();
}
