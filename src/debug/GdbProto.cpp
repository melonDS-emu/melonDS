
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
#include <errno.h>

#ifndef _WIN32
#include <poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#endif

#include "../Platform.h"
#include "hexutil.h"

#include "GdbProto.h"


using Platform::Log;
using Platform::LogLevel;

namespace Gdb
{

/*
 * TODO commands to support:
 * m M g G c s p P H
 * ? D r
 * qC qfThreadInfo qsThreadInfo
 * z0 Z0 z1 Z1 z4 Z4
 * qCRC
 * vAttach;addr
 * vKill;pid
 * qRcmd? qSupported?
 */
u8 Cmdbuf[GDBPROTO_BUFFER_CAPACITY];
ssize_t Cmdlen;

namespace Proto
{

u8 PacketBuf[GDBPROTO_BUFFER_CAPACITY];
u8 RespBuf[GDBPROTO_BUFFER_CAPACITY+5];

ReadResult MsgRecv(int connfd, u8 cmd_dest[/*static GDBPROTO_BUFFER_CAPACITY*/])
{
	static ssize_t dataoff = 0;

	ssize_t recv_total = dataoff;
	ssize_t cksumoff = -1;
	u8 sum = 0;

	bool first = true;

	//printf("--- dataoff=%zd\n", dataoff);
	if (dataoff != 0) {
		printf("--- got preexisting: %s\n", PacketBuf);

		ssize_t datastart = 0;
		while (true)
		{
			if (PacketBuf[datastart] == '\x04') return ReadResult::Eof;
			else if (PacketBuf[datastart] == '+' || PacketBuf[datastart] == '-')
			{
				/*if (PacketBuf[datastart] == '+') SendAck(connfd);
				else SendNak(connfd);*/
				++datastart;
				continue;
			}
			else if (PacketBuf[datastart] == '$')
			{
				++datastart;
				break;
			}
			else
			{
				__builtin_trap();
				return ReadResult::Wut;
			}
		}
		printf("--- datastart=%zd\n", datastart);

		for (ssize_t i = datastart; i < dataoff; ++i)
		{
			if (PacketBuf[i] == '#')
			{
				cksumoff = i + 1;
				printf("--- cksumoff=%zd\n", cksumoff);
				break;
			}

			sum += PacketBuf[i];
		}

		if (cksumoff >= 0)
		{
			recv_total = dataoff - datastart + 1;
			dataoff = cksumoff + 2 - datastart + 1;
			cksumoff -= datastart - 1;

			memmove(&PacketBuf[1], &PacketBuf[datastart], recv_total);
			PacketBuf[0] = '$';
			PacketBuf[recv_total] = 0;

			printf("=== cksumoff=%zi recv_total=%zi datastart=%zi dataoff=%zi\n==> %s\n",
					cksumoff, recv_total, datastart, dataoff, PacketBuf);
			//break;
		}
	}

	while (cksumoff < 0)
	{
		u8* pkt = &PacketBuf[dataoff];
		ssize_t n, blehoff = 0;

		memset(pkt, 0, sizeof(PacketBuf) - dataoff);
		int flag = 0;
#if MOCKTEST
		static bool FIRST = false;
		if (FIRST) {
			printf("%s", "[==>] TEST DONE\n");
			__builtin_trap();
		}
		FIRST = true;

		const char* testinp1 = "+$qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+;fork-events+;vfork-events+;exec-events+;vContSupported+;QThreadEvents+;no-resumed+;memory-tagging+;xmlRegisters=i386#77$qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+;fork-events+;vfork-events+;exec-events+;vContSupported+;QThreadEvents+;no-resumed+;memory-tagging+;xmlRegisters=i386#77$qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+;fork-events+;vfork-events+;exec-events+;vContSupported+;QThreadEvents+;no-resumed+;memory-tagging+;xmlRegisters=i386#77$qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+;fork-events+;vfork-events+;exec-events+;vContSupported+;QThreadEvents+;no-resumed+;memory-tagging+;xmlRegisters=i386#77";
		const char* testinp2 = "+$qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+;fork-events+;vfork-events+;exec-events+;vContSupported+;QThreadEvents+;no-resumed+;memory-tagging+;xmlRegisters=i386#77$qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+;fork-events+;vfork-events+;exec-events+;vContSupported+;QThreadEvents+;no-resumed+;memory-tagging+;xmlRegisters=i386#77$qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+;fork-events+;vfork-events+;exec-events+;vContSupported+;QThreadEvents+;no-resumed+;memory-tagging+;xmlRegisters=i386#77$qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+;fork-events+;vfork-events+;exec-events+;vContSupported+;QThreadEvents+;no-resumed+;memory-tagging+;xmlRegisters=i386#77---+$vMustReplyEmpty#3a";

		const char* testinp = testinp1;

		n = strlen(testinp);
		memcpy(pkt, testinp, strlen(testinp));
#else
#ifndef _WIN32
		if (first) flag |= MSG_DONTWAIT;
		n = recv(connfd, pkt, sizeof(PacketBuf) - dataoff, flag);
#else
		// fuck windows
		n = recv(connfd, (char*)pkt, sizeof(PacketBuf) - dataoff, flag);
#endif
#endif

		if (n <= 0)
		{
			if (first) return ReadResult::NoPacket;
			else
			{
				Log(LogLevel::Debug, "[GDB] recv() error %zi, errno=%d (%s)\n", n, errno, strerror(errno));
				return ReadResult::Eof;
			}
		}

		Log(LogLevel::Debug, "[GDB] recv() %zd bytes: '%s' (%02x)\n", n, pkt, pkt[0]);
		first = false;

		do
		{
			if (dataoff == 0)
			{
				if (pkt[blehoff] == '\x04') return ReadResult::Eof;
				else if (pkt[blehoff] == '\x03') return ReadResult::Break;
				else if (pkt[blehoff] != '$')
				{
					++blehoff;
					--n;
				}
				else break;

				if (n == 0) goto next_outer;
			}
		}
		while (true);

		if (blehoff > 0)
		{
			memmove(pkt, &pkt[blehoff], n - blehoff + 1);
			n -= blehoff - 1; // ???
		}

		recv_total += n;

		Log(LogLevel::Debug, "[GDB] recv() after skipping: n=%zd, recv_total=%zd\n", n, recv_total);

		for (ssize_t i = (dataoff == 0) ? 1 : 0; i < n; ++i)
		{
			u8 v = pkt[i];
			if (v == '#')
			{
				cksumoff = dataoff + i + 1;
				break;
			}

			sum += pkt[i];
		}

		if (cksumoff < 0)
		{
			// oops, need more data
			dataoff += n;
		}

	next_outer:;
	}

	u8 ck = (hex2nyb(PacketBuf[cksumoff+0]) << 4)
		| hex2nyb(PacketBuf[cksumoff+1]);

	Log(LogLevel::Debug, "[GDB] got pkt, checksum: %02x vs %02x\n", ck, sum);

	if (ck != sum)
	{
		//__builtin_trap();
		return ReadResult::CksumErr;
	}

	if (cksumoff + 2 > recv_total)
	{
		Log(LogLevel::Error, "[GDB] BIG MISTAKE: %zi > %zi which shouldn't happen!\n", cksumoff + 2, recv_total);
		//__builtin_trap();
		return ReadResult::Wut;
	}
	else
	{
		Cmdlen = cksumoff - 2;
		memcpy(Cmdbuf, &PacketBuf[1], Cmdlen);
		Cmdbuf[Cmdlen] = 0;

		if (cksumoff + 2 < recv_total) {
			// huh, we have the start of the next packet
			dataoff = recv_total - (cksumoff + 2);
			memmove(PacketBuf, &PacketBuf[cksumoff + 2], (size_t)dataoff);
			PacketBuf[dataoff] = 0;
			Log(LogLevel::Debug, "[GDB] got more: cksumoff=%zd, recvtotal=%zd, remain=%zd\n==> %s\n", cksumoff, recv_total, dataoff, PacketBuf);
		}
		else dataoff = 0;
	}

	return ReadResult::CmdRecvd;
}

int SendAck(int connfd)
{
	Log(LogLevel::Debug, "[GDB] send ack\n");
	u8 v = '+';
#if MOCKTEST
	return 1;
#endif

#ifdef _WIN32
	// fuck windows
	return send(connfd, (const char*)&v, 1, 0);
#else
	return send(connfd, &v, 1, 0);
#endif
}

int SendNak(int connfd)
{
	Log(LogLevel::Debug, "[GDB] send nak\n");
	u8 v = '-';
#if MOCKTEST
	return 1;
#endif

#ifdef _WIN32
	// fuck windows
	return send(connfd, (const char*)&v, 1, 0);
#else
	return send(connfd, &v, 1, 0);
#endif
}

int WaitAckBlocking(int connfd, u8* ackp, int to_ms)
{
#if MOCKTEST
	*ackp = '+';
	return 0;
#endif

#ifdef _WIN32
	fd_set infd, outfd, errfd;
	FD_ZERO(&infd); FD_ZERO(&outfd); FD_ZERO(&errfd);
	FD_SET(connfd, &infd);

	struct timeval to;
	to.tv_sec = to_ms / 1000;
	to.tv_usec = (to_ms % 1000) * 1000;

	int r = select(connfd+1, &infd, &outfd, &errfd, &to);

	if (FD_ISSET(connfd, &errfd)) return -1;
	else if (FD_ISSET(connfd, &infd))
	{
		r = recv(connfd, (char*)ackp, 1, 0);
		if (r < 0) return r;
		return 0;
	}

	return -1;
#else
	struct pollfd pfd;

	pfd.fd = connfd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	ssize_t r = (ssize_t)poll(&pfd, 1, to_ms);
	if (r < 0) return r;
	if (r == 0) return -1;

	if (pfd.revents & (POLLHUP|POLLERR)) return -69;

	r = recv(connfd, ackp, 1, 0);
	if (r < 0) return r;

	return (r == 1) ? 0 : -1;
#endif
}

int Resp(int connfd, const u8* data1, size_t len1, const u8* data2, size_t len2, bool noack)
{
	u8 cksum = 0;
	int tries = 0;

	size_t totallen = len1 + len2;

	if (totallen >= GDBPROTO_BUFFER_CAPACITY)
	{
		Log(LogLevel::Error, "[GDB] packet with len %zu can't fit in buffer!\n", totallen);
		return -42;
	}

	RespBuf[0] = '$';
	for (size_t i = 0; i < len1; ++i)
	{
		cksum += data1[i];
		RespBuf[i+1] = data1[i];
	}
	for (size_t i = 0; i < len2; ++i)
	{
		cksum += data2[i];
		RespBuf[len1+i+1] = data2[i];
	}
	RespBuf[totallen+1] = '#';
	hexfmt8(&RespBuf[totallen+2], cksum);
	RespBuf[totallen+4] = 0;

	do
	{
		ssize_t r;
		u8 ack;

		Log(LogLevel::Debug, "[GDB] send resp: '%s'\n", RespBuf);
#if MOCKTEST
		r = totallen+4;
#else
#ifdef _WIN32
		r = send(connfd, (const char*)RespBuf, totallen+4, 0);
#else
		r = send(connfd, RespBuf, totallen+4, 0);
#endif
#endif
		if (r < 0) return r;

		if (noack) break;

		r = WaitAckBlocking(connfd, &ack, 2000);
		//Log(LogLevel::Debug, "[GDB] got ack: '%c'\n", ack);
		if (r == 0 && ack == '+') break;

		++tries;
	}
	while (tries < 3);

	return 0;
}

}

}

