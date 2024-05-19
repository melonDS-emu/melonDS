/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)mbuf.h	8.3 (Berkeley) 1/21/94
 * mbuf.h,v 1.9 1994/11/14 13:54:20 bde Exp
 */

#ifndef MBUF_H
#define MBUF_H

/*
 * Macros for type conversion
 * mtod(m,t) -	convert mbuf pointer to data pointer of correct type
 */
#define mtod(m, t) ((t)(m)->m_data)

/* XXX About mbufs for slirp:
 * Only one mbuf is ever used in a chain, for each "cell" of data.
 * m_nextpkt points to the next packet, if fragmented.
 * If the data is too large, the M_EXT is used, and a larger block
 * is alloced.  Therefore, m_free[m] must check for M_EXT and if set
 * free the m_ext.  This is inefficient memory-wise, but who cares.
 */

/*
 * mbufs allow to have a gap between the start of the allocated buffer (m_ext if
 * M_EXT is set, m_dat otherwise) and the in-use data:
 *
 *  |--gapsize----->|---m_len------->
 *  |----------m_size------------------------------>
 *                  |----M_ROOM-------------------->
 *                                   |-M_FREEROOM-->
 *
 *  ^               ^                               ^
 *  m_dat/m_ext     m_data                          end of buffer
 */

/*
 * How much room is in the mbuf, from m_data to the end of the mbuf
 */
#define M_ROOM(m)                                                        \
    ((m->m_flags & M_EXT) ? (((m)->m_ext + (m)->m_size) - (m)->m_data) : \
                            (((m)->m_dat + (m)->m_size) - (m)->m_data))

/*
 * How much free room there is
 */
#define M_FREEROOM(m) (M_ROOM(m) - (m)->m_len)

/*
 * How much free room there is before m_data
 */
#define M_ROOMBEFORE(m) \
    (((m)->m_flags & M_EXT) ? (m)->m_data - (m)->m_ext \
                            : (m)->m_data - (m)->m_dat)

struct mbuf {
    /* XXX should union some of these! */
    /* header at beginning of each mbuf: */
    struct mbuf *m_next; /* Linked list of mbufs */
    struct mbuf *m_prev;
    struct mbuf *m_nextpkt; /* Next packet in queue/record */
    struct mbuf *m_prevpkt; /* Flags aren't used in the output queue */
    int m_flags; /* Misc flags */

    int m_size; /* Size of mbuf, from m_dat or m_ext */
    struct socket *m_so;

    char *m_data; /* Current location of data */
    int m_len; /* Amount of data in this mbuf, from m_data */

    Slirp *slirp;
    bool resolution_requested;
    uint64_t expiration_date;
    char *m_ext;
    /* start of dynamic buffer area, must be last element */
    char m_dat[];
};

static inline void ifs_remque(struct mbuf *ifm)
{
    ifm->m_prevpkt->m_nextpkt = ifm->m_nextpkt;
    ifm->m_nextpkt->m_prevpkt = ifm->m_prevpkt;
}

#define M_EXT 0x01 /* m_ext points to more (malloced) data */
#define M_FREELIST 0x02 /* mbuf is on free list */
#define M_USEDLIST 0x04 /* XXX mbuf is on used list (for dtom()) */
#define M_DOFREE                                      \
    0x08 /* when m_free is called on the mbuf, free() \
          * it rather than putting it on the free list */

/* Called by slirp_new */
void m_init(Slirp *);

/* Called by slirp_cleanup */
void m_cleanup(Slirp *slirp);

/* Allocate an mbuf */
struct mbuf *m_get(Slirp *);

/* Release an mbuf (put possibly put it in allocation cache */
void m_free(struct mbuf *);

/* Catenate the second buffer to the end of the first buffer, and release the second */
void m_cat(struct mbuf *, struct mbuf *);

/* Grow the mbuf to the given size */
void m_inc(struct mbuf *, int);

/* If len is positive, trim that amount from the head of the mbuf. If it is negative, trim it from the tail of the mbuf */
void m_adj(struct mbuf *, int len);

/* Copy len bytes from the first buffer at the given offset, to the end of the second buffer */
int m_copy(struct mbuf *, struct mbuf *, int off, int len);

/*
 * Duplicate the mbuf
 *
 * copy_header specifies whether the bytes before m_data should also be copied.
 * header_size specifies how many bytes are to be reserved before m_data.
 */
struct mbuf *m_dup(Slirp *slirp, struct mbuf *m, bool copy_header, size_t header_size);

/*
 * Given a pointer into an mbuf, return the mbuf
 * XXX This is a kludge, I should eliminate the need for it
 * Fortunately, it's not used often
 */
struct mbuf *dtom(Slirp *, void *);

/* Check that the mbuf contains at least len bytes, and return the data */
void *mtod_check(struct mbuf *, size_t len);

/* Return the end of the data of the mbuf */
void *m_end(struct mbuf *);

/* Initialize the ifs queue of the mbuf */
static inline void ifs_init(struct mbuf *ifm)
{
    ifm->m_nextpkt = ifm->m_prevpkt = ifm;
}

#ifdef SLIRP_DEBUG
#  define MBUF_DEBUG 1
#else
#  ifdef HAVE_VALGRIND
#    include <valgrind/valgrind.h>
#    define MBUF_DEBUG RUNNING_ON_VALGRIND
#  else
#    define MBUF_DEBUG 0
#  endif
#endif

/*
 * When a function is given an mbuf as well as the responsibility to free it, we
 * want valgrind etc. to properly identify the new responsible for the
 * free. Achieve this by making a new copy. For instance:
 *
 * f0(void) {
 *   struct mbuf *m = m_get(slirp);
 *   [...]
 *   switch (something) {
 *   case 1:
 *     f1(m);
 *     break;
 *   case 2:
 *     f2(m);
 *     break;
 *   [...]
 *   }
 * }
 *
 * f1(struct mbuf *m) {
 *   M_DUP_DEBUG(m->slirp, m);
 *   [...]
 *   m_free(m); // but author of f1 might be forgetting this
 * }
 *
 * f0 transfers the freeing responsibility to f1, f2, etc.  Without the
 * M_DUP_DEBUG call in f1, valgrind would tell us that it is f0 where the buffer
 * was allocated, but it's difficult to know whether a leak is actually in f0,
 * or in f1, or in f2, etc.  Duplicating the mbuf in M_DUP_DEBUG each time the
 * responsibility is transferred allows to immediately know where the leak
 * actually is.
 */
#define M_DUP_DEBUG(slirp, m, copy_header, header_size) do { \
    if (MBUF_DEBUG) { \
        struct mbuf *__n; \
        __n = m_dup((slirp), (m), (copy_header), (header_size)); \
        m_free(m); \
        (m) = __n; \
    } else { \
        (void) (slirp); (void) (copy_header); \
        g_assert(M_ROOMBEFORE(m) >= (header_size)); \
    } \
} while(0)

#endif
