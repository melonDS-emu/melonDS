
#include <stdio.h>
#include <assert.h>

#include "../CRC32.h"
#include "../Platform.h"
#include "hexutil.h"

#include "GdbProto.h"

using Platform::Log;
using Platform::LogLevel;

namespace Gdb {

enum class GdbSignal : int {
	INT  = 2,
	TRAP = 5,
	EMT  = 7, // "emulation trap"
	SEGV = 11,
	ILL  = 4
};


const char* TARGET_INFO_ARM7 = "cputype:arm;cpusubtype:armv4t:ostype:none;vendor:none;endian:little;ptrsize:4;";
const char* TARGET_INFO_ARM9 = "cputype:arm;cpusubtype:armv5te:ostype:none;vendor:none;endian:little;ptrsize:4;";


#define TARGET_XML__CORE_REGS \
	"<reg name=\"r0\" bitsize=\"32\" type=\"uint32\"/>" \
	"<reg name=\"r1\" bitsize=\"32\" type=\"uint32\"/>" \
	"<reg name=\"r2\" bitsize=\"32\" type=\"uint32\"/>" \
	"<reg name=\"r3\" bitsize=\"32\" type=\"uint32\"/>" \
	"<reg name=\"r4\" bitsize=\"32\" type=\"uint32\"/>" \
	"<reg name=\"r5\" bitsize=\"32\" type=\"uint32\"/>" \
	"<reg name=\"r6\" bitsize=\"32\" type=\"uint32\"/>" \
	"<reg name=\"r7\" bitsize=\"32\" type=\"uint32\"/>" \
	"<reg name=\"r8\" bitsize=\"32\" type=\"uint32\"/>" \
	"<reg name=\"r9\" bitsize=\"32\" type=\"uint32\"/>" \
	"<reg name=\"r10\" bitsize=\"32\" type=\"uint32\"/>" \
	"<reg name=\"r11\" bitsize=\"32\" type=\"uint32\"/>" \
	"<reg name=\"r12\" bitsize=\"32\" type=\"uint32\"/>" \
	"<reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>" \
	"<reg name=\"lr\" bitsize=\"32\" type=\"code_ptr\"/>" \
	"<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>" \
	/* 16 regs */ \

#define TARGET_XML__MODE_REGS \
	"<reg name=\"cpsr\" bitsize=\"32\" regnum=\"25\"/>" \
	"<reg name=\"sp_usr\" bitsize=\"32\" regnum=\"26\" type=\"data_ptr\"/>" \
	"<reg name=\"lr_usr\" bitsize=\"32\" regnum=\"27\" type=\"code_ptr\"/>" \
	"<reg name=\"r8_fiq\" bitsize=\"32\" type=\"uint32\" regnum=\"28\"/>" \
	"<reg name=\"r9_fiq\" bitsize=\"32\" type=\"uint32\" regnum=\"29\"/>" \
	"<reg name=\"r10_fiq\" bitsize=\"32\" type=\"uint32\" regnum=\"30\"/>" \
	"<reg name=\"r11_fiq\" bitsize=\"32\" type=\"uint32\" regnum=\"31\"/>" \
	"<reg name=\"r12_fiq\" bitsize=\"32\" type=\"uint32\" regnum=\"32\"/>" \
	"<reg name=\"sp_fiq\" bitsize=\"32\" regnum=\"33\" type=\"data_ptr\"/>" \
	"<reg name=\"lr_fiq\" bitsize=\"32\" regnum=\"34\" type=\"code_ptr\"/>" \
	"<reg name=\"sp_irq\" bitsize=\"32\" regnum=\"35\" type=\"data_ptr\"/>" \
	"<reg name=\"lr_irq\" bitsize=\"32\" regnum=\"36\" type=\"code_ptr\"/>" \
	"<reg name=\"sp_svc\" bitsize=\"32\" regnum=\"37\" type=\"data_ptr\"/>" \
	"<reg name=\"lr_svc\" bitsize=\"32\" regnum=\"38\" type=\"code_ptr\"/>" \
	"<reg name=\"sp_abt\" bitsize=\"32\" regnum=\"39\" type=\"data_ptr\"/>" \
	"<reg name=\"lr_abt\" bitsize=\"32\" regnum=\"40\" type=\"code_ptr\"/>" \
	"<reg name=\"sp_und\" bitsize=\"32\" regnum=\"41\" type=\"data_ptr\"/>" \
	"<reg name=\"lr_und\" bitsize=\"32\" regnum=\"42\" type=\"code_ptr\"/>" \
	"<reg name=\"spsr_fiq\" bitsize=\"32\" regnum=\"43\"/>" \
	"<reg name=\"spsr_irq\" bitsize=\"32\" regnum=\"44\"/>" \
	"<reg name=\"spsr_svc\" bitsize=\"32\" regnum=\"45\"/>" \
	"<reg name=\"spsr_abt\" bitsize=\"32\" regnum=\"46\"/>" \
	"<reg name=\"spsr_und\" bitsize=\"32\" regnum=\"47\"/>" \
	/* 23 regs */ \


const char* TARGET_XML_ARM7 =
	"<target version=\"1.0\">"
	"<architecture>armv4t</architecture>"
	"<osabi>none</osabi>"
	"<feature name=\"org.gnu.gdb.arm.core\">"
	TARGET_XML__CORE_REGS
	TARGET_XML__MODE_REGS
	// 39 regs total
	"</feature>"
	"</target>";


const char* TARGET_XML_ARM9 =
	"<target version=\"1.0\">"
	"<architecture>armv5te</architecture>"
	"<osabi>none</osabi>"
	"<feature name=\"org.gnu.gdb.arm.core\">"
	TARGET_XML__CORE_REGS
	TARGET_XML__MODE_REGS
	// 39 regs total
	"</feature>"
	"</target>";
	// TODO: CP15?


int do_q_response(int connfd, const u8* query, const char* data, const size_t len) {
	size_t qaddr, qlen;

	Log(LogLevel::Debug, "[GDB qresp] query='%s'\n", query);
	if (sscanf((const char*)query, "%zx,%zx", &qaddr, &qlen) != 2) {
		return Proto::RespStr(connfd, "E01");
	} else if (qaddr >  len) {
		return Proto::RespStr(connfd, "E01");
	} else if (qaddr == len) {
		return Proto::RespStr(connfd, "l");
	}

	size_t bleft = len - qaddr;
	size_t outlen = qlen;
	if (outlen > bleft) outlen = bleft;
	Log(LogLevel::Debug, "[GDB qresp] qaddr=%zu qlen=%zu left=%zu outlen=%zu\n",
			qaddr, qlen, bleft, outlen);

	return Proto::RespC(connfd, "m", 1, (const u8*)&data[qaddr], outlen);
}

__attribute__((__aligned__(4)))
u8 tempdatabuf[1024];

ExecResult GdbStub::Handle_g(GdbStub* stub, const u8* cmd, ssize_t len) {

	u8* regstrbuf = tempdatabuf;

	for (size_t i = 0; i < GDB_ARCH_N_REG; ++i) {
		u32 v = stub->cb->ReadReg(stub->ud, static_cast<Register>(i));
		hexfmt32(&regstrbuf[i*4*2], v);
	}

	Proto::Resp(stub->connfd, regstrbuf, GDB_ARCH_N_REG*4*2);

	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_G(GdbStub* stub, const u8* cmd, ssize_t len) {
	if (len != GDB_ARCH_N_REG*4*2) {
		Log(LogLevel::Error, "[GDB] REG WRITE ERR: BAD LEN: %zd != %d!\n", len, GDB_ARCH_N_REG*4*2);
		Proto::RespStr(stub->connfd, "E01");
		return ExecResult::Ok;
	}

	for (int i = 0; i < GDB_ARCH_N_REG; ++i) {
		u32 v = unhex32(&cmd[i*4*2]);
		stub->cb->WriteReg(stub->ud, static_cast<Register>(i), v);
	}

	Proto::RespStr(stub->connfd, "OK");

	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_m(GdbStub* stub, const u8* cmd, ssize_t len) {
	u32 addr = 0, llen = 0, end;

	if (sscanf((const char*)cmd, "%08X,%08X", &addr, &llen) != 2) {
		Proto::RespStr(stub->connfd, "E01");
		return ExecResult::Ok;
	} else if (llen > (GDBPROTO_BUFFER_CAPACITY/2)) {
		Proto::RespStr(stub->connfd, "E02");
		return ExecResult::Ok;
	}
	end = addr + llen;

	u8* datastr = tempdatabuf;
	u8* dataptr = datastr;

	// pre-align: byte
	if ((addr & 1)) {
		if ((end-addr) >= 1) {
			u32 v = stub->cb->ReadMem(stub->ud, addr, 8);
			hexfmt8(dataptr, v&0xff);
			++addr;
			dataptr += 2;
		} else goto end;
	}

	// pre-align: short
	if ((addr & 2)) {
		if ((end-addr) >= 2) {
			u32 v = stub->cb->ReadMem(stub->ud, addr, 16);
			hexfmt16(dataptr, v&0xffff);
			addr += 2;
			dataptr += 4;
		} else if ((end-addr) == 1) { // last byte
			u32 v = stub->cb->ReadMem(stub->ud, addr, 8);
			hexfmt8(dataptr, v&0xff);
			++addr;
			dataptr += 2;
		} else goto end;
	}

	// main loop: 4-byte chunks
	while (addr < end) {
		if (end - addr < 4) break; // post-align stuff

		u32 v = stub->cb->ReadMem(stub->ud, addr, 32);
		hexfmt32(dataptr, v);
		addr += 4;
		dataptr += 8;
	}

	// post-align: short
	if ((end-addr) & 2) {
		u32 v = stub->cb->ReadMem(stub->ud, addr, 16);
		hexfmt16(dataptr, v&0xffff);
		addr += 2;
		dataptr += 4;
	}

	// post-align: byte
	if ((end-addr) == 1) {
		u32 v = stub->cb->ReadMem(stub->ud, addr, 8);
		hexfmt8(dataptr, v&0xff);
		++addr;
		dataptr += 2;
	}

end:
	assert(addr == end);

	Proto::Resp(stub->connfd, datastr, llen*2);

	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_M(GdbStub* stub, const u8* cmd, ssize_t len) {
	u32 addr, llen, end;
	int inoff;

	if (sscanf((const char*)cmd, "%08X,%08X:%n", &addr, &llen, &inoff) != 2) {
		Proto::RespStr(stub->connfd, "E01");
		return ExecResult::Ok;
	} else if (llen > (GDBPROTO_BUFFER_CAPACITY/2)) {
		Proto::RespStr(stub->connfd, "E02");
		return ExecResult::Ok;
	}
	end = addr + llen;

	const u8* dataptr = cmd + inoff;

	// pre-align: byte
	if ((addr & 1)) {
		if ((end-addr) >= 1) {
			u8 v = unhex8(dataptr);
			stub->cb->WriteMem(stub->ud, addr, 8, v);
			++addr;
			dataptr += 2;
		} else goto end;
	}

	// pre-align: short
	if ((addr & 2)) {
		if ((end-addr) >= 2) {
			u16 v = unhex16(dataptr);
			stub->cb->WriteMem(stub->ud, addr, 16, v);
			addr += 2;
			dataptr += 4;
		} else if ((end-addr) == 1) { // last byte
			u8 v = unhex8(dataptr);
			stub->cb->WriteMem(stub->ud, addr, 8, v);
			++addr;
			dataptr += 2;
		} else goto end;
	}

	// main loop: 4-byte chunks
	while (addr < end) {
		if (end - addr < 4) break; // post-align stuff

		u32 v = unhex32(dataptr);
		stub->cb->WriteMem(stub->ud, addr, 32, v);
		addr += 4;
		dataptr += 8;
	}

	// post-align: short
	if ((end-addr) & 2) {
		u16 v = unhex16(dataptr);
		stub->cb->WriteMem(stub->ud, addr, 16, v);
		addr += 2;
		dataptr += 4;
	}

	// post-align: byte
	if ((end-addr) == 1) {
		u8 v = unhex8(dataptr);
		stub->cb->WriteMem(stub->ud, addr, 8, v);
		++addr;
		dataptr += 2;
	}

end:
	assert(addr == end);

	Proto::RespStr(stub->connfd, "OK");

	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_X(GdbStub* stub, const u8* cmd, ssize_t len) {
	u32 addr, llen, end;
	int inoff;

	if (sscanf((const char*)cmd, "%08X,%08X:%n", &addr, &llen, &inoff) != 2) {
		Proto::RespStr(stub->connfd, "E01");
		return ExecResult::Ok;
	} else if (llen > (GDBPROTO_BUFFER_CAPACITY/2)) {
		Proto::RespStr(stub->connfd, "E02");
		return ExecResult::Ok;
	}
	end = addr + llen;

	const u8* dataptr = cmd + inoff;

	// pre-align: byte
	if ((addr & 1)) {
		if ((end-addr) >= 1) {
			u8 v = *dataptr;
			stub->cb->WriteMem(stub->ud, addr, 8, v);
			++addr;
			dataptr += 1;
		} else goto end;
	}

	// pre-align: short
	if ((addr & 2)) {
		if ((end-addr) >= 2) {
			u16 v = dataptr[0] | ((u16)dataptr[1] << 8);
			stub->cb->WriteMem(stub->ud, addr, 16, v);
			addr += 2;
			dataptr += 2;
		} else if ((end-addr) == 1) { // last byte
			u8 v = *dataptr;
			stub->cb->WriteMem(stub->ud, addr, 8, v);
			++addr;
			dataptr += 1;
		} else goto end;
	}

	// main loop: 4-byte chunks
	while (addr < end) {
		if (end - addr < 4) break; // post-align stuff

		u32 v = dataptr[0] | ((u32)dataptr[1] << 8)
			| ((u32)dataptr[2] << 16) | ((u32)dataptr[3] << 24);
		stub->cb->WriteMem(stub->ud, addr, 32, v);
		addr += 4;
		dataptr += 4;
	}

	// post-align: short
	if ((end-addr) & 2) {
		u16 v = dataptr[0] | ((u16)dataptr[1] << 8);
		stub->cb->WriteMem(stub->ud, addr, 16, v);
		addr += 2;
		dataptr += 2;
	}

	// post-align: byte
	if ((end-addr) == 1) {
		u8 v = unhex8(dataptr);
		stub->cb->WriteMem(stub->ud, addr, 8, v);
		++addr;
		dataptr += 1;
	}

end:
	assert(addr == end);

	Proto::RespStr(stub->connfd, "OK");

	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_c(GdbStub* stub, const u8* cmd, ssize_t len) {
	u32 addr = ~(u32)0;

	if (len > 0) {
		if (len <= 8) {
			if (sscanf((const char*)cmd, "%08X", &addr) != 1) {
				Proto::RespStr(stub->connfd, "E01");
			} // else: ok
		} else {
			Proto::RespStr(stub->connfd, "E01");
		}
	} // else: continue at current

	if (~addr) {
		stub->cb->WriteReg(stub->ud, Register::pc, addr);
	}

	return ExecResult::Continue;
}

ExecResult GdbStub::Handle_s(GdbStub* stub, const u8* cmd, ssize_t len) {
	u32 addr = ~(u32)0;

	if (len > 0) {
		if (len <= 8) {
			if (sscanf((const char*)cmd, "%08X", &addr) != 1) {
				Proto::RespStr(stub->connfd, "E01");
				return ExecResult::Ok;
			} // else: ok
		} else {
			Proto::RespStr(stub->connfd, "E01");
			return ExecResult::Ok;
		}
	} // else: continue at current

	if (~addr != 0) {
		stub->cb->WriteReg(stub->ud, Register::pc, addr);
	}

	return ExecResult::Step;
}

ExecResult GdbStub::Handle_p(GdbStub* stub, const u8* cmd, ssize_t len) {
	int reg;
	if (sscanf((const char*)cmd, "%x", &reg) != 1 || reg < 0 || reg >= GDB_ARCH_N_REG) {
		Proto::RespStr(stub->connfd, "E01");
		return ExecResult::Ok;
	}

	u32 v = stub->cb->ReadReg(stub->ud, static_cast<Register>(reg));
	hexfmt32(tempdatabuf, v);
	Proto::Resp(stub->connfd, tempdatabuf, 4*2);

	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_P(GdbStub* stub, const u8* cmd, ssize_t len) {
	int reg, dataoff;

	if (sscanf((const char*)cmd, "%x=%n", &reg, &dataoff) != 1 || reg < 0
			|| reg >= GDB_ARCH_N_REG || dataoff + 4*2 > len) {
		Proto::RespStr(stub->connfd, "E01");
		return ExecResult::Ok;
	}

	u32 v = unhex32(&cmd[dataoff]);
	stub->cb->WriteReg(stub->ud, static_cast<Register>(reg), v);

	Proto::RespStr(stub->connfd, "OK");

	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_H(GdbStub* stub, const u8* cmd, ssize_t len) {
	u8 operation = cmd[0];
	u32 thread_id;
	sscanf((const char*)&cmd[1], "%u", &thread_id);

	(void)operation;
	if (thread_id <= 1) {
		Proto::RespStr(stub->connfd, "OK");
	} else {
		Proto::RespStr(stub->connfd, "E01");
	}

	return ExecResult::Ok;
}


ExecResult GdbStub::Handle_Question(GdbStub* stub, const u8* cmd, ssize_t len) {
	// "request reason for target halt" (which must also halt)

	TgtStatus st = stub->stat;
	u32 arg = ~(u32)0;
	int typ = 0;

	switch (st) {
	case TgtStatus::None: // no target!
		Proto::RespStr(stub->connfd, "W00");
		break;

	case TgtStatus::Running: // will break very soon due to retval
	case TgtStatus::BreakReq:
		Proto::RespFmt(stub->connfd, "S%02X", GdbSignal::INT);
		break;

	case TgtStatus::SingleStep:
		Proto::RespFmt(stub->connfd, "S%02X", GdbSignal::TRAP);
		break;

	case TgtStatus::Bkpt:
		arg = stub->cur_bkpt;
		typ = 1;
		goto bkpt_rest;
	case TgtStatus::Watchpt:
		arg = stub->cur_watchpt;
		typ = 2;
	bkpt_rest:
		if (!~arg) {
			Proto::RespFmt(stub->connfd, "S%02X", GdbSignal::TRAP);
		} else {
			switch (typ) {
			case 1:
				Proto::RespFmt(stub->connfd, "S%02X", GdbSignal::TRAP);
				//Proto::RespFmt(stub->connfd, "T%02Xhwbreak:"/*"%08X"*/";", GdbSignal::TRAP/*, arg*/);
				break;
			case 2:
				Proto::RespFmt(stub->connfd, "S%02X", GdbSignal::TRAP);
				//Proto::RespFmt(stub->connfd, "T%02Xwatch:"/*"%08X"*/";", GdbSignal::TRAP/*, arg*/);
				break;
			default:
				Proto::RespFmt(stub->connfd, "S%02X", GdbSignal::TRAP);
				break;
			}
		}
		break;
	case TgtStatus::BkptInsn:
		Proto::RespFmt(stub->connfd, "T%02Xswbreak:%08X;", GdbSignal::TRAP,
				stub->cb->ReadReg(stub->ud, Register::pc));
		break;

		// these three should technically be a SIGBUS but gdb etc don't really
		// like that (plus it sounds confusing)
	case TgtStatus::FaultData:
	case TgtStatus::FaultIAcc:
		Proto::RespFmt(stub->connfd, "S%02X", GdbSignal::SEGV);
		break;
	case TgtStatus::FaultInsn:
		Proto::RespFmt(stub->connfd, "S%02X", GdbSignal::ILL);
		break;
	}

	return ExecResult::InitialBreak;
}

ExecResult GdbStub::Handle_Exclamation(GdbStub* stub, const u8* cmd, ssize_t len) {
	Proto::RespStr(stub->connfd, "OK");
	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_D(GdbStub* stub, const u8* cmd, ssize_t len) {
	Proto::RespStr(stub->connfd, "OK");
	return ExecResult::Detached;
}

ExecResult GdbStub::Handle_r(GdbStub* stub, const u8* cmd, ssize_t len) {
	stub->cb->Reset(stub->ud);
	return ExecResult::Ok;
}
ExecResult GdbStub::Handle_R(GdbStub* stub, const u8* cmd, ssize_t len) {
	stub->cb->Reset(stub->ud);
	return ExecResult::Ok;
}


ExecResult GdbStub::Handle_z(GdbStub* stub, const u8* cmd, ssize_t len) {
	int typ;
	u32 addr, kind;

	if (sscanf((const char*)cmd, "%d,%x,%u", &typ, &addr, &kind) != 3) {
		Proto::RespStr(stub->connfd, "E01");
		return ExecResult::Ok;
	}

	switch (typ) {
	case 0: case 1: // remove breakpoint (we cheat & always insert a hardware breakpoint)
		stub->DelBkpt(addr, kind);
		break;
	case 2: case 3: case 4: // watchpoint. currently not distinguishing between reads & writes oops
		stub->DelWatchpt(addr, kind, typ);
		break;
	default:
		Proto::RespStr(stub->connfd, "E02");
		return ExecResult::Ok;
	}

	Proto::RespStr(stub->connfd, "OK");
	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_Z(GdbStub* stub, const u8* cmd, ssize_t len) {
	int typ;
	u32 addr, kind;

	if (sscanf((const char*)cmd, "%d,%x,%u", &typ, &addr, &kind) != 3) {
		Proto::RespStr(stub->connfd, "E01");
		return ExecResult::Ok;
	}

	switch (typ) {
	case 0: case 1: // insert breakpoint (we cheat & always insert a hardware breakpoint)
		stub->AddBkpt(addr, kind);
		break;
	case 2: case 3: case 4: // watchpoint. currently not distinguishing between reads & writes oops
		stub->AddWatchpt(addr, kind, typ);
		break;
	default:
		Proto::RespStr(stub->connfd, "E02");
		return ExecResult::Ok;
	}

	Proto::RespStr(stub->connfd, "OK");
	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_q_HostInfo(GdbStub* stub, const u8* cmd, ssize_t len) {
	const char* resp = "";

	switch (stub->cb->cpu) {
	case 7: resp = TARGET_INFO_ARM7; break;
	case 9: resp = TARGET_INFO_ARM9; break;
	default: break;
	}

	Proto::RespStr(stub->connfd, resp);
	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_q_Rcmd(GdbStub* stub, const u8* cmd, ssize_t len) {

	memset(tempdatabuf, 0, sizeof tempdatabuf);
	for (ssize_t i = 0; i < len/2; ++i) {
		tempdatabuf[i] = unhex8(&cmd[i*2]);
	}

	int r = stub->cb->RemoteCmd(stub->ud, tempdatabuf, len/2);

	if (r) {
		Proto::RespFmt(stub->connfd, "E%02X", r&0xff);
	} else {
		Proto::RespStr(stub->connfd, "OK");
	}

	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_q_Supported(GdbStub* stub,
		const u8* cmd, ssize_t len) {
	// TODO: support Xfer:memory-map:read::
	//       but NWRAM is super annoying with that
	Proto::RespFmt(stub->connfd, "PacketSize=%X;qXfer:features:read+;swbreak-;hwbreak+", GDBPROTO_BUFFER_CAPACITY);
	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_q_CRC(GdbStub* stub,
		const u8* cmd, ssize_t llen) {
	static u8 crcbuf[128];

	u32 addr, len;
	if (sscanf((const char*)cmd, "%x,%x", &addr, &len) != 2) {
		Proto::RespStr(stub->connfd, "E01");
		return ExecResult::Ok;
	}

	u32 val = 0; // start at 0
	u32 caddr = addr;
	u32 realend = addr + len;

	for (; caddr < addr + len; ) {
		// calc partial CRC in 128-byte chunks
		u32 end = caddr + sizeof(crcbuf)/sizeof(crcbuf[0]);
		if (end > realend) end = realend;
		u32 clen = end - caddr;

		for (size_t i = 0; caddr < end; ++caddr, ++i) {
			crcbuf[i] = stub->cb->ReadMem(stub->ud, caddr, 8);
		}

		val = CRC32(crcbuf, clen, val);
	}

	Proto::RespFmt(stub->connfd, "C%x", val);

	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_q_C(GdbStub* stub, const u8* cmd, ssize_t len) {
	Proto::RespStr(stub->connfd, "QC1"); // current thread ID is 1
	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_q_fThreadInfo(GdbStub* stub, const u8* cmd, ssize_t len) {
	Proto::RespStr(stub->connfd, "m1"); // one thread
	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_q_sThreadInfo(GdbStub* stub, const u8* cmd, ssize_t len) {
	Proto::RespStr(stub->connfd, "l"); // end of thread list
	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_q_features(GdbStub* stub, const u8* cmd, ssize_t len) {
	const char* resp;

	Log(LogLevel::Debug, "[GDB] CPU type = %d\n", stub->cb->cpu);
	switch (stub->cb->cpu) {
	case 7: resp = TARGET_XML_ARM7; break;
	case 9: resp = TARGET_XML_ARM9; break;
	default: resp = ""; break;
	}

	do_q_response(stub->connfd, cmd, resp, strlen(resp));
	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_q_Attached(GdbStub* stub, const u8* cmd, ssize_t len) {
	Proto::RespStr(stub->connfd, "1"); // always "attach to a process"
	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_v_Attach(GdbStub* stub, const u8* cmd, ssize_t len) {

	TgtStatus st = stub->stat;

	if (st == TgtStatus::None) {
		// no target
		Proto::RespStr(stub->connfd, "E01");
		return ExecResult::Ok;
	}

	Proto::RespStr(stub->connfd, "T05thread:1;");

	if (st == TgtStatus::Running) {
		return ExecResult::MustBreak;
	} else return ExecResult::Ok;
}

ExecResult GdbStub::Handle_v_Kill(GdbStub* stub, const u8* cmd, ssize_t len) {
	TgtStatus st = stub->stat;

	stub->cb->Reset(stub->ud);

	Proto::RespStr(stub->connfd, "OK");

	return (st != TgtStatus::Running && st != TgtStatus::None) ? ExecResult::Detached : ExecResult::Ok;
}

ExecResult GdbStub::Handle_v_Run(GdbStub* stub, const u8* cmd, ssize_t len) {
	TgtStatus st = stub->stat;

	stub->cb->Reset(stub->ud);

	// TODO: handle cmdline for homebrew?

	return (st != TgtStatus::Running && st != TgtStatus::None) ? ExecResult::Continue : ExecResult::Ok;
}

ExecResult GdbStub::Handle_v_Stopped(GdbStub* stub, const u8* cmd, ssize_t len) {
	TgtStatus st = stub->stat;

	static bool notified = true;

	// not sure if i understand this correctly
	if (st != TgtStatus::Running) {
		if (notified) {
			Proto::RespStr(stub->connfd, "OK");
		} else {
			Proto::RespStr(stub->connfd, "W00");
		}

		notified = !notified;
	} else {
		Proto::RespStr(stub->connfd, "OK");
	}

	return ExecResult::Ok;
}

ExecResult GdbStub::Handle_v_MustReplyEmpty(GdbStub* stub, const u8* cmd, ssize_t len) {
	Proto::Resp(stub->connfd, NULL, 0);
	return ExecResult::Ok;
}

}

