
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

	enum gdbtgt_status (*target_get_status)(void);

	// 0..14: as usual
	// 15: pc *pipeline-corrected*
	// 16: cpsr
	uint32_t (* read_reg)(int reg);
	void     (*write_reg)(int reg, uint32_t value);

	uint32_t (* read_mem)(uint32_t addr, uint32_t len);
	void     (*write_mem)(uint32_t addr, uint32_t len, uint32_t value);

	void (*add_bkpt)(uint32_t addr, uint32_t len);
	void (*del_bkpt)(uint32_t addr, uint32_t len);

	void (*add_watchpt)(uint32_t addr, uint32_t len);
	void (*del_watchpt)(uint32_t addr, uint32_t len);

	uint32_t (*get_cur_bkpt)(void);
	uint32_t (*get_cur_watchpt)(void);
};

enum gdbstub_state {
	gdbstat_noconn,
	gdbstat_none,
	gdbstat_break,
	gdbstat_continue,
	gdbstat_disconnect
};

struct gdbstub {
	int sockfd;
	int connfd;
	struct sockaddr_in server, client;
	const struct gdbstub_callbacks* cb;
};

struct gdbstub* gdbstub_new(const struct gdbstub_callbacks* cb, int port);
void gdbstub_close(struct gdbstub* stub);

enum gdbstub_state gdbstub_poll(struct gdbstub* stub);

#endif

