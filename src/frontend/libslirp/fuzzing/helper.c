#include "helper.h"
#include <glib.h>
#include <stdlib.h>
#include "../src/libslirp.h"
#include "../src/ip6.h"
#include "slirp_base_fuzz.h"

#define MIN_NUMBER_OF_RUNS 1
#define EXIT_TEST_SKIP 77

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
struct in6_addr ip6_host;
struct in6_addr ip6_dns;

/// Function to compute the checksum of the ip header, should be compatible with
/// TCP and UDP checksum calculation too.
uint16_t compute_checksum(uint8_t *Data, size_t Size)
{
    uint32_t sum = 0;
    uint16_t *Data_as_u16 = (uint16_t *)Data;

    for (size_t i = 0; i < Size / 2; i++) {
        uint16_t val = ntohs(*(Data_as_u16 + i));
        sum += val;
    }
    if (Size % 2 == 1)
        sum += Data[Size - 1] << 8;

    uint16_t carry = sum >> 16;
    uint32_t sum_val = carry + (sum & 0xFFFF);
    uint16_t result = (sum_val >> 16) + (sum_val & 0xFFFF);
    return ~result;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    /* FIXME: fail on some addr? */
    return 0;
}

int listen(int sockfd, int backlog)
{
    return 0;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    /* FIXME: fail on some addr? */
    return 0;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    /* FIXME: partial send? */
    return len;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen)
{
    /* FIXME: partial send? */
    return len;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    memset(buf, 0, len);
    return len / 2;
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen)
{
    memset(buf, 0, len);
    memset(src_addr, 0, *addrlen);
    return len / 2;
}

int setsockopt(int sockfd, int level, int optname, const void *optval,
               socklen_t optlen)
{
    return 0;
}

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
static void empty_logging_func(const gchar *log_domain,
                               GLogLevelFlags log_level, const gchar *message,
                               gpointer user_data)
{
}
#endif

/* Disables logging for oss-fuzz. Must be used with each target. */
static void fuzz_set_logging_func(void)
{
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    g_log_set_default_handler(empty_logging_func, NULL);
#endif
}

static ssize_t send_packet(const void *pkt, size_t pkt_len, void *opaque)
{
    return pkt_len;
}

static int64_t clock_get_ns(void *opaque)
{
    return 0;
}

static void *timer_new(SlirpTimerCb cb, void *cb_opaque, void *opaque)
{
    return NULL;
}

static void timer_mod(void *timer, int64_t expire_timer, void *opaque)
{
}

static void timer_free(void *timer, void *opaque)
{
}

static void guest_error(const char *msg, void *opaque)
{
}

static void register_poll_fd(int fd, void *opaque)
{
}

static void unregister_poll_fd(int fd, void *opaque)
{
}

static void notify(void *opaque)
{
}

static const SlirpCb slirp_cb = {
    .send_packet = send_packet,
    .guest_error = guest_error,
    .clock_get_ns = clock_get_ns,
    .timer_new = timer_new,
    .timer_mod = timer_mod,
    .timer_free = timer_free,
    .register_poll_fd = register_poll_fd,
    .unregister_poll_fd = unregister_poll_fd,
    .notify = notify,
};

#define MAX_EVID 1024
static int fake_events[MAX_EVID];

static int add_poll_cb(int fd, int events, void *opaque)
{
    g_assert(fd < G_N_ELEMENTS(fake_events));
    fake_events[fd] = events;
    return fd;
}

static int get_revents_cb(int idx, void *opaque)
{
    return fake_events[idx] & ~(SLIRP_POLL_ERR | SLIRP_POLL_HUP);
}

// Fuzzing strategy is the following : 
//  LLVMFuzzerTestOneInput :
//      - build a slirp instance,
//      - extract the packets from the pcap one by one,
//      - send the data to `slirp_input`
//      - call `slirp_pollfds_fill` and `slirp_pollfds_poll` to advance slirp
//      - cleanup slirp when the whole pcap has been unwrapped.
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    Slirp *slirp = NULL;
    struct in_addr net = { .s_addr = htonl(0x0a000200) }; /* 10.0.2.0 */
    struct in_addr mask = { .s_addr = htonl(0xffffff00) }; /* 255.255.255.0 */
    struct in_addr host = { .s_addr = htonl(0x0a000202) }; /* 10.0.2.2 */
    struct in_addr fwd = { .s_addr = htonl(0x0a000205) }; /* 10.0.2.5 */
    struct in_addr dhcp = { .s_addr = htonl(0x0a00020f) }; /* 10.0.2.15 */
    struct in_addr dns = { .s_addr = htonl(0x0a000203) }; /* 10.0.2.3 */
    struct in6_addr ip6_prefix;
    int ret, vprefix6_len = 64;
    const char *vhostname = NULL;
    const char *tftp_server_name = NULL;
    const char *tftp_export = "fuzzing/tftp";
    const char *bootfile = NULL;
    const char **dnssearch = NULL;
    const char *vdomainname = NULL;
    const pcap_hdr_t *hdr = (const void *)data;
    const pcaprec_hdr_t *rec = NULL;
    uint32_t timeout = 0;

    if (size < sizeof(pcap_hdr_t)) {
        return 0;
    }
    data += sizeof(*hdr);
    size -= sizeof(*hdr);

    if (hdr->magic_number == 0xd4c3b2a1) {
        g_debug("FIXME: byteswap fields");
        return 0;
    } /* else assume native pcap file */
    if (hdr->network != 1) {
        return 0;
    }

    setenv("SLIRP_FUZZING", "1", 0);

    fuzz_set_logging_func();

    ret = inet_pton(AF_INET6, "fec0::", &ip6_prefix);
    g_assert_cmpint(ret, ==, 1);

    ip6_host = ip6_prefix;
    ip6_host.s6_addr[15] |= 2;
    ip6_dns = ip6_prefix;
    ip6_dns.s6_addr[15] |= 3;

    slirp =
        slirp_init(false, true, net, mask, host, true, ip6_prefix, vprefix6_len,
                   ip6_host, vhostname, tftp_server_name, tftp_export, bootfile,
                   dhcp, dns, ip6_dns, dnssearch, vdomainname, &slirp_cb, NULL);

    slirp_add_exec(slirp, "cat", &fwd, 1234);


    for ( ; size > sizeof(*rec); data += rec->incl_len, size -= rec->incl_len) {
        rec = (const void *)data;
        data += sizeof(*rec);
        size -= sizeof(*rec);

        if (rec->incl_len != rec->orig_len) {
            g_debug("unsupported rec->incl_len != rec->orig_len");
            break;
        }
        if (rec->incl_len > size) {
            break;
        }

        if (rec->incl_len >= 14) {
            if (data[12] == 0x08 && data[13] == 0x00) {
                /* IPv4 */
                if (rec->incl_len >= 14 + 16) {
                    uint32_t ipsource = * (uint32_t*) (data + 14 + 12);

                    // This an answer, which we will produce, so don't receive
                    if (ipsource == htonl(0x0a000202) || ipsource == htonl(0x0a000203))
                        continue;
                }
            } else if (data[12] == 0x86 && data[13] ==  0xdd) {
                if (rec->incl_len >= 14 + 24) {
                    struct in6_addr *ipsource = (struct in6_addr *) (data + 14 + 8);

                    // This an answer, which we will produce, so don't receive
                    if (in6_equal(ipsource, &ip6_host) || in6_equal(ipsource, &ip6_dns))
                        continue;
                }
            }
        }

        slirp_input(slirp, data, rec->incl_len);
        slirp_pollfds_fill(slirp, &timeout, add_poll_cb, NULL);
        slirp_pollfds_poll(slirp, 0, get_revents_cb, NULL);
    }

    slirp_cleanup(slirp);

    return 0;
}
