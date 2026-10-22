/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 1995 Danny Gasparovski.
 */

#include "slirp.h"

#include <limits.h>

static void sbappendsb(struct sbuf *sb, const struct mbuf *m);

void sbfree(struct sbuf *sb)
{
    g_free(sb->sb_data);
}

bool sbdrop(struct sbuf *sb, size_t num)
{
    uint32_t limit = sb->sb_datalen / 2;

    DEBUG_CALL("sbdrop");
    DEBUG_ARG("num = %" SLIRP_PRIsize_t, num);

    g_warn_if_fail(num <= sb->sb_cc);
    if (num > sb->sb_cc)
        num = sb->sb_cc;

    if (sizeof(num) > sizeof(int) && num > UINT32_MAX)
        g_error("sbdrop: sizeof(num) > sizeof(int), num = %" SLIRP_PRIsize_t "\n", num);

    sb->sb_cc -= (uint32_t) num;
    sb->sb_rptr += num;
    if (sb->sb_rptr >= sb->sb_data + sb->sb_datalen)
        sb->sb_rptr -= sb->sb_datalen;

    if (sb->sb_cc < limit && sb->sb_cc + num >= limit)
        return true;

    return false;
}

void sbreserve(struct sbuf *sb, size_t size)
{
    if (sizeof(size) > sizeof(uint32_t) && size > UINT32_MAX)
        g_error("sbreserve: sizeof(size) > sizeof(uint32_t), size = %" SLIRP_PRIsize_t "\n", size);

    sb->sb_wptr = sb->sb_rptr = sb->sb_data = g_realloc(sb->sb_data, size);
    sb->sb_cc = 0;
    sb->sb_datalen = (uint32_t) size;
}

void sbappend(struct socket *so, struct mbuf *m)
{
    int ret = 0;

    DEBUG_CALL("sbappend");
    DEBUG_ARG("so = %p", so);
    DEBUG_ARG("m = %p", m);
    DEBUG_ARG("m->m_len = %d", m->m_len);

    /* Shouldn't happen, but...  e.g. foreign host closes connection */
    if (m->m_len <= 0) {
        m_free(m);
        return;
    }

    /*
     * If there is urgent data, call sosendoob
     * if not all was sent, sowrite will take care of the rest
     * (The rest of this function is just an optimisation)
     */
    if (so->so_urgc) {
        sbappendsb(&so->so_rcv, m);
        m_free(m);
        sosendoob(so);
        return;
    }

    /*
     * We only write if there's nothing in the buffer,
     * otherwise it'll arrive out of order, and hence corrupt
     */
    if (!so->so_rcv.sb_cc) {
        slirp_ssize_t n_sent = slirp_send(so, m->m_data, m->m_len, 0);

        /* Almost impossible for this g_error() to execute, except in a OS error.  */
        if (n_sent > (slirp_ssize_t) m->m_len)
            g_error("sbappend: n_sent > m->m_len, n_sent = %" SLIRP_PRIssize_t "\n", n_sent);

        ret = (int) n_sent;
    }

    if (ret <= 0) {
        /*
         * Nothing was written
         * It's possible that the socket has closed, but
         * we don't need to check because if it has closed,
         * it will be detected in the normal way by soread()
         */
        sbappendsb(&so->so_rcv, m);
    } else if (ret != m->m_len) {
        /*
         * Something was written, but not everything..
         * sbappendsb the rest
         */
        m->m_len -= ret;
        m->m_data += ret;
        sbappendsb(&so->so_rcv, m);
    } /* else */
    /* Whatever happened, we free the mbuf */
    m_free(m);
}

/*
 * Copy the data from m into sb
 * The caller is responsible to make sure there's enough room
 */
static void sbappendsb(struct sbuf *sb, const struct mbuf *m)
{
    int len;
    slirp_ssize_t n;

    len = m->m_len;

    if (sb->sb_wptr < sb->sb_rptr) {
        n = sb->sb_rptr - sb->sb_wptr;
        if (n > len)
            n = len;
        memcpy(sb->sb_wptr, m->m_data, n);
    } else {
        /* Do the right edge first */
        n = sb->sb_data + sb->sb_datalen - sb->sb_wptr;
        if (n > len)
            n = len;
        memcpy(sb->sb_wptr, m->m_data, n);
        len -= n;
        if (len) {
            /* Now the left edge */
            slirp_ssize_t nn = sb->sb_rptr - sb->sb_data;
            if (nn > len)
                nn = len;
            memcpy(sb->sb_data, m->m_data + n, nn);
            n += nn;
        }
    }

    sb->sb_cc += (uint32_t) n;
    sb->sb_wptr += n;
    if (sb->sb_wptr >= sb->sb_data + sb->sb_datalen)
        sb->sb_wptr -= sb->sb_datalen;
}

void sbcopy(struct sbuf *sb, size_t off, size_t len, char *to)
{
    char *from;
    const char *right_edge = sb->sb_data + sb->sb_datalen;
    slirp_ssize_t ptr_diff = sb->sb_wptr - sb->sb_rptr;

    if (ptr_diff < 0)
        ptr_diff += sb->sb_datalen;

    DEBUG_CALL("sbcopy");
    DEBUG_ARG("len        = %" SLIRP_PRIsize_t, len);
    DEBUG_ARG("off        = %" SLIRP_PRIsize_t, off);
    DEBUG_ARG("sb->sb_cc  = %u", sb->sb_cc);
    DEBUG_ARG("ptr diff   = %" SLIRP_PRIssize_t, ptr_diff);

    /* Ensure that sb->sb_cc is consistent when the read and write pointers are
     * on top of each other: either the socket buffer is full or it's empty.
     *
     * Likewise, ensure that ptr_diff == sb->sb_cc and len + off <= sb->sb_cc. */
    if (ptr_diff == 0 && sb->sb_cc != 0 && sb->sb_cc != sb->sb_datalen) {
        g_error("sbcopy: ptr_diff == 0: sb->sb_cc (%" PRIu32 ") != sb->sb_datalen (%" PRIu32 "), sb->sb_cc != 0\n",
                   sb->sb_cc, sb->sb_datalen);
    } else if (ptr_diff != 0 && ptr_diff != (size_t) sb->sb_cc) {
        g_error("sbcopy: ptr_diff (%" SLIRP_PRIssize_t ") != sb->sb_cc (%" PRIu32 ")\n", ptr_diff, sb->sb_cc);
    } else if (len + off > (size_t) sb->sb_cc) {
        g_error("sbcopy: len (%" SLIRP_PRIsize_t ") + off (%" SLIRP_PRIsize_t ") > sb->sb_cc (%" PRIu32 ")\n",
                len, off, sb->sb_cc);
    }

    from = sb->sb_rptr + off;
    /* Jumped off the sbuf's right edge? Wrap around relative to the left edge
     * (i.e., modulo sb_datalen.) */
    if (from >= right_edge)
        from -= sb->sb_datalen;

    if (from < sb->sb_wptr) {
        /* Simple linear copy from the sbuf. */
        memcpy(to, from, len);
    } else {
        size_t n_right = MIN(len, (size_t) (right_edge - from));

        /* Copy the right side of the circular buffer. */
        memcpy(to, from, n_right);
        if (n_right < len) {
            /* Then the left side, if anything remains to be copied. */
            memcpy(to + n_right, sb->sb_data, len - n_right);
        }
    }
}
