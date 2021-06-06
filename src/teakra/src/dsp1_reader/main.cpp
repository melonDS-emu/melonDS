#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <teakra/disassembler.h>
#include "../common_types.h"

class Dsp1 {
public:
    Dsp1(std::vector<u8> raw);

    struct Header {
        u8 signature[0x100];
        u8 magic[4];
        u32 binary_size;
        u16 memory_layout;
        u16 padding;
        u8 unknown;
        u8 filter_segment_type;
        u8 num_segments;
        u8 flags;
        u32 filter_segment_address;
        u32 filter_segment_size;
        u64 zero;
        struct Segment {
            u32 offset;
            u32 address;
            u32 size;
            u8 pa, pb, pc;
            u8 memory_type;
            u8 sha256[0x20];
        } segments[10];
    };

    static_assert(sizeof(Header) == 0x300);

    struct Segment {
        std::vector<u8> data;
        u8 memory_type;
        u32 target;
    };

    std::vector<Segment> segments;
    bool recv_data_on_start;
};

Dsp1::Dsp1(std::vector<u8> raw) {
    Header header;
    std::memcpy(&header, raw.data(), sizeof(header));

    recv_data_on_start = (header.flags & 1) != 0;

    printf("Memory layout = %04X\n", header.memory_layout);
    printf("Unk = %02X\n", header.unknown);
    printf("Filter segment type = %d\n", header.filter_segment_type);
    printf("Num segments = %d\n", header.num_segments);
    printf("Flags = %d\n", header.flags);
    printf("Filter address = 2 * %08X\n", header.filter_segment_address);
    printf("Filter size = %08X\n", header.filter_segment_size);

    for (u32 i = 0; i < header.num_segments; ++i) {
        Segment segment;
        segment.data =
            std::vector<u8>(raw.begin() + header.segments[i].offset,
                            raw.begin() + header.segments[i].offset + header.segments[i].size);
        segment.memory_type = header.segments[i].memory_type;
        segment.target = header.segments[i].address; /*header.segments[i].address * 2 +
             (segment.memory_type == 2 ? 0x1FF40000 : 0x1FF00000);*/
        segments.push_back(segment);

        printf("[Segment %d]\n", i);
        printf("memory_type = %d\n", segment.memory_type);
        printf("target = %08X\n", segment.target);
        printf("size = %08X\n", (u32)segment.data.size());
    }
}

int main(int argc, char** argv) {
    if (argc < 3)
        return -1;

    FILE* file = fopen(argv[1], "rb");
    std::vector<u8> raw;
    u8 ch;
    while (fread(&ch, 1, 1, file) == 1) {
        raw.push_back(ch);
    }
    fclose(file);
    Dsp1 dsp(raw);

    file = fopen(argv[2], "wt");

    for (const auto& segment : dsp.segments) {
        if (segment.memory_type == 0 || segment.memory_type == 1) {
            fprintf(file, "\n>>>>>>>> Segment <<<<<<<<\n\n");
            for (unsigned pos = 0; pos < segment.data.size(); pos += 2) {
                u16 opcode = segment.data[pos] | (segment.data[pos + 1] << 8);
                fprintf(file, "%08X  %04X         ", segment.target + pos / 2, opcode);
                bool expand = false;
                u16 expand_value = 0;
                if (Teakra::Disassembler::NeedExpansion(opcode)) {
                    expand = true;
                    pos += 2;
                    expand_value = segment.data[pos] | (segment.data[pos + 1] << 8);
                }
                std::string result = Teakra::Disassembler::Do(opcode, expand_value);
                fprintf(file, "%s\n", result.c_str());
                if (expand) {
                    fprintf(file, "%08X  %04X ^^^\n", segment.target + pos / 2, expand_value);
                }
            }
        }

        if (segment.memory_type == 2) {
            fprintf(file, "\n>>>>>>>> Data Segment <<<<<<<<\n\n");
            for (unsigned pos = 0; pos < segment.data.size(); pos += 2) {
                u16 opcode = segment.data[pos] | (segment.data[pos + 1] << 8);
                fprintf(file, "%08X  %04X\n", segment.target + pos / 2, opcode);
            }
        }
    }

    fclose(file);
}
