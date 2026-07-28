/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 1995 Danny Gasparovski.
 */

#ifndef SLIRP_SOCKET_H
#define SLIRP_SOCKET_H

#include <string.h>

#ifndef _WIN32
#include <sys/un.h>
#endif

#include "misc.h"
#include "sbuf.h"

#define SO_EXPIRE 240000
#define SO_EXPIREFAST 10000

/* Helps unify some in/in6 routines. */
union in4or6_addr {
    struct in_addr addr4;
    struct in6_addr addr6;
};
typedef union in4or6_addr in4or6_addr;

/*
 * Our socket structure
 */

union slirp_sockaddr {
    struct sockaddr sa;
    struct sockaddr_storage ss;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
};

struct socket {
    struct socket *so_next, *so_prev; /* For a linked list of sockets */

    slirp_os_socket s;     /* The actual socket */
    slirp_os_socket s_aux; /* An auxiliary socket for miscellaneous use. Currently used to
                            * reserve OS ports in UNIX-to-inet translation. */
    struct gfwd_list *guestfwd;

    int pollfds_idx; /* GPollFD GArray index */

    Slirp *slirp; /* managing slirp instance */

    /* XXX union these with not-yet-used sbuf params */
    struct mbuf *so_m; /* Pointer to the original SYN packet,
                        * for non-blocking connect()'s, and
                        * PING reply's */
    struct tcpiphdr *so_ti; /* Pointer to the original ti within
                             * so_mconn, for non-blocking connections */
    uint32_t so_urgc;
    union slirp_sockaddr fhost; /* Foreign host */
#define so_faddr fhost.sin.sin_addr
#define so_fport fhost.sin.sin_port
#define so_faddr6 fhost.sin6.sin6_addr
#define so_fport6 fhost.sin6.sin6_port
#define so_ffamily fhost.ss.ss_family

    union slirp_sockaddr lhost; /* Local host */
#define so_laddr lhost.sin.sin_addr
#define so_lport lhost.sin.sin_port
#define so_laddr6 lhost.sin6.sin6_addr
#define so_lport6 lhost.sin6.sin6_port
#define so_lfamily lhost.ss.ss_family

    uint8_t so_iptos; /* Type of service */
    uint8_t so_emu; /* Is the socket emulated? */

    uint8_t so_type; /* Protocol of the socket. May be 0 if loading old
                      * states. */
    int32_t so_state; /* internal state flags SS_*, below */

    struct tcpcb *so_tcpcb; /* pointer to TCP protocol control block */
    unsigned so_expire; /* When the socket will expire */

    int so_queued; /* Number of packets queued from this socket */
    int so_nqueued; /* Number of packets queued in a row
                     * Used to determine when to "downgrade" a session
                     * from fastq to batchq */

    struct sbuf so_rcv; /* Receive buffer */
    struct sbuf so_snd; /* Send buffer */
};


/*
 * Socket state bits. (peer means the host on the Internet,
 * local host means the host on the other end of the modem)
 */
#define SS_NOFDREF 0x001 /* No fd reference */

#define SS_ISFCONNECTING \
    0x002 /* Socket is connecting to peer (non-blocking connect()'s) */
#define SS_ISFCONNECTED 0x004 /* Socket is connected to peer */
#define SS_FCANTRCVMORE \
    0x008 /* Socket can't receive more from peer (for half-closes) */
#define SS_FCANTSENDMORE \
    0x010 /* Socket can't send more to peer (for half-closes) */
#define SS_FWDRAIN \
    0x040 /* We received a FIN, drain data and set SS_FCANTSENDMORE */

#define SS_CTL 0x080
#define SS_FACCEPTCONN \
    0x100 /* Socket is accepting connections from a host on the internet */
#define SS_FACCEPTONCE \
    0x200 /* If set, the SS_FACCEPTCONN socket will die after one accept */

#define SS_PERSISTENT_MASK 0xf000 /* Unremovable state bits */
#define SS_HOSTFWD 0x1000 /* Socket describes host->guest forwarding */
#define SS_INCOMING \
    0x2000 /* Connection was initiated by a host on the internet */
#define SS_HOSTFWD_V6ONLY 0x4000 /* Only bind on v6 addresses */

/* Check that two addresses are equal */
static inline int sockaddr_equal(const struct sockaddr_storage *a,
                                 const struct sockaddr_storage *b)
{
    if (a->ss_family != b->ss_family) {
        return 0;
    }

    switch (a->ss_family) {
    case AF_INET: {
        const struct sockaddr_in *a4 = (const struct sockaddr_in *)a;
        const struct sockaddr_in *b4 = (const struct sockaddr_in *)b;
        return a4->sin_addr.s_addr == b4->sin_addr.s_addr &&
               a4->sin_port == b4->sin_port;
    }
    case AF_INET6: {
        const struct sockaddr_in6 *a6 = (const struct sockaddr_in6 *)a;
        const struct sockaddr_in6 *b6 = (const struct sockaddr_in6 *)b;
        return (in6_equal(&a6->sin6_addr, &b6->sin6_addr) &&
                a6->sin6_port == b6->sin6_port);
    }
#ifndef _WIN32
    case AF_UNIX: {
        const struct sockaddr_un *aun = (const struct sockaddr_un *)a;
        const struct sockaddr_un *bun = (const struct sockaddr_un *)b;
        return strncmp(aun->sun_path, bun->sun_path, sizeof(aun->sun_path)) == 0;
    }
#endif
    default:
        g_assert_not_reached();
    }

    return 0;
}

/* Get the size of an address */
static inline socklen_t sockaddr_size(const struct sockaddr_storage *a)
{
    switch (a->ss_family) {
    case AF_INET:
        return sizeof(struct sockaddr_in);
    case AF_INET6:
        return sizeof(struct sockaddr_in6);
#ifndef _WIN32
    case AF_UNIX:
        return sizeof(struct sockaddr_un);
#endif
    default:
        g_assert_not_reached();
    }
}

/* Copy an address */
static inline void sockaddr_copy(struct sockaddr *dst, socklen_t dstlen, const struct sockaddr *src, socklen_t srclen)
{
    socklen_t len = sockaddr_size((const struct sockaddr_storage *) src);
    g_assert(len <= srclen);
    g_assert(len <= dstlen);
    memcpy(dst, src, len);
}

/* Find the socket corresponding to lhost & fhost, trying last as a guess */
struct socket *solookup(struct socket **last, struct socket *head,
                        const struct sockaddr_storage *lhost,
                        const struct sockaddr_storage *fhost);
/* Create a new socket */
struct socket *socreate(Slirp *, int);
/* Release a socket */
void sofree(struct socket *);
/* Receive the available data from the Internet socket and queue it on the sb */
int soread(struct socket *);
/* Receive the available OOB data from the Internet socket and try to send it immediately */
int sorecvoob(struct socket *);
/* Send OOB data to the Internet socket */
int sosendoob(struct socket *);
/* Send data to the Internet socket */
int sowrite(struct socket *);
/* Receive the available data from the Internet UDP socket, and send it to the guest */
void sorecvfrom(struct socket *);
/* Send data to the Internet UDP socket */
int sosendto(struct socket *, struct mbuf *);
/* Listen for incoming TCPv4 connections on this haddr+hport */
struct socket *tcp_listen(Slirp *, uint32_t haddr, unsigned hport, uint32_t laddr, unsigned lport, int flags);
/*
 * Listen for incoming TCP connections on this haddr
 * On failure errno contains the reason.
 */
struct socket *tcpx_listen(Slirp *slirp,
                           const struct sockaddr *haddr, socklen_t haddrlen,
                           const struct sockaddr *laddr, socklen_t laddrlen,
                           int flags);
/* Note that the socket is connecting */
void soisfconnecting(register struct socket *);
/* Note that the socket is connected */
void soisfconnected(register struct socket *);
/*
 * Set write drain mode
 * Set CANTSENDMORE once all data has been write()n
 */
void sofwdrain(struct socket *);
struct iovec; /* For win32 */
/* Prepare iov for storing into the sb */
size_t sopreprbuf(struct socket *so, struct iovec *iov, int *np);
/* Get data from the buffer and queue it on the sb */
int soreadbuf(struct socket *so, const char *buf, int size);

/* Translate addr into host addr when it is a virtual address, before sending to the Internet */
int sotranslate_out(struct socket *, struct sockaddr_storage *);
/* Translate addr into virtual address when it is host, before sending to the guest */
void sotranslate_in(struct socket *, struct sockaddr_storage *);
/* Translate connections from localhost to the real hostname */
void sotranslate_accept(struct socket *);
/* Drop num bytes from the reading end of the socket */
void sodrop(struct socket *, int num);
/* Forwarding a connection to the guest, try to find the guest address to use, fill lhost with it */
int soassign_guest_addr_if_needed(struct socket *so);

#endif /* SLIRP_SOCKET_H */
