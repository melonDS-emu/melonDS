
#ifdef _WIN32
#include <WS2tcpip.h>
#include <winsock.h>
#include <winsock2.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#endif


#include "../Platform.h"
#include "GdbProto.h"

using namespace melonDS;
using Platform::Log;
using Platform::LogLevel;

static int SocketSetBlocking(int fd, bool block)
{
#if MOCKTEST
	return 0;
#endif

	if (fd < 0) return -1;

#ifdef _WIN32
	unsigned long mode = block ? 0 : 1;
	return ioctlsocket(fd, FIONBIO, &mode);
#else
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) return -1;
	flags = block ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
	return fcntl(fd, F_SETFL, flags);
#endif
}

namespace Gdb
{

GdbStub::GdbStub(StubCallbacks* cb, int port)
	: Cb(cb), Port(port)
	, SockFd(0), ConnFd(0)
	, Stat(TgtStatus::None), CurBkpt(0), CurWatchpt(0), StatFlag(false), NoAck(false)
	, ServerSA((void*)new struct sockaddr_in())
	, ClientSA((void*)new struct sockaddr_in())
{ }

bool GdbStub::Init()
{
	Log(LogLevel::Info, "[GDB] initializing GDB stub for core %d on port %d\n",
		Cb->GetCPU(), Port);

#if MOCKTEST
	SockFd = 0;
	return true;
#endif

#ifndef _WIN32
	/*void* fn = SIG_IGN;
	struct sigaction act = { 0 };
	act.sa_flags = SA_SIGINFO;
	act.sa_sigaction = (sighandler_t)fn;
	if (sigaction(SIGPIPE, &act, NULL) == -1) {
		Log(LogLevel::Warn, "[GDB] couldn't ignore SIGPIPE, stuff may fail on GDB disconnect.\n");
	}*/
	signal(SIGPIPE, SIG_IGN);
#else
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		Log(LogLevel::Error, "[GDB] winsock could not be initialized (%d).\n", WSAGetLastError());
		return false;
	}
#endif

	int r;
	struct sockaddr_in* server = (struct sockaddr_in*)ServerSA;
	struct sockaddr_in* client = (struct sockaddr_in*)ClientSA;

	int typ = SOCK_STREAM;
#ifdef __linux__
	typ |= SOCK_NONBLOCK;
#endif
	SockFd = socket(AF_INET, SOCK_STREAM, 0);
	if (SockFd < 0)
	{
		Log(LogLevel::Error, "[GDB] err: can't create a socket fd\n");
		goto err;
	}
#ifndef __linux__
	SocketSetBlocking(SockFd, false);
#endif

	server->sin_family = AF_INET;
	server->sin_addr.s_addr = htonl(INADDR_ANY);
	server->sin_port = htons(Port);

	r = bind(SockFd, (const sockaddr*)server, sizeof(*server));
	if (r < 0)
	{
		Log(LogLevel::Error, "[GDB] err: can't bind to address <any> and port %d\n", Port);
		goto err;
	}

	r = listen(SockFd, 5);
	if (r < 0)
	{
		Log(LogLevel::Error, "[GDB] err: can't listen to SockFd\n");
		goto err;
	}

	return true;

err:
	if (SockFd != 0)
	{
#ifdef _WIN32
		closesocket(SockFd);
#else
		close(SockFd);
#endif
		SockFd = 0;
	}

	return false;
}

void GdbStub::Close()
{
	Disconnect();
	if (SockFd > 0) close(SockFd);
	SockFd = 0;
}

void GdbStub::Disconnect()
{
	if (ConnFd > 0) close(ConnFd);
	ConnFd = 0;
}

GdbStub::~GdbStub()
{
	Close();
	delete (struct sockaddr_in*)ServerSA;
	delete (struct sockaddr_in*)ClientSA;
}

SubcmdHandler GdbStub::Handlers_v[] = {
	{ .MainCmd = 'v', .SubStr = "Attach;"       , .Handler = GdbStub::Handle_v_Attach },
	{ .MainCmd = 'v', .SubStr = "Kill;"         , .Handler = GdbStub::Handle_v_Kill },
	{ .MainCmd = 'v', .SubStr = "Run"           , .Handler = GdbStub::Handle_v_Run },
	{ .MainCmd = 'v', .SubStr = "Stopped"       , .Handler = GdbStub::Handle_v_Stopped },
	{ .MainCmd = 'v', .SubStr = "MustReplyEmpty", .Handler = GdbStub::Handle_v_MustReplyEmpty },
	{ .MainCmd = 'v', .SubStr = "Cont?"         , .Handler = GdbStub::Handle_v_ContQuery },
	{ .MainCmd = 'v', .SubStr = "Cont"          , .Handler = GdbStub::Handle_v_Cont },

	{ .MainCmd = 'v', .SubStr = NULL, .Handler = NULL }
};

SubcmdHandler GdbStub::Handlers_q[] = {
	{ .MainCmd = 'q', .SubStr = "HostInfo"   , .Handler = GdbStub::Handle_q_HostInfo },
	{ .MainCmd = 'q', .SubStr = "ProcessInfo", .Handler = GdbStub::Handle_q_HostInfo },
	{ .MainCmd = 'q', .SubStr = "Rcmd,"      , .Handler = GdbStub::Handle_q_Rcmd },
	{ .MainCmd = 'q', .SubStr = "Supported:" , .Handler = GdbStub::Handle_q_Supported },
	{ .MainCmd = 'q', .SubStr = "CRC:"       , .Handler = GdbStub::Handle_q_CRC },
	{ .MainCmd = 'q', .SubStr = "C"          , .Handler = GdbStub::Handle_q_C },
	{ .MainCmd = 'q', .SubStr = "fThreadInfo", .Handler = GdbStub::Handle_q_fThreadInfo },
	{ .MainCmd = 'q', .SubStr = "sThreadInfo", .Handler = GdbStub::Handle_q_sThreadInfo },
	{ .MainCmd = 'q', .SubStr = "Attached"   , .Handler = GdbStub::Handle_q_Attached },
	{ .MainCmd = 'q', .SubStr = "Xfer:features:read:target.xml:", .Handler = GdbStub::Handle_q_features },

	{ .MainCmd = 'q', .SubStr = NULL, .Handler = NULL },
};

SubcmdHandler GdbStub::Handlers_Q[] = {
	{ .MainCmd = 'Q', .SubStr = "StartNoAckMode", .Handler = GdbStub::Handle_Q_StartNoAckMode },

	{ .MainCmd = 'Q', .SubStr = NULL, .Handler = NULL },
};

ExecResult GdbStub::Handle_q(GdbStub* stub, const u8* cmd, ssize_t len)
{
	return stub->SubcmdExec(cmd, len, Handlers_q);
}

ExecResult GdbStub::Handle_v(GdbStub* stub, const u8* cmd, ssize_t len)
{
	return stub->SubcmdExec(cmd, len, Handlers_v);
}

ExecResult GdbStub::Handle_Q(GdbStub* stub, const u8* cmd, ssize_t len)
{
	return stub->SubcmdExec(cmd, len, Handlers_Q);
}

CmdHandler GdbStub::Handlers_top[] = {
	{ .Cmd = 'g', .Handler = GdbStub::Handle_g },
	{ .Cmd = 'G', .Handler = GdbStub::Handle_G },
	{ .Cmd = 'm', .Handler = GdbStub::Handle_m },
	{ .Cmd = 'M', .Handler = GdbStub::Handle_M },
	{ .Cmd = 'X', .Handler = GdbStub::Handle_X },
	{ .Cmd = 'c', .Handler = GdbStub::Handle_c },
	{ .Cmd = 's', .Handler = GdbStub::Handle_s },
	{ .Cmd = 'p', .Handler = GdbStub::Handle_p },
	{ .Cmd = 'P', .Handler = GdbStub::Handle_P },
	{ .Cmd = 'H', .Handler = GdbStub::Handle_H },
	{ .Cmd = 'T', .Handler = GdbStub::Handle_H },

	{ .Cmd = '?', .Handler = GdbStub::Handle_Question },
	{ .Cmd = '!', .Handler = GdbStub::Handle_Exclamation },
	{ .Cmd = 'D', .Handler = GdbStub::Handle_D },
	{ .Cmd = 'r', .Handler = GdbStub::Handle_r },
	{ .Cmd = 'R', .Handler = GdbStub::Handle_R },
	{ .Cmd = 'k', .Handler = GdbStub::Handle_k },

	{ .Cmd = 'z', .Handler = GdbStub::Handle_z },
	{ .Cmd = 'Z', .Handler = GdbStub::Handle_Z },

	{ .Cmd = 'q', .Handler = GdbStub::Handle_q },
	{ .Cmd = 'v', .Handler = GdbStub::Handle_v },
	{ .Cmd = 'Q', .Handler = GdbStub::Handle_Q },

	{ .Cmd = 0, .Handler = NULL }
};


StubState GdbStub::HandlePacket()
{
	ExecResult r = CmdExec(Handlers_top);

	if (r == ExecResult::MustBreak)
	{
		if (Stat == TgtStatus::None || Stat == TgtStatus::Running)
			Stat = TgtStatus::BreakReq;
		return StubState::Break;
	}
	else if (r == ExecResult::InitialBreak)
	{
		Stat = TgtStatus::BreakReq;
		return StubState::Attach;
	/*}
	else if (r == ExecResult::Detached)
	{
		Stat = TgtStatus::None;
		return StubState::Disconnect;*/
	}
	else if (r == ExecResult::Continue)
	{
		Stat = TgtStatus::Running;
		return StubState::Continue;
	}
	else if (r == ExecResult::Step)
	{
		return StubState::Step;
	}
	else if (r == ExecResult::Ok || r == ExecResult::UnkCmd)
	{
		return StubState::None;
	}
	else
	{
		Stat = TgtStatus::None;
		return StubState::Disconnect;
	}
}

StubState GdbStub::Poll(bool wait)
{
	int r;

	if (ConnFd <= 0)
	{
		SocketSetBlocking(SockFd, wait);

		// not yet connected, so let's wait for one
		// nonblocking only done in part of read_packet(), so that it can still
		// quickly handle partly-received packets
		struct sockaddr_in* client = (struct sockaddr_in*)ClientSA;
		socklen_t len = sizeof(*client);
#if MOCKTEST
		ConnFd = 0;
#else
#ifdef __linux__
		ConnFd = accept4(SockFd, (struct sockaddr*)client, &len, /*SOCK_NONBLOCK|*/SOCK_CLOEXEC);
#else
		ConnFd = accept(SockFd, (struct sockaddr*)client, &len);
#endif
#endif

		if (ConnFd < 0) return StubState::NoConn;

		u8 a;
		if (Proto::WaitAckBlocking(ConnFd, &a, 1000) < 0)
		{
			Log(LogLevel::Error, "[GDB] inital handshake: didn't receive inital ack!\n");
			close(ConnFd);
			ConnFd = 0;
			return StubState::Disconnect;
		}

		if (a != '+')
		{
			Log(LogLevel::Error, "[GDB] inital handshake: unexpected character '%c'!\n", a);
		}
		SendAck();

		Stat = TgtStatus::Running; // on connected
		StatFlag = false;
	}

	if (StatFlag)
	{
		StatFlag = false;
		//Log(LogLevel::Debug, "[GDB] STAT FLAG WAS TRUE\n");

		Handle_Question(this, NULL, 0); // ugly hack but it should work
	}

#if MOCKTEST
	// nothing...
#else
#ifndef _WIN32
	struct pollfd pfd;
	pfd.fd = ConnFd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	r = poll(&pfd, 1, wait ? -1 : 0);

	if (r == 0) return StubState::None; // nothing is happening

	if (pfd.revents & (POLLHUP|POLLERR|POLLNVAL))
	{
		// oopsie, something happened
		Disconnect();
		return StubState::Disconnect;
	}
#else
	fd_set infd, outfd, errfd;
	FD_ZERO(&infd); FD_ZERO(&outfd); FD_ZERO(&errfd);
	FD_SET(ConnFd, &infd);

	struct timeval to;
	if (wait)
	{
		to.tv_sec = ~(time_t)0;
		to.tv_usec = ~(long)0;
	}
	else
	{
		to.tv_sec = 0;
		to.tv_usec = 0;
	}

	r = select(ConnFd+1, &infd, &outfd, &errfd, &to);

	if (FD_ISSET(ConnFd, &errfd))
	{
		Disconnect();
		return StubState::Disconnect;
	}
	else if (!FD_ISSET(ConnFd, &infd))
	{
		return StubState::None;
	}
#endif
#endif

	ReadResult res = Proto::MsgRecv(ConnFd, Cmdbuf);

	switch (res)
	{
	case ReadResult::NoPacket:
		return StubState::None;
	case ReadResult::Break:
		return StubState::Break;
	case ReadResult::Wut:
		Log(LogLevel::Info, "[GDB] WUT\n");
	case_gdbp_eof:
	case ReadResult::Eof:
		Log(LogLevel::Info, "[GDB] EOF!\n");
		close(ConnFd);
		ConnFd = 0;
		return StubState::Disconnect;
	case ReadResult::CksumErr:
		Log(LogLevel::Info, "[GDB] checksum err!\n");
		if (SendNak() < 0) {
			Log(LogLevel::Error, "[GDB] send nak after cksum fail errored!\n");
			goto case_gdbp_eof;
		}
		return StubState::None;
	case ReadResult::CmdRecvd:
		/*if (SendAck() < 0) {
			Log(LogLevel::Error, "[GDB] send packet ack failed!\n");
			goto case_gdbp_eof;
		}*/
		break;
	}

	return HandlePacket();
}

ExecResult GdbStub::SubcmdExec(const u8* cmd, ssize_t len, const SubcmdHandler* handlers)
{
	//Log(LogLevel::Debug, "[GDB] subcommand in: '%s'\n", cmd);

	for (size_t i = 0; handlers[i].Handler != NULL; ++i) {
		// check if prefix matches
		if (!strncmp((const char*)cmd, handlers[i].SubStr, strlen(handlers[i].SubStr)))
		{
			if (SendAck() < 0)
			{
				Log(LogLevel::Error, "[GDB] send packet ack failed!\n");
				return ExecResult::NetErr;
			}
			return handlers[i].Handler(this, &cmd[strlen(handlers[i].SubStr)], len-strlen(handlers[i].SubStr));
		}
	}

	Log(LogLevel::Info, "[GDB] unknown subcommand '%s'!\n", cmd);
	/*if (SendNak() < 0)
	{
		Log(LogLevel::Error, "[GDB] send nak after cksum fail errored!\n");
		return ExecResult::NetErr;
	}*/
	//Resp("E99");
	Resp(NULL, 0);
	return ExecResult::UnkCmd;
}

ExecResult GdbStub::CmdExec(const CmdHandler* handlers)
{
	Log(LogLevel::Debug, "[GDB] command in: '%s'\n", Cmdbuf);

	for (size_t i = 0; handlers[i].Handler != NULL; ++i)
	{
		if (handlers[i].Cmd == Cmdbuf[0])
		{
			if (SendAck() < 0)
			{
				Log(LogLevel::Error, "[GDB] send packet ack failed!\n");
				return ExecResult::NetErr;
			}
			return handlers[i].Handler(this, &Cmdbuf[1], Cmdlen-1);
		}
	}

	Log(LogLevel::Info, "[GDB] unknown command '%c'!\n", Cmdbuf[0]);
	/*if (SendNak() < 0)
	{
		Log(LogLevel::Error, "[GDB] send nak after cksum fail errored!\n");
		return ExecResult::NetErr;
	}*/
	//RespStr("E99");
	Resp(NULL, 0);
	return ExecResult::UnkCmd;
}


void GdbStub::SignalStatus(TgtStatus stat, u32 arg)
{
	//Log(LogLevel::Debug, "[GDB] SIGNAL STATUS %d!\n", stat);

	this->Stat = stat;
	StatFlag = true;

	if (stat == TgtStatus::Bkpt) CurBkpt = arg;
	else if (stat == TgtStatus::Watchpt) CurWatchpt = arg;
}


StubState GdbStub::Enter(bool stay, TgtStatus stat, u32 arg, bool wait_for_conn)
{
	if (stat != TgtStatus::NoEvent) SignalStatus(stat, arg);

	StubState st;
	bool do_next = true;
	do
	{
		bool was_conn = ConnFd > 0;
		st = Poll(wait_for_conn);
		bool has_conn = ConnFd > 0;

		if (has_conn && !was_conn) stay = true;

		switch (st)
		{
		case StubState::Break:
			Log(LogLevel::Info, "[GDB] break execution\n");
			SignalStatus(TgtStatus::BreakReq, ~(u32)0);
			break;
		case StubState::Continue:
			Log(LogLevel::Info, "[GDB] continue execution\n");
			do_next = false;
			break;
		case StubState::Step:
			Log(LogLevel::Info, "[GDB] single-step\n");
			do_next = false;
			break;
		case StubState::Disconnect:
			Log(LogLevel::Info, "[GDB] disconnect\n");
			SignalStatus(TgtStatus::None, ~(u32)0);
			do_next = false;
			break;
		default: break;
		}
	}
	while (do_next && stay);

	if (st != StubState::None && st != StubState::NoConn)
	{
		Log(LogLevel::Debug, "[GDB] enter exit: %d\n", st);
	}
	return st;
}

void GdbStub::AddBkpt(u32 addr, int kind)
{
	BpWp np;
	np.addr = addr ^ (addr & 1); // clear lowest bit to not break on thumb mode weirdnesses
	np.len = 0;
	np.kind = kind;

	{
		// already in the map
		auto search = BpList.find(np.addr);
		if (search != BpList.end()) return;
	}

	BpList.insert({np.addr, np});

	Log(LogLevel::Debug, "[GDB] added bkpt:\n");
	size_t i = 0;
	for (auto search = BpList.begin(); search != BpList.end(); ++search, ++i)
	{
		Log(LogLevel::Debug, "\t[%zu]: addr=%08x, kind=%d\n", i, search->first, search->second.kind);
	}
}
void GdbStub::AddWatchpt(u32 addr, u32 len, int kind)
{
	BpWp np;
	np.addr = addr;
	np.len = len;
	np.kind = kind;

	for (auto search = WpList.begin(); search != WpList.end(); ++search)
	{
		if (search->addr > addr)
		{
			WpList.insert(search, np);
			return;
		}
		else if (search->addr == addr && search->kind == kind)
		{
			if (search->len < len) search->len = len;
			return;
		}
	}

	WpList.push_back(np);
}

void GdbStub::DelBkpt(u32 addr, int kind)
{
	addr = addr ^ (addr & 1);

	auto search = BpList.find(addr);
	if (search != BpList.end())
	{
		BpList.erase(search);
	}
}
void GdbStub::DelWatchpt(u32 addr, u32 len, int kind)
{
	(void)len; (void)kind;

	for (auto search = WpList.begin(); search != WpList.end(); ++search)
	{
		if (search->addr == addr && search->kind == kind)
		{
			WpList.erase(search);
			return;
		}
		else if (search->addr > addr) return;
	}
}

void GdbStub::DelAllBpWp()
{
	BpList.erase(BpList.begin(), BpList.end());
	WpList.erase(WpList.begin(), WpList.end());
}

StubState GdbStub::CheckBkpt(u32 addr, bool enter, bool stay)
{
	addr ^= (addr & 1); // clear lowest bit to not break on thumb mode weirdnesses

	auto search = BpList.find(addr);
	if (search == BpList.end()) return StubState::CheckNoHit;

	if (enter)
	{
		StubState r = Enter(stay, TgtStatus::Bkpt, addr);
		Log(LogLevel::Debug, "[GDB] ENTER st=%d\n", r);
		return r;
	}
	else
	{
		SignalStatus(TgtStatus::Bkpt, addr);
		return StubState::None;
	}
}
StubState GdbStub::CheckWatchpt(u32 addr, int kind, bool enter, bool stay)
{
	for (auto search = WpList.begin(); search != WpList.end(); ++search)
	{
		if (search->addr > addr) break;

		if (addr >= search->addr && addr < search->addr + search->len && search->kind == kind)
		{
			if (enter) return Enter(stay, TgtStatus::Watchpt, addr);
			else
			{
				SignalStatus(TgtStatus::Watchpt, addr);
				return StubState::None;
			}
		}
	}

	return StubState::CheckNoHit;
}

int GdbStub::SendAck()
{
	if (NoAck) return 1;
	return Proto::SendAck(ConnFd);
}
int GdbStub::SendNak()
{
	if (NoAck) return 1;
	return Proto::SendNak(ConnFd);
}

int GdbStub::Resp(const u8* data1, size_t len1, const u8* data2, size_t len2)
{
	return Proto::Resp(ConnFd, data1, len1, data2, len2, NoAck);
}
int GdbStub::RespC(const char* data1, size_t len1, const u8* data2, size_t len2)
{
	return Proto::Resp(ConnFd, (const u8*)data1, len1, data2, len2, NoAck);
}
#if defined(__GCC__) || defined(__clang__)
__attribute__((__format__(printf, 2/*includes implicit this*/, 3)))
#endif
int GdbStub::RespFmt(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int r = vsnprintf((char*)&Proto::RespBuf[1], sizeof(Proto::RespBuf)-5, fmt, args);
	va_end(args);

	if (r < 0) return r;

	if ((size_t)r >= sizeof(Proto::RespBuf)-5)
	{
		Log(LogLevel::Error, "[GDB] truncated response in send_fmt()! (lost %zd bytes)\n",
				(ssize_t)r - (ssize_t)(sizeof(Proto::RespBuf)-5));
		r = sizeof(Proto::RespBuf)-5;
	}

	return Resp(&Proto::RespBuf[1], r);
}

int GdbStub::RespStr(const char* str)
{
	return Resp((const u8*)str, strlen(str));
}

}

