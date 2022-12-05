
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>

#include "gdbstub.h"
#include "gdbproto.h"
#include "gdbcmds.h"

struct gdbstub* gdbstub_new(const struct gdbstub_callbacks* cb, int port) {
	struct gdbstub* stub = (struct gdbstub*)calloc(1, sizeof(struct gdbstub));
	int r;

	stub->cb = cb;
	stub->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (stub->sockfd < 0) {
		printf("[GDB] err: can't create a socket fd\n");
		goto err;
	}

	stub->server.sin_family = AF_INET;
	stub->server.sin_addr.s_addr = htonl(INADDR_ANY);
	stub->server.sin_port = htons(port);

	r = bind(stub->sockfd, &stub->server, sizeof(stub->server));
	if (r < 0) {
		printf("[GDB] err: can't bind to address <any> and port %d\n", port);
		goto err;
	}

	r = listen(stub->sockfd, 5);
	if (r < 0) {
		printf("[GDB] err: can't listen to sockfd\n");
		goto err;
	}

	return stub;

err:
	if (stub) {
		if (stub->sockfd != 0) {
			close(stub->sockfd);
			stub->sockfd = 0;
		}
		free(stub);
	}
	return NULL;
}

void gdbstub_close(struct gdbstub* stub) {
	if (!stub) return;

	free(stub);
}

static void disconnect(struct gdbstub* stub) {
	if (stub->connfd > 0) {
		close(stub->connfd);
	}
	stub->connfd = 0;
}

static struct gdbproto_subcmd_handler handlers_v[] = {
	{ .maincmd = 'v', .substr = "Attach;"       , .handler = gdb_handle_v_Attach },
	{ .maincmd = 'v', .substr = "Kill;"         , .handler = gdb_handle_v_Kill },
	{ .maincmd = 'v', .substr = "Run"           , .handler = gdb_handle_v_Run },
	{ .maincmd = 'v', .substr = "Stopped"       , .handler = gdb_handle_v_Stopped },
	{ .maincmd = 'v', .substr = "MustReplyEmpty", .handler = gdb_handle_v_MustReplyEmpty },

	{ .maincmd = 'v', .substr = NULL, .handler = NULL }
};

static struct gdbproto_subcmd_handler handlers_q[] = {
	{ .maincmd = 'q', .substr = "Rcmd,"      , .handler = gdb_handle_q_Rcmd },
	{ .maincmd = 'q', .substr = "Supported:" , .handler = gdb_handle_q_Supported },
	{ .maincmd = 'q', .substr = "CRC:"       , .handler = gdb_handle_q_CRC },
	{ .maincmd = 'q', .substr = "C"          , .handler = gdb_handle_q_C },
	{ .maincmd = 'q', .substr = "fThreadInfo", .handler = gdb_handle_q_fThreadInfo },
	{ .maincmd = 'q', .substr = "sThreadInfo", .handler = gdb_handle_q_sThreadInfo },
	{ .maincmd = 'q', .substr = "Attached"   , .handler = gdb_handle_q_Attached },
	{ .maincmd = 'q', .substr = "Xfer:features:read:target.xml:", .handler = gdb_handle_q_features },

	{ .maincmd = 'q', .substr = NULL, .handler = NULL },
};

static enum gdbproto_exec_result gdb_handle_q(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	return gdbproto_subcmd_exec(stub, cmd, len, handlers_q);
}

static enum gdbproto_exec_result gdb_handle_v(struct gdbstub* stub,
		const uint8_t* cmd, ssize_t len) {
	return gdbproto_subcmd_exec(stub, cmd, len, handlers_v);
}

static struct gdbproto_cmd_handler handlers_top[] = {
	{ .cmd = 'g', .handler = gdb_handle_g },
	{ .cmd = 'G', .handler = gdb_handle_G },
	{ .cmd = 'm', .handler = gdb_handle_m },
	{ .cmd = 'M', .handler = gdb_handle_M },
	{ .cmd = 'c', .handler = gdb_handle_c },
	{ .cmd = 's', .handler = gdb_handle_s },
	{ .cmd = 'p', .handler = gdb_handle_p },
	{ .cmd = 'P', .handler = gdb_handle_P },
	{ .cmd = 'H', .handler = gdb_handle_H },

	{ .cmd = '?', .handler = gdb_handle_Question },
	{ .cmd = 'D', .handler = gdb_handle_D },
	{ .cmd = 'r', .handler = gdb_handle_r },

	{ .cmd = 'z', .handler = gdb_handle_z },
	{ .cmd = 'Z', .handler = gdb_handle_Z },

	{ .cmd = 'q', .handler = gdb_handle_q },
	{ .cmd = 'v', .handler = gdb_handle_v },

	//{ .cmd = 0, .handler = NULL }
};


static enum gdbstub_state handle_packet(struct gdbstub* stub) {
	enum gdbproto_exec_result r = gdbproto_cmd_exec(stub, handlers_top);

	if (r == gdbe_must_break) {
		return gdbstat_break;
	} else if (r == gdbe_detached) {
		return gdbstat_continue;
	} else if (r == gdbe_ok || r == gdbe_unk_cmd) {
		return gdbstat_none;
	} else {
		return gdbstat_disconnect;
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

enum gdbstub_state gdbstub_poll(struct gdbstub* stub) {
	int r;

	if (stub->connfd <= 0) {
		// not yet connected, so let's wait for one
		// nonblocking only done in part of read_packet(), so that it can still
		// quickly handle partly-received packets
		socklen_t len = sizeof(stub->client);
		stub->connfd = accept4(stub->sockfd, (struct sockaddr*)&stub->client,
				&len, /*SOCK_NONBLOCK|*/SOCK_CLOEXEC);

		if (stub->connfd < 0) {
			return gdbstat_noconn;
		}
	}

	struct pollfd pfd;
	pfd.fd = stub->connfd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	r = poll(&pfd, 1, 0);

	if (r == 0) return gdbstat_none; // nothing is happening

	if (pfd.revents & (POLLHUP|POLLERR|POLLNVAL)) {
		// oopsie, something happened
		disconnect(stub);
		return gdbstat_disconnect;
	}

	enum gdbproto_read_result res = gdbproto_msg_recv(stub->connfd, gdbproto_cmdbuf);

	switch (res) {
	case gdbp_no_packet:
		return gdbstat_none;
	case gdbp_wut:
		printf("[GDB] WUT\n");
	case_gdbp_eof:
	case gdbp_eof:
		printf("[GDB] EOF!\n");
		close(stub->connfd);
		stub->connfd = 0;
		return gdbstat_disconnect;
	case gdbp_cksum_err:
		printf("[GDB] checksum err!\n");
		if (gdbproto_send_nak(stub->connfd) < 0) {
			printf("[GDB] send nak after cksum fail errored!\n");
			goto case_gdbp_eof;
		}
		return gdbstat_none;
	case gdbp_cmd_recvd:
		/*if (gdbproto_send_ack(stub->connfd) < 0) {
			printf("[GDB] send packet ack failed!\n");
			goto case_gdbp_eof;
		}*/
		break;
	}

	return handle_packet(stub);
}

enum gdbproto_exec_result gdbproto_subcmd_exec(struct gdbstub* stub, const uint8_t* cmd,
		ssize_t len, const struct gdbproto_subcmd_handler* handlers) {
	printf("[GDB] subcommand in: '%s'\n", cmd);

	for (size_t i = 0; handlers[i].handler != NULL; ++i) {
		// check if prefix matches
		if (!strncmp(cmd, handlers[i].substr, strlen(handlers[i].substr))) {
			if (gdbproto_send_ack(stub->connfd) < 0) {
				printf("[GDB] send packet ack failed!\n");
				return gdbe_net_err;
			}
			return handlers[i].handler(stub, &cmd[strlen(handlers[i].substr)], len);
		}
	}

	printf("[GDB] unknown subcommand '%s'!\n", cmd);
	/*if (gdbproto_send_nak(stub->connfd) < 0) {
		printf("[GDB] send nak after cksum fail errored!\n");
		return gdbe_net_err;
	}*/
	//gdbproto_resp_str(stub->connfd, "E99");
	gdbproto_resp(stub->connfd, NULL, 0);
	return gdbe_unk_cmd;
}

enum gdbproto_exec_result gdbproto_cmd_exec(struct gdbstub* stub, const struct
		gdbproto_cmd_handler* handlers) {
	printf("[GDB] command in: '%s'\n", gdbproto_cmdbuf);

	for (size_t i = 0; i < sizeof(handlers_top)/sizeof(*handlers_top); ++i) {
		if (handlers_top[i].cmd == gdbproto_cmdbuf[0]) {
			if (gdbproto_send_ack(stub->connfd) < 0) {
				printf("[GDB] send packet ack failed!\n");
				return gdbe_net_err;
			}
			return handlers_top[i].handler(stub, &gdbproto_cmdbuf[1], gdbproto_cmdlen-1);
		}
	}

	printf("[GDB] unknown command '%c'!\n", gdbproto_cmdbuf[0]);
	/*if (gdbproto_send_nak(stub->connfd) < 0) {
		printf("[GDB] send nak after cksum fail errored!\n");
		return gdbe_net_err;
	}*/
	//gdbproto_resp_str(stub->connfd, "E99");
	gdbproto_resp(stub->connfd, NULL, 0);
	return gdbe_unk_cmd;
}

