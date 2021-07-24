#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/time.h>
#include <endian.h>

#include "../types.h"
#include "LAN_Capture.h"

std::ofstream packetDumpOutput;

int LAN_Capture::CreatePacketDump(const char* filename)
{
    packetDumpOutput.open(filename);
    pcap_header fileHeader;

    // i fucking hate this
    fileHeader.magicNumber = htobe32(0xA1B2C3D4);
    fileHeader.majorVersion = htobe16(2);
    fileHeader.minorVersion = htobe16(4);
    fileHeader.reserved1 = htobe32(0);
    fileHeader.reserved2 = htobe32(0);
    fileHeader.snapLen = htobe32(65535);
    fileHeader.linkType = htobe32(105);             // LINKTYPE_IEEE802_11 (105)

    packetDumpOutput.write((char*)&fileHeader, sizeof(fileHeader));

    return 0;
}

int LAN_Capture::ClosePacketDump()
{
    packetDumpOutput.close();

    return 0;
}

int LAN_Capture::Write(u8* data, int len)
{
    pcap_packet_record packetRecord;
    timeval timestamp;

    gettimeofday(&timestamp, nullptr);

    packetRecord.timestampSeconds = htobe32(timestamp.tv_sec);
    packetRecord.timestampMicroseconds = htobe32(timestamp.tv_usec);
    packetRecord.capturedLen = htobe32(len);
    packetRecord.wireLen = htobe32(len);

    packetDumpOutput.write((char*)&packetRecord, sizeof(packetRecord));
    packetDumpOutput.write((char*)data, len);
    printf("packet cap: wrote %#x worth of data\n", len);

    return 0;
}