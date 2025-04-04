
#ifndef GDBSTUB_H_
#define GDBSTUB_H_

#include <stddef.h>
#include <sys/types.h>
#include <map>
#include <vector>

#include "../types.h"

#include "GdbArch.h"

namespace Gdb
{

using namespace melonDS;
enum class TgtStatus
{
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

class StubCallbacks
{
public:
	StubCallbacks(){}
	virtual ~StubCallbacks(){};

	virtual int GetCPU() const = 0; // 7 or 9 (currently, maybe also xtensa in the future?)

	// 0..14: as usual
	// 15: pc *pipeline-corrected*
	// 16: cpsr
	virtual u32  ReadReg (Register reg) = 0;
	virtual void WriteReg(Register reg, u32 value) = 0;

	virtual u32  ReadMem (u32 addr, int len) = 0;
	virtual void WriteMem(u32 addr, int len, u32 value) = 0;

	virtual void ResetGdb() = 0;
	virtual int RemoteCmd(const u8* cmd, size_t len) = 0;
};

enum class StubState
{
	NoConn,
	None,
	Break,
	Continue,
	Step,
	Disconnect,
	Attach,
	CheckNoHit
};

enum class ReadResult
{
	NoPacket,
	Eof,
	CksumErr,
	CmdRecvd,
	Wut,
	Break
};

enum class ExecResult
{
	Ok,
	UnkCmd,
	NetErr,
	InitialBreak,
	MustBreak,
	Detached,
	Step,
	Continue
};

constexpr int GDBPROTO_BUFFER_CAPACITY = 1024+128;

class GdbStub;

typedef ExecResult (*GdbProtoCmd)(GdbStub* stub, const u8* cmd, ssize_t len);

struct SubcmdHandler
{
	char MainCmd;
	const char* SubStr;
	GdbProtoCmd Handler;
};

struct CmdHandler
{
	char Cmd;
	GdbProtoCmd Handler;
};

class GdbStub
{
public:
	struct BpWp
	{
	public:
		u32 addr, len;
		int kind;
	};

	GdbStub(StubCallbacks* cb);
	~GdbStub();

	bool Init(int port);
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

public:
	int Resp(const u8* data1, size_t len1, const u8* data2 = NULL, size_t len2 = 0);
	int RespC(const char* data1, size_t len1, const u8* data2 = NULL, size_t len2 = 0);
#if defined(__GCC__) || defined(__clang__)
	__attribute__((__format__(printf, 2, 3)))
#endif
	int RespFmt(const char* fmt, ...);

	int RespStr(const char* str);
	inline bool IsConnected() { return ConnFd > 0; }

private:
	void Disconnect();
	StubState HandlePacket();

	Gdb::ReadResult MsgRecv();

	Gdb::ReadResult TryParsePacket(size_t start, size_t& packetStart, size_t& packetSize, size_t& packetContentSize);
	Gdb::ReadResult ParseAndSetupPacket();	

	void SetupCommand(size_t packetStart, size_t packetSize);

	int SendAck();
	int SendNak();

	int Resp(const u8* data1, size_t len1, const u8* data2, size_t len2, bool noack);

	int WaitAckBlocking(u8* ackp, int to_ms);

	StubCallbacks* Cb;

	//struct sockaddr_in server, client;
	void *ServerSA, *ClientSA;
	int Port;
	int SockFd;
	int ConnFd;

	TgtStatus Stat;
	u32 CurBkpt, CurWatchpt;
	bool StatFlag;
	bool NoAck;

	std::array<u8, GDBPROTO_BUFFER_CAPACITY> RecvBuffer;
	u32 RecvBufferFilled = 0;
	std::array<u8, GDBPROTO_BUFFER_CAPACITY> RespBuf;

	std::array<u8, GDBPROTO_BUFFER_CAPACITY> Cmdbuf;
	ssize_t Cmdlen;

	std::map<u32, BpWp> BpList;
	std::vector<BpWp> WpList;

	static SubcmdHandler Handlers_v[];
	static SubcmdHandler Handlers_q[];
	static SubcmdHandler Handlers_Q[];
	static CmdHandler Handlers_top[];
};

}

#endif

