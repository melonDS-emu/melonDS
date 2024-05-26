#ifndef _HELPER_H
#define _HELPER_H

#ifdef _WIN32
/* as defined in sdkddkver.h */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600 /* Vista */
#endif
#include <ws2tcpip.h>
#endif

#include <stdlib.h>
#include <stdint.h>
#include <netinet/in.h>

#define PSEUDO_IP_SIZE (4*2 + 4)
#define PSEUDO_IPV6_SIZE (16*2 + 4)

uint16_t compute_checksum(uint8_t *Data, size_t Size);

extern struct in6_addr ip6_host;
extern struct in6_addr ip6_dns;

#endif /* _HELPER_H */
