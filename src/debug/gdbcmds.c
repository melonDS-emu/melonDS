
#include <stdio.h>

#include "hexutil.h"
#include "gdbproto.h"

#include "gdbcmds.h"


enum gdb_signal {
	GDB_SIGINT  = 2,
	GDB_SIGTRAP = 5,
	GDB_SIGSEGV = 11,
	GDB_SIGILL  = 4
};


static const char* TARGET_INFO_ARM7 = "cputype:arm;cpusubtype:armv4t:ostype:none;vendor:none;endian:little;ptrsize:4;";
static const char* TARGET_INFO_ARM9 = "cputype:arm;cpusubtype:armv5te:ostype:none;vendor:none;endian:little;ptrsize:4;";

static const char* TARGET_XML_ARM7 =
	"<target version=\"1.0\">"
	"<architecture>armv4t</architecture>"
	"<osabi>none</osabi>"
	"<feature name=\"org.gnu.gdb.arm.core\">"
	"<reg name=\"r0\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r1\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r2\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r3\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r4\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r5\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r6\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r7\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r8\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r9\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r10\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r11\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r12\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>"
	"<reg name=\"lr\" bitsize=\"32\"/>"
	"<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>"
	"<reg name=\"cpsr\" bitsize=\"32\" regnum=\"25\"/>"
	"</feature>"
	"</target>";

static const char* TARGET_XML_ARM9 =
	"<target version=\"1.0\">"
	"<architecture>armv5te</architecture>"
	"<osabi>none</osabi>"
	"<feature name=\"org.gnu.gdb.arm.core\">"
	"<reg name=\"r0\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r1\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r2\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r3\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r4\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r5\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r6\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r7\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r8\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r9\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r10\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r11\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"r12\" bitsize=\"32\" type=\"uint32\"/>"
	"<reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>"
	"<reg name=\"lr\" bitsize=\"32\"/>"
	"<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>"
	"<reg name=\"cpsr\" bitsize=\"32\" regnum=\"25\"/>"
	"</feature>"
	"</target>";
	// TODO: CP15?


static int do_q_response(int connfd, const char* query, const uint8_t* data, const size_t len) {
	size_t qaddr, qlen;

	//printf("[GDB qresp] query='%s'\n", query);
	if (sscanf(query, "%zx,%zx", &qaddr, &qlen) < 2) {
		return gdbproto_resp_str(connfd, "E01");
	} else if (qaddr >  len) {
		return gdbproto_resp_str(connfd, "E01");
	} else if (qaddr == len) {
		return gdbproto_resp_str(connfd, "l");
	}

	size_t outlen = len - qlen;
	if (outlen > len) outlen = len;

	return gdbproto_resp_2(connfd, "m", 1, &data[qaddr], outlen);
}

__attribute__((__aligned__(4)))
static uint8_t tempdatabuf[1024];

enum gdbproto_exec_result gdb_handle_g(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {

	uint8_t* regstrbuf = tempdatabuf;

	for (size_t i = 0; i < 17; ++i) {
		uint32_t v = stub->cb->read_reg(i);
		hexfmt32(&regstrbuf[i*4*2], v);
	}

	gdbproto_resp(stub->connfd, regstrbuf, 17*4*2);

	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_G(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO: recombobulate mem write data
	int reg=0;
	uint32_t v = 0;
	stub->cb->write_reg(reg, v);
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_m(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	uint32_t addr = 0, llen = 0;

	if (sscanf(cmd, "%08X,%08X", &addr, &llen) < 2) {
		gdbproto_resp_str(stub->connfd, "E01");
		return gdbe_ok;
	} else if (len > (GDBPROTO_BUFFER_CAPACITY/2)) {
		gdbproto_resp_str(stub->connfd, "E02");
		return gdbe_ok;
	}

	uint8_t* datastr = tempdatabuf;

	for (size_t i = 0; i < llen; i += 4) {
		uint32_t left = 4;
		if (llen - i < left) left = llen - i;

		// FIXME: only aligned memory accesses here!
		uint32_t v = stub->cb->read_mem(addr + i, left*8);
		//printf("[GDB] addr %08lx (%d bytes) -> %08x\n", addr + i, left, v);

		if (left == 4) {
			hexfmt32(&datastr[i*2], v);
		} else for (size_t j = 0; j < left; ++j, ++i, v >>= 8) {
			hexfmt8(&datastr[i*2], v);
		}
	}

	gdbproto_resp(stub->connfd, datastr, llen*2);

	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_M(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO: recombobulate mem write data
	uint32_t addr = 0, llen = 0, v = 0;
	stub->cb->write_mem(addr, llen, v);
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_c(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_s(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_p(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_P(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_H(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {

	uint8_t operation = cmd[0];
	uint32_t thread_id;
	sscanf(&cmd[1], "%u", &thread_id);

	(void)operation;
	if (thread_id <= 1) {
		gdbproto_resp_str(stub->connfd, "OK");
	} else {
		gdbproto_resp_str(stub->connfd, "E01");
	}

	// TODO
	return gdbe_ok;
}


enum gdbproto_exec_result gdb_handle_Question(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// "request reason for target halt" (which must also halt)

	enum gdbtgt_status st = stub->cb->target_get_status();
	uint32_t arg = ~(uint32_t)0;
	int typ = 0;

	switch (st) {
	case gdbt_none: // no target!
		gdbproto_resp_str(stub->connfd, "W00");
		break;

	case gdbt_running: // will break very soon due to retval
	case gdbt_break_req:
		gdbproto_resp_fmt(stub->connfd, "T%02X", GDB_SIGINT);
		break;

	case gdbt_bkpt:
		arg = stub->cb->get_cur_bkpt();
		typ = 1;
		goto case_gdbt_bkpt_insn;
	case gdbt_watchpt:
		arg = stub->cb->get_cur_watchpt();
		typ = 2;
	case_gdbt_bkpt_insn:
	case gdbt_bkpt_insn:
		if (!~arg) {
			gdbproto_resp_fmt(stub->connfd, "T%02X", GDB_SIGTRAP);
		} else {
			switch (typ) {
			case 1:
				gdbproto_resp_fmt(stub->connfd, "T%02Xbreak:%08X;", GDB_SIGTRAP, arg);
				break;
			case 2:
				gdbproto_resp_fmt(stub->connfd, "T%02Xwatch:%08X;", GDB_SIGTRAP, arg);
				break;
			default:
				gdbproto_resp_fmt(stub->connfd, "T%02X", GDB_SIGTRAP);
				break;
			}
		}
		break;

		// these three should technically be a SIGBUS but gdb etc don't really
		// like that (plus it sounds confusing)
	case gdbt_fault_data:
	case gdbt_fault_iacc:
		gdbproto_resp_fmt(stub->connfd, "T%02X", GDB_SIGINT);
		break;
	case gdbt_fault_insn:
		gdbproto_resp_fmt(stub->connfd, "T%02X", GDB_SIGILL);
		break;
	}

	return gdbe_must_break;
}

enum gdbproto_exec_result gdb_handle_D(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	gdbproto_resp_str(stub->connfd, "OK");
	return gdbe_detached;
}

enum gdbproto_exec_result gdb_handle_r(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO
	return gdbe_ok;
}


enum gdbproto_exec_result gdb_handle_z(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_Z(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_q_Rcmd(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_q_Supported(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO: support Xfer:memory-map:read::
	//       but NWRAM is super annoying with that
	gdbproto_resp_fmt(stub->connfd, "PacketSize=%X;qXfer:features:read+;swbreak-;hwbreak+", GDBPROTO_BUFFER_CAPACITY);
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_q_CRC(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_q_C(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	gdbproto_resp_str(stub->connfd, "QC1"); // current thread ID is 1
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_q_fThreadInfo(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	gdbproto_resp_str(stub->connfd, "m1"); // one thread
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_q_sThreadInfo(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	gdbproto_resp_str(stub->connfd, "l"); // end of thread list
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_q_features(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	const char* resp;

	//printf("CPU type = %d\n", stub->cb->cpu);
	switch (stub->cb->cpu) {
	case 7: resp = TARGET_XML_ARM7; break;
	case 9: resp = TARGET_XML_ARM9; break;
	default: resp = ""; break;
	}

	do_q_response(stub->connfd, cmd, resp, strlen(resp));
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_q_Attached(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	gdbproto_resp_str(stub->connfd, "1"); // always "attach to a process"
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_v_Attach(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_v_Kill(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_v_Run(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_v_Stopped(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_v_MustReplyEmpty(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// TODO
	gdbproto_resp(stub->connfd, NULL, 0);
	return gdbe_ok;
}

