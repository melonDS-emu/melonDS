/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 1995 Danny Gasparovski.
 */

#ifndef SBUF_H
#define SBUF_H

/* How many bytes are free in the sbuf */
#define sbspace(sb) ((sb)->sb_datalen - (sb)->sb_cc)

struct sbuf {
    uint32_t sb_cc; /* actual chars in buffer */
    uint32_t sb_datalen; /* Length of data  */
    char *sb_wptr; /* write pointer. points to where the next
                    * bytes should be written in the sbuf */
    char *sb_rptr; /* read pointer. points to where the next
                    * byte should be read from the sbuf */
    char *sb_data; /* Actual data */
};

/* Release the sbuf */
void sbfree(struct sbuf *sb);

/* Drop len bytes from the reading end of the sbuf */
bool sbdrop(struct sbuf *sb, size_t len);

/* (re)Allocate sbuf buffer to store size bytes */
void sbreserve(struct sbuf *sb, size_t size);

/*
 * Try and write() to the socket, whatever doesn't get written
 * append to the buffer... for a host with a fast net connection,
 * this prevents an unnecessary copy of the data
 * (the socket is non-blocking, so we won't hang)
 */
void sbappend(struct socket *sb, struct mbuf *mb);

/*
 * Copy data from sbuf to a normal, straight buffer
 * Don't update the sbuf rptr, this will be
 * done in sbdrop when the data is acked
 */
void sbcopy(struct sbuf *sb, size_t off, size_t len, char *p);

#endif
