
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "GdbStub.h"
#include "Platform.h"

static u32 my_read_reg(void* ud, Gdb::Register reg) {
	printf("[==>] read reg %d\n", (int)reg);

	if (reg == Gdb::Register::pc) return 0x000000df; // cpsr: irq,fiq disabled, arm, sys mode
	else return 0x69420;
}
static void my_write_reg(void* ud, Gdb::Register reg, u32 value) {
	printf("[==>] write reg %d: 0x%08x\n", (int)reg, value);
}
static u32 my_read_mem(void* ud, u32 addr, int len) {
	printf("[==>] read mem 0x%08x (size %u)\n", addr, len);

	static const u32 words[] = {
		0xeafffffe,
		0xe0211002,
		0xe12fff1e,
		0
	};

	// $: b $ (arm)
	return words[(addr>>2)&3] & ((1uLL<<len)-1);
}
static void my_write_mem(void* ud, u32 addr, int len, u32 value) {
	printf("[==>] write addr 0x%08x (size %u): 0x%08x\n", addr, len, value);
}

static void my_reset(void* ud) {
	printf("[==>] RESET!!!\n");
}
static int my_remote_cmd(void* ud, const u8* cmd, size_t len) {
	printf("[==>] Rcmd: %s\n", cmd);
	return 0;
}

const static Gdb::StubCallbacks cb = {
	.cpu = 9,

	.ReadReg = my_read_reg,
	.WriteReg = my_write_reg,
	.ReadMem = my_read_mem,
	.WriteMem = my_write_mem,

	.Reset = my_reset,
	.RemoteCmd = my_remote_cmd,
};

int main(int argc, char** argv) {
	Gdb::GdbStub stub(&cb, 3333, NULL);
	if (!stub.Init()) return 1;

	do {
		while (true) {
			Gdb::StubState s = stub.Poll();

			if (s == Gdb::StubState::None || s == Gdb::StubState::NoConn) {
				struct timespec ts;
				ts.tv_sec = 0;
				ts.tv_nsec = 1000*1000; // 1 ms
				nanosleep(&ts, NULL);
				continue;
			}

			switch (s) {
			case Gdb::StubState::Attach:
				printf("[==>] attached\n");
				break;
			case Gdb::StubState::Break:
				printf("[==>] break execution\n");
				stub.SignalStatus(Gdb::TgtStatus::BreakReq, ~(u32)0);
				break;
			case Gdb::StubState::Continue:
				printf("[==>] continue execution\n");
				// TODO: send signal status on SIGSTOP? eh.
				break;
			case Gdb::StubState::Step:
				printf("[==>] single-step\n");
				stub.SignalStatus(Gdb::TgtStatus::SingleStep, ~(u32)0);
				break;
			case Gdb::StubState::Disconnect:
				printf("[==>] disconnect\n");
				stub.SignalStatus(Gdb::TgtStatus::None, ~(u32)0);
				break;
			}

			if (s == Gdb::StubState::Disconnect) break;
		}
	} while (false);

	stub.Close();
	return 0;
}

namespace Platform {
void Log(LogLevel level, const char* fmt, ...)
{
    if (fmt == nullptr)
        return;

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
}

