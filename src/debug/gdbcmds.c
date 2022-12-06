
#include <stdio.h>
#include <assert.h>

#include "hexutil.h"
#include "gdbproto.h"

#include "../CRC32.h"

#include "gdbcmds.h"

#include "gdbstub_internal.h"

#define ARCH_N_REG 17


enum gdb_signal {
	GDB_SIGINT  = 2,
	GDB_SIGTRAP = 5,
	GDB_SIGEMT  = 7, // "emulation trap"
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
	if (sscanf(query, "%zx,%zx", &qaddr, &qlen) != 2) {
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

	for (size_t i = 0; i < ARCH_N_REG; ++i) {
		uint32_t v = stub->cb->read_reg(stub->ud, i);
		hexfmt32(&regstrbuf[i*4*2], v);
	}

	gdbproto_resp(stub->connfd, regstrbuf, ARCH_N_REG*4*2);

	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_G(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	if (len != ARCH_N_REG*4*2) {
		printf("[GDB] REG WRITE ERR: BAD LEN: %zd != %d!\n", len, ARCH_N_REG*4*2);
		gdbproto_resp_str(stub->connfd, "E01");
		return gdbe_ok;
	}

	for (int i = 0; i < ARCH_N_REG; ++i) {
		uint32_t v = unhex32(&cmd[i*4*2]);
		stub->cb->write_reg(stub->ud, i, v);
	}

	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_m(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	uint32_t addr = 0, llen = 0, end;

	if (sscanf(cmd, "%08X,%08X", &addr, &llen) != 2) {
		gdbproto_resp_str(stub->connfd, "E01");
		return gdbe_ok;
	} else if (llen > (GDBPROTO_BUFFER_CAPACITY/2)) {
		gdbproto_resp_str(stub->connfd, "E02");
		return gdbe_ok;
	}
	end = addr + llen;

	uint8_t* datastr = tempdatabuf;
	uint8_t* dataptr = datastr;

	// pre-align: byte
	if ((addr & 1)) {
		if ((end-addr) >= 1) {
			uint32_t v = stub->cb->read_mem(stub->ud, addr, 8);
			hexfmt8(dataptr, v&0xff);
			++addr;
			dataptr += 2;
		} else goto end;
	}

	// pre-align: short
	if ((addr & 2)) {
		if ((end-addr) >= 2) {
			uint32_t v = stub->cb->read_mem(stub->ud, addr, 16);
			hexfmt16(dataptr, v&0xffff);
			addr += 2;
			dataptr += 4;
		} else if ((end-addr) == 1) { // last byte
			uint32_t v = stub->cb->read_mem(stub->ud, addr, 8);
			hexfmt8(dataptr, v&0xff);
			++addr;
			dataptr += 2;
		} else goto end;
	}

	// main loop: 4-byte chunks
	while (addr < end) {
		if (end - addr < 4) break; // post-align stuff

		uint32_t v = stub->cb->read_mem(stub->ud, addr, 32);
		hexfmt32(dataptr, v);
		addr += 4;
		dataptr += 8;
	}

	// post-align: short
	if ((end-addr) & 2) {
		uint32_t v = stub->cb->read_mem(stub->ud, addr, 16);
		hexfmt16(dataptr, v&0xffff);
		addr += 2;
		dataptr += 4;
	}

	// post-align: byte
	if ((end-addr) == 1) {
		uint32_t v = stub->cb->read_mem(stub->ud, addr, 8);
		hexfmt8(dataptr, v&0xff);
		++addr;
		dataptr += 2;
	}

end:
	assert(addr == end);

	gdbproto_resp(stub->connfd, datastr, llen*2);

	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_M(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {

	uint32_t addr, llen, end;
	int inoff;

	if (sscanf(cmd, "%08X,%08X:%n", &addr, &llen, &inoff) != 2) {
		gdbproto_resp_str(stub->connfd, "E01");
		return gdbe_ok;
	} else if (llen > (GDBPROTO_BUFFER_CAPACITY/2)) {
		gdbproto_resp_str(stub->connfd, "E02");
		return gdbe_ok;
	}
	end = addr + llen;

	const uint8_t* dataptr = cmd + inoff;

	// pre-align: byte
	if ((addr & 1)) {
		if ((end-addr) >= 1) {
			uint8_t v = unhex8(dataptr);
			stub->cb->write_mem(stub->ud, addr, 8, v);
			++addr;
			dataptr += 2;
		} else goto end;
	}

	// pre-align: short
	if ((addr & 2)) {
		if ((end-addr) >= 2) {
			uint16_t v = unhex16(dataptr);
			stub->cb->write_mem(stub->ud, addr, 16, v);
			addr += 2;
			dataptr += 4;
		} else if ((end-addr) == 1) { // last byte
			uint8_t v = unhex8(dataptr);
			stub->cb->write_mem(stub->ud, addr, 8, v);
			++addr;
			dataptr += 2;
		} else goto end;
	}

	// main loop: 4-byte chunks
	while (addr < end) {
		if (end - addr < 4) break; // post-align stuff

		uint32_t v = unhex32(dataptr);
		stub->cb->write_mem(stub->ud, addr, 32, v);
		addr += 4;
		dataptr += 8;
	}

	// post-align: short
	if ((end-addr) & 2) {
		uint16_t v = unhex16(dataptr);
		stub->cb->write_mem(stub->ud, addr, 16, v);
		addr += 2;
		dataptr += 4;
	}

	// post-align: byte
	if ((end-addr) == 1) {
		uint8_t v = unhex8(dataptr);
		stub->cb->write_mem(stub->ud, addr, 8, v);
		++addr;
		dataptr += 2;
	}

end:
	assert(addr == end);

	gdbproto_resp_str(stub->connfd, "OK");

	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_X(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {

	uint32_t addr, llen, end;
	int inoff;

	if (sscanf(cmd, "%08X,%08X:%n", &addr, &llen, &inoff) != 2) {
		gdbproto_resp_str(stub->connfd, "E01");
		return gdbe_ok;
	} else if (llen > (GDBPROTO_BUFFER_CAPACITY/2)) {
		gdbproto_resp_str(stub->connfd, "E02");
		return gdbe_ok;
	}
	end = addr + llen;

	const uint8_t* dataptr = cmd + inoff;

	// pre-align: byte
	if ((addr & 1)) {
		if ((end-addr) >= 1) {
			uint8_t v = *dataptr;
			stub->cb->write_mem(stub->ud, addr, 8, v);
			++addr;
			dataptr += 1;
		} else goto end;
	}

	// pre-align: short
	if ((addr & 2)) {
		if ((end-addr) >= 2) {
			uint16_t v = dataptr[0] | ((uint16_t)dataptr[1] << 8);
			stub->cb->write_mem(stub->ud, addr, 16, v);
			addr += 2;
			dataptr += 2;
		} else if ((end-addr) == 1) { // last byte
			uint8_t v = *dataptr;
			stub->cb->write_mem(stub->ud, addr, 8, v);
			++addr;
			dataptr += 1;
		} else goto end;
	}

	// main loop: 4-byte chunks
	while (addr < end) {
		if (end - addr < 4) break; // post-align stuff

		uint32_t v = dataptr[0] | ((uint32_t)dataptr[1] << 8)
			| ((uint32_t)dataptr[2] << 16) | ((uint32_t)dataptr[3] << 24);
		stub->cb->write_mem(stub->ud, addr, 32, v);
		addr += 4;
		dataptr += 4;
	}

	// post-align: short
	if ((end-addr) & 2) {
		uint16_t v = dataptr[0] | ((uint16_t)dataptr[1] << 8);
		stub->cb->write_mem(stub->ud, addr, 16, v);
		addr += 2;
		dataptr += 2;
	}

	// post-align: byte
	if ((end-addr) == 1) {
		uint8_t v = unhex8(dataptr);
		stub->cb->write_mem(stub->ud, addr, 8, v);
		++addr;
		dataptr += 1;
	}

end:
	assert(addr == end);

	gdbproto_resp_str(stub->connfd, "OK");

	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_c(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	uint32_t addr = ~(uint32_t)0;

	if (len > 0) {
		if (len <= 8) {
			if (sscanf(cmd, "%08X", &addr) != 1) {
				gdbproto_resp_str(stub->connfd, "E01");
			} // else: ok
		} else {
			gdbproto_resp_str(stub->connfd, "E01");
		}
	} // else: continue at current

	if (!~addr) {
		stub->cb->write_reg(stub->ud, 15, addr); // set pc
	}

	return gdbe_continue;
}

enum gdbproto_exec_result gdb_handle_s(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	uint32_t addr = ~(uint32_t)0;

	if (len > 0) {
		if (len <= 8) {
			if (sscanf(cmd, "%08X", &addr) != 1) {
				gdbproto_resp_str(stub->connfd, "E01");
				return gdbe_ok;
			} // else: ok
		} else {
			gdbproto_resp_str(stub->connfd, "E01");
			return gdbe_ok;
		}
	} // else: continue at current

	if (~addr != 0) {
		stub->cb->write_reg(stub->ud, 15, addr); // set pc
	}

	return gdbe_step;
}

enum gdbproto_exec_result gdb_handle_p(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	int reg;
	if (sscanf(cmd, "%x", &reg) != 1 || reg < 0 || reg >= ARCH_N_REG) {
		gdbproto_resp_str(stub->connfd, "E01");
		return gdbe_ok;
	}

	uint32_t v = stub->cb->read_reg(stub->ud, reg);
	hexfmt32(tempdatabuf, v);
	gdbproto_resp(stub->connfd, tempdatabuf, 4*2);

	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_P(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	int reg, dataoff;

	if (sscanf(cmd, "%x=%n", &reg, &dataoff) != 1 || reg < 0
			|| reg >= ARCH_N_REG || dataoff + 4*2 > len) {
		gdbproto_resp_str(stub->connfd, "E01");
		return gdbe_ok;
	}

	uint32_t v = unhex32(&cmd[dataoff]);
	stub->cb->write_reg(stub->ud, reg, v);

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

	return gdbe_ok;
}


enum gdbproto_exec_result gdb_handle_Question(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	// "request reason for target halt" (which must also halt)

	enum gdbtgt_status st = stub->stat;
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

	case gdbt_singlestep:
		gdbproto_resp_fmt(stub->connfd, "S%02X", GDB_SIGTRAP);
		break;

	case gdbt_bkpt:
		arg = stub->cur_bkpt;
		typ = 1;
		goto bkpt_rest;
	case gdbt_watchpt:
		arg = stub->cur_watchpt;
		typ = 2;
	bkpt_rest:
		if (!~arg) {
			gdbproto_resp_fmt(stub->connfd, "T%02X", GDB_SIGTRAP);
		} else {
			switch (typ) {
			case 1:
				gdbproto_resp_fmt(stub->connfd, "T%02Xswbreak:%08X;", GDB_SIGTRAP, arg);
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
	case gdbt_bkpt_insn:
		gdbproto_resp_fmt(stub->connfd, "T%02Xswbreak:%08X;", GDB_SIGTRAP, stub->cb->read_reg(stub->ud, 15));
		break;

		// these three should technically be a SIGBUS but gdb etc don't really
		// like that (plus it sounds confusing)
	case gdbt_fault_data:
	case gdbt_fault_iacc:
		gdbproto_resp_fmt(stub->connfd, "T%02X", GDB_SIGSEGV);
		break;
	case gdbt_fault_insn:
		gdbproto_resp_fmt(stub->connfd, "T%02X", GDB_SIGILL);
		break;
	}

	return gdbe_initial_break;
}

enum gdbproto_exec_result gdb_handle_Exclamation(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	gdbproto_resp_str(stub->connfd, "OK");
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_D(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	gdbproto_resp_str(stub->connfd, "OK");
	return gdbe_detached;
}

enum gdbproto_exec_result gdb_handle_r(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	stub->cb->reset(stub->ud);
	return gdbe_ok;
}
enum gdbproto_exec_result gdb_handle_R(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	stub->cb->reset(stub->ud);
	return gdbe_ok;
}


enum gdbproto_exec_result gdb_handle_z(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
/*
[GDB] recv() 10 bytes: '$vCont?#49'
[GDB] command in: 'vCont?'
[GDB] subcommand in: 'Cont?'
[GDB] unknown subcommand 'Cont?'!
[GDB] send resp: '$#00'
[GDB] got ack: '+'
[GDB] recv() 7 bytes: '$Hc0#db'
[GDB] command in: 'Hc0'
[GDB] send resp: '$OK#9a'
[GDB] got ack: '+'
[GDB] recv() 5 bytes: '$c#63'
[GDB] command in: 'c'
[==>] write reg 15: 0xffffffff
[==>] continue execution
[GDB] recv() 1 bytes: ''
[GDB] recv() error 0, errno=0 (Success)
*/

	int typ;
	uint32_t addr, kind;

	if (sscanf("%d,%x,%u", cmd, &typ, &addr, &kind) != 3) {
		gdbproto_resp_str(stub->connfd, "E01");
		return gdbe_ok;
	}

	switch (typ) {
	case 0: case 1: // remove breakpoint (we cheat & always insert a hardware breakpoint)
		gdbstub_del_bkpt(stub, addr, kind);
		break;
	case 2: case 3: case 4: // watchpoint. currently not distinguishing between reads & writes oops
		gdbstub_del_watchpt(stub, addr, kind, typ);
		break;
	default:
		gdbproto_resp_str(stub->connfd, "E02");
		return gdbe_ok;
	}

	gdbproto_resp_str(stub->connfd, "OK");
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_Z(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	int typ;
	uint32_t addr, kind;

	if (sscanf("%d,%x,%u", cmd, &typ, &addr, &kind) != 3) {
		gdbproto_resp_str(stub->connfd, "E01");
		return gdbe_ok;
	}

	switch (typ) {
	case 0: case 1: // insert breakpoint (we cheat & always insert a hardware breakpoint)
		gdbstub_add_bkpt(stub, addr, kind);
		break;
	case 2: case 3: case 4: // watchpoint. currently not distinguishing between reads & writes oops
		gdbstub_add_watchpt(stub, addr, kind, typ);
		break;
	default:
		gdbproto_resp_str(stub->connfd, "E02");
		return gdbe_ok;
	}

	gdbproto_resp_str(stub->connfd, "OK");
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_q_HostInfo(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	const char* resp = "";

	switch (stub->cb->cpu) {
	case 7: resp = TARGET_INFO_ARM7; break;
	case 9: resp = TARGET_INFO_ARM9; break;
	default: break;
	}

	gdbproto_resp_str(stub->connfd, resp);
	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_q_Rcmd(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {

	memset(tempdatabuf, 0, sizeof tempdatabuf);
	for (ssize_t i = 0; i < len/2; ++i) {
		tempdatabuf[i] = unhex8(&cmd[i*2]);
	}

	int r = stub->cb->remote_cmd(stub->ud, tempdatabuf, len/2);

	if (r) {
		gdbproto_resp_fmt(stub->connfd, "E%02X", r&0xff);
	} else {
		gdbproto_resp_str(stub->connfd, "OK");
	}

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
		const uint8_t* cmd, ssize_t llen) {
	static uint8_t crcbuf[128];

	uint32_t addr, len;
	if (sscanf(cmd, "%x,%x", &addr, &len) != 2) {
		gdbproto_resp_str(stub->connfd, "E01");
		return gdbe_ok;
	}

	uint32_t val = 0; // start at 0
	uint32_t caddr = addr;
	uint32_t realend = addr + len;

	for (; caddr < addr + len; ) {
		// calc partial CRC in 128-byte chunks
		uint32_t end = caddr + sizeof(crcbuf)/sizeof(crcbuf[0]);
		if (end > realend) end = realend;
		uint32_t clen = end - caddr;

		for (size_t i = 0; caddr < end; ++caddr, ++i) {
			crcbuf[i] = stub->cb->read_mem(stub->ud, caddr, 8);
		}

		val = CRC32(crcbuf, clen, val);
	}

	gdbproto_resp_fmt(stub->connfd, "C%x", val);

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

	enum gdbtgt_status st = stub->stat;

	if (st == gdbt_none) {
		// no target
		gdbproto_resp_str(stub->connfd, "E01");
		return gdbe_ok;
	}

	gdbproto_resp_str(stub->connfd, "T05thread:1;");

	if (st == gdbt_running) {
		return gdbe_must_break;
	} else return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_v_Kill(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {

	enum gdbtgt_status st = stub->stat;

	stub->cb->reset(stub->ud);

	gdbproto_resp_str(stub->connfd, "OK");

	return (st != gdbt_running && st != gdbt_none) ? gdbe_detached : gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_v_Run(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {

	enum gdbtgt_status st = stub->stat;

	stub->cb->reset(stub->ud);

	// TODO: handle cmdline for homebrew?

	return (st != gdbt_running && st != gdbt_none) ? gdbe_continue : gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_v_Stopped(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	enum gdbtgt_status st = stub->stat;

	static bool notified = true;

	// not sure if i understand this correctly
	if (st != gdbt_running) {
		if (notified) {
			gdbproto_resp_str(stub->connfd, "OK");
		} else {
			gdbproto_resp_str(stub->connfd, "W00");
		}

		notified = !notified;
	} else {
		gdbproto_resp_str(stub->connfd, "OK");
	}

	return gdbe_ok;
}

enum gdbproto_exec_result gdb_handle_v_MustReplyEmpty(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	gdbproto_resp(stub->connfd, NULL, 0);
	return gdbe_ok;
}

