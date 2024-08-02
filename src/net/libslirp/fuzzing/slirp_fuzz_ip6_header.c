#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include "../src/libslirp.h"
#include "../src/ip6.h"
#include "helper.h"
#include "slirp_base_fuzz.h"

#ifdef CUSTOM_MUTATOR
extern size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);
size_t LLVMFuzzerCustomMutator(uint8_t *Data, size_t Size, size_t MaxSize, unsigned int Seed);

/// This is a custom mutator, this allows us to mutate only specific parts of
/// the input and fix the checksum so the packet isn't rejected for bad reasons.
extern size_t LLVMFuzzerCustomMutator(uint8_t *Data, size_t Size,
                                      size_t MaxSize, unsigned int Seed)
{
    size_t current_size = Size;
    uint8_t *Data_ptr = Data;
    uint8_t *ip_data;
    bool mutated = false;

    pcap_hdr_t *hdr = (void *)Data_ptr;
    pcaprec_hdr_t *rec = NULL;

    if (current_size < sizeof(pcap_hdr_t)) {
        return 0;
    }

    Data_ptr += sizeof(*hdr);
    current_size -= sizeof(*hdr);

    if (hdr->magic_number == 0xd4c3b2a1) {
        g_debug("FIXME: byteswap fields");
        return 0;
    } /* else assume native pcap file */
    if (hdr->network != 1) {
        return 0;
    }

    for ( ; current_size > sizeof(*rec); Data_ptr += rec->incl_len, current_size -= rec->incl_len) {
        rec = (void *)Data_ptr;
        Data_ptr += sizeof(*rec);
        current_size -= sizeof(*rec);

        if (rec->incl_len != rec->orig_len) {
            return 0;
        }
        if (rec->incl_len > current_size) {
            return 0;
        }
        if (rec->incl_len < 14 + 1) {
            return 0;
        }

        ip_data = Data_ptr + 14;

        if (rec->incl_len >= 14 + 24) {
            struct in6_addr *ipsource = (struct in6_addr *) (ip_data + 8);

            // This an answer, which we will produce, so don't receive
            if (in6_equal(ipsource, &ip6_host) || in6_equal(ipsource, &ip6_dns))
                continue;
        }

        uint8_t Data_to_mutate[MaxSize];
        uint8_t ip_hl_in_bytes = sizeof(struct ip6); /* ip header length */

        // Fixme : don't use ip_hl_in_bytes inside the fuzzing code, maybe use the
        //         rec->incl_len and manually calculate the size.
        if (ip_hl_in_bytes > rec->incl_len - 14)
            return 0;

        // Copy interesting data to the `Data_to_mutate` array
        // here we want to fuzz everything in the ip header, maybe the IPs or
        // total length should be excluded ?
        memset(Data_to_mutate, 0, MaxSize);
        memcpy(Data_to_mutate, ip_data, ip_hl_in_bytes);

        // Call to libfuzzer's mutation function.
        // For now we dont want to change the header size as it would require to
        // resize the `Data` array to include the new bytes inside the whole
        // packet.
        // This should be easy as LibFuzzer probably does it by itself or
        // reserved enough space in Data beforehand, needs some research to
        // confirm.
        // FIXME: allow up to grow header size to 60 bytes,
        //      requires to update the `header length` before calculating
        //      checksum
        LLVMFuzzerMutate(Data_to_mutate, ip_hl_in_bytes, ip_hl_in_bytes);

        // Copy the mutated data back to the `Data` array
        memcpy(ip_data, Data_to_mutate, ip_hl_in_bytes);

        mutated = true;
    }

    if (!mutated)
        return 0;

    return Size;
}
#endif // CUSTOM_MUTATOR
