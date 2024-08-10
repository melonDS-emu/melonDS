/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 1995 Danny Gasparovski.
 */

#ifndef SLIRP_MAIN_H
#define SLIRP_MAIN_H

#include "libslirp.h"

/* The current guest virtual time */
extern unsigned curtime;
/* Always equal to INADDR_LOOPBACK, in network order */
extern struct in_addr loopback_addr;
/* Always equal to IN_CLASSA_NET, in network order */
extern unsigned long loopback_mask;

/* Send a packet to the guest */
int if_encap(Slirp *slirp, struct mbuf *ifm);
/* Send a frame to the guest. Flags are passed to the send() call */
slirp_ssize_t slirp_send(struct socket *so, const void *buf, size_t len, int flags);

#endif
