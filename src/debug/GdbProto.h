
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

Gdb::ReadResult MsgRecv(int connfd, u8 cmd_dest[/*static GDBPROTO_BUFFER_CAPACITY*/]);

int SendAck(int connfd);
int SendNak(int connfd);

int Resp(int connfd, const u8* data1, size_t len1, const u8* data2 = NULL, size_t len2 = 0);
inline int RespC(int connfd, const char* data1, size_t len1, const u8* data2 = NULL, size_t len2 = 0) {
	return Resp(connfd, (const u8*)data1, len1, data2, len2);
}
#if defined(__GCC__) || defined(__clang__)
__attribute__((__format__(printf, 2, 3)))
#endif
int RespFmt(int connfd, const char* fmt, ...);

inline int RespStr(int connfd, const char* str) {
	return Resp(connfd, (const u8*)str, strlen(str));
}

}

}

#endif

