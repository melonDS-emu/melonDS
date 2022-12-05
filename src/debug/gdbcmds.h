
#ifndef GDBCMDS_H_
#define GDBCMDS_H_

#include "gdbstub.h"
#include "gdbproto.h"

enum gdbproto_exec_result gdb_handle_g(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_G(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_m(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_M(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_X(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);

enum gdbproto_exec_result gdb_handle_c(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_s(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_p(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_P(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_H(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);

enum gdbproto_exec_result gdb_handle_Question(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_Exclamation(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_D(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_r(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_R(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);

enum gdbproto_exec_result gdb_handle_z(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_Z(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);

enum gdbproto_exec_result gdb_handle_q_HostInfo(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_q_Rcmd(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_q_Supported(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_q_CRC(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_q_C(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_q_fThreadInfo(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_q_sThreadInfo(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_q_TStatus(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_q_features(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_q_Attached(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);

enum gdbproto_exec_result gdb_handle_v_Attach(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_v_Kill(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_v_Run(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_v_Stopped(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);
enum gdbproto_exec_result gdb_handle_v_MustReplyEmpty(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len);

#endif

