/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2003-2008 Fabrice Bellard
 * Copyright (c) 2010-2019 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef UTIL_H_
#define UTIL_H_

#include <glib.h>

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#endif

#include "libslirp.h"

#ifdef __GNUC__
#define SLIRP_PACKED_BEGIN
#if defined(_WIN32) && (defined(__x86_64__) || defined(__i386__))
#define SLIRP_PACKED_END __attribute__((gcc_struct, packed))
#else
#define SLIRP_PACKED_END __attribute__((packed))
#endif
#elif defined(_MSC_VER)
#define SLIRP_PACKED_BEGIN __pragma(pack(push, 1))
#define SLIRP_PACKED_END __pragma(pack(pop))
#endif

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n, d) (((n) + (d)-1) / (d))
#endif

#ifndef container_of
#define container_of(ptr, type, member) \
        ((type *) (((char *)(ptr)) - offsetof(type, member)))
#endif

#ifndef G_SIZEOF_MEMBER
#define G_SIZEOF_MEMBER(type, member) sizeof(((type *)0)->member)
#endif

/* size_t, ssize_t format specifier. Windows, naturally, has to be different
 * and, despite implementing "%z", MinGW hasn't caught up. */
#if defined(__MINGW64__) || defined(_WIN64)
#  if defined(PRIu64)
#    define SLIRP_PRIsize_t PRIu64
#  else
#    define SLIRP_PRIsize_t "llu"
#  endif
#  if defined(PRId64)
#    define SLIRP_PRIssize_t PRId64
#  else
#    define SLIRP_PRIssize_t "lld"
#  endif
#elif defined(__MINGW32__) || defined(_WIN32)
#  if defined(PRIu32)
#    define SLIRP_PRIsize_t PRIu32
#  else
#    define SLIRP_PRIsize_t "lu"
#  endif
#  if defined(PRId32)
#    define SLIRP_PRIssize_t PRId32
#  else
#    define SLIRP_PRIssize_t "ld"
#  endif
#else
#define SLIRP_PRIsize_t "zu"
#define SLIRP_PRIssize_t "zd"
#endif

#if defined(_WIN32) /* CONFIG_IOVEC */
#if !defined(IOV_MAX) /* XXX: to avoid duplicate with QEMU osdep.h */
struct iovec {
    void *iov_base;
    size_t iov_len;
};
#endif
#else
#include <sys/uio.h>
#endif

#define stringify(s) tostring(s)
#define tostring(s) #s

#define SCALE_MS 1000000

#define ETH_ALEN 6
#define ETH_ADDRSTRLEN 18 /* "xx:xx:xx:xx:xx:xx", with trailing NUL */
#define ETH_HLEN 14
#define ETH_MINLEN 60
#define ETH_P_IP (0x0800) /* Internet Protocol packet  */
#define ETH_P_ARP (0x0806) /* Address Resolution packet */
#define ETH_P_IPV6 (0x86dd)
#define ETH_P_VLAN (0x8100)
#define ETH_P_DVLAN (0x88a8)
#define ETH_P_NCSI (0x88f8)
#define ETH_P_UNKNOWN (0xffff)


/* Windows: BLUF -- these functions have to have wrappers because Windows, just to
 * be Windows, has error constants that are not the same as <errno.h>. Consequently,
 * if one of the functions returns an error, the winsock2 error from WSAGetLastError()
 * needs to be translated.
 *
 * To make the problem more complex, we have to ensure to include <errno.h>, which
 * defines errno in a thread safe way. If we do not include <errno.h>, errno gets
 * defined as WSAGetLastError(), which makes assigning a translated value impossible.
 * Or ends up making the errno checking much more interesting than it has to be.
 */
#if defined(_WIN32)
#undef accept
#undef bind
#undef closesocket
#undef connect
#undef getpeername
#undef getsockname
#undef getsockopt
#undef ioctlsocket
#undef listen
#undef recv
#undef recvfrom
#undef send
#undef sendto
#undef setsockopt
#undef shutdown
#undef socket

#define connect slirp_connect_wrap
int slirp_connect_wrap(slirp_os_socket fd, const struct sockaddr *addr, int addrlen);
#define listen slirp_listen_wrap
int slirp_listen_wrap(slirp_os_socket fd, int backlog);
#define bind slirp_bind_wrap
int slirp_bind_wrap(slirp_os_socket fd, const struct sockaddr *addr, int addrlen);
#define socket slirp_socket_wrap
slirp_os_socket slirp_socket_wrap(int domain, int type, int protocol);
#define accept slirp_accept_wrap
slirp_os_socket slirp_accept_wrap(slirp_os_socket fd, struct sockaddr *addr, int *addrlen);
#define shutdown slirp_shutdown_wrap
int slirp_shutdown_wrap(slirp_os_socket fd, int how);
#define getpeername slirp_getpeername_wrap
int slirp_getpeername_wrap(slirp_os_socket fd, struct sockaddr *addr, int *addrlen);
#define getsockname slirp_getsockname_wrap
int slirp_getsockname_wrap(slirp_os_socket fd, struct sockaddr *addr, int *addrlen);
#define send slirp_send_wrap
slirp_ssize_t slirp_send_wrap(slirp_os_socket fd, const void *buf, size_t len, int flags);
#define sendto slirp_sendto_wrap
slirp_ssize_t slirp_sendto_wrap(slirp_os_socket fd, const void *buf, size_t len, int flags,
                          const struct sockaddr *dest_addr, int addrlen);
#define recv slirp_recv_wrap
slirp_ssize_t slirp_recv_wrap(slirp_os_socket fd, void *buf, size_t len, int flags);
#define recvfrom slirp_recvfrom_wrap
slirp_ssize_t slirp_recvfrom_wrap(slirp_os_socket fd, void *buf, size_t len, int flags,
                            struct sockaddr *src_addr, int *addrlen);
#define closesocket slirp_closesocket_wrap
int slirp_closesocket_wrap(slirp_os_socket fd);
#define ioctlsocket slirp_ioctlsocket_wrap
int slirp_ioctlsocket_wrap(slirp_os_socket fd, int req, void *val);
#define getsockopt slirp_getsockopt_wrap
int slirp_getsockopt_wrap(slirp_os_socket sockfd, int level, int optname, void *optval,
                          int *optlen);
#define setsockopt slirp_setsockopt_wrap
int slirp_setsockopt_wrap(slirp_os_socket sockfd, int level, int optname,
                          const void *optval, int optlen);
#define inet_aton slirp_inet_aton

#if WINVER < 0x0601
/* Windows versions older than Windows 7: */

#undef inet_pton
#undef inet_ntop

#define inet_pton slirp_inet_pton
#define inet_ntop slirp_inet_ntop

int slirp_inet_pton(int af, const char *src, void *dst);
const char *slirp_inet_ntop(int af, const void *src, char *dst, socklen_t size);
#endif

#else
#define closesocket(s) close(s)
#define ioctlsocket(s, r, v) ioctl(s, r, v)
#endif

slirp_os_socket slirp_socket(int domain, int type, int protocol);
void slirp_set_nonblock(slirp_os_socket fd);

static inline int slirp_socket_set_v6only(slirp_os_socket fd, int v)
{
    return setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (const void *) &v, sizeof(v));
}

static inline int slirp_socket_set_nodelay(slirp_os_socket fd)
{
    int v = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const void *) &v, sizeof(v));
}

static inline int slirp_socket_set_fast_reuse(slirp_os_socket fd)
{
#ifndef _WIN32
    int v = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
#else
#ifdef GLIB_UNUSED_PARAM
    GLIB_UNUSED_PARAM(fd);
#endif

    /* Enabling the reuse of an endpoint that was used by a socket still in
     * TIME_WAIT state is usually performed by setting SO_REUSEADDR. On Windows
     * fast reuse is the default and SO_REUSEADDR does strange things. So we
     * don't have to do anything here. More info can be found at:
     * http://msdn.microsoft.com/en-us/library/windows/desktop/ms740621.aspx */
    return 0;
#endif
}

/* Socket error check */
static inline int have_valid_socket(slirp_os_socket s)
{
#if !defined(_WIN32)
    return (s >= 0);
#else
    return (s != SLIRP_INVALID_SOCKET);
#endif
}

/* And the inverse -- the code reads more smoothly vs "!have_valid_socket" */
static inline int not_valid_socket(slirp_os_socket s)
{
    return !have_valid_socket(s);
}

void slirp_pstrcpy(char *buf, int buf_size, const char *str);

int slirp_fmt(char *str, size_t size, const char *format, ...) G_GNUC_PRINTF(3, 4);
int slirp_fmt0(char *str, size_t size, const char *format, ...) G_GNUC_PRINTF(3, 4);

/*
 * Pretty print a MAC address into out_str.
 * As a convenience returns out_str.
 */
const char *slirp_ether_ntoa(const uint8_t *addr, char *out_str,
                             size_t out_str_len);

#endif
