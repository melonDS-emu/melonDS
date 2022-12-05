
#ifndef GDBPROTO_H_
#define GDBPROTO_H_

#include <string.h>
#include <sys/types.h>

#include "gdbstub.h" /* struct gdbstub */

#define GDBPROTO_BUFFER_CAPACITY 1024

extern uint8_t gdbproto_cmdbuf[GDBPROTO_BUFFER_CAPACITY];
extern ssize_t gdbproto_cmdlen;

enum gdbproto_read_result {
	gdbp_no_packet,
	gdbp_eof,
	gdbp_cksum_err,
	gdbp_cmd_recvd,
	gdbp_wut
};

enum gdbproto_exec_result {
	gdbe_ok,
	gdbe_unk_cmd,
	gdbe_net_err,
	gdbe_must_break,
	gdbe_detached
	// TODO
};

typedef enum gdbproto_exec_result (*gdbproto_cmd_f)(struct gdbstub*,
		const uint8_t* cmd, ssize_t len);

struct gdbproto_subcmd_handler {
	char maincmd;
	const uint8_t* substr;
	gdbproto_cmd_f handler;
};

struct gdbproto_cmd_handler {
	char cmd;
	gdbproto_cmd_f handler;
};

enum gdbproto_read_result gdbproto_msg_recv(int connfd,
		uint8_t cmd_dest[static GDBPROTO_BUFFER_CAPACITY]);

enum gdbproto_exec_result gdbproto_subcmd_exec(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len, const struct gdbproto_subcmd_handler* handlers);

enum gdbproto_exec_result gdbproto_cmd_exec(struct gdbstub* stub,
		const struct gdbproto_cmd_handler* handlers);

int gdbproto_send_ack(int connfd);
int gdbproto_send_nak(int connfd);

int gdbproto_resp_2(int connfd, const uint8_t* data1, size_t len1,
                                const uint8_t* dataé, size_t len2);
__attribute__((__format__(printf, 2, 3)))
int gdbproto_resp_fmt(int connfd, const char* fmt, ...);


inline static int gdbproto_resp(int connfd, const uint8_t* data, size_t len) {
	return gdbproto_resp_2(connfd, data, len, NULL, 0);
}
inline static int gdbproto_resp_str(int connfd, const char* str) {
	return gdbproto_resp(connfd, str, strlen(str));
}

#endif

