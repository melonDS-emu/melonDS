/* SPDX-License-Identifier: MIT */
/*
 * util.c (mostly based on QEMU os-win32.c)
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 * Copyright (c) 2010-2016 Red Hat, Inc.
 *
 * QEMU library functions for win32 which are shared between QEMU and
 * the QEMU tools.
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

/* slirp.h is not included here. However, if TARGET_WINVER is set,
 * ensure that WINVER and _WIN32_WINNT are also properly set.
 */
#include "winver.h"

#include <glib.h>
#include <fcntl.h>
#include <stdint.h>
/* Windows: If errno.h is not included, then errno is a preprocessor define
 * for WSAGetLastError(). errno.h redefines errno as a thread safe function,
 * i.e. "*errno()", which allows us to assign errno a value. */
#include <errno.h>

#include "util.h"
#include "debug.h"

#if defined(_WIN32)
int slirp_inet_aton(const char *cp, struct in_addr *ia)
{
    uint32_t addr;
    int valid_addr;

#if WINVER >= 0x0601
    valid_addr = inet_pton(AF_INET, cp, &addr) > 0;
#else
    addr = inet_addr(cp);
    valid_addr = (addr != 0xffffffff);
#endif

    if (valid_addr) {
        ia->s_addr = addr;
        return 1;
    }

    /* Invalid address. */
    return 0;
}

#if WINVER < 0x0601
/* Something older than Windows 7 and TARGET_WINVER obviously was set. There
 * are more than a few calls to inet_pton() and inet_ntop(), so provide suitable
 * stubs with renames. */

int slirp_inet_pton(int af, const char *src, void *dst)
{
  struct sockaddr_storage ss;
  int size = (int) sizeof(ss);
  char src_copy[INET6_ADDRSTRLEN + 1];

  ZeroMemory(&ss, sizeof(ss));
  strncpy (src_copy, src, INET6_ADDRSTRLEN);
  src_copy[INET6_ADDRSTRLEN] = '\0';

  if (WSAStringToAddress(src_copy, af, NULL, (struct sockaddr *)&ss, &size) == 0) {
    switch(af) {
      case AF_INET:
        *((struct in_addr *) dst) = ((const struct sockaddr_in *) &ss)->sin_addr;
        return 1;
      case AF_INET6:
        *((struct in6_addr *) dst) = ((const struct sockaddr_in6 *) &ss)->sin6_addr;
        return 1;
    }
  }

  return 0;
}

const char *slirp_inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
  struct sockaddr_storage ss;
  DWORD s = (DWORD) size;

  ZeroMemory(&ss, sizeof(ss));
  ss.ss_family = af;

  switch(af) {
    case AF_INET:
      ((struct sockaddr_in *) &ss)->sin_addr = *((const struct in_addr *) src);
      break;
    case AF_INET6:
      ((struct sockaddr_in6 *) &ss)->sin6_addr = *((const struct in6_addr *) src);
      break;
    default:
      return NULL;
  }

  return (WSAAddressToString((struct sockaddr *)&ss, sizeof(ss), NULL, dst, &s) == 0) ? dst : NULL;
}
#endif
#endif


void slirp_set_nonblock(slirp_os_socket fd)
{
#if !defined(_WIN32)
    int f;
    f = fcntl(fd, F_GETFL);
    assert(f != -1);
    f = fcntl(fd, F_SETFL, f | O_NONBLOCK);
    assert(f != -1);
#else
    unsigned long opt = 1;
    ioctlsocket(fd, FIONBIO, &opt);
#endif
}

static void slirp_set_cloexec(slirp_os_socket fd)
{
#if !defined(_WIN32)
    int f;
    f = fcntl(fd, F_GETFD);
    assert(f != -1);
    f = fcntl(fd, F_SETFD, f | FD_CLOEXEC);
    assert(f != -1);
#else
#ifdef GLIB_UNUSED_PARAM
    GLIB_UNUSED_PARAM(fd);
#endif
#endif
}

/*
 * Opens a socket with FD_CLOEXEC set
 * On failure errno contains the reason.
 */
slirp_os_socket slirp_socket(int domain, int type, int protocol)
{
    slirp_os_socket ret;

#ifdef SOCK_CLOEXEC
    ret = socket(domain, type | SOCK_CLOEXEC, protocol);
    if (ret != -1 || errno != EINVAL) {
        return ret;
    }
#endif
    ret = socket(domain, type, protocol);
    if (have_valid_socket(ret)) {
        slirp_set_cloexec(ret);
    }

    return ret;
}

#if defined(_WIN32)
static int win32_socket_error(void)
{
    switch (WSAGetLastError()) {
    case 0:
        return 0;
    case WSAEINTR:
        return EINTR;
    case WSAEINVAL:
        return EINVAL;
    case WSA_INVALID_HANDLE:
        return EBADF;
    case WSA_NOT_ENOUGH_MEMORY:
        return ENOMEM;
    case WSA_INVALID_PARAMETER:
        return EINVAL;
    case WSAENAMETOOLONG:
        return ENAMETOOLONG;
    case WSAENOTEMPTY:
        return ENOTEMPTY;
    case WSAEWOULDBLOCK:
        /* not using EWOULDBLOCK as we don't want code to have
         * to check both EWOULDBLOCK and EAGAIN */
        return EAGAIN;
    case WSAEINPROGRESS:
        return EINPROGRESS;
    case WSAEALREADY:
        return EALREADY;
    case WSAENOTSOCK:
        return ENOTSOCK;
    case WSAEDESTADDRREQ:
        return EDESTADDRREQ;
    case WSAEMSGSIZE:
        return EMSGSIZE;
    case WSAEPROTOTYPE:
        return EPROTOTYPE;
    case WSAENOPROTOOPT:
        return ENOPROTOOPT;
    case WSAEPROTONOSUPPORT:
        return EPROTONOSUPPORT;
    case WSAEOPNOTSUPP:
        return EOPNOTSUPP;
    case WSAEAFNOSUPPORT:
        return EAFNOSUPPORT;
    case WSAEADDRINUSE:
        return EADDRINUSE;
    case WSAEADDRNOTAVAIL:
        return EADDRNOTAVAIL;
    case WSAENETDOWN:
        return ENETDOWN;
    case WSAENETUNREACH:
        return ENETUNREACH;
    case WSAENETRESET:
        return ENETRESET;
    case WSAECONNABORTED:
        return ECONNABORTED;
    case WSAECONNRESET:
        return ECONNRESET;
    case WSAENOBUFS:
        return ENOBUFS;
    case WSAEISCONN:
        return EISCONN;
    case WSAENOTCONN:
        return ENOTCONN;
    case WSAETIMEDOUT:
        return ETIMEDOUT;
    case WSAECONNREFUSED:
        return ECONNREFUSED;
    case WSAELOOP:
        return ELOOP;
    case WSAEHOSTUNREACH:
        return EHOSTUNREACH;
    default:
        return EIO;
    }
}

#undef ioctlsocket
int slirp_ioctlsocket_wrap(slirp_os_socket fd, int req, void *val)
{
    int ret;
    ret = ioctlsocket(fd, req, val);
    if (ret == SOCKET_ERROR) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef closesocket
int slirp_closesocket_wrap(slirp_os_socket fd)
{
    int ret;
    ret = closesocket(fd);
    if (ret == SOCKET_ERROR) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef connect
int slirp_connect_wrap(slirp_os_socket sockfd, const struct sockaddr *addr, int addrlen)
{
    int ret;
    ret = connect(sockfd, addr, addrlen);
    if (ret == SOCKET_ERROR) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef listen
int slirp_listen_wrap(slirp_os_socket sockfd, int backlog)
{
    int ret;
    ret = listen(sockfd, backlog);
    if (ret == SOCKET_ERROR) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef bind
int slirp_bind_wrap(slirp_os_socket sockfd, const struct sockaddr *addr, int addrlen)
{
    int ret;
    ret = bind(sockfd, addr, addrlen);
    if (ret == SOCKET_ERROR) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef socket
slirp_os_socket slirp_socket_wrap(int domain, int type, int protocol)
{
    slirp_os_socket ret;
    ret = socket(domain, type, protocol);
    if (ret == INVALID_SOCKET) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef accept
slirp_os_socket slirp_accept_wrap(slirp_os_socket sockfd, struct sockaddr *addr, int *addrlen)
{
    slirp_os_socket ret;
    ret = accept(sockfd, addr, addrlen);
    if (ret == INVALID_SOCKET) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef shutdown
int slirp_shutdown_wrap(slirp_os_socket sockfd, int how)
{
    int ret;
    ret = shutdown(sockfd, how);
    if (ret == SOCKET_ERROR) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef getsockopt
int slirp_getsockopt_wrap(slirp_os_socket sockfd, int level, int optname, void *optval,
                          int *optlen)
{
    int ret;
    ret = getsockopt(sockfd, level, optname, optval, optlen);
    if (ret == SOCKET_ERROR) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef setsockopt
int slirp_setsockopt_wrap(slirp_os_socket sockfd, int level, int optname,
                          const void *optval, int optlen)
{
    int ret;
    ret = setsockopt(sockfd, level, optname, optval, optlen);
    if (ret == SOCKET_ERROR) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef getpeername
int slirp_getpeername_wrap(slirp_os_socket sockfd, struct sockaddr *addr, int *addrlen)
{
    int ret;
    ret = getpeername(sockfd, addr, addrlen);
    if (ret == SOCKET_ERROR) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef getsockname
int slirp_getsockname_wrap(slirp_os_socket sockfd, struct sockaddr *addr, int *addrlen)
{
    int ret;
    ret = getsockname(sockfd, addr, addrlen);
    if (ret == SOCKET_ERROR) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef send
slirp_ssize_t slirp_send_wrap(slirp_os_socket sockfd, const void *buf, size_t len, int flags)
{
    int ret;

    ret = send(sockfd, buf, len, flags);
    if (ret == SOCKET_ERROR) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef sendto
slirp_ssize_t slirp_sendto_wrap(slirp_os_socket sockfd, const void *buf, size_t len, int flags,
                                const struct sockaddr *addr, int addrlen)
{
    int ret;
    ret = sendto(sockfd, buf, len, flags, addr, addrlen);
    if (ret == SOCKET_ERROR) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef recv
slirp_ssize_t slirp_recv_wrap(slirp_os_socket sockfd, void *buf, size_t len, int flags)
{
    int ret;
    ret = recv(sockfd, buf, len, flags);
    if (ret == SOCKET_ERROR) {
        errno = win32_socket_error();
    }
    return ret;
}

#undef recvfrom
slirp_ssize_t slirp_recvfrom_wrap(slirp_os_socket sockfd, void *buf, size_t len, int flags,
                                  struct sockaddr *addr, int *addrlen)
{
    int ret;
    ret = recvfrom(sockfd, buf, len, flags, addr, addrlen);
    if (ret == SOCKET_ERROR) {
        errno = win32_socket_error();
    }
    return ret;
}
#endif /* WIN32 */

void slirp_pstrcpy(char *buf, int buf_size, const char *str)
{
    int c;
    char *q = buf;

    if (buf_size <= 0)
        return;

    for (;;) {
        c = *str++;
        if (c == 0 || q >= buf + buf_size - 1)
            break;
        *q++ = (char) c;
    }
    *q = '\0';
}

G_GNUC_PRINTF(3, 0)
static int slirp_vsnprintf(char *str, size_t size,
                           const char *format, va_list args)
{
    int rv = g_vsnprintf(str, size, format, args);

    if (rv < 0) {
        g_error("g_vsnprintf() failed: %s", g_strerror(errno));
    }

    return rv;
}

/*
 * A snprintf()-like function that:
 * - returns the number of bytes written (excluding optional \0-ending)
 * - dies on error
 * - warn on truncation
 */
int slirp_fmt(char *str, size_t size, const char *format, ...)
{
    va_list args;
    int rv;

    va_start(args, format);
    rv = slirp_vsnprintf(str, size, format, args);
    va_end(args);

    if (rv >= size) {
        g_critical("slirp_fmt() truncation");
    }

    return MIN(rv, size);
}

/*
 * A snprintf()-like function that:
 * - always \0-end (unless size == 0)
 * - returns the number of bytes actually written, including \0 ending
 * - dies on error
 * - warn on truncation
 */
int slirp_fmt0(char *str, size_t size, const char *format, ...)
{
    va_list args;
    int rv;

    va_start(args, format);
    rv = slirp_vsnprintf(str, size, format, args);
    va_end(args);

    if (rv >= size) {
        g_critical("slirp_fmt0() truncation");
        if (size > 0)
            str[size - 1] = '\0';
        rv = size;
    } else {
        rv += 1; /* include \0 */
    }

    return rv;
}

const char *slirp_ether_ntoa(const uint8_t *addr, char *out_str,
                             size_t out_str_size)
{
    assert(out_str_size >= ETH_ADDRSTRLEN);

    slirp_fmt0(out_str, out_str_size, "%02x:%02x:%02x:%02x:%02x:%02x",
               addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    return out_str;
}

/* Programatically set and reset debugging flags in slirp_debug vice
 * setting them via the SLIRP_DEBUG environment variable. */

void slirp_set_debug(unsigned int flags)
{
  slirp_debug |= flags;
}

void slirp_reset_debug(unsigned int flags)
{
  slirp_debug &= ~flags;
}
