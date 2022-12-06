
#ifndef GDBSTUB_INTERNAL_H_
#define GDBSTUB_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct bpwp {
	uint32_t addr, len;
	int kind;
	bool used;
};

struct gdbstub {
	int sockfd;
	int connfd;
	struct sockaddr_in server, client;

	const struct gdbstub_callbacks* cb;
    void* ud;

	enum gdbtgt_status stat;
	uint32_t cur_bkpt, cur_watchpt;
	bool stat_flag;

	struct bpwp* bp_list;
	size_t bp_size;
	struct bpwp* wp_list;
	size_t wp_size;
};

#ifdef __cplusplus
}
#endif

#endif

