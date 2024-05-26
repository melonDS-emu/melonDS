/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 1995 Danny Gasparovski.
 */

#include "slirp.h"

static void ifs_insque(struct mbuf *ifm, struct mbuf *ifmhead)
{
    ifm->m_nextpkt = ifmhead->m_nextpkt;
    ifmhead->m_nextpkt = ifm;
    ifm->m_prevpkt = ifmhead;
    ifm->m_nextpkt->m_prevpkt = ifm;
}

void if_init(Slirp *slirp)
{
    slirp->if_fastq.qh_link = slirp->if_fastq.qh_rlink = &slirp->if_fastq;
    slirp->if_batchq.qh_link = slirp->if_batchq.qh_rlink = &slirp->if_batchq;
}

/*
 * if_output: Queue packet into an output queue.
 * There are 2 output queue's, if_fastq and if_batchq.
 * Each output queue is a doubly linked list of double linked lists
 * of mbufs, each list belonging to one "session" (socket).  This
 * way, we can output packets fairly by sending one packet from each
 * session, instead of all the packets from one session, then all packets
 * from the next session, etc.  Packets on the if_fastq get absolute
 * priority, but if one session hogs the link, it gets "downgraded"
 * to the batchq until it runs out of packets, then it'll return
 * to the fastq (eg. if the user does an ls -alR in a telnet session,
 * it'll temporarily get downgraded to the batchq)
 */
void if_output(struct socket *so, struct mbuf *ifm)
{
    Slirp *slirp = ifm->slirp;
    M_DUP_DEBUG(slirp, ifm, 0, 0);

    struct mbuf *ifq;
    int on_fastq = 1;

    DEBUG_CALL("if_output");
    DEBUG_ARG("so = %p", so);
    DEBUG_ARG("ifm = %p", ifm);

    /*
     * First remove the mbuf from m_usedlist,
     * since we're gonna use m_next and m_prev ourselves
     * XXX Shouldn't need this, gotta change dtom() etc.
     */
    if (ifm->m_flags & M_USEDLIST) {
        slirp_remque(ifm);
        ifm->m_flags &= ~M_USEDLIST;
    }

    /*
     * See if there's already a batchq list for this session.
     * This can include an interactive session, which should go on fastq,
     * but gets too greedy... hence it'll be downgraded from fastq to batchq.
     * We mustn't put this packet back on the fastq (or we'll send it out of
     * order)
     * XXX add cache here?
     */
    if (so) {
        for (ifq = (struct mbuf *)slirp->if_batchq.qh_rlink;
             (struct slirp_quehead *)ifq != &slirp->if_batchq;
	     ifq = ifq->m_prev) {
            if (so == ifq->m_so) {
                /* A match! */
                ifm->m_so = so;
                ifs_insque(ifm, ifq->m_prevpkt);
                goto diddit;
            }
        }
    }

    /* No match, check which queue to put it on */
    if (so && (so->so_iptos & IPTOS_LOWDELAY)) {
        ifq = (struct mbuf *)slirp->if_fastq.qh_rlink;
        on_fastq = 1;
        /*
         * Check if this packet is a part of the last
         * packet's session
         */
        if (ifq->m_so == so) {
            ifm->m_so = so;
            ifs_insque(ifm, ifq->m_prevpkt);
            goto diddit;
        }
    } else {
        ifq = (struct mbuf *)slirp->if_batchq.qh_rlink;
    }

    /* Create a new doubly linked list for this session */
    ifm->m_so = so;
    ifs_init(ifm);
    slirp_insque(ifm, ifq);

diddit:
    if (so) {
        /* Update *_queued */
        so->so_queued++;
        so->so_nqueued++;
        /*
         * Check if the interactive session should be downgraded to
         * the batchq.  A session is downgraded if it has queued 6
         * packets without pausing, and at least 3 of those packets
         * have been sent over the link
         * (XXX These are arbitrary numbers, probably not optimal..)
         */
        if (on_fastq &&
            ((so->so_nqueued >= 6) && (so->so_nqueued - so->so_queued) >= 3)) {
            /* Remove from current queue... */
            slirp_remque(ifm->m_nextpkt);

            /* ...And insert in the new.  That'll teach ya! */
            slirp_insque(ifm->m_nextpkt, &slirp->if_batchq);
        }
    }

    /*
     * This prevents us from malloc()ing too many mbufs
     */
    if_start(ifm->slirp);
}

void if_start(Slirp *slirp)
{
    uint64_t now = slirp->cb->clock_get_ns(slirp->opaque);
    bool from_batchq = false;
    struct mbuf *ifm, *ifm_next, *ifqt;

    DEBUG_VERBOSE_CALL("if_start");

    if (slirp->if_start_busy) {
        return;
    }
    slirp->if_start_busy = true;

    struct mbuf *batch_head = NULL;
    if (slirp->if_batchq.qh_link != &slirp->if_batchq) {
        batch_head = (struct mbuf *)slirp->if_batchq.qh_link;
    }

    if (slirp->if_fastq.qh_link != &slirp->if_fastq) {
        ifm_next = (struct mbuf *)slirp->if_fastq.qh_link;
    } else if (batch_head) {
        /* Nothing on fastq, pick up from batchq */
        ifm_next = batch_head;
        from_batchq = true;
    } else {
        ifm_next = NULL;
    }

    while (ifm_next) {
        ifm = ifm_next;

        ifm_next = ifm->m_next;
        if ((struct slirp_quehead *)ifm_next == &slirp->if_fastq) {
            /* No more packets in fastq, switch to batchq */
            ifm_next = batch_head;
            from_batchq = true;
        }
        if ((struct slirp_quehead *)ifm_next == &slirp->if_batchq) {
            /* end of batchq */
            ifm_next = NULL;
        }

        /* Try to send packet unless it already expired */
        if (ifm->expiration_date >= now && !if_encap(slirp, ifm)) {
            /* Packet is delayed due to pending ARP or NDP resolution */
            continue;
        }

        /* Remove it from the queue */
        ifqt = ifm->m_prev;
        slirp_remque(ifm);

        /* If there are more packets for this session, re-queue them */
        if (ifm->m_nextpkt != ifm) {
            struct mbuf *next = ifm->m_nextpkt;

            slirp_insque(next, ifqt);
            ifs_remque(ifm);
            if (!from_batchq) {
                ifm_next = next;
            }
        }

        /* Update so_queued */
        if (ifm->m_so && --ifm->m_so->so_queued == 0) {
            /* If there's no more queued, reset nqueued */
            ifm->m_so->so_nqueued = 0;
        }

        m_free(ifm);
    }

    slirp->if_start_busy = false;
}
