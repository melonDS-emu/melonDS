
#ifdef _WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#include <winsock2.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <algorithm>

#ifndef _WIN32
#include <poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#endif

#include "../Platform.h"
#include "hexutil.h"

#include "GdbStub.h"

using namespace melonDS;
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


Gdb::ReadResult GdbStub::TryParsePacket(size_t start, size_t& packetStart, size_t& packetSize, size_t& packetContentSize)
{
	RecvBuffer[RecvBufferFilled] = '\0';
	//Log(LogLevel::Debug, "[GDB] Trying to parse packet %s %d %d\n", &RecvBuffer[0], start, RecvBufferFilled);
	size_t i = start;
	while (i < RecvBufferFilled)
	{
		char curChar = RecvBuffer[i++];
		if (curChar == '\x04') return ReadResult::Eof;
		else if (curChar == '\x03')
		{
			packetStart = i - 1;
			packetSize = packetContentSize = 1;
			return ReadResult::Break;
		}
		else if (curChar == '+' || curChar == '-') continue;
		else if (curChar == '$')
		{
			packetStart = i;
			uint8_t checksumGot = 0;
			while (i < RecvBufferFilled)
			{
				curChar = RecvBuffer[i];
				if (curChar == '#' && i + 2 < RecvBufferFilled)
				{
					u8 checksumShould = (hex2nyb(RecvBuffer[i+1]) << 4)
						| hex2nyb(RecvBuffer[i+2]);

					Log(LogLevel::Debug, "[GDB] found pkt, checksumGot: %02x vs %02x\n", checksumShould, checksumGot);

					if (checksumShould != checksumGot)
					{
						return ReadResult::CksumErr;
					}

					packetContentSize = i - packetStart;
					packetSize = packetContentSize + 3;
					return ReadResult::CmdRecvd;
				}
				else
				{
					checksumGot += curChar;
				}

				i++;
			}
		}
		else
		{
			Log(LogLevel::Error, "[GDB] Received unknown character %c (%d)\n", curChar, curChar);
			return ReadResult::Wut;
		}
	}

	return ReadResult::NoPacket;
}

Gdb::ReadResult GdbStub::ParseAndSetupPacket()
{
	// This complicated logic seems to be unfortunately necessary
	// to handle the case of packet resends when we answered too slowly.
	// GDB only expects a single response (as it assumes the previous packet was dropped)
	size_t i = 0;
	size_t prevPacketStart = SIZE_MAX, prevPacketSize, prevPacketContentSize;
	size_t packetStart, packetSize, packetContentSize;
	ReadResult result, prevResult;
	while (true)
	{
		result = TryParsePacket(i, packetStart, packetSize, packetContentSize);
		if (result == ReadResult::NoPacket)
			break;
		if (result != ReadResult::CmdRecvd && result != ReadResult::Break)
			return result;

		// looks like there is a different packet coming up
		// so we quit here
		if (prevPacketStart != SIZE_MAX &&
			(packetContentSize != prevPacketContentSize ||
				memcmp(&RecvBuffer[packetStart], &RecvBuffer[prevPacketStart], prevPacketContentSize) != 0))
		{
			Log(LogLevel::Debug, "[GDB] found differing packet further back %zu %zu\n", packetContentSize, prevPacketContentSize);
			break;
		}

		i = packetStart + packetSize;
		prevPacketStart = packetStart;
		prevPacketSize = packetSize;
		prevPacketContentSize = packetContentSize;
		prevResult = result;
	}

	if (prevPacketStart != SIZE_MAX)
	{
		memcpy(&Cmdbuf[0], &RecvBuffer[prevPacketStart], prevPacketContentSize);
		Cmdbuf[prevPacketContentSize] = '\0';
		Cmdlen = static_cast<ssize_t>(prevPacketContentSize);

		RecvBufferFilled -= prevPacketStart + prevPacketSize;
		if (RecvBufferFilled > 0)
			memmove(&RecvBuffer[0], &RecvBuffer[prevPacketStart + prevPacketSize], RecvBufferFilled);

		assert(prevResult == ReadResult::CmdRecvd || prevResult == ReadResult::Break);
		return prevResult;
	}

	assert(result == ReadResult::NoPacket);
	return ReadResult::NoPacket;
}

ReadResult GdbStub::MsgRecv()
{
	{
		ReadResult result = ParseAndSetupPacket();
		if (result != ReadResult::NoPacket)
			return result;
	}

	bool first = true;
	while (true)
	{
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
		ssize_t receivedNum = recv(ConnFd, &RecvBuffer[RecvBufferFilled], sizeof(RecvBuffer) - RecvBufferFilled, flag);
		Log(LogLevel::Debug, "[GDB] receiving from stream %d\n", receivedNum);
#else
		// fuck windows
		ssize_t receivedNum = recv(ConnFd, (char*)&RecvBuffer[RecvBufferFilled], sizeof(RecvBuffer) - RecvBufferFilled, flag);
#endif
#endif

		if (receivedNum <= 0)
		{
			if (first) return ReadResult::NoPacket;
			else
			{
				Log(LogLevel::Debug, "[GDB] recv() error %zi, errno=%d (%s)\n", receivedNum, errno, strerror(errno));
				return ReadResult::Eof;
			}
		}
		RecvBufferFilled += static_cast<u32>(receivedNum); 

		ReadResult result = ParseAndSetupPacket();
		if (result != ReadResult::NoPacket)
			return result;
	}
}

int GdbStub::SendAck()
{
	if (NoAck) return 1;

	Log(LogLevel::Debug, "[GDB] send ack\n");
	u8 v = '+';
#if MOCKTEST
	return 1;
#endif

#ifdef _WIN32
	// fuck windows
	return send(ConnFd, (const char*)&v, 1, 0);
#else
	return send(ConnFd, &v, 1, 0);
#endif
}

int GdbStub::SendNak()
{
	if (NoAck) return 1;

	Log(LogLevel::Debug, "[GDB] send nak\n");
	u8 v = '-';
#if MOCKTEST
	return 1;
#endif

#ifdef _WIN32
	// fuck windows
	return send(ConnFd, (const char*)&v, 1, 0);
#else
	return send(ConnFd, &v, 1, 0);
#endif
}

int GdbStub::WaitAckBlocking(u8* ackp, int to_ms)
{
#if MOCKTEST
	*ackp = '+';
	return 0;
#endif

#ifdef _WIN32
	fd_set infd, outfd, errfd;
	FD_ZERO(&infd); FD_ZERO(&outfd); FD_ZERO(&errfd);
	FD_SET(ConnFd, &infd);

	struct timeval to;
	to.tv_sec = to_ms / 1000;
	to.tv_usec = (to_ms % 1000) * 1000;

	int r = select(ConnFd+1, &infd, &outfd, &errfd, &to);

	if (FD_ISSET(ConnFd, &errfd)) return -1;
	else if (FD_ISSET(ConnFd, &infd))
	{
		r = recv(ConnFd, (char*)ackp, 1, 0);
		if (r < 0) return r;
		return 0;
	}

	return -1;
#else
	struct pollfd pfd;

	pfd.fd = ConnFd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	ssize_t r = (ssize_t)poll(&pfd, 1, to_ms);
	if (r < 0) return r;
	if (r == 0) return -1;

	if (pfd.revents & (POLLHUP|POLLERR)) return -69;

	r = recv(ConnFd, ackp, 1, 0);
	if (r < 0) return r;

	return (r == 1) ? 0 : -1;
#endif
}

int GdbStub::Resp(const u8* data1, size_t len1, const u8* data2, size_t len2, bool noack)
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

		Log(LogLevel::Debug, "[GDB] send resp: '%s'\n", &RespBuf[0]);
#if MOCKTEST
		r = totallen+4;
#else
#ifdef _WIN32
		r = send(ConnFd, (const char*)&RespBuf[0], totallen+4, 0);
#else
		r = send(ConnFd, &RespBuf[0], totallen+4, 0);
#endif
#endif
		if (r < 0) return r;

		if (noack) break;

		r = WaitAckBlocking(&ack, 2000);
		Log(LogLevel::Debug, "[GDB] got ack: '%c'\n", ack);
		if (r == 0 && ack == '+') break;

		++tries;
	}
	while (tries < 3);

	return 0;
}

}


