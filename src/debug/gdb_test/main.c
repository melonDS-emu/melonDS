
#include <stdio.h>
#include <time.h>

#include "gdbstub.h"

static uint32_t my_read_reg(void* ud, int reg) {
	printf("[==>] read reg %d\n", reg);

	if (reg == 16) return 0x000000df; // cpsr: irq,fiq disabled, arm, sys mode
	else return 0x69420;
}
static void my_write_reg(void* ud, int reg, uint32_t value) {
	printf("[==>] write reg %d: 0x%08x\n", reg, value);
}
static uint32_t my_read_mem(void* ud, uint32_t addr, int len) {
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
static void my_write_mem(void* ud, uint32_t addr, int len, uint32_t value) {
	printf("[==>] write addr 0x%08x (size %u): 0x%08x\n", addr, len, value);
}

static void my_reset(void* ud) {
	printf("[==>] RESET!!!\n");
}
static int my_remote_cmd(void* ud, const uint8_t* cmd, size_t len) {
	printf("[==>] Rcmd: %s\n", cmd);
	return 0;
}

const static struct gdbstub_callbacks cb = {
	.cpu = 9,

	.read_reg = my_read_reg,
	.write_reg = my_write_reg,
	.read_mem = my_read_mem,
	.write_mem = my_write_mem,

	.reset = my_reset,
	.remote_cmd = my_remote_cmd,
};

int main(int argc, char** argv) {
	struct gdbstub* stub;

	stub = gdbstub_new(&cb, 3333, NULL);
	if (!stub) return 1;

	do {
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
			case gdbstat_attach:
				printf("[==>] attached\n");
				break;
			case gdbstat_break:
				printf("[==>] break execution\n");
				gdbstub_signal_status(stub, gdbt_break_req, ~(uint32_t)0);
				break;
			case gdbstat_continue:
				printf("[==>] continue execution\n");
				// TODO: send signal status on SIGSTOP? eh.
				break;
			case gdbstat_step:
				printf("[==>] single-step\n");
				gdbstub_signal_status(stub, gdbt_singlestep, ~(uint32_t)0);
				break;
			case gdbstat_disconnect:
				printf("[==>] disconnect\n");
				gdbstub_signal_status(stub, gdbt_none, ~(uint32_t)0);
				break;
			}

			if (s == gdbstat_disconnect) break;
		}
	} while (false);

	gdbstub_close(stub);
	return 0;
}

