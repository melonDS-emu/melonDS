
#ifndef GDBSTUB_H_
#define GDBSTUB_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

enum gdbtgt_status {
	gdbt_none,
	gdbt_running,
	gdbt_singlestep,
	gdbt_break_req, // "break" command from gdb client
	gdbt_bkpt,
	gdbt_watchpt,
	gdbt_bkpt_insn, // "bkpt" instruction
	gdbt_fault_data, // data abort
	gdbt_fault_iacc, // instruction fetch abort
	gdbt_fault_insn, // illegal instruction
};

struct gdbstub_callbacks {
	int cpu; // 7 or 9 (currently, maybe also xtensa in the future?)

	// 0..14: as usual
	// 15: pc *pipeline-corrected*
	// 16: cpsr
	uint32_t (* read_reg)(int reg);
	void     (*write_reg)(int reg, uint32_t value);

	uint32_t (* read_mem)(uint32_t addr, int len);
	void     (*write_mem)(uint32_t addr, int len, uint32_t value);

	void (*add_bkpt)(uint32_t addr, uint32_t kind);
	void (*del_bkpt)(uint32_t addr, uint32_t kind);

	void (*add_watchpt)(uint32_t addr, uint32_t len);
	void (*del_watchpt)(uint32_t addr, uint32_t len);

	void (*del_all_bp_wp)(void);

	void (*reset)(void);
	int (*remote_cmd)(const uint8_t* cmd, size_t len);
	uint32_t (*crc_32)(uint32_t addr, uint32_t len);
};

enum gdbstub_state {
	gdbstat_noconn,
	gdbstat_none,
	gdbstat_break,
	gdbstat_continue,
	gdbstat_step,
	gdbstat_disconnect,
	gdbstat_attach
};

struct gdbstub {
	int sockfd;
	int connfd;
	struct sockaddr_in server, client;
	const struct gdbstub_callbacks* cb;
	enum gdbtgt_status stat;
	uint32_t cur_bkpt, cur_watchpt;
	bool stat_flag;
};

struct gdbstub* gdbstub_new(const struct gdbstub_callbacks* cb, int port);
void gdbstub_close(struct gdbstub* stub);

enum gdbstub_state gdbstub_poll(struct gdbstub* stub);

void gdbstub_signal_status(struct gdbstub* stub, enum gdbtgt_status stat, uint32_t arg);

enum gdbstub_state gdbstub_enter_reason(struct gdbstub* stub, bool stay, enum gdbtgt_status stat, uint32_t arg);
static inline enum gdbstub_state gdbstub_enter(struct gdbstub* stub, bool stay) {
	return gdbstub_enter_reason(stub, stay, -1, ~(uint32_t)0);
}


#endif

