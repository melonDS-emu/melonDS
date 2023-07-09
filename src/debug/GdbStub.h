
#ifndef GDBSTUB_H_
#define GDBSTUB_H_

#include <stddef.h>
#include <map>
#include <vector>

#include "../types.h"

#include "GdbArch.h"

namespace Gdb {

enum class TgtStatus {
	NoEvent,

	None,
	Running,
	SingleStep,
	BreakReq, // "break" command from gdb client
	Bkpt,
	Watchpt,
	BkptInsn, // "bkpt" instruction
	FaultData, // data abort
	FaultIAcc, // instruction fetch abort
	FaultInsn, // illegal instruction
};

struct StubCallbacks {
	int cpu; // 7 or 9 (currently, maybe also xtensa in the future?)

	// 0..14: as usual
	// 15: pc *pipeline-corrected*
	// 16: cpsr
	u32  (* ReadReg)(void* ud, Register reg);
	void (*WriteReg)(void* ud, Register reg, u32 value);

	u32  (* ReadMem)(void* ud, u32 addr, int len);
	void (*WriteMem)(void* ud, u32 addr, int len, u32 value);

	void (*Reset)(void* ud);
	int (*RemoteCmd)(void* ud, const u8* cmd, size_t len);
};

enum class StubState {
	NoConn,
	None,
	Break,
	Continue,
	Step,
	Disconnect,
	Attach,
	CheckNoHit
};

enum class ReadResult {
	NoPacket,
	Eof,
	CksumErr,
	CmdRecvd,
	Wut,
	Break
};

enum class ExecResult {
	Ok,
	UnkCmd,
	NetErr,
	InitialBreak,
	MustBreak,
	Detached,
	Step,
	Continue
};

class GdbStub;

typedef ExecResult (*GdbProtoCmd)(GdbStub* stub, const u8* cmd, ssize_t len);

struct SubcmdHandler {
	char maincmd;
	const char* substr;
	GdbProtoCmd handler;
};

struct CmdHandler {
	char cmd;
	GdbProtoCmd handler;
};

class GdbStub {
public:
	struct BpWp {
	public:
		u32 addr, len;
		int kind;
	};

	GdbStub(const StubCallbacks* cb, int port, void* ud);
	~GdbStub();

	bool Init();
	void Close();

	StubState Poll(bool wait = false);
	void SignalStatus(TgtStatus stat, u32 arg);
	StubState Enter(bool stay, TgtStatus stat=TgtStatus::NoEvent, u32 arg=~(u32)0u, bool wait_for_conn=false);

	// kind: 2=thumb, 3=thumb2 (not relevant), 4=arm
	void AddBkpt(u32 addr, int kind);
	void DelBkpt(u32 addr, int kind);
	// kind: 2=read, 3=write, 4=rdwr
	void AddWatchpt(u32 addr, u32 len, int kind);
	void DelWatchpt(u32 addr, u32 len, int kind);

	void DelAllBpWp();

	StubState CheckBkpt(u32 addr, bool enter, bool stay);
	StubState CheckWatchpt(u32 addr, int kind, bool enter, bool stay);

#include "GdbCmds.h"

	Gdb::ExecResult SubcmdExec(const u8* cmd, ssize_t len, const SubcmdHandler* handlers);
	Gdb::ExecResult CmdExec(const CmdHandler* handlers);

private:
	void Disconnect();
	StubState HandlePacket();

private:
	const StubCallbacks* cb;
    void* ud;

	//struct sockaddr_in server, client;
	void *serversa, *clientsa;
	int port;
	int sockfd;
	int connfd;

	TgtStatus stat;
	u32 cur_bkpt, cur_watchpt;
	bool stat_flag;

	std::map<u32, BpWp> bp_list;
	std::vector<BpWp> wp_list;

	static SubcmdHandler handlers_v[];
	static SubcmdHandler handlers_q[];
	static CmdHandler handlers_top[];
};

}

#endif

