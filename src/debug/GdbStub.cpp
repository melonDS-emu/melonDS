
#ifdef _WIN32
#include <WS2tcpip.h>
#include <winsock.h>
#include <winsock2.h>
#endif

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

using Platform::Log;
using Platform::LogLevel;

static int sock_set_block(int fd, bool block) {
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

namespace Gdb {

GdbStub::GdbStub(const StubCallbacks* cb, int port, void* ud)
	: cb(cb), ud(ud), port(port)
	, sockfd(0), connfd(0)
	, stat(TgtStatus::None), cur_bkpt(0), cur_watchpt(0), stat_flag(false)
	, serversa((void*)new struct sockaddr_in())
	, clientsa((void*)new struct sockaddr_in())
{ }

bool GdbStub::Init() {
	Log(LogLevel::Info, "[GDB] initializing GDB stub for core %d on port %d\n",
		cb->cpu, port);

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
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		Log(LogLevel::Error, "[GDB] winsock could not be initialized (%d).\n", WSAGetLastError());
		return false;
	}
#endif

	int r;
	struct sockaddr_in* server = (struct sockaddr_in*)serversa;
	struct sockaddr_in* client = (struct sockaddr_in*)clientsa;

	int typ = SOCK_STREAM;
#ifdef __linux__
	typ |= SOCK_NONBLOCK;
#endif
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		Log(LogLevel::Error, "[GDB] err: can't create a socket fd\n");
		goto err;
	}
#ifndef __linux__
	sock_set_block(sockfd, false);
#endif

	server->sin_family = AF_INET;
	server->sin_addr.s_addr = htonl(INADDR_ANY);
	server->sin_port = htons(port);

	r = bind(sockfd, (const sockaddr*)server, sizeof(*server));
	if (r < 0) {
		Log(LogLevel::Error, "[GDB] err: can't bind to address <any> and port %d\n", port);
		goto err;
	}

	r = listen(sockfd, 5);
	if (r < 0) {
		Log(LogLevel::Error, "[GDB] err: can't listen to sockfd\n");
		goto err;
	}

	return true;

err:
	if (sockfd != 0) {
#ifdef _WIN32
		closesocket(sockfd);
#else
		close(sockfd);
#endif
		sockfd = 0;
	}

	return false;
}

void GdbStub::Close() {
	Disconnect();
	if (sockfd > 0) close(sockfd);
	sockfd = 0;
}

void GdbStub::Disconnect() {
	if (connfd > 0) close(connfd);
	connfd = 0;
}

GdbStub::~GdbStub() {
	Close();
	delete (struct sockaddr_in*)serversa;
	delete (struct sockaddr_in*)clientsa;
}

SubcmdHandler GdbStub::handlers_v[] = {
	{ .maincmd = 'v', .substr = "Attach;"       , .handler = GdbStub::Handle_v_Attach },
	{ .maincmd = 'v', .substr = "Kill;"         , .handler = GdbStub::Handle_v_Kill },
	{ .maincmd = 'v', .substr = "Run"           , .handler = GdbStub::Handle_v_Run },
	{ .maincmd = 'v', .substr = "Stopped"       , .handler = GdbStub::Handle_v_Stopped },
	{ .maincmd = 'v', .substr = "MustReplyEmpty", .handler = GdbStub::Handle_v_MustReplyEmpty },

	{ .maincmd = 'v', .substr = NULL, .handler = NULL }
};

SubcmdHandler GdbStub::handlers_q[] = {
	{ .maincmd = 'q', .substr = "HostInfo"   , .handler = GdbStub::Handle_q_HostInfo },
	{ .maincmd = 'q', .substr = "Rcmd,"      , .handler = GdbStub::Handle_q_Rcmd },
	{ .maincmd = 'q', .substr = "Supported:" , .handler = GdbStub::Handle_q_Supported },
	{ .maincmd = 'q', .substr = "CRC:"       , .handler = GdbStub::Handle_q_CRC },
	{ .maincmd = 'q', .substr = "C"          , .handler = GdbStub::Handle_q_C },
	{ .maincmd = 'q', .substr = "fThreadInfo", .handler = GdbStub::Handle_q_fThreadInfo },
	{ .maincmd = 'q', .substr = "sThreadInfo", .handler = GdbStub::Handle_q_sThreadInfo },
	{ .maincmd = 'q', .substr = "Attached"   , .handler = GdbStub::Handle_q_Attached },
	{ .maincmd = 'q', .substr = "Xfer:features:read:target.xml:", .handler = GdbStub::Handle_q_features },

	{ .maincmd = 'q', .substr = NULL, .handler = NULL },
};

ExecResult GdbStub::Handle_q(GdbStub* stub, const u8* cmd, ssize_t len) {
	return stub->SubcmdExec(cmd, len, handlers_q);
}

ExecResult GdbStub::Handle_v(GdbStub* stub, const u8* cmd, ssize_t len) {
	return stub->SubcmdExec(cmd, len, handlers_v);
}

CmdHandler GdbStub::handlers_top[] = {
	{ .cmd = 'g', .handler = GdbStub::Handle_g },
	{ .cmd = 'G', .handler = GdbStub::Handle_G },
	{ .cmd = 'm', .handler = GdbStub::Handle_m },
	{ .cmd = 'M', .handler = GdbStub::Handle_M },
	{ .cmd = 'X', .handler = GdbStub::Handle_X },
	{ .cmd = 'c', .handler = GdbStub::Handle_c },
	{ .cmd = 's', .handler = GdbStub::Handle_s },
	{ .cmd = 'p', .handler = GdbStub::Handle_p },
	{ .cmd = 'P', .handler = GdbStub::Handle_P },
	{ .cmd = 'H', .handler = GdbStub::Handle_H },
	{ .cmd = 'T', .handler = GdbStub::Handle_H },

	{ .cmd = '?', .handler = GdbStub::Handle_Question },
	{ .cmd = '!', .handler = GdbStub::Handle_Exclamation },
	{ .cmd = 'D', .handler = GdbStub::Handle_D },
	{ .cmd = 'r', .handler = GdbStub::Handle_r },
	{ .cmd = 'R', .handler = GdbStub::Handle_R },

	{ .cmd = 'z', .handler = GdbStub::Handle_z },
	{ .cmd = 'Z', .handler = GdbStub::Handle_Z },

	{ .cmd = 'q', .handler = GdbStub::Handle_q },
	{ .cmd = 'v', .handler = GdbStub::Handle_v },

	{ .cmd = 0, .handler = NULL }
};


StubState GdbStub::HandlePacket() {
	ExecResult r = CmdExec(handlers_top);

	if (r == ExecResult::MustBreak) {
		if (stat == TgtStatus::None || stat == TgtStatus::Running)
			stat = TgtStatus::BreakReq;
		return StubState::Break;
	} else if (r == ExecResult::InitialBreak) {
		stat = TgtStatus::BreakReq;
		return StubState::Attach;
	/*} else if (r == ExecResult::Detached) {
		stat = TgtStatus::None;
		return StubState::Disconnect;*/
	} else if (r == ExecResult::Continue) {
		stat = TgtStatus::Running;
		return StubState::Continue;
	} else if (r == ExecResult::Step) {
		return StubState::Step;
	} else if (r == ExecResult::Ok || r == ExecResult::UnkCmd) {
		return StubState::None;
	} else {
		stat = TgtStatus::None;
		return StubState::Disconnect;
	}

	// +
	// $qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+;fork-events+;vfork-events+;exec-events+;vContSupported+;QThreadEvents+;no-resumed+;memory-tagging+#ec
	// $qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+;fork-events+;vfork-events+;exec-events+;vContSupported+;QThreadEvents+;no-resumed+;memory-tagging+#ec
	// $qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+;fork-events+;vfork-events+;exec-events+;vContSupported+;QThreadEvents+;no-resumed+;memory-tagging+#ec
	// $qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+;fork-events+;vfork-events+;exec-events+;vContSupported+;QThreadEvents+;no-resumed+;memory-tagging+#ec
	// ---+
	// $vMustReplyEmpty#3a
	// $vMustReplyEmpty#3a
	// $vMustReplyEmpty#3a
	// $vMustReplyEmpty#3a
	// ---+
}

StubState GdbStub::Poll(bool wait) {
	int r;

	if (connfd <= 0) {
		sock_set_block(sockfd, wait);

		// not yet connected, so let's wait for one
		// nonblocking only done in part of read_packet(), so that it can still
		// quickly handle partly-received packets
		struct sockaddr_in* client = (struct sockaddr_in*)clientsa;
		socklen_t len = sizeof(*client);
#ifdef __linux__
		connfd = accept4(sockfd, (struct sockaddr*)client, &len, /*SOCK_NONBLOCK|*/SOCK_CLOEXEC);
#else
		connfd = accept(sockfd, (struct sockaddr*)client, &len);
#endif

		if (connfd < 0) {
			return StubState::NoConn;
		}

		stat = TgtStatus::Running; // on connected
		stat_flag = false;
	}

	if (stat_flag) {
		stat_flag = false;
		//Log(LogLevel::Debug, "[GDB] STAT FLAG WAS TRUE\n");

		Handle_Question(this, NULL, 0); // ugly hack but it should work
	}

#ifndef _WIN32
	struct pollfd pfd;
	pfd.fd = connfd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	r = poll(&pfd, 1, wait ? -1 : 0);

	if (r == 0) return StubState::None; // nothing is happening

	if (pfd.revents & (POLLHUP|POLLERR|POLLNVAL)) {
		// oopsie, something happened
		Disconnect();
		return StubState::Disconnect;
	}
#else
	fd_set infd, outfd, errfd;
	FD_ZERO(&infd); FD_ZERO(&outfd); FD_ZERO(&errfd);
	FD_SET(connfd, &infd);

	struct timeval to;
	if (wait) {
		to.tv_sec = ~(time_t)0; to.tv_usec = ~(long)0;
	} else {
		to.tv_sec = 0; to.tv_usec = 0;
	}

	r = select(1+1, &infd, &outfd, &errfd, &to);

	if (FD_ISSET(connfd, &errfd)) {
		Disconnect();
		return StubState::Disconnect;
	} else if (!FD_ISSET(connfd, &infd)) {
		return StubState::None;
	}
#endif

	ReadResult res = Proto::MsgRecv(connfd, Cmdbuf);

	switch (res) {
	case ReadResult::NoPacket:
		return StubState::None;
	case ReadResult::Break:
		return StubState::Break;
	case ReadResult::Wut:
		Log(LogLevel::Info, "[GDB] WUT\n");
	case_gdbp_eof:
	case ReadResult::Eof:
		Log(LogLevel::Info, "[GDB] EOF!\n");
		close(connfd);
		connfd = 0;
		return StubState::Disconnect;
	case ReadResult::CksumErr:
		Log(LogLevel::Info, "[GDB] checksum err!\n");
		if (Proto::SendNak(connfd) < 0) {
			Log(LogLevel::Error, "[GDB] send nak after cksum fail errored!\n");
			goto case_gdbp_eof;
		}
		return StubState::None;
	case ReadResult::CmdRecvd:
		/*if (Proto::SendAck(connfd) < 0) {
			Log(LogLevel::Error, "[GDB] send packet ack failed!\n");
			goto case_gdbp_eof;
		}*/
		break;
	}

	return HandlePacket();
}

ExecResult GdbStub::SubcmdExec(const u8* cmd, ssize_t len, const SubcmdHandler* handlers) {
	//Log(LogLevel::Debug, "[GDB] subcommand in: '%s'\n", cmd);

	for (size_t i = 0; handlers[i].handler != NULL; ++i) {
		// check if prefix matches
		if (!strncmp((const char*)cmd, handlers[i].substr, strlen(handlers[i].substr))) {
			if (Proto::SendAck(connfd) < 0) {
				Log(LogLevel::Error, "[GDB] send packet ack failed!\n");
				return ExecResult::NetErr;
			}
			return handlers[i].handler(this, &cmd[strlen(handlers[i].substr)], len-strlen(handlers[i].substr));
		}
	}

	Log(LogLevel::Info, "[GDB] unknown subcommand '%s'!\n", cmd);
	/*if (Proto::SendNak(connfd) < 0) {
		Log(LogLevel::Error, "[GDB] send nak after cksum fail errored!\n");
		return ExecResult::NetErr;
	}*/
	//Proto::RespStr(connfd, "E99");
	Proto::Resp(connfd, NULL, 0);
	return ExecResult::UnkCmd;
}

ExecResult GdbStub::CmdExec(const CmdHandler* handlers) {
	//Log(LogLevel::Debug, "[GDB] command in: '%s'\n", Cmdbuf);

	for (size_t i = 0; handlers[i].handler != NULL; ++i) {
		if (handlers[i].cmd == Cmdbuf[0]) {
			if (Proto::SendAck(connfd) < 0) {
				Log(LogLevel::Error, "[GDB] send packet ack failed!\n");
				return ExecResult::NetErr;
			}
			return handlers[i].handler(this, &Cmdbuf[1], Cmdlen-1);
		}
	}

	Log(LogLevel::Info, "[GDB] unknown command '%c'!\n", Cmdbuf[0]);
	/*if (Proto::SendNak(connfd) < 0) {
		Log(LogLevel::Error, "[GDB] send nak after cksum fail errored!\n");
		return ExecResult::NetErr;
	}*/
	//Proto::RespStr(connfd, "E99");
	Proto::Resp(connfd, NULL, 0);
	return ExecResult::UnkCmd;
}


void GdbStub::SignalStatus(TgtStatus stat, u32 arg) {
	//Log(LogLevel::Debug, "[GDB] SIGNAL STATUS %d!\n", stat);

	this->stat = stat;
	stat_flag = true;

	if (stat == TgtStatus::Bkpt) {
		cur_bkpt = arg;
	} else if (stat == TgtStatus::Watchpt) {
		cur_watchpt = arg;
	}
}


StubState GdbStub::Enter(bool stay, TgtStatus stat, u32 arg, bool wait_for_conn) {
	if (stat != TgtStatus::NoEvent) SignalStatus(stat, arg);

	StubState st;
	bool do_next = true;
	do {
		bool was_conn = connfd > 0;
		st = Poll(wait_for_conn);
		bool has_conn = connfd > 0;

		if (has_conn && !was_conn) stay = true;

		switch (st) {
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
		}
	} while (do_next && stay);

	if (st != StubState::None && st != StubState::NoConn)
		Log(LogLevel::Debug, "[GDB] enter exit: %d\n", st);
	return st;
}

void GdbStub::AddBkpt(u32 addr, int kind) {
	BpWp np;
	np.addr = addr ^ (addr & 1); // clear lowest bit to not break on thumb mode weirdnesses
	np.len = 0;
	np.kind = kind;

	{
		// already in the map
		auto search = bp_list.find(np.addr);
		if (search != bp_list.end()) return;
	}

	bp_list.insert({np.addr, np});

	Log(LogLevel::Debug, "[GDB] added bkpt:\n");
	size_t i = 0;
	for (auto search = bp_list.begin(); search != bp_list.end(); ++search, ++i) {
		Log(LogLevel::Debug, "\t[%zu]: addr=%08x, kind=%d\n", i, search->first, search->second.kind);
	}
}
void GdbStub::AddWatchpt(u32 addr, u32 len, int kind) {
	BpWp np;
	np.addr = addr;
	np.len = len;
	np.kind = kind;

	for (auto search = wp_list.begin(); search != wp_list.end(); ++search) {
		if (search->addr > addr) {
			wp_list.insert(search, np);
			return;
		} else if (search->addr == addr && search->kind == kind) {
			if (search->len < len) search->len = len;
			return;
		}
	}

	wp_list.push_back(np);
}

void GdbStub::DelBkpt(u32 addr, int kind) {
	addr = addr ^ (addr & 1);

	auto search = bp_list.find(addr);
	if (search != bp_list.end())
		bp_list.erase(search);
}
void GdbStub::DelWatchpt(u32 addr, u32 len, int kind) {
	(void)len; (void)kind;

	for (auto search = wp_list.begin(); search != wp_list.end(); ++search) {
		if (search->addr == addr && search->kind == kind) {
			wp_list.erase(search);
			return;
		} else if (search->addr > addr) return;
	}
}

void GdbStub::DelAllBpWp() {
	bp_list.erase(bp_list.begin(), bp_list.end());
	wp_list.erase(wp_list.begin(), wp_list.end());
}

StubState GdbStub::CheckBkpt(u32 addr, bool enter, bool stay) {
	addr ^= (addr & 1); // clear lowest bit to not break on thumb mode weirdnesses

	auto search = bp_list.find(addr);
	if (search == bp_list.end()) return StubState::CheckNoHit;

	if (enter) {
		StubState r = Enter(stay, TgtStatus::Bkpt, addr);
		Log(LogLevel::Debug, "[GDB] ENTER st=%d\n", r);
		return r;
	} else {
		SignalStatus(TgtStatus::Bkpt, addr);
		return StubState::None;
	}
}
StubState GdbStub::CheckWatchpt(u32 addr, int kind, bool enter, bool stay) {
	for (auto search = wp_list.begin(); search != wp_list.end(); ++search) {
		if (search->addr > addr) break;

		if (addr >= search->addr && addr < search->addr + search->len && search->kind == kind) {
			if (enter) return Enter(stay, TgtStatus::Watchpt, addr);
			else {
				SignalStatus(TgtStatus::Watchpt, addr);
				return StubState::None;
			}
		}
	}

	return StubState::CheckNoHit;
}

}

