#pragma once

#include <fstream>

#include "../types.h"

// Header for PCAP file
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

// Packet record for the PCAP file
struct pcap_packet_record 
{
    u32 timestampSeconds;
    u32 timestampMicroseconds;
    u32 capturedLen;
    u32 wireLen;
};

namespace LAN_Capture
{
    void CreatePacketDump(const char* filename);
    void ClosePacketDump();

    int Write(u8* data, int len);
    bool IsOpen();
}