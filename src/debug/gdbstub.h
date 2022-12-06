
#ifndef GDBSTUB_H_
#define GDBSTUB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

enum gdbtgt_status {
	gdbt_no_event,

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
	uint32_t (* read_reg)(void* ud, int reg);
	void     (*write_reg)(void* ud, int reg, uint32_t value);

	uint32_t (* read_mem)(void* ud, uint32_t addr, int len);
	void     (*write_mem)(void* ud, uint32_t addr, int len, uint32_t value);

	void (*reset)(void* ud);
	int (*remote_cmd)(void* ud, const uint8_t* cmd, size_t len);
};

enum gdbstub_state {
	gdbstat_noconn,
	gdbstat_none,
	gdbstat_break,
	gdbstat_continue,
	gdbstat_step,
	gdbstat_disconnect,
	gdbstat_attach,
	gdbstat_check_no_hit
};

struct gdbstub;

struct gdbstub* gdbstub_new(const struct gdbstub_callbacks* cb, int port, void* ud);
void gdbstub_close(struct gdbstub* stub);

enum gdbstub_state gdbstub_poll(struct gdbstub* stub);

void gdbstub_signal_status(struct gdbstub* stub, enum gdbtgt_status stat, uint32_t arg);

enum gdbstub_state gdbstub_enter_reason(struct gdbstub* stub, bool stay, enum gdbtgt_status stat, uint32_t arg);
static inline enum gdbstub_state gdbstub_enter(struct gdbstub* stub, bool stay) {
	return gdbstub_enter_reason(stub, stay, gdbt_no_event, ~(uint32_t)0);
}


// kind: 2=thumb, 3=thumb2 (not relevant), 4=arm
void gdbstub_add_bkpt(struct gdbstub* stub, uint32_t addr, int kind);
void gdbstub_del_bkpt(struct gdbstub* stub, uint32_t addr, int kind);

// kind: 2=read, 3=write, 4=rdwr
void gdbstub_add_watchpt(struct gdbstub* stub, uint32_t addr, uint32_t len, int kind);
void gdbstub_del_watchpt(struct gdbstub* stub, uint32_t addr, uint32_t len, int kind);

void gdbstub_del_all_bp_wp(struct gdbstub* stub);

enum gdbstub_state gdbstub_check_bkpt(struct gdbstub* stub, uint32_t addr, bool enter, bool stay);
enum gdbstub_state gdbstub_check_watchpt(struct gdbstub* stub, uint32_t addr, int kind, bool enter, bool stay);

#ifdef __cplusplus
}
#endif

#endif

