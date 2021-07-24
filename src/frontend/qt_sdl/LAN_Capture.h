#pragma once

#include <fstream>

#include "../types.h"

// shit
struct pcap_header
{
    u32 magicNumber;
    u16 majorVersion;
    u16 minorVersion;
    u32 reserved1;
    u32 reserved2;
    u32 snapLen;
    u32 linkType;
};

struct pcap_packet_record 
{
    u32 timestampSeconds;
    u32 timestampMicroseconds;
    u32 capturedLen;
    u32 wireLen;
};

namespace LAN_Capture
{
    int CreatePacketDump(const char* filename);
    int ClosePacketDump();

    int Write(u8* data, int len);
}