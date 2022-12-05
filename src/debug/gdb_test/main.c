
#include <stdio.h>
#include <time.h>

#include "gdbstub.h"

static uint32_t my_read_reg(int reg) {
	printf("[==>] read reg %d\n", reg);

	if (reg == 16) return 0x000000df; // cpsr: irq,fiq disabled, arm, sys mode
	else return 0x69420;
}
static void my_write_reg(int reg, uint32_t value) {
	printf("[==>] write reg %d: 0x%08x\n", reg, value);
}
static uint32_t my_read_mem(uint32_t addr, uint32_t len) {
	printf("[==>] read mem 0x%08x (size %u)\n", addr, len);

	static const uint32_t words[] = {
		0xeafffffe,
		0xe0211002,
		0xe12fff1e,
		0
	};

	// $: b $ (arm)
	return words[(addr>>2)&3] & ((1uLL<<len)-1);
}
static void my_write_mem(uint32_t addr, uint32_t len, uint32_t value) {
	printf("[==>] write addr 0x%08x (size %u): 0x%08x\n", addr, len, value);
}

void my_add_bkpt(uint32_t addr, uint32_t len) {
	printf("[==>] add bkpt at 0x%08x, len %u\n", addr, len);
}
void my_del_bkpt(uint32_t addr, uint32_t len) {
	printf("[==>] del bkpt at 0x%08x, len %u\n", addr, len);
}
void my_del_watchpt(uint32_t addr, uint32_t len) {
	printf("[==>] add watchpt at 0x%08x, len %u\n", addr, len);
}
void my_add_watchpt(uint32_t addr, uint32_t len) {
	printf("[==>] del watchpt at 0x%08x, len %u\n", addr, len);
}

enum gdbtgt_status my_target_get_status(void) {
	return gdbt_break_req;
}

const static struct gdbstub_callbacks cb = {
	.cpu = 9,

	.target_get_status = my_target_get_status,

	.read_reg = my_read_reg,
	.write_reg = my_write_reg,
	.read_mem = my_read_mem,
	.write_mem = my_write_mem,
	.add_bkpt = my_add_bkpt,
	.del_bkpt = my_del_bkpt,
	.add_watchpt = my_add_watchpt,
	.del_watchpt = my_del_watchpt
};

int main(int argc, char** argv) {
	struct gdbstub* stub;

	stub = gdbstub_new(&cb, 3333);
	if (!stub) return 1;

	while (true) {
		enum gdbstub_state s = gdbstub_poll(stub);

		if (s == gdbstat_none || s == gdbstat_noconn) {
			struct timespec ts;
			ts.tv_sec = 0;
			ts.tv_nsec = 1000*1000; // 1 ms
			nanosleep(&ts, NULL);
			continue;
		}

		switch (s) {
		case gdbstat_break: printf("[==>] break execution\n"); break;
		case gdbstat_continue: printf("[==>] continue execution\n"); break;
		case gdbstat_disconnect: printf("[==>] disconnect\n"); break;
		}

		if (s == gdbstat_disconnect) break;
	}

	gdbstub_close(stub);
	return 0;
}

