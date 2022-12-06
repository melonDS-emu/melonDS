
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

#include "gdbstub_internal.h"

struct gdbstub* gdbstub_new(const struct gdbstub_callbacks* cb, int port, void* ud) {
	struct gdbstub* stub = (struct gdbstub*)calloc(1, sizeof(struct gdbstub));
	int r;

	stub->cb = cb;
	stub->ud = ud;
	stub->sockfd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
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

	stub->bp_size = 13;
	stub->wp_size = 7;
	stub->bp_list = (struct bpwp*)calloc(stub->bp_size, sizeof(struct bpwp));
	stub->wp_list = (struct bpwp*)calloc(stub->wp_size, sizeof(struct bpwp));

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

	free(stub->bp_list);
	free(stub->wp_list);

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
	{ .maincmd = 'q', .substr = "HostInfo"   , .handler = gdb_handle_q_HostInfo },
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
	{ .cmd = 'X', .handler = gdb_handle_X },
	{ .cmd = 'c', .handler = gdb_handle_c },
	{ .cmd = 's', .handler = gdb_handle_s },
	{ .cmd = 'p', .handler = gdb_handle_p },
	{ .cmd = 'P', .handler = gdb_handle_P },
	{ .cmd = 'H', .handler = gdb_handle_H },
	{ .cmd = 'T', .handler = gdb_handle_H },

	{ .cmd = '?', .handler = gdb_handle_Question },
	{ .cmd = '!', .handler = gdb_handle_Exclamation },
	{ .cmd = 'D', .handler = gdb_handle_D },
	{ .cmd = 'r', .handler = gdb_handle_r },
	{ .cmd = 'R', .handler = gdb_handle_R },

	{ .cmd = 'z', .handler = gdb_handle_z },
	{ .cmd = 'Z', .handler = gdb_handle_Z },

	{ .cmd = 'q', .handler = gdb_handle_q },
	{ .cmd = 'v', .handler = gdb_handle_v },

	//{ .cmd = 0, .handler = NULL }
};


static enum gdbstub_state handle_packet(struct gdbstub* stub) {
	enum gdbproto_exec_result r = gdbproto_cmd_exec(stub, handlers_top);

	if (r == gdbe_must_break) {
		if (stub->stat == gdbt_none || stub->stat == gdbt_running)
			stub->stat = gdbt_break_req;
		return gdbstat_break;
	} else if (r == gdbe_initial_break) {
		stub->stat = gdbt_break_req;
		return gdbstat_attach;
	/*} else if (r == gdbe_detached) {
		stub->stat = gdbt_none;
		return gdbstat_disconnect;*/
	} else if (r == gdbe_continue) {
		stub->stat = gdbt_running;
		return gdbstat_continue;
	} else if (r == gdbe_step) {
		return gdbstat_step;
	} else if (r == gdbe_ok || r == gdbe_unk_cmd) {
		return gdbstat_none;
	} else {
		stub->stat = gdbt_none;
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

	if (!stub) return gdbstat_noconn;

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

		stub->stat = gdbt_running; // on connected
		stub->stat_flag = false;
	}

	if (stub->stat_flag) {
		stub->stat_flag = false;
		printf("[GDB] STAT FLAG WAS TRUE\n");

		gdb_handle_Question(stub, NULL, 0); // ugly hack but it should work
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
	case gdbp_break:
		return gdbstat_break;
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
			return handlers[i].handler(stub, &cmd[strlen(handlers[i].substr)], len-strlen(handlers[i].substr));
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


void gdbstub_signal_status(struct gdbstub* stub, enum gdbtgt_status stat, uint32_t arg) {
	if (!stub) return;
	printf("[GDB] SIGNAL STATUS %d!\n", stat);
	//__builtin_trap();

	stub->stat = stat;
	stub->stat_flag = true;

	if (stat == gdbt_bkpt) {
		stub->cur_bkpt = arg;
	} else if (stat == gdbt_watchpt) {
		stub->cur_watchpt = arg;
	}
}


enum gdbstub_state gdbstub_enter_reason(struct gdbstub* stub, bool stay, enum gdbtgt_status stat, uint32_t arg) {
	if (stat != gdbt_no_event) gdbstub_signal_status(stub, stat, arg);

	enum gdbstub_state st;
	bool do_next = true;
	do {
		st = gdbstub_poll(stub);

		switch (st) {
		case gdbstat_break:
			printf("[GDB] break execution\n");
			gdbstub_signal_status(stub, gdbt_break_req, ~(uint32_t)0);
			break;
		case gdbstat_continue:
			printf("[GDB] continue execution\n");
			do_next = false;
			break;
		case gdbstat_step:
			printf("[GDB] single-step\n");
			do_next = false;
			break;
		case gdbstat_disconnect:
			printf("[GDB] disconnect\n");
			gdbstub_signal_status(stub, gdbt_none, ~(uint32_t)0);
			do_next = false;
			break;
		}

		if (!stay) break;
	} while (do_next);

	if (st != 0 && st != 1) printf("[GDB] enter exit: %d\n", st);
	return st;
}

void gdbstub_add_bkpt(struct gdbstub* stub, uint32_t addr, int kind) {
	struct bpwp new;
	new.addr = addr;
	new.kind = kind;
	new.used = true;

	size_t newsize = stub->bp_size;
	uint32_t ind;
	while (true) {
		ind = addr % stub->bp_size;
		if (!stub->bp_list[ind].used) break;

		newsize = newsize * 2 + 1; // not exactly following primes but good enough
		struct bpwp* newlist = calloc(newsize, sizeof(struct bpwp));
		for (size_t i = 0; i < stub->bp_size; ++i) {
			struct bpwp a = stub->bp_list[i];

			if (!a.used) continue;
			if (newlist[a.addr % newsize].used) {
				// aaaa
				free(newlist);
				goto continue_outer;
			}

			newlist[a.addr % newsize] = a;
		}

		free(stub->bp_list);
		stub->bp_list = newlist;
		stub->bp_size = newsize;

	continue_outer:;
	}

	stub->bp_list[ind] = new;
}
void gdbstub_add_watchpt(struct gdbstub* stub, uint32_t addr, uint32_t len, int kind) {
	struct bpwp new;
	new.addr = addr;
	new.len  = len ;
	new.kind = kind;
	new.used = true;

	size_t newsize = stub->wp_size;
	uint32_t ind;
	while (true) {
		ind = addr % stub->wp_size;
		if (!stub->wp_list[ind].used) break;

		newsize = newsize * 2 + 1; // not exactly following primes but good enough
		struct bpwp* newlist = calloc(newsize, sizeof(struct bpwp));
		for (size_t i = 0; i < stub->wp_size; ++i) {
			struct bpwp a = stub->wp_list[i];

			if (!a.used) continue;
			if (newlist[a.addr % newsize].used) {
				// aaaa
				free(newlist);
				goto continue_outer;
			}

			newlist[a.addr % newsize] = a;
		}

		free(stub->wp_list);
		stub->wp_list = newlist;
		stub->wp_size = newsize;

	continue_outer:;
	}

	stub->wp_list[ind] = new;
}

void gdbstub_del_bkpt(struct gdbstub* stub, uint32_t addr, int kind) {
	(void)kind;

	uint32_t ind = addr % stub->bp_size;
	if (stub->bp_list[ind].used && stub->bp_list[ind].addr == addr) {
		stub->bp_list[ind].used = false;
	}
}
void gdbstub_del_watchpt(struct gdbstub* stub, uint32_t addr, uint32_t len, int kind) {
	(void)kind; (void)len;

	uint32_t ind = addr % stub->bp_size;
	if (stub->bp_list[ind].used && stub->bp_list[ind].addr == addr) {
		stub->bp_list[ind].used = false;
	}
}

void gdbstub_del_all_bp_wp(struct gdbstub* stub) {
	free(stub->bp_list);
	free(stub->wp_list);

	stub->bp_size = 13;
	stub->wp_size = 7;
	stub->bp_list = (struct bpwp*)calloc(stub->bp_size, sizeof(struct bpwp));
	stub->wp_list = (struct bpwp*)calloc(stub->wp_size, sizeof(struct bpwp));
}

enum gdbstub_state gdbstub_check_bkpt(struct gdbstub* stub, uint32_t addr, bool enter, bool stay) {
	uint32_t ind = addr % stub->bp_size;

	if (stub->bp_list[ind].used && stub->bp_list[ind].addr == addr) {
		if (enter)
			return gdbstub_enter_reason(stub, stay, gdbt_bkpt, addr);
		else {
			gdbstub_signal_status(stub, gdbt_bkpt, addr);
			return gdbstat_none;
		}
	}

	return gdbstat_check_no_hit;
}
enum gdbstub_state gdbstub_check_watchpt(struct gdbstub* stub, uint32_t addr, int kind, bool enter, bool stay) {
	uint32_t ind = addr % stub->wp_size;

	// TODO: check address ranges!
	if (stub->wp_list[ind].used && stub->wp_list[ind].addr == addr && stub->wp_list[ind].kind == kind) {
		if (enter)
			return gdbstub_enter_reason(stub, stay, gdbt_watchpt, addr);
		else {
			gdbstub_signal_status(stub, gdbt_watchpt, addr);
			return gdbstat_none;
		}
	}

	return gdbstat_check_no_hit;
}

