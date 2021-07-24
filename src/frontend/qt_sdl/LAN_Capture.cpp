#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/time.h>

#ifdef __WIN32__
	#include <ws2tcpip.h>
#else
	#include <sys/socket.h>
	#include <netdb.h>
	#include <poll.h>
	#include <time.h>
#endif

#include "../types.h"
#include "LAN_Capture.h"

std::ofstream packetDumpOutput;

void LAN_Capture::CreatePacketDump(const char* filename)
{
    packetDumpOutput.open(filename);
    pcap_header fileHeader;

    fileHeader.magicNumber = htonl(0xA1B2C3D4);     // PCAP Magic number
    fileHeader.majorVersion = htons(2);             // https://tools.ietf.org/id/draft-gharris-opsawg-pcap-00.html
    fileHeader.minorVersion = htons(4);             // don't want to forget this
    fileHeader.reserved1 = htonl(0);                //
    fileHeader.reserved2 = htonl(0);                //
    fileHeader.snapLen = htonl(65535);              // max packet len
    fileHeader.linkType = htonl(105);               // LINKTYPE_IEEE802_11 / Wireless LAN (105)

    // Write the header in
    packetDumpOutput.write((char*)&fileHeader, sizeof(fileHeader));
}

void LAN_Capture::ClosePacketDump()
{
    packetDumpOutput.close();
}

int LAN_Capture::Write(u8* data, int len)
{
    pcap_packet_record packetRecord;
    timeval timestamp;

    if (gettimeofday(&timestamp, nullptr) != 0)
    {
        printf("LAN_Capture: could not get system timestamp\n");
        return -1;
    }

    packetRecord.timestampSeconds = htonl(timestamp.tv_sec);
    packetRecord.timestampMicroseconds = htonl(timestamp.tv_usec);
    packetRecord.capturedLen = htonl(len);
    packetRecord.wireLen = htonl(len);

    packetDumpOutput.write((char*)&packetRecord, sizeof(packetRecord));
    packetDumpOutput.write((char*)data, len);

    return 0;
}

bool LAN_Capture::IsOpen()
{
    return packetDumpOutput.is_open();
}