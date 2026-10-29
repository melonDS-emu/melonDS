/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 1988, 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
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
 *      @(#)in_cksum.c  8.1 (Berkeley) 6/10/93
 * in_cksum.c,v 1.2 1994/08/02 07:48:16 davidg Exp
 */

#include <limits.h>
#include "slirp.h"

/*
 * Checksum routine adapted from NetBSD's implementation. Since we never span more
 * than one mbuf or start at a non-zero offset, we can optimise.
 *
 * This routine is very heavily used in the network code and should be modified
 * for each CPU to be as fast as possible.
 */

uint16_t cksum(struct mbuf *m, size_t len)
{
#if GLIB_SIZEOF_VOID_P == 8
    size_t mlen;
    uint64_t sum, partial;
    unsigned int final_acc;
    uint8_t *data;
    bool needs_swap, started_on_odd;

    started_on_odd = false;
    sum = 0;

    mlen = m->m_len;
    data = mtod(m, uint8_t *);
    if (mlen == 0)
        return ((uint16_t) ~0);
    if (mlen > len)
        mlen = len;
    if (len > mlen) {
        DEBUG_ERROR("cksum: mbuf data underrun (out of data, len > mlen)");
        DEBUG_ERROR(" len  = %" SLIRP_PRIsize_t, len);
        DEBUG_ERROR(" mlen = %" SLIRP_PRIsize_t, mlen);
    }

    partial = 0;
    if ((uintptr_t)data & 1) {
        /* Align on word boundary */
        started_on_odd = !started_on_odd;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        partial = *data << 8;
#else
        partial = *data;
#endif
        ++data;
        --mlen;
    }
    needs_swap = started_on_odd;
    if ((uintptr_t)data & 2) {
        if (mlen < 2)
            goto trailing_bytes;
        partial += *(uint16_t *)data;
        data += 2;
        mlen -= 2;
    }
    while (mlen >= 64) {
        partial += *(uint32_t *)data;
        partial += *(uint32_t *)(data + 4);
        partial += *(uint32_t *)(data + 8);
        partial += *(uint32_t *)(data + 12);
        partial += *(uint32_t *)(data + 16);
        partial += *(uint32_t *)(data + 20);
        partial += *(uint32_t *)(data + 24);
        partial += *(uint32_t *)(data + 28);
        partial += *(uint32_t *)(data + 32);
        partial += *(uint32_t *)(data + 36);
        partial += *(uint32_t *)(data + 40);
        partial += *(uint32_t *)(data + 44);
        partial += *(uint32_t *)(data + 48);
        partial += *(uint32_t *)(data + 52);
        partial += *(uint32_t *)(data + 56);
        partial += *(uint32_t *)(data + 60);
        data += 64;
        mlen -= 64;
        if (partial & (3ULL << 62)) {
            if (needs_swap)
                partial = (partial << 8) + (partial >> 56);
            sum += (partial >> 32);
            sum += (partial & 0xffffffff);
            partial = 0;
        }
    }
    /*
        * mlen is not updated below as the remaining tests
        * are using bit masks, which are not affected.
        */
    if (mlen & 32) {
        partial += *(uint32_t *)data;
        partial += *(uint32_t *)(data + 4);
        partial += *(uint32_t *)(data + 8);
        partial += *(uint32_t *)(data + 12);
        partial += *(uint32_t *)(data + 16);
        partial += *(uint32_t *)(data + 20);
        partial += *(uint32_t *)(data + 24);
        partial += *(uint32_t *)(data + 28);
        data += 32;
    }
    if (mlen & 16) {
        partial += *(uint32_t *)data;
        partial += *(uint32_t *)(data + 4);
        partial += *(uint32_t *)(data + 8);
        partial += *(uint32_t *)(data + 12);
        data += 16;
    }
    if (mlen & 8) {
        partial += *(uint32_t *)data;
        partial += *(uint32_t *)(data + 4);
        data += 8;
    }
    if (mlen & 4) {
        partial += *(uint32_t *)data;
        data += 4;
    }
    if (mlen & 2) {
        partial += *(uint16_t *)data;
        data += 2;
    }
trailing_bytes:
    if (mlen & 1) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        partial += *data;
#else
        partial += *data << 8;
#endif
        started_on_odd = !started_on_odd;
    }

    if (needs_swap)
        partial = (partial << 8) + (partial >> 56);

    sum += (partial >> 32) + (partial & 0xffffffff);
    /*
        * Reduce sum to allow potential byte swap
        * in the next iteration without carry.
        */
    sum = (sum >> 32) + (sum & 0xffffffff);

    final_acc = (sum >> 48) + ((sum >> 32) & 0xffff) +
        ((sum >> 16) & 0xffff) + (sum & 0xffff);
    final_acc = (final_acc >> 16) + (final_acc & 0xffff);
    final_acc = (final_acc >> 16) + (final_acc & 0xffff);

    return ((uint16_t) ~final_acc);
#else /* Assume 32-bit architecture */
    size_t mlen;
    uint32_t sum, partial;
    uint8_t *data;
    bool needs_swap, started_on_odd;

    needs_swap = false;
    started_on_odd = false;
    sum = 0;

    mlen = m->m_len;
    data = mtod(m, uint8_t *);
    if (mlen == 0)
        return ((uint16_t) ~0);

    if (mlen > len)
        mlen = len;
    if (len > mlen) {
        DEBUG_ERROR("cksum: mbuf data underrun (out of data, len > mlen)");
        DEBUG_ERROR(" len  = %" SLIRP_PRIsize_t, len);
        DEBUG_ERROR(" mlen = %" SLIRP_PRIsize_t, mlen);
    }

    partial = 0;
    if ((uintptr_t) data & 1) {
        /* Align on word boundary */
        started_on_odd = !started_on_odd;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        partial = *data << 8;
#else
        partial = *data;
#endif
        ++data;
        --mlen;
    }
    needs_swap = started_on_odd;
    while (mlen >= 32) {
        partial += *(uint16_t *)data;
        partial += *(uint16_t *)(data + 2);
        partial += *(uint16_t *)(data + 4);
        partial += *(uint16_t *)(data + 6);
        partial += *(uint16_t *)(data + 8);
        partial += *(uint16_t *)(data + 10);
        partial += *(uint16_t *)(data + 12);
        partial += *(uint16_t *)(data + 14);
        partial += *(uint16_t *)(data + 16);
        partial += *(uint16_t *)(data + 18);
        partial += *(uint16_t *)(data + 20);
        partial += *(uint16_t *)(data + 22);
        partial += *(uint16_t *)(data + 24);
        partial += *(uint16_t *)(data + 26);
        partial += *(uint16_t *)(data + 28);
        partial += *(uint16_t *)(data + 30);
        data += 32;
        mlen -= 32;
        if (partial & 0xc0000000) {
            if (needs_swap)
                partial = (partial << 8) + (partial >> 24);
            sum += (partial >> 16);
            sum += (partial & 0xffff);
            partial = 0;
        }
    }
    /*
     * mlen is not updated below as the remaining tests
     * are using bit masks, which are not affected.
     */
    if (mlen & 16) {
        partial += *(uint16_t *)data;
        partial += *(uint16_t *)(data + 2);
        partial += *(uint16_t *)(data + 4);
        partial += *(uint16_t *)(data + 6);
        partial += *(uint16_t *)(data + 8);
        partial += *(uint16_t *)(data + 10);
        partial += *(uint16_t *)(data + 12);
        partial += *(uint16_t *)(data + 14);
        data += 16;
    }
    if (mlen & 8) {
        partial += *(uint16_t *)data;
        partial += *(uint16_t *)(data + 2);
        partial += *(uint16_t *)(data + 4);
        partial += *(uint16_t *)(data + 6);
        data += 8;
    }
    if (mlen & 4) {
        partial += *(uint16_t *)data;
        partial += *(uint16_t *)(data + 2);
        data += 4;
    }
    if (mlen & 2) {
        partial += *(uint16_t *)data;
        data += 2;
    }
    if (mlen & 1) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        partial += *data;
#else
        partial += *data << 8;
#endif
        started_on_odd = !started_on_odd;
    }

    if (needs_swap)
        partial = (partial << 8) + (partial >> 24);

    sum += (partial >> 16) + (partial & 0xffff);
    /*
     * Reduce sum to allow potential byte swap
     * in the next iteration without carry.
     */
    sum = (sum >> 16) + (sum & 0xffff);
    return ((uint16_t) ~sum);
#endif
}

uint16_t ip6_cksum(struct mbuf *m)
{
    /* TODO: Optimize this by being able to pass the ip6_pseudohdr to cksum
     * separately from the mbuf */
    struct ip6 save_ip, *ip = mtod(m, struct ip6 *);
    struct ip6_pseudohdr *ih = mtod(m, struct ip6_pseudohdr *);
    uint16_t sum;

    save_ip = *ip;

    ih->ih_src = save_ip.ip_src;
    ih->ih_dst = save_ip.ip_dst;
    ih->ih_pl = htonl((uint32_t)ntohs(save_ip.ip_pl));
    ih->ih_zero_hi = 0;
    ih->ih_zero_lo = 0;
    ih->ih_nh = save_ip.ip_nh;

    sum = cksum(m, ((int)sizeof(struct ip6_pseudohdr)) + ntohl(ih->ih_pl));

    *ip = save_ip;

    return sum;
}
