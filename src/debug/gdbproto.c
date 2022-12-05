
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <poll.h>

#include "hexutil.h"

#include "gdbproto.h"

static uint8_t packetbuf[GDBPROTO_BUFFER_CAPACITY];

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
uint8_t gdbproto_cmdbuf[GDBPROTO_BUFFER_CAPACITY];
ssize_t gdbproto_cmdlen;
#define cmdbuf gdbproto_cmdbuf
#define cmdlen gdbproto_cmdlen

static uint8_t respbuf[GDBPROTO_BUFFER_CAPACITY+5];

enum gdbproto_read_result gdbproto_msg_recv(int connfd,
		uint8_t cmd_dest[static GDBPROTO_BUFFER_CAPACITY]) {
	static ssize_t dataoff = 0;

	ssize_t recv_total = dataoff;
	ssize_t cksumoff = -1;
	uint8_t sum = 0;

	bool first = true;

	if (dataoff != 0) {
		if (packetbuf[0] == '\x04') { // EOF
			return gdbp_eof;
		} else if (packetbuf[0] != '$') {
			__builtin_trap();
			return gdbp_wut;
		}

		for (ssize_t i = 1; i < dataoff; ++i) {
			if (packetbuf[i] == '#') {
				cksumoff = dataoff + i + 1;
				break;
			}

			sum += packetbuf[i];
		}

		if (cksumoff >= 0) {
			recv_total = dataoff;
			dataoff = 0;

			//break;
		}
	}

	while (cksumoff < 0) {
		uint8_t* pkt = &packetbuf[dataoff];
		ssize_t n, blehoff = 0;

		memset(pkt, 0, sizeof(packetbuf) - dataoff);
		n = recv(connfd, pkt, sizeof(packetbuf) - dataoff, first ? MSG_DONTWAIT : 0);

		if (n <= 0) {
			if (first) return gdbp_no_packet;
			else {
				printf("[GDB] recv() error %zi, errno=%d (%s)\n", n, errno, strerror(errno));
				return gdbp_eof;
			}
		}

		printf("[GDB] recv() %zd bytes: '%s' (%02x)\n", n, pkt, pkt[0]);
		first = false;

		do {
			if (dataoff == 0) {
				if (pkt[blehoff] == '\x04') { // EOF
					return gdbp_eof;
				} else if (pkt[blehoff] == '\x03') { // break request
					return gdbp_break;
				} else if (pkt[blehoff] != '$') {
					++blehoff;
					--n;
				} else {
					break;
				}

				if (n == 0) goto next_outer;
			}
		} while (true);

		if (blehoff > 0) {
			memmove(pkt, &pkt[blehoff], n - blehoff + 1);
			n -= blehoff - 1; // ???
		}

		recv_total += n;

		//printf("[GDB] recv() after skipping: n=%zd, recv_total=%zd\n", n, recv_total);

		for (ssize_t i = (dataoff == 0) ? 1 : 0; i < n; ++i) {
			uint8_t v = pkt[i];
			if (v == '#') {
				cksumoff = dataoff + i + 1;
				break;
			}

			sum += pkt[i];
		}

		if (cksumoff < 0) {
			// oops, need more data
			dataoff += n;
		}

	next_outer:;
	}

	uint8_t ck = (hex2nyb(packetbuf[cksumoff+0]) << 4)
		| hex2nyb(packetbuf[cksumoff+1]);

	//printf("[GDB] got pkt, checksum: %02x vs %02x\n", ck, sum);

	if (ck != sum) {
		__builtin_trap();
		return gdbp_cksum_err;
	}

	if (cksumoff + 2 > recv_total) {
		printf("[GDB] BIG MISTAKE: %zi > %zi which shouldn't happen!\n", cksumoff + 2, recv_total);
		__builtin_trap();
		return gdbp_wut;
	} else {
		cmdlen = cksumoff - 2;
		memcpy(cmdbuf, &packetbuf[1], cmdlen);
		cmdbuf[cmdlen] = 0;

		if (cksumoff + 2 < recv_total) {
			// huh, we have the start of the next packet
			dataoff = recv_total - (cksumoff + 2);
			memmove(packetbuf, &packetbuf[cksumoff + 2], (size_t)dataoff);
		} else {
			dataoff = 0;
		}
	}

	return gdbp_cmd_recvd;
}

int gdbproto_send_ack(int connfd) {
	//printf("[GDB] send ack\n");
	uint8_t v = '+';
	return send(connfd, &v, 1, 0);
}

int gdbproto_send_nak(int connfd) {
	printf("[GDB] send nak\n");
	uint8_t v = '-';
	return send(connfd, &v, 1, 0);
}

static int wait_ack_blocking(int connfd, uint8_t* ackp, int to_ms) {
	struct pollfd pfd;

	pfd.fd = connfd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	ssize_t r = (ssize_t)poll(&pfd, 1, to_ms);
	if (r < 0) return r;
	if (r == 0) return -1;

	if (pfd.revents & (POLLHUP|POLLERR)) {
		return -69;
	}

	r = recv(connfd, ackp, 1, 0);
	if (r < 0) return r;

	return (r == 1) ? 0 : -1;
}

int gdbproto_resp_2(int connfd, const uint8_t* data1, size_t len1,
                                const uint8_t* data2, size_t len2) {
	uint8_t cksum = 0;
	int tries = 0;

	size_t totallen = len1 + len2;

	if (totallen >= GDBPROTO_BUFFER_CAPACITY) return -42;

	respbuf[0] = '$';
	for (size_t i = 0; i < len1; ++i) {
		cksum += data1[i];
		respbuf[i+1] = data1[i];
	}
	for (size_t i = 0; i < len2; ++i) {
		cksum += data2[i];
		respbuf[len1+i+1] = data2[i];
	}
	respbuf[totallen+1] = '#';
	hexfmt8(&respbuf[totallen+2], cksum);
	respbuf[totallen+4] = 0;

	do {
		ssize_t r;
		uint8_t ack;

		printf("[GDB] send resp: '%s'\n", respbuf);
		r = send(connfd, respbuf, totallen+4, 0);
		if (r < 0) return r;

		r = wait_ack_blocking(connfd, &ack, 2000);
		printf("[GDB] got ack: '%c'\n", ack);
		if (r == 0 && ack == '+') break;

		++tries;
	} while (tries < 3);

	return 0;
}

__attribute__((__format__(printf, 2, 3)))
int gdbproto_resp_fmt(int connfd, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int r = vsnprintf(&respbuf[1], sizeof(respbuf)-5, fmt, args);
	va_end(args);

	if (r < 0) return r;

	if (r >= sizeof(respbuf)-5) {
		printf("[GDB] truncated response in send_fmt()! (lost %zd bytes)\n",
				(ssize_t)r - (ssize_t)(sizeof(respbuf)-5));
		r = sizeof(respbuf)-5;
	}

	return gdbproto_resp(connfd, &respbuf[1], r);
}

