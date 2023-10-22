
#ifndef GDBPROTO_H_
#define GDBPROTO_H_

#include <string.h>
#include <sys/types.h>

#include "GdbStub.h" /* class GdbStub */


#define MOCKTEST 0


namespace Gdb {

constexpr int GDBPROTO_BUFFER_CAPACITY = 1024+128;

extern u8 Cmdbuf[GDBPROTO_BUFFER_CAPACITY];
extern ssize_t Cmdlen;

namespace Proto {

extern u8 PacketBuf[GDBPROTO_BUFFER_CAPACITY];
extern u8 RespBuf[GDBPROTO_BUFFER_CAPACITY+5];

Gdb::ReadResult MsgRecv(int connfd, u8 cmd_dest[/*static GDBPROTO_BUFFER_CAPACITY*/]);

int SendAck(int connfd);
int SendNak(int connfd);

int Resp(int connfd, const u8* data1, size_t len1, const u8* data2, size_t len2, bool noack);

int WaitAckBlocking(int connfd, u8* ackp, int to_ms);

}

}

#endif

