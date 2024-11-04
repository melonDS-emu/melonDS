#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include "../src/libslirp.h"
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
    uint32_t ipsource;
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

        if (rec->incl_len >= 14 + 16) {
            ipsource = * (uint32_t*) (ip_data + 12);

            // This an answer, which we will produce, so don't mutate
            if (ipsource == htonl(0x0a000202) || ipsource == htonl(0x0a000203))
                continue;
        }

        // Exclude packets that are not UDP from the mutation strategy
        if (ip_data[9] != IPPROTO_UDP)
            continue;

        uint8_t Data_to_mutate[MaxSize];
        uint8_t ip_hl = (ip_data[0] & 0xF);
        uint8_t ip_hl_in_bytes = ip_hl * 4; /* ip header length */

        // The size inside the packet can't be trusted, if it is too big it can
        // lead to heap overflows in the fuzzing code.
        // Fixme : don't use ip_hl_in_bytes inside the fuzzing code, maybe use the
        //         rec->incl_len and manually calculate the size.
        if (ip_hl_in_bytes > rec->incl_len - 14)
            return 0;

        uint8_t *start_of_udp = ip_data + ip_hl_in_bytes;
        uint16_t udp_size = ntohs(*(uint16_t *)(start_of_udp + 4));

        // The size inside the packet can't be trusted, if it is too big it can
        // lead to heap overflows in the fuzzing code.
        // Fixme : don't use udp_size inside the fuzzing code, maybe use the
        //         rec->incl_len and manually calculate the size.
        if (udp_size > MaxSize || udp_size > rec->incl_len - 14 - ip_hl_in_bytes)
            return 0;

        // Copy interesting data to the `Data_to_mutate` array
        // here we want to fuzz everything in the udp packet
        memset(Data_to_mutate, 0, MaxSize);
        memcpy(Data_to_mutate, start_of_udp, udp_size);

        // Call to libfuzzer's mutation function.
        // Pass the whole UDP packet, mutate it and then fix checksum value
        // so the packet isn't rejected.
        // The new size of the data is returned by LLVMFuzzerMutate.
        // Fixme: allow to change the size of the UDP packet, this will require
        //     to fix the size before calculating the new checksum and change
        //     how the Data_ptr is advanced.
        //     Most offsets bellow should be good for when the switch will be
        //     done to avoid overwriting new/mutated data.
        LLVMFuzzerMutate(Data_to_mutate, udp_size, udp_size);

        // Drop checksum
        *(uint16_t *)(Data_to_mutate + 6) = 0;

        // Copy the mutated data back to the `Data` array
        memcpy(start_of_udp, Data_to_mutate, udp_size);

        mutated = true;
    }

    if (!mutated)
        return 0;

    return Size;
}
#endif // CUSTOM_MUTATOR
