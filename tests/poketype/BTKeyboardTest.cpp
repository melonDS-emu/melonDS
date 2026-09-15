/*
    Copyright 2016-2026 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <cstring>
#include <vector>

#include "NDSCart/BTKeyboard.h"
#include "BTKeyboardSupport.h"
#include "TestSupport.h"

using melonDS::u8;
using melonDS::u16;
using melonDS::u32;
using melonDS::NDSCart::BTKeyboard;

namespace
{

void testResetAnswersCommandComplete()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x0C03, {}));          // HCI_Reset
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 1);
    if (pkts.empty()) return;

    const std::vector<u8>& e = pkts[0];
    CHECK_EQ(e[0], 0x04);                   // HCI event packet
    CHECK_EQ(e[1], 0x0E);                   // Command Complete
    CHECK_EQ(e[2], 0x04);                   // parameter total length
    CHECK_EQ(e[3], 0x01);                   // command packets the host may send
    CHECK_EQ(e[4], 0x03);                   // opcode low
    CHECK_EQ(e[5], 0x0C);                   // opcode high
    CHECK_EQ(e[6], 0x00);                   // status: success
}

void testInquiryReportsAKeyboard()
{
    BTKeyboard kb;
    kb.Reset();

    // Inquiry as the game sends it: LAP 9E8B33, length 0x0A, unlimited responses
    Feed(kb, Command(0x0401, {0x33, 0x8B, 0x9E, 0x0A, 0x00}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 3);
    if (pkts.size() != 3) return;

    CHECK_EQ(pkts[0][1], 0x0F);             // Command Status
    CHECK_EQ(pkts[0][3], 0x00);             // status: success

    const std::vector<u8>& r = pkts[1];
    CHECK_EQ(r[1], 0x02);                   // Inquiry Result
    CHECK_EQ(r[2], 15);                     // parameter total length
    CHECK_EQ(r[3], 0x01);                   // one response
    CHECK_EQ(r[4], 0x33);                   // BD_ADDR, little-endian
    CHECK_EQ(r[9], 0x00);
    // class of device: the game filters on 000540 and drops anything else
    CHECK_EQ(r[13], 0x40);
    CHECK_EQ(r[14], 0x05);
    CHECK_EQ(r[15], 0x00);

    CHECK_EQ(pkts[2][1], 0x01);             // Inquiry Complete
    CHECK_EQ(pkts[2][3], 0x00);             // status: success
}

void testCreateConnectionReportsTheHandle()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x0405, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00,
                              0x18, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() != 2) return;

    CHECK_EQ(pkts[0][1], 0x0F);             // Command Status
    const std::vector<u8>& c = pkts[1];
    CHECK_EQ(c[1], 0x03);                   // Connection Complete
    CHECK_EQ(c[3], 0x00);                   // status: success
    CHECK_EQ(c[4], 0x42);                   // handle low
    CHECK_EQ(c[5], 0x00);                   // handle high
    CHECK_EQ(c[12], 0x01);                  // link type: ACL
}

void testClockOffsetVersionAndFeaturesComplete()
{
    BTKeyboard kb;
    kb.Reset();
    Feed(kb, Command(0x0405, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00,
                              0x18, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}));
    Drain(kb);

    Feed(kb, Command(0x041F, {0x42, 0x00}));    // Read Clock Offset
    auto off = Drain(kb);
    CHECK_EQ(off.size(), 2);
    if (off.size() == 2)
    {
        CHECK_EQ(off[0][1], 0x0F);              // Command Status
        CHECK_EQ(off[1][1], 0x1C);              // Read Clock Offset Complete
        CHECK_EQ(off[1][3], 0x00);              // status: success
        CHECK_EQ(off[1][4], 0x42);              // handle low
    }

    Feed(kb, Command(0x041D, {0x42, 0x00}));    // Read Remote Version Information
    auto ver = Drain(kb);
    CHECK_EQ(ver.size(), 2);
    if (ver.size() == 2)
    {
        CHECK_EQ(ver[1][1], 0x0C);              // Read Remote Version Complete
        CHECK_EQ(ver[1][3], 0x00);
        CHECK_EQ(ver[1][6], 0x04);              // LMP version: Bluetooth 2.1
    }

    Feed(kb, Command(0x041B, {0x42, 0x00}));    // Read Remote Supported Features
    auto feat = Drain(kb);
    CHECK_EQ(feat.size(), 2);
    if (feat.size() == 2)
    {
        CHECK_EQ(feat[1][1], 0x0B);             // Read Remote Features Complete
        CHECK_EQ(feat[1][3], 0x00);
        CHECK_EQ(feat[1][2], 11);               // parameter total length
    }
}

void testRemoteNameIsPaddedTo248Bytes()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x0419, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00,
                              0x01, 0x00, 0x00, 0x00}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() != 2) return;

    const std::vector<u8>& n = pkts[1];
    CHECK_EQ(n[1], 0x07);                   // Remote Name Request Complete
    CHECK_EQ(n[2], 255);                    // 1 status + 6 address + 248 name
    CHECK_EQ(n.size(), 258);
    CHECK_EQ(n[3], 0x00);                   // status: success
    CHECK_EQ(n[10], 'N');                   // name starts right after the address
}

void testAclIsAcknowledged()
{
    BTKeyboard kb;
    kb.Reset();

    // unknown CID: no reply, but the host's buffer must still be released
    Feed(kb, Acl(0x0099, {0xAA, 0xBB}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 1);
    if (pkts.empty()) return;

    const std::vector<u8>& e = pkts[0];
    CHECK_EQ(e[0], 0x04);
    CHECK_EQ(e[1], 0x13);                   // Number Of Completed Packets
    CHECK_EQ(e[2], 0x05);                   // parameter total length
    CHECK_EQ(e[3], 0x01);                   // one handle
    CHECK_EQ(e[4], 0x42);                   // handle low
    CHECK_EQ(e[5], 0x00);                   // handle high
    CHECK_EQ(e[6], 0x01);                   // one packet completed
    CHECK_EQ(e[7], 0x00);
}

void testAclWithAnOverlongL2capLengthIsDropped()
{
    BTKeyboard kb;
    kb.Reset();

    std::vector<u8> pkt;
    pkt.push_back(0x02);                            // HCI ACL data packet
    pkt.push_back(0x42);                            // handle low, PB/BC flags 0
    pkt.push_back(0x00);                            // handle high
    pkt.push_back(0x0A);                            // ACL data total length low (10)
    pkt.push_back(0x00);                            // ACL data total length high
    pkt.push_back(0x40);                            // L2CAP length low (claiming 0x0040)
    pkt.push_back(0x00);                            // L2CAP length high
    pkt.push_back(0x99);                            // L2CAP CID low
    pkt.push_back(0x00);                            // L2CAP CID high
    pkt.push_back(0xAA);                            // only 2 bytes of payload
    pkt.push_back(0xBB);

    Feed(kb, pkt);
    auto pkts = Drain(kb);

    // only the ACL ack: the malformed frame is not handled
    CHECK_EQ(pkts.size(), 1);
    if (pkts.empty()) return;

    const std::vector<u8>& e = pkts[0];
    CHECK_EQ(e[0], 0x04);
    CHECK_EQ(e[1], 0x13);                   // Number Of Completed Packets
    CHECK_EQ(e[2], 0x05);                   // parameter total length
    CHECK_EQ(e[3], 0x01);                   // one handle
    CHECK_EQ(e[4], 0x42);                   // handle low
    CHECK_EQ(e[5], 0x00);                   // handle high
    CHECK_EQ(e[6], 0x01);                   // one packet completed
    CHECK_EQ(e[7], 0x00);
}

void testL2capConnectAndConfigure()
{
    BTKeyboard kb;
    kb.Reset();

    // the game's Connection Request: code 02, id 03, length 4, PSM 0x0001 (SDP), SCID 0x0041
    Feed(kb, Acl(0x0001, {0x02, 0x03, 0x04, 0x00, 0x01, 0x00, 0x41, 0x00}));
    auto pkts = Drain(kb);

    // a Connection Response, our own Configuration Request, then the ACL ack
    CHECK_EQ(pkts.size(), 3);
    if (pkts.size() != 3) return;

    const std::vector<u8>& rsp = pkts[0];
    CHECK_EQ(rsp[0], 0x02);                 // ACL data
    CHECK_EQ(rsp[7], 0x01);                 // signalling CID
    CHECK_EQ(rsp[8], 0x00);
    CHECK_EQ(rsp[9], 0x03);                 // Connection Response
    CHECK_EQ(rsp[10], 0x03);                // same identifier as the request
    CHECK_EQ(rsp[13], 0x40);                // DCID: our first channel
    CHECK_EQ(rsp[14], 0x00);
    CHECK_EQ(rsp[15], 0x41);                // SCID: echoed back
    CHECK_EQ(rsp[16], 0x00);
    CHECK_EQ(rsp[17], 0x00);                // result: success
    CHECK_EQ(rsp[18], 0x00);

    CHECK_EQ(pkts[1][9], 0x04);             // Configuration Request
    CHECK_EQ(pkts[2][1], 0x13);             // Number Of Completed Packets

    // Configuration Request: code 04, id 05, length 4, DCID 0x0040, flags 0, no options
    Feed(kb, Acl(0x0001, {0x04, 0x05, 0x04, 0x00, 0x40, 0x00, 0x00, 0x00}));
    auto cfg = Drain(kb);

    CHECK_EQ(cfg.size(), 2);
    if (cfg.size() != 2) return;

    const std::vector<u8>& cr = cfg[0];
    CHECK_EQ(cr[9], 0x05);                  // Configuration Response
    CHECK_EQ(cr[10], 0x05);                 // same identifier
    CHECK_EQ(cr[13], 0x41);                 // SCID: the host's channel
    CHECK_EQ(cr[14], 0x00);
    CHECK_EQ(cr[17], 0x00);                 // result: success
    CHECK_EQ(cr[18], 0x00);
}

void testL2capConfigureForAnUnknownChannelIsRejected()
{
    BTKeyboard kb;
    kb.Reset();

    // Configuration Request: code 04, id 07, length 4, DCID 0x0055 (never opened), flags 0
    Feed(kb, Acl(0x0001, {0x04, 0x07, 0x04, 0x00, 0x55, 0x00, 0x00, 0x00}));
    auto pkts = Drain(kb);

    // a Command Reject, then the ACL ack
    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() != 2) return;

    const std::vector<u8>& rej = pkts[0];
    CHECK_EQ(rej.size(), 19);
    if (rej.size() != 19) return;

    CHECK_EQ(rej[9], 0x01);                 // Command Reject
    CHECK_EQ(rej[10], 0x07);                // same identifier
    CHECK_EQ(rej[11], 0x06);                // length
    CHECK_EQ(rej[12], 0x00);
    CHECK_EQ(rej[13], 0x02);                // reason: invalid CID in request
    CHECK_EQ(rej[14], 0x00);
    CHECK_EQ(rej[15], 0x55);                // local CID, as the request named it
    CHECK_EQ(rej[16], 0x00);
    CHECK_EQ(rej[17], 0x00);                // remote CID: unknown
    CHECK_EQ(rej[18], 0x00);

    CHECK_EQ(pkts[1][1], 0x13);             // Number Of Completed Packets
}

// Disconnection Request: code 06, length 4, DCID (ours), SCID (theirs)
std::vector<u8> DisconnectReq(u8 id, u16 dcid, u16 scid)
{
    return Acl(0x0001, {0x06, id, 0x04, 0x00,
                        (u8)(dcid & 0xFF), (u8)(dcid >> 8),
                        (u8)(scid & 0xFF), (u8)(scid >> 8)});
}

void testL2capDisconnectClosesTheChannel()
{
    BTKeyboard kb;
    kb.Reset();
    u16 local = OpenChannel(kb, 0x0001, 0x0041, 0x03);

    Feed(kb, DisconnectReq(0x09, local, 0x0041));
    auto pkts = Drain(kb);

    // a Disconnection Response, then the ACL ack
    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() != 2) return;

    const std::vector<u8>& rsp = pkts[0];
    CHECK_EQ(rsp.size(), 17);
    if (rsp.size() != 17) return;

    CHECK_EQ(rsp[9], 0x07);                 // Disconnection Response
    CHECK_EQ(rsp[10], 0x09);                // same identifier
    CHECK_EQ(rsp[13], (u8)(local & 0xFF));  // DCID: ours
    CHECK_EQ(rsp[14], (u8)(local >> 8));
    CHECK_EQ(rsp[15], 0x41);                // SCID: theirs
    CHECK_EQ(rsp[16], 0x00);

    // the channel is gone, so asking again is an invalid CID
    Feed(kb, DisconnectReq(0x0A, local, 0x0041));
    auto again = Drain(kb);
    CHECK(!again.empty() && again[0].size() > 9 && again[0][9] == 0x01);
}

void testL2capDisconnectForAnUnknownChannelIsRejected()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, DisconnectReq(0x0B, 0x0055, 0x0066));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);               // a Command Reject, then the ACL ack
    if (pkts.size() != 2) return;

    const std::vector<u8>& rej = pkts[0];
    CHECK_EQ(rej.size(), 19);
    if (rej.size() != 19) return;

    CHECK_EQ(rej[9], 0x01);                 // Command Reject
    CHECK_EQ(rej[10], 0x0B);
    CHECK_EQ(rej[13], 0x02);                // reason: invalid CID in request
    CHECK_EQ(rej[14], 0x00);
    CHECK_EQ(rej[15], 0x55);                // local CID: the request's DCID
    CHECK_EQ(rej[16], 0x00);
    CHECK_EQ(rej[17], 0x66);                // remote CID: the request's SCID
    CHECK_EQ(rej[18], 0x00);
}

void testL2capDisconnectWithTheWrongSourceCidIsIgnored()
{
    BTKeyboard kb;
    kb.Reset();
    u16 local = OpenChannel(kb, 0x0001, 0x0041, 0x03);

    Feed(kb, DisconnectReq(0x0C, local, 0x0099));
    auto pkts = Drain(kb);
    CHECK_EQ(pkts.size(), 1);               // only the ACL ack
    if (!pkts.empty()) CHECK_EQ(pkts[0][1], 0x13);

    // and the channel is still open
    Feed(kb, DisconnectReq(0x0D, local, 0x0041));
    auto closed = Drain(kb);
    CHECK(!closed.empty() && closed[0].size() > 9 && closed[0][9] == 0x07);
}

void testL2capInformationRequest()
{
    BTKeyboard kb;
    kb.Reset();

    // Information Request: code 0A, id 01, length 2, type 2 (extended features)
    Feed(kb, Acl(0x0001, {0x0A, 0x01, 0x02, 0x00, 0x02, 0x00}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() != 2) return;

    const std::vector<u8>& rsp = pkts[0];
    CHECK_EQ(rsp[9], 0x0B);                 // Information Response
    CHECK_EQ(rsp[10], 0x01);                // same identifier
    CHECK_EQ(rsp[13], 0x02);                // type echoed
    CHECK_EQ(rsp[15], 0x00);                // result: success
}

u16 OpenSDPChannel(BTKeyboard& kb)
{
    Feed(kb, Acl(0x0001, {0x02, 0x03, 0x04, 0x00, 0x01, 0x00, 0x41, 0x00}));
    auto pkts = Drain(kb);
    if (pkts.empty()) return 0;

    u16 local = (u16)(pkts[0][13] | (pkts[0][14] << 8));
    Feed(kb, Acl(0x0001, {0x04, 0x05, 0x04, 0x00,
                          (u8)(local & 0xFF), (u8)(local >> 8), 0x00, 0x00}));
    Drain(kb);
    return local;
}

// ServiceSearchAttributeRequest for one 16-bit UUID and one attribute range
std::vector<u8> SdpRequest(u16 tid, u16 uuid, u16 attrfirst, u16 attrlast,
                           u16 maxbytes, const std::vector<u8>& cont)
{
    std::vector<u8> params;
    params.push_back(0x35); params.push_back(0x03);             // sequence, 3 bytes
    params.push_back(0x19);                                     // UUID, 16-bit
    params.push_back((u8)(uuid >> 8)); params.push_back((u8)(uuid & 0xFF));
    params.push_back((u8)(maxbytes >> 8)); params.push_back((u8)(maxbytes & 0xFF));
    params.push_back(0x35); params.push_back(0x05);             // sequence, 5 bytes
    params.push_back(0x0A);                                     // unsigned int, 32-bit
    params.push_back((u8)(attrfirst >> 8)); params.push_back((u8)(attrfirst & 0xFF));
    params.push_back((u8)(attrlast >> 8)); params.push_back((u8)(attrlast & 0xFF));
    params.push_back((u8)cont.size());
    params.insert(params.end(), cont.begin(), cont.end());

    std::vector<u8> pdu;
    pdu.push_back(0x06);                                        // ServiceSearchAttributeRequest
    pdu.push_back((u8)(tid >> 8)); pdu.push_back((u8)(tid & 0xFF));
    pdu.push_back((u8)(params.size() >> 8));
    pdu.push_back((u8)(params.size() & 0xFF));
    pdu.insert(pdu.end(), params.begin(), params.end());
    return pdu;
}

// strips the ACL and L2CAP headers; used for HIDP frames too
std::vector<u8> SdpPayload(const std::vector<u8>& pkt)
{
    if (pkt.size() < 9) return {};
    return std::vector<u8>(pkt.begin() + 9, pkt.end());
}

void CheckBytesEqual(const std::vector<u8>& actual, const std::vector<u8>& expected)
{
    CHECK_EQ(actual.size(), expected.size());
    size_t n = actual.size() < expected.size() ? actual.size() : expected.size();
    for (size_t i = 0; i < n; i++)
        CHECK_EQ(actual[i], expected[i]);
}

std::vector<u8> ServiceSearchReq(u16 tid, u16 uuid, u16 maxrecords,
                                 const std::vector<u8>& cont)
{
    std::vector<u8> params;
    params.push_back(0x35); params.push_back(0x03);             // sequence, 3 bytes
    params.push_back(0x19);                                     // UUID, 16-bit
    params.push_back((u8)(uuid >> 8)); params.push_back((u8)(uuid & 0xFF));
    params.push_back((u8)(maxrecords >> 8)); params.push_back((u8)(maxrecords & 0xFF));
    params.push_back((u8)cont.size());
    params.insert(params.end(), cont.begin(), cont.end());

    std::vector<u8> pdu;
    pdu.push_back(0x02);                                        // ServiceSearchRequest
    pdu.push_back((u8)(tid >> 8)); pdu.push_back((u8)(tid & 0xFF));
    pdu.push_back((u8)(params.size() >> 8));
    pdu.push_back((u8)(params.size() & 0xFF));
    pdu.insert(pdu.end(), params.begin(), params.end());
    return pdu;
}

std::vector<u8> ServiceAttributeReq(u16 tid, u32 handle, u16 attrfirst, u16 attrlast,
                                    u16 maxbytes, const std::vector<u8>& cont)
{
    std::vector<u8> params;
    params.push_back((u8)(handle >> 24)); params.push_back((u8)(handle >> 16));
    params.push_back((u8)(handle >> 8));  params.push_back((u8)(handle & 0xFF));
    params.push_back((u8)(maxbytes >> 8)); params.push_back((u8)(maxbytes & 0xFF));
    params.push_back(0x35); params.push_back(0x05);             // sequence, 5 bytes
    params.push_back(0x0A);                                     // unsigned int, 32-bit
    params.push_back((u8)(attrfirst >> 8)); params.push_back((u8)(attrfirst & 0xFF));
    params.push_back((u8)(attrlast >> 8)); params.push_back((u8)(attrlast & 0xFF));
    params.push_back((u8)cont.size());
    params.insert(params.end(), cont.begin(), cont.end());

    std::vector<u8> pdu;
    pdu.push_back(0x04);                                        // ServiceAttributeRequest
    pdu.push_back((u8)(tid >> 8)); pdu.push_back((u8)(tid & 0xFF));
    pdu.push_back((u8)(params.size() >> 8));
    pdu.push_back((u8)(params.size() & 0xFF));
    pdu.insert(pdu.end(), params.begin(), params.end());
    return pdu;
}

// must match attribute 0x0000 (ServiceRecordHandle) of the keyboard's SDP record
static constexpr u32 ServiceRecordHandle = 0x00010000;

// The game's SDP ServiceSearchAttributeRequest:
//   06                  SDP_ServiceSearchAttributeRequest
//   00 00               transaction ID 0x0000
//   00 0D               parameter length 13
//   35 03 19 11 24      ServiceSearchPattern: DES(3), UUID16 0x1124 (HID)
//   00 F0               MaximumAttributeByteCount = 240
//   35 03 09 00 01      AttributeIDList: DES(3), UINT16 0x0001 (ServiceClassIDList)
//   00                  no continuation state
void testSdpAnswersTheCapturedRequest()
{
    BTKeyboard kb;
    kb.Reset();
    u16 local = OpenSDPChannel(kb);
    CHECK_EQ(local, 0x0040);

    Feed(kb, Acl(local, {0x06, 0x00, 0x00, 0x00, 0x0D,
                         0x35, 0x03, 0x19, 0x11, 0x24,
                         0x00, 0xF0,
                         0x35, 0x03, 0x09, 0x00, 0x01,
                         0x00}));
    auto pkts = Drain(kb);

    CHECK(pkts.size() >= 2);
    if (pkts.size() < 2) return;

    std::vector<u8> rsp = SdpPayload(pkts[0]);

    // PDU 0x07, TID 0x0000, 14-byte AttributeLists holding one list
    // (0x0001 ServiceClassIDList -> UUID16 0x1124), no continuation
    static const u8 expected[] = {
        0x07, 0x00, 0x00, 0x00, 0x11, 0x00, 0x0E, 0x36, 0x00, 0x0B,
        0x36, 0x00, 0x08, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19, 0x11, 0x24, 0x00
    };
    CheckBytesEqual(rsp, std::vector<u8>(expected, expected + sizeof(expected)));
}

void testSdpReturnsOnlyTheAttributesAsked()
{
    BTKeyboard kb;
    kb.Reset();
    OpenSDPChannel(kb);

    // HID service class, attribute 0x0001 only, plenty of room
    Feed(kb, Acl(0x0040, SdpRequest(0x0001, 0x1124, 0x0001, 0x0001, 0x02A0, {})));
    auto pkts = Drain(kb);

    CHECK(pkts.size() >= 2);
    if (pkts.size() < 2) return;

    std::vector<u8> rsp = SdpPayload(pkts[0]);
    CHECK_EQ(rsp[0], 0x07);                 // ServiceSearchAttributeResponse
    CHECK_EQ(rsp[1], 0x00);                 // transaction ID echoed
    CHECK_EQ(rsp[2], 0x01);
    CHECK_EQ(rsp.back(), 0x00);             // no continuation state

    // attribute 0x0001 and HID UUID 0x1124 present, nothing from 0x0004 (L2CAP UUID 0x0100)
    bool foundid = false, foundhid = false, foundl2cap = false;
    for (size_t i = 0; i + 2 < rsp.size(); i++)
    {
        if (rsp[i] == 0x09 && rsp[i + 1] == 0x00 && rsp[i + 2] == 0x01) foundid = true;
        if (rsp[i] == 0x19 && rsp[i + 1] == 0x11 && rsp[i + 2] == 0x24) foundhid = true;
        if (rsp[i] == 0x19 && rsp[i + 1] == 0x01 && rsp[i + 2] == 0x00) foundl2cap = true;
    }
    CHECK(foundid);
    CHECK(foundhid);
    CHECK(!foundl2cap);
}

void testSdpRejectsAServiceItDoesNotOffer()
{
    BTKeyboard kb;
    kb.Reset();
    OpenSDPChannel(kb);

    // 0x1108: Headset
    Feed(kb, Acl(0x0040, SdpRequest(0x0002, 0x1108, 0x0000, 0xFFFF, 0x02A0, {})));
    auto pkts = Drain(kb);

    CHECK(pkts.size() >= 2);
    if (pkts.size() < 2) return;

    std::vector<u8> rsp = SdpPayload(pkts[0]);
    CHECK_EQ(rsp[0], 0x07);
    CHECK_EQ(rsp[5], 0x00);                 // AttributeListsByteCount high
    CHECK_EQ(rsp[6], 0x02);                 // an empty sequence, two bytes
    CHECK_EQ(rsp[7], 0x35);                 // sequence
    CHECK_EQ(rsp[8], 0x00);                 // holding nothing
}

void testSdpContinuesAcrossRequests()
{
    BTKeyboard kb;
    kb.Reset();
    OpenSDPChannel(kb);

    // attributes 0x0001..0x0004 encode to 32 bytes, under the 43-byte chunk
    // clamp, so one response holds them all: the reference for the chunked read
    Feed(kb, Acl(0x0040, SdpRequest(0x0009, 0x1124, 0x0001, 0x0004, 0x00F0, {})));
    auto whole = Drain(kb);
    CHECK(whole.size() >= 2);
    if (whole.size() < 2) return;

    std::vector<u8> wholersp = SdpPayload(whole[0]);
    u16 wholecount = (u16)((wholersp[5] << 8) | wholersp[6]);
    CHECK_EQ(wholecount, 32);
    CHECK_EQ(wholersp[7 + wholecount], 0x00);           // fits in one shot: no continuation
    std::vector<u8> reference(wholersp.begin() + 7, wholersp.begin() + 7 + wholecount);

    // the same range, but only 20 bytes at a time
    Feed(kb, Acl(0x0040, SdpRequest(0x000A, 0x1124, 0x0001, 0x0004, 0x0014, {})));
    auto first = Drain(kb);

    CHECK(first.size() >= 2);
    if (first.size() < 2) return;

    std::vector<u8> rsp = SdpPayload(first[0]);
    u16 count = (u16)((rsp[5] << 8) | rsp[6]);
    CHECK_EQ(count, 0x0014);                // filled to the cap
    u8 contlen = rsp[7 + count];
    CHECK_EQ(contlen, 0x02);                // and there is more to come

    std::vector<u8> cont(rsp.begin() + 8 + count, rsp.end());
    CHECK_EQ(cont.size(), 2);
    u16 contvalue = (u16)((cont[0] << 8) | cont[1]);
    CHECK_EQ(contvalue, 0x0014);            // offset where chunk 1 stopped

    std::vector<u8> chunk1(rsp.begin() + 7, rsp.begin() + 7 + count);
    CheckBytesEqual(chunk1, std::vector<u8>(reference.begin(), reference.begin() + 0x0014));

    // asking again with that continuation state must resume, not repeat
    Feed(kb, Acl(0x0040, SdpRequest(0x000B, 0x1124, 0x0001, 0x0004, 0x0014, cont)));
    auto second = Drain(kb);

    CHECK(second.size() >= 2);
    if (second.size() < 2) return;

    std::vector<u8> rsp2 = SdpPayload(second[0]);
    CHECK_EQ(rsp2[1], 0x00);                // transaction ID echoed
    CHECK_EQ(rsp2[2], 0x0B);
    u16 count2 = (u16)((rsp2[5] << 8) | rsp2[6]);
    CHECK_EQ(count2, 32 - 0x0014);           // exactly the remainder
    CHECK_EQ(rsp2[7 + count2], 0x00);        // and now there is no more

    std::vector<u8> chunk2(rsp2.begin() + 7, rsp2.begin() + 7 + count2);
    CheckBytesEqual(chunk2, std::vector<u8>(reference.begin() + 0x0014, reference.end()));
}

void testSdpRejectsAnUnsupportedPdu()
{
    BTKeyboard kb;
    kb.Reset();
    OpenSDPChannel(kb);

    // PDU 0x08 is not a defined SDP request
    Feed(kb, Acl(0x0040, {0x08, 0x00, 0x09, 0x00, 0x00}));
    auto pkts = Drain(kb);

    CHECK(pkts.size() >= 2);
    if (pkts.size() < 2) return;

    std::vector<u8> rsp = SdpPayload(pkts[0]);
    CHECK_EQ(rsp[0], 0x01);                 // ErrorResponse
    CHECK_EQ(rsp[2], 0x09);                 // transaction ID echoed
    CHECK_EQ(rsp[5], 0x00);                 // error code 0x0003, high byte
    CHECK_EQ(rsp[6], 0x03);                 // invalid request syntax
}

// Read Buffer Size advertises a 57-byte ACL packet, so an SDP response chunk may
// hold at most 57 - 14 = 43 bytes, even though the game asks for 240. Attribute
// 0x0206 (the HID report descriptor) encodes to 81 bytes.
void testSdpClampsChunksToTheAdvertisedAclSize()
{
    BTKeyboard kb;
    kb.Reset();
    OpenSDPChannel(kb);

    static const u8 expected[] = {
        0x36, 0x00, 0x4E, 0x36, 0x00, 0x4B, 0x09, 0x02, 0x06, 0x35, 0x46, 0x35,
        0x44, 0x08, 0x22, 0x26, 0x00, 0x3F, 0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
        0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
        0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05,
        0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02, 0x95, 0x01,
        0x75, 0x03, 0x91, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
        0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0,
    };
    CHECK_EQ(sizeof(expected), 81);

    Feed(kb, Acl(0x0040, SdpRequest(0x0005, 0x1124, 0x0206, 0x0206, 0x00F0, {})));
    auto first = Drain(kb);
    CHECK(first.size() >= 2);
    if (first.size() < 2) return;

    std::vector<u8> rsp = SdpPayload(first[0]);
    u16 count = (u16)((rsp[5] << 8) | rsp[6]);
    CHECK(count <= 43);
    CHECK_EQ(count, 43);
    u8 contlen = rsp[7 + count];
    CHECK_EQ(contlen, 0x02);
    std::vector<u8> cont(rsp.begin() + 8 + count, rsp.end());
    CHECK_EQ(cont.size(), 2);
    u16 contvalue = (u16)((cont[0] << 8) | cont[1]);
    CHECK_EQ(contvalue, 43);

    std::vector<u8> reassembled(rsp.begin() + 7, rsp.begin() + 7 + count);

    Feed(kb, Acl(0x0040, SdpRequest(0x0006, 0x1124, 0x0206, 0x0206, 0x00F0, cont)));
    auto second = Drain(kb);
    CHECK(second.size() >= 2);
    if (second.size() < 2) return;

    std::vector<u8> rsp2 = SdpPayload(second[0]);
    u16 count2 = (u16)((rsp2[5] << 8) | rsp2[6]);
    CHECK(count2 <= 43);
    CHECK_EQ(count2, 38);                    // the rest of the 81 bytes
    CHECK_EQ(rsp2[7 + count2], 0x00);        // no continuation

    reassembled.insert(reassembled.end(), rsp2.begin() + 7, rsp2.begin() + 7 + count2);
    CheckBytesEqual(reassembled, std::vector<u8>(expected, expected + sizeof(expected)));
}

// without Disconnection Complete the game's driver never reconnects after its
// first SDP exchange
void testDisconnectAnswersWithCommandStatusThenDisconnectionComplete()
{
    BTKeyboard kb;
    kb.Reset();

    // as the game sends it: handle 0x0042 (LE), reason 0x13 (remote user terminated)
    Feed(kb, Command(0x0406, {0x42, 0x00, 0x13}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() != 2) return;

    CHECK_EQ(pkts[0][1], 0x0F);             // Command Status
    CHECK_EQ(pkts[0][3], 0x00);             // status: success
    CHECK_EQ(pkts[0][5], 0x06);             // opcode low
    CHECK_EQ(pkts[0][6], 0x04);             // opcode high

    const std::vector<u8>& ev = pkts[1];
    CHECK_EQ(ev[1], 0x05);                  // Disconnection Complete
    CHECK_EQ(ev[3], 0x00);                  // status: success
    CHECK_EQ(ev[4], 0x42);                  // handle low
    CHECK_EQ(ev[5], 0x00);                  // handle high
    CHECK_EQ(ev[6], 0x13);                  // reason echoed
}

void testReadRemoteExtendedFeaturesAnswersWithCommandStatusThenCompleteEvent()
{
    BTKeyboard kb;
    kb.Reset();

    // as the game sends it: handle 0x0042 (LE), page 0
    Feed(kb, Command(0x041C, {0x42, 0x00, 0x00}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() != 2) return;

    CHECK_EQ(pkts[0][1], 0x0F);             // Command Status
    CHECK_EQ(pkts[0][3], 0x00);             // status: success
    CHECK_EQ(pkts[0][5], 0x1C);             // opcode low
    CHECK_EQ(pkts[0][6], 0x04);             // opcode high

    const std::vector<u8>& ev = pkts[1];
    CHECK_EQ(ev[1], 0x23);                  // Read Remote Extended Features Complete
    CHECK_EQ(ev[3], 0x00);                  // status: success
    CHECK_EQ(ev[4], 0x42);                  // handle low
    CHECK_EQ(ev[5], 0x00);                  // handle high
    CHECK_EQ(ev[6], 0x00);                  // page number echoed
    CHECK_EQ(ev[7], 0x01);                  // maximum page number
    for (int i = 0; i < 8; i++)
        CHECK_EQ(ev[8 + i], 0x00);          // features: none, so no Secure Simple Pairing
}

// the game's second SDP request: UUID 0x1124, MaximumServiceRecordCount 0x0015
void testServiceSearchRequestFindsOurRecord()
{
    BTKeyboard kb;
    kb.Reset();
    OpenSDPChannel(kb);

    Feed(kb, Acl(0x0040, ServiceSearchReq(0x0000, 0x1124, 0x0015, {})));
    auto pkts = Drain(kb);

    CHECK(pkts.size() >= 2);
    if (pkts.size() < 2) return;

    std::vector<u8> rsp = SdpPayload(pkts[0]);
    static const u8 expected[] = {
        0x03, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00
    };
    CheckBytesEqual(rsp, std::vector<u8>(expected, expected + sizeof(expected)));
}

// zero records, and an empty (not omitted) handle list
void testServiceSearchRequestFindsNothingForAnUnknownService()
{
    BTKeyboard kb;
    kb.Reset();
    OpenSDPChannel(kb);

    // 0x1108: Headset
    Feed(kb, Acl(0x0040, ServiceSearchReq(0x0002, 0x1108, 0x0015, {})));
    auto pkts = Drain(kb);

    CHECK(pkts.size() >= 2);
    if (pkts.size() < 2) return;

    std::vector<u8> rsp = SdpPayload(pkts[0]);
    static const u8 expected[] = {
        0x03, 0x00, 0x02, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    CheckBytesEqual(rsp, std::vector<u8>(expected, expected + sizeof(expected)));
}

// unlike ServiceSearchAttributeResponse, the AttributeList is one 0x36 sequence,
// not a sequence of one
void testServiceAttributeRequestReturnsTheAttribute()
{
    BTKeyboard kb;
    kb.Reset();
    OpenSDPChannel(kb);

    Feed(kb, Acl(0x0040, ServiceAttributeReq(0x0007, ServiceRecordHandle,
                                             0x0001, 0x0001, 0x00F0, {})));
    auto pkts = Drain(kb);

    CHECK(pkts.size() >= 2);
    if (pkts.size() < 2) return;

    std::vector<u8> rsp = SdpPayload(pkts[0]);
    static const u8 expected[] = {
        0x05, 0x00, 0x07, 0x00, 0x0E, 0x00, 0x0B,
        0x36, 0x00, 0x08, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19, 0x11, 0x24, 0x00
    };
    CheckBytesEqual(rsp, std::vector<u8>(expected, expected + sizeof(expected)));
}

// SDP error 0x0002: invalid service record handle
void testServiceAttributeRequestRejectsAnUnknownHandle()
{
    BTKeyboard kb;
    kb.Reset();
    OpenSDPChannel(kb);

    Feed(kb, Acl(0x0040, ServiceAttributeReq(0x0008, 0x12345678,
                                             0x0000, 0xFFFF, 0x00F0, {})));
    auto pkts = Drain(kb);

    CHECK(pkts.size() >= 2);
    if (pkts.size() < 2) return;

    std::vector<u8> rsp = SdpPayload(pkts[0]);
    static const u8 expected[] = { 0x01, 0x00, 0x08, 0x00, 0x02, 0x00, 0x02 };
    CheckBytesEqual(rsp, std::vector<u8>(expected, expected + sizeof(expected)));
}

void testHidChannelsMakeTheKeyboardConnected()
{
    BTKeyboard kb;
    kb.Reset();

    CHECK(!kb.Connected());

    u16 ctrl = OpenChannel(kb, 0x0011, 0x0042, 0x10);
    CHECK(ctrl != 0);
    CHECK(!kb.Connected());                 // one channel is not a connection

    u16 intr = OpenChannel(kb, 0x0013, 0x0043, 0x20);
    CHECK(intr != 0);
    CHECK(kb.Connected());
}

void testGetReportReturnsTheCurrentInputReport()
{
    BTKeyboard kb;
    kb.Reset();

    u16 ctrl = OpenChannel(kb, 0x0011, 0x0042, 0x10);
    OpenChannel(kb, 0x0013, 0x0043, 0x20);

    // nothing held yet: GET_REPORT, report type 1 (input)
    Feed(kb, Acl(ctrl, {0x41}));
    auto idle = Drain(kb);

    CHECK(idle.size() >= 2);
    if (idle.size() < 2) return;

    std::vector<u8> rsp = SdpPayload(idle[0]);
    CHECK_EQ(rsp.size(), 9);
    CHECK_EQ(rsp[0], 0xA1);                 // DATA, input report
    CHECK_EQ(rsp[1], 0x00);                 // no modifiers
    CHECK_EQ(rsp[3], 0x00);                 // no key codes

    // SET_REPORT type 1: left shift held with A down
    Feed(kb, Acl(ctrl, {0x51, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}));
    auto set = Drain(kb);
    CHECK(set.size() >= 2);
    if (set.size() >= 2)
        CHECK_EQ(SdpPayload(set[0])[0], 0x00);      // HANDSHAKE, successful

    Feed(kb, Acl(ctrl, {0x41}));
    auto held = Drain(kb);

    CHECK(held.size() >= 2);
    if (held.size() < 2) return;

    std::vector<u8> back = SdpPayload(held[0]);
    CHECK_EQ(back.size(), 9);
    CHECK_EQ(back[0], 0xA1);
    CHECK_EQ(back[1], 0x02);                // left shift
    CHECK_EQ(back[3], 0x04);                // usage code for A
}

// The game asks for report ID 3, the battery status, although our report
// descriptor declares no IDs. A single 0xFF byte reads as a full battery.
void testGetReportForBatteryIdReturnsTheBatteryPayload()
{
    BTKeyboard kb;
    kb.Reset();

    u16 ctrl = OpenChannel(kb, 0x0011, 0x0042, 0x10);
    OpenChannel(kb, 0x0013, 0x0043, 0x20);

    // GET_REPORT, report type 1 (input), with a report ID byte: 3
    Feed(kb, Acl(ctrl, {0x41, 0x03}));
    auto pkts = Drain(kb);

    CHECK(pkts.size() >= 2);
    if (pkts.size() < 2) return;

    std::vector<u8> rsp = SdpPayload(pkts[0]);
    static const u8 expected[] = { 0xA1, 0x03, 0xFF };
    CheckBytesEqual(rsp, std::vector<u8>(expected, expected + sizeof(expected)));
}

// The game only sends GET_REPORT with a report ID; a request without one gets
// the boot report without one.
void testGetReportWithNoReportIdStillReturnsThePlainBootReport()
{
    BTKeyboard kb;
    kb.Reset();

    u16 ctrl = OpenChannel(kb, 0x0011, 0x0042, 0x10);
    OpenChannel(kb, 0x0013, 0x0043, 0x20);

    // GET_REPORT, report type 1 (input), one byte only -- no report ID
    Feed(kb, Acl(ctrl, {0x41}));
    auto pkts = Drain(kb);

    CHECK(pkts.size() >= 2);
    if (pkts.size() < 2) return;

    std::vector<u8> rsp = SdpPayload(pkts[0]);
    CHECK_EQ(rsp.size(), 9);
    CHECK_EQ(rsp[0], 0xA1);                 // DATA, input report
    for (int i = 1; i < 9; i++)
        CHECK_EQ(rsp[i], 0x00);             // all-zero boot report, no ID prefix
}

void testSetReportStoresTheLedState()
{
    BTKeyboard kb;
    kb.Reset();

    u16 ctrl = OpenChannel(kb, 0x0011, 0x0042, 0x10);
    OpenChannel(kb, 0x0013, 0x0043, 0x20);

    // SET_REPORT, report type 2 (output): caps lock LED on
    Feed(kb, Acl(ctrl, {0x52, 0x02}));
    auto set = Drain(kb);

    CHECK(set.size() >= 2);
    if (set.size() < 2) return;
    CHECK_EQ(SdpPayload(set[0])[0], 0x00);  // HANDSHAKE, successful

    // GET_REPORT, report type 2, hands it back
    Feed(kb, Acl(ctrl, {0x42}));
    auto get = Drain(kb);

    CHECK(get.size() >= 2);
    if (get.size() < 2) return;

    std::vector<u8> rsp = SdpPayload(get[0]);
    CHECK_EQ(rsp.size(), 2);
    CHECK_EQ(rsp[0], 0xA2);                 // DATA, output report
    CHECK_EQ(rsp[1], 0x02);                 // the LED byte that was set
}

void testProtocolModeRoundTrips()
{
    BTKeyboard kb;
    kb.Reset();

    u16 ctrl = OpenChannel(kb, 0x0011, 0x0042, 0x10);
    OpenChannel(kb, 0x0013, 0x0043, 0x20);

    // GET_PROTOCOL: boot protocol until told otherwise
    Feed(kb, Acl(ctrl, {0x60}));
    auto boot = Drain(kb);
    CHECK(boot.size() >= 2);
    if (boot.size() >= 2)
    {
        std::vector<u8> rsp = SdpPayload(boot[0]);
        CHECK_EQ(rsp[0], 0xA0);             // DATA, other
        CHECK_EQ(rsp[1], 0x00);             // boot protocol
    }

    // SET_PROTOCOL to report protocol
    Feed(kb, Acl(ctrl, {0x71}));
    auto set = Drain(kb);
    CHECK(set.size() >= 2);
    if (set.size() >= 2)
        CHECK_EQ(SdpPayload(set[0])[0], 0x00);

    Feed(kb, Acl(ctrl, {0x60}));
    auto report = Drain(kb);
    CHECK(report.size() >= 2);
    if (report.size() >= 2)
        CHECK_EQ(SdpPayload(report[0])[1], 0x01);   // report protocol
}

void testUnknownHidRequestIsRefused()
{
    BTKeyboard kb;
    kb.Reset();

    u16 ctrl = OpenChannel(kb, 0x0011, 0x0042, 0x10);
    OpenChannel(kb, 0x0013, 0x0043, 0x20);

    // transaction type 8 is not defined by HIDP
    Feed(kb, Acl(ctrl, {0x80}));
    auto pkts = Drain(kb);

    CHECK(pkts.size() >= 2);
    if (pkts.size() < 2) return;
    CHECK_EQ(SdpPayload(pkts[0])[0], 0x03); // HANDSHAKE, unsupported request
}

// The real keyboard is only discoverable when switched on with Fn held, as the
// game's pairing prompt asks. With auto-pair off, EnterPairingMode() is that Fn press.
void testInquiryFindsNothingWithAutoPairOffUntilFnIsHeld()
{
    BTKeyboard kb;
    kb.Reset();
    kb.SetAutoPair(false);

    Feed(kb, Command(0x0401, {0x33, 0x8B, 0x9E, 0x0A, 0x00}));
    auto pkts = Drain(kb);
    CHECK_EQ(pkts.size(), 1);               // Command Status only: the scan is running
    if (pkts.size() == 1) CHECK_EQ(pkts[0][1], 0x0F);

    // an empty scan lasts the Inquiry_Length asked for (0x0A x 1.28 s), seven 2 s
    // ticks; answering at once makes the game re-issue it thirty times a second
    for (int i = 0; i < 6; i++)
    {
        CHECK(!kb.InquiryTick());
        CHECK_EQ(Drain(kb).size(), 0);
    }
    CHECK(kb.InquiryTick());
    pkts = Drain(kb);
    CHECK_EQ(pkts.size(), 1);
    if (pkts.size() == 1)
    {
        CHECK_EQ(pkts[0][1], 0x01);         // Inquiry Complete
        CHECK_EQ(pkts[0][3], 0x00);         // status: success, just no result
    }
    CHECK(!kb.InquiryTick());               // and nothing further

    CHECK(kb.EnterPairingMode());           // the player holds Fn
    CHECK_EQ(Drain(kb).size(), 0);          // no scan in progress to answer

    Feed(kb, Command(0x0401, {0x33, 0x8B, 0x9E, 0x0A, 0x00}));
    pkts = Drain(kb);
    CHECK_EQ(pkts.size(), 3);
    if (pkts.size() == 3)
    {
        CHECK_EQ(pkts[1][1], 0x02);         // Inquiry Result
        CHECK_EQ(pkts[1][4], 0x33);         // our BD_ADDR, little-endian
    }
}

void testFnDuringAPendingInquiryAnswersItAtOnce()
{
    BTKeyboard kb;
    kb.Reset();
    kb.SetAutoPair(false);

    Feed(kb, Command(0x0401, {0x33, 0x8B, 0x9E, 0x0A, 0x00}));
    Drain(kb);
    CHECK(!kb.InquiryTick());               // one tick in, still scanning

    CHECK(kb.EnterPairingMode());
    auto pkts = Drain(kb);
    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() == 2)
    {
        CHECK_EQ(pkts[0][1], 0x02);         // Inquiry Result
        CHECK_EQ(pkts[0][4], 0x33);
        CHECK_EQ(pkts[1][1], 0x01);         // Inquiry Complete
    }

    // the timer must not complete the scan a second time
    for (int i = 0; i < 10; i++)
        CHECK(!kb.InquiryTick());
    CHECK_EQ(Drain(kb).size(), 0);
}

// after Inquiry_Cancel (0x0402) no Inquiry Complete is owed
void testInquiryCancelDropsThePendingScan()
{
    BTKeyboard kb;
    kb.Reset();
    kb.SetAutoPair(false);

    Feed(kb, Command(0x0401, {0x33, 0x8B, 0x9E, 0x0A, 0x00}));
    Drain(kb);
    Feed(kb, Command(0x0402, {}));
    auto pkts = Drain(kb);
    CHECK_EQ(pkts.size(), 1);
    if (pkts.size() == 1)
    {
        CHECK_EQ(pkts[0][1], 0x0E);         // Command Complete
        CHECK_EQ(pkts[0][4], 0x02);         // opcode 0x0402 echoed
        CHECK_EQ(pkts[0][5], 0x04);
    }

    for (int i = 0; i < 10; i++)
        CHECK(!kb.InquiryTick());
    CHECK_EQ(Drain(kb).size(), 0);
}

void testAutoPairMakesTheKeyboardDiscoverableFromTheStart()
{
    BTKeyboard kb;
    kb.Reset();
    kb.SetAutoPair(true);

    Feed(kb, Command(0x0401, {0x33, 0x8B, 0x9E, 0x0A, 0x00}));
    auto pkts = Drain(kb);
    CHECK_EQ(pkts.size(), 3);
    if (pkts.size() == 3) CHECK_EQ(pkts[1][1], 0x02);   // Inquiry Result
}

// auto-pair mirrors the user's config; the Fn gesture is per-session
void testAutoPairSurvivesResetButAPairingRequestDoesNot()
{
    BTKeyboard kb;
    kb.Reset();
    kb.SetAutoPair(false);
    CHECK(!kb.Discoverable());

    CHECK(kb.EnterPairingMode());
    CHECK(kb.Discoverable());

    kb.Reset();
    CHECK(!kb.Discoverable());              // Fn gesture forgotten, setting kept

    kb.SetAutoPair(true);
    kb.Reset();
    CHECK(kb.Discoverable());               // setting kept across reset
}

// once a link has been up, EnterPairingMode() returns false so the frontend
// handles Fn as a normal key
void testFnIsThePairingGestureOnlyUntilTheGameConnects()
{
    BTKeyboard kb;
    kb.Reset();
    kb.SetAutoPair(false);

    CHECK(kb.EnterPairingMode());

    Feed(kb, Command(0x0405, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00,
                              0x18, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}));
    Drain(kb);
    CHECK(!kb.EnterPairingMode());

    Feed(kb, Command(0x0406, {0x42, 0x00, 0x13}));   // Disconnect
    Drain(kb);
    CHECK(!kb.EnterPairingMode());          // sticky until reset

    kb.Reset();
    CHECK(kb.EnterPairingMode());
}

// a link counts whichever side brought it up
void testAnAcceptedPageEndsThePairingGestureToo()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x0C1A, {0x02}));      // page scan on, no link yet
    Drain(kb);

    CHECK(kb.PageTick());
    Drain(kb);
    Feed(kb, Command(0x0409, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00, 0x01}));   // Accept_Connection_Request
    Drain(kb);
    CHECK(!kb.EnterPairingMode());
}

// On a save that knows the keyboard, the game enables page scan and waits to be
// paged. Forcing its "keyboard present" flag in that window makes its driver skip
// the battery request, and the prompt then ignores every key.
void testGameHasKeyboardFromPageScanOnwards()
{
    BTKeyboard kb;
    kb.Reset();
    CHECK(!kb.GameHasKeyboard());

    Feed(kb, Command(0x0C1A, {0x02}));      // page scan on
    Drain(kb);
    CHECK(kb.GameHasKeyboard());

    Feed(kb, Command(0x0C1A, {0x00}));      // scans off, no link ever came up
    Drain(kb);
    CHECK(!kb.GameHasKeyboard());

    Feed(kb, Command(0x0C1A, {0x01}));      // inquiry scan only
    Drain(kb);
    CHECK(!kb.GameHasKeyboard());

    Feed(kb, Command(0x0C1A, {0x02}));
    Drain(kb);
    CHECK(kb.PageTick());
    Drain(kb);
    Feed(kb, Command(0x0409, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00, 0x01}));   // Accept_Connection_Request
    Drain(kb);
    Feed(kb, Command(0x0C1A, {0x00}));
    Drain(kb);
    CHECK(kb.GameHasKeyboard());            // sticky once a link has been up

    kb.Reset();
    CHECK(!kb.GameHasKeyboard());
}

// PageTick() never sends an input report of its own
void testNothingIsTypedOnTheGamesBehalf()
{
    BTKeyboard kb;
    kb.Reset();
    kb.SetAutoPair(true);

    OpenChannel(kb, 0x0011, 0x0042, 0x10);
    OpenChannel(kb, 0x0013, 0x0043, 0x20);
    CHECK(kb.Connected());
    Feed(kb, Command(0x0C1A, {0x02}));      // page scan on, so PageTick has work to do
    Drain(kb);

    int sent = 0;
    for (int i = 0; i < 500; i++)
        if (kb.PageTick()) sent++;
    CHECK(sent > 0);

    for (const auto& pkt : Drain(kb))
        CHECK(!(pkt[0] == 0x02 && pkt.size() > 9 && pkt[9] == 0xA1));   // no HIDP DATA input report
}

// The game authenticates once both HID channels are up, starting with
// Delete_Stored_Link_Key (0x0C12). No keys are stored, so none are deleted.
void testDeleteStoredLinkKeyReportsNoKeysDeleted()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x0C12, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00, 0x00}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 1);
    if (pkts.empty()) return;

    const std::vector<u8>& e = pkts[0];
    CHECK_EQ(e[1], 0x0E);                   // Command Complete
    CHECK_EQ(e[6], 0x00);                   // status: success
    CHECK_EQ(e[7], 0x00);                   // keys deleted, low
    CHECK_EQ(e[8], 0x00);                   // keys deleted, high
}

void testAuthenticationRequestedAsksForLinkKey()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x0411, {0x42, 0x00}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() != 2) return;

    CHECK_EQ(pkts[0][1], 0x0F);             // Command Status
    CHECK_EQ(pkts[0][3], 0x00);             // status: success
    CHECK_EQ(pkts[0][5], 0x11);             // opcode low
    CHECK_EQ(pkts[0][6], 0x04);             // opcode high

    const std::vector<u8>& ev = pkts[1];
    CHECK_EQ(ev[1], 0x17);                  // Link_Key_Request
    CHECK_EQ(ev[2], 6);                     // BD_ADDR, 6 bytes
    CHECK_EQ(ev[3], 0x33);                  // our BD_ADDR, little-endian
    CHECK_EQ(ev[8], 0x00);
}

// any link key the host offers is accepted
void testLinkKeyRequestReplyCompletesAuthentication()
{
    BTKeyboard kb;
    kb.Reset();

    std::vector<u8> params = {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00};
    for (int i = 0; i < 16; i++) params.push_back((u8)i);
    Feed(kb, Command(0x040B, params));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() != 2) return;

    const std::vector<u8>& cc = pkts[0];
    CHECK_EQ(cc[1], 0x0E);                  // Command Complete
    CHECK_EQ(cc[6], 0x00);                  // status: success
    CHECK_EQ(cc[7], 0x33);                  // BD_ADDR echoed

    const std::vector<u8>& ev = pkts[1];
    CHECK_EQ(ev[1], 0x06);                  // Authentication_Complete
    CHECK_EQ(ev[3], 0x00);                  // status: success
    CHECK_EQ(ev[4], 0x42);                  // handle low
    CHECK_EQ(ev[5], 0x00);                  // handle high
}

void testLinkKeyRequestNegativeReplyAsksForPin()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x040C, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() != 2) return;

    const std::vector<u8>& cc = pkts[0];
    CHECK_EQ(cc[1], 0x0E);                  // Command Complete
    CHECK_EQ(cc[6], 0x00);                  // status: success
    CHECK_EQ(cc[7], 0x33);                  // BD_ADDR echoed

    const std::vector<u8>& ev = pkts[1];
    CHECK_EQ(ev[1], 0x16);                  // PIN_Code_Request
    CHECK_EQ(ev[3], 0x33);                  // our BD_ADDR, little-endian
}

// any PIN is accepted
void testPinCodeRequestReplyNotifiesLinkKeyThenCompletesAuthentication()
{
    BTKeyboard kb;
    kb.Reset();

    std::vector<u8> params = {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00, 0x04};
    for (int i = 0; i < 16; i++) params.push_back((u8)('0' + i));
    Feed(kb, Command(0x040D, params));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 3);
    if (pkts.size() != 3) return;

    const std::vector<u8>& cc = pkts[0];
    CHECK_EQ(cc[1], 0x0E);                  // Command Complete
    CHECK_EQ(cc[6], 0x00);                  // status: success
    CHECK_EQ(cc[7], 0x33);                  // BD_ADDR echoed

    const std::vector<u8>& notify = pkts[1];
    CHECK_EQ(notify[1], 0x18);              // Link_Key_Notification
    CHECK_EQ(notify[2], 23);                // BD_ADDR(6) + key(16) + type(1)
    CHECK_EQ(notify[3], 0x33);              // our BD_ADDR
    CHECK_EQ(notify[25], 0x00);             // key type: combination key

    const std::vector<u8>& ev = pkts[2];
    CHECK_EQ(ev[1], 0x06);                  // Authentication_Complete
    CHECK_EQ(ev[3], 0x00);                  // status: success
    CHECK_EQ(ev[4], 0x42);                  // handle low
    CHECK_EQ(ev[5], 0x00);                  // handle high
}

void testPinCodeRequestNegativeReplyFailsAuthentication()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x040E, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() != 2) return;

    const std::vector<u8>& cc = pkts[0];
    CHECK_EQ(cc[1], 0x0E);                  // Command Complete
    CHECK_EQ(cc[6], 0x00);                  // status: success
    CHECK_EQ(cc[7], 0x33);                  // BD_ADDR echoed

    const std::vector<u8>& ev = pkts[1];
    CHECK_EQ(ev[1], 0x06);                  // Authentication_Complete
    CHECK_EQ(ev[3], 0x05);                  // status: authentication failure
    CHECK_EQ(ev[4], 0x42);                  // handle low
    CHECK_EQ(ev[5], 0x00);                  // handle high
}

void testFullNegativeReplyPairingPathCompletesAuthentication()
{
    BTKeyboard kb;
    kb.Reset();

    // Authentication_Requested: handle (2, LE)
    Feed(kb, Command(0x0411, {0x42, 0x00}));
    auto authReq = Drain(kb);
    CHECK_EQ(authReq.size(), 2);
    if (authReq.size() != 2) return;
    CHECK_EQ(authReq[0][1], 0x0F);          // Command Status
    CHECK_EQ(authReq[1][1], 0x17);          // Link_Key_Request

    // the host has no stored key: Link_Key_Request_Negative_Reply (0x040C)
    Feed(kb, Command(0x040C, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00}));
    auto negReply = Drain(kb);
    CHECK_EQ(negReply.size(), 2);
    if (negReply.size() != 2) return;
    CHECK_EQ(negReply[0][1], 0x0E);         // Command Complete
    CHECK_EQ(negReply[0][6], 0x00);         // status: success
    CHECK_EQ(negReply[1][1], 0x16);         // PIN_Code_Request

    // the host supplies a PIN: PIN_Code_Request_Reply (0x040D)
    std::vector<u8> pinParams = {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00, 0x04};
    for (int i = 0; i < 16; i++) pinParams.push_back((u8)('0' + i));
    Feed(kb, Command(0x040D, pinParams));
    auto pinReply = Drain(kb);

    CHECK_EQ(pinReply.size(), 3);
    if (pinReply.size() != 3) return;

    const std::vector<u8>& cc = pinReply[0];
    CHECK_EQ(cc[1], 0x0E);                  // Command Complete
    CHECK_EQ(cc[6], 0x00);                  // status: success
    CHECK_EQ(cc[7], 0x33);                  // BD_ADDR echoed

    const std::vector<u8>& notify = pinReply[1];
    CHECK_EQ(notify[1], 0x18);              // Link_Key_Notification
    CHECK_EQ(notify[2], 23);                // BD_ADDR(6) + key(16) + type(1)
    CHECK_EQ(notify[3], 0x33);              // our BD_ADDR
    CHECK_EQ(notify[25], 0x00);             // key type: combination key

    const std::vector<u8>& done = pinReply[2];
    CHECK_EQ(done[1], 0x06);                // Authentication_Complete
    CHECK_EQ(done[3], 0x00);                // status: success
    CHECK_EQ(done[4], 0x42);                // handle low
    CHECK_EQ(done[5], 0x00);                // handle high
}

void testSetConnectionEncryptionReportsEncryptionChange()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x0413, {0x42, 0x00, 0x01}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() != 2) return;

    CHECK_EQ(pkts[0][1], 0x0F);             // Command Status
    CHECK_EQ(pkts[0][3], 0x00);             // status: success

    const std::vector<u8>& ev = pkts[1];
    CHECK_EQ(ev[1], 0x08);                  // Encryption_Change
    CHECK_EQ(ev[3], 0x00);                  // status: success
    CHECK_EQ(ev[4], 0x42);                  // handle low
    CHECK_EQ(ev[5], 0x00);                  // handle high
    CHECK_EQ(ev[6], 0x01);                  // encryption enabled, echoed
}

// as the game sends it: handle 0x0042, max interval 0x00B4, min interval 0x0096,
// attempt 4, timeout 4
void testSniffModeReportsModeChange()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x0803, {0x42, 0x00, 0xB4, 0x00, 0x96, 0x00, 0x04, 0x00, 0x04, 0x00}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() != 2) return;

    CHECK_EQ(pkts[0][1], 0x0F);             // Command Status
    CHECK_EQ(pkts[0][3], 0x00);             // status: success

    const std::vector<u8>& ev = pkts[1];
    CHECK_EQ(ev[1], 0x14);                  // Mode_Change
    CHECK_EQ(ev[3], 0x00);                  // status: success
    CHECK_EQ(ev[4], 0x42);                  // handle low
    CHECK_EQ(ev[5], 0x00);                  // handle high
    CHECK_EQ(ev[6], 0x02);                  // current mode: sniff
    CHECK_EQ(ev[7], 0xB4);                  // interval: max interval echoed, low
    CHECK_EQ(ev[8], 0x00);                  // interval: max interval echoed, high
}

// The game polls Read_RSSI every ten seconds while linked and reads the handle
// and RSSI back as the keyboard's battery/signal state. 0x00 means inside the golden
// receive power range.
void testReadRssiReportsAHealthyLink()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x1405, {0x42, 0x00}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 1);
    if (pkts.size() != 1) return;

    const std::vector<u8>& e = pkts[0];
    CHECK_EQ(e[0], 0x04);                   // HCI event packet
    CHECK_EQ(e[1], 0x0E);                   // Command Complete
    CHECK_EQ(e[2], 0x07);                   // parameter total length: 3 + 4 return params
    CHECK_EQ(e[3], 0x01);                   // command packets the host may send
    CHECK_EQ(e[4], 0x05);                   // opcode low
    CHECK_EQ(e[5], 0x14);                   // opcode high
    CHECK_EQ(e[6], 0x00);                   // status: success
    CHECK_EQ(e[7], 0x42);                   // handle low
    CHECK_EQ(e[8], 0x00);                   // handle high
    CHECK_EQ(e[9], 0x00);                   // RSSI: 0 dB from the golden range
}

// After registration the game drops the link and enables page scan, waiting for
// the bonded keyboard to reconnect. PageTick() pages it, but never while a link is up.
void testNoConnectionRequestWhileLinkIsUp()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x0405, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00,
                              0x18, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}));
    Drain(kb);

    Feed(kb, Command(0x0C1A, {0x02}));      // Write_Scan_Enable: page scan on
    Drain(kb);

    CHECK(!kb.PageTick());
    CHECK_EQ(Drain(kb).size(), 0);
}

void testPageScanAfterDisconnectSendsConnectionRequest()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x0405, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00,
                              0x18, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}));
    Drain(kb);

    Feed(kb, Command(0x0406, {0x42, 0x00, 0x13}));   // Disconnect
    Drain(kb);

    Feed(kb, Command(0x0C1A, {0x02}));               // Write_Scan_Enable: page scan on
    Drain(kb);

    CHECK(kb.PageTick());

    auto pkts = Drain(kb);
    CHECK_EQ(pkts.size(), 1);
    if (pkts.empty()) return;

    const std::vector<u8>& ev = pkts[0];
    CHECK_EQ(ev[0], 0x04);                  // HCI event packet
    CHECK_EQ(ev[1], 0x04);                  // Connection_Request
    CHECK_EQ(ev[2], 10);                    // parameter total length
    CHECK_EQ(ev[3], 0x33);                  // BD_ADDR, little-endian
    CHECK_EQ(ev[4], 0x22);
    CHECK_EQ(ev[5], 0x11);
    CHECK_EQ(ev[6], 0x32);
    CHECK_EQ(ev[7], 0x1F);
    CHECK_EQ(ev[8], 0x00);
    CHECK_EQ(ev[9], 0x40);                  // class of device
    CHECK_EQ(ev[10], 0x05);
    CHECK_EQ(ev[11], 0x00);
    CHECK_EQ(ev[12], 0x01);                 // link type: ACL
}

// 0x0409 only ever answers our own page, so the keyboard opens both HID channels itself
void testAcceptConnectionRequestCompletesTheConnection()
{
    BTKeyboard kb;
    kb.Reset();

    // BD_ADDR (6), role (1) -- 0x01, remain slave
    Feed(kb, Command(0x0409, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00, 0x01}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 4);
    if (pkts.size() != 4) return;

    CHECK_EQ(pkts[0][1], 0x0F);             // Command Status
    CHECK_EQ(pkts[0][3], 0x00);             // status: success
    CHECK_EQ(pkts[0][5], 0x09);             // opcode low
    CHECK_EQ(pkts[0][6], 0x04);             // opcode high

    const std::vector<u8>& c = pkts[1];
    CHECK_EQ(c[1], 0x03);                   // Connection Complete
    CHECK_EQ(c[3], 0x00);                   // status: success
    CHECK_EQ(c[4], 0x42);                   // handle low
    CHECK_EQ(c[5], 0x00);                   // handle high
    CHECK_EQ(c[12], 0x01);                  // link type: ACL

    CHECK_EQ(pkts[2][9], 0x02);             // Connection Request: HID control
    CHECK_EQ(pkts[2][13], 0x11);
    CHECK_EQ(pkts[2][14], 0x00);

    CHECK_EQ(pkts[3][9], 0x02);             // Connection Request: HID interrupt
    CHECK_EQ(pkts[3][13], 0x13);
    CHECK_EQ(pkts[3][14], 0x00);
}

void testRejectConnectionRequestFailsTheConnection()
{
    BTKeyboard kb;
    kb.Reset();

    // BD_ADDR (6), reason (1) -- 0x04, page timeout
    Feed(kb, Command(0x040A, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00, 0x04}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);
    if (pkts.size() != 2) return;

    CHECK_EQ(pkts[0][1], 0x0F);             // Command Status
    CHECK_EQ(pkts[0][5], 0x0A);             // opcode low
    CHECK_EQ(pkts[0][6], 0x04);             // opcode high

    const std::vector<u8>& c = pkts[1];
    CHECK_EQ(c[1], 0x03);                   // Connection Complete
    CHECK_EQ(c[3], 0x04);                   // status: the reason, not success
}

// the DS never answers our page
void testPagingRetryIsBounded()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x0C1A, {0x02}));      // Write_Scan_Enable: page scan on
    Drain(kb);

    int sent = 0;
    for (int i = 0; i < 100; i++)
        if (kb.PageTick())
            sent++;

    CHECK(sent > 0);
    CHECK(sent < 100);

    CHECK(!kb.PageTick());

    auto pkts = Drain(kb);
    CHECK_EQ(pkts.size(), (u32)sent);       // one Connection_Request per attempt sent
}

// Pages the DS, has it accept, and drains our two HID Connection Requests.
// After a fresh Reset() our control CID is 0x0040 and interrupt 0x0041.
void BringUpPagedLinkAndOpenHIDChannels(BTKeyboard& kb)
{
    Feed(kb, Command(0x0C1A, {0x02}));      // Write_Scan_Enable: page scan on
    Drain(kb);

    kb.PageTick();
    Drain(kb);

    Feed(kb, Command(0x0409, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00, 0x01}));
    Drain(kb);
}

// on a channel we opened, the Connection Response's DCID is the game's CID and
// our Configuration Request must be addressed to it
void testConnectionResponseIsAbsorbedAndChannelRecorded()
{
    BTKeyboard kb;
    kb.Reset();
    BringUpPagedLinkAndOpenHIDChannels(kb);

    // Connection Response for our control channel, CID 0x0040
    Feed(kb, Acl(0x0001, {0x03, 0x01, 0x08, 0x00,
                          0x50, 0x00,             // DCID: the game's CID
                          0x40, 0x00,             // SCID: echoed back
                          0x00, 0x00,             // result: success
                          0x00, 0x00}));          // status
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);               // our Configuration Request, then the ACL ack
    if (pkts.size() != 2) return;

    const std::vector<u8>& req = pkts[0];
    CHECK_EQ(req[9], 0x04);                 // Configuration Request
    CHECK_EQ(req[13], 0x50);                // DCID: the game's CID
    CHECK_EQ(req[14], 0x00);
    CHECK_EQ(req[15], 0x00);                // flags: no continuation
    CHECK_EQ(req[16], 0x00);

    CHECK_EQ(pkts[1][1], 0x13);             // Number Of Completed Packets
}

void testInitiatedHIDChannelsConfigureInBothDirections()
{
    BTKeyboard kb;
    kb.Reset();
    BringUpPagedLinkAndOpenHIDChannels(kb);

    CHECK(!kb.Connected());

    // control channel: our CID 0x0040, the game's 0x0050
    Feed(kb, Acl(0x0001, {0x03, 0x01, 0x08, 0x00, 0x50, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00}));
    Drain(kb);                              // our Configuration Request, ACL ack
    CHECK(!kb.Connected());

    // the game answers our Configuration Request
    Feed(kb, Acl(0x0001, {0x05, 0x03, 0x06, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}));
    Drain(kb);
    CHECK(!kb.Connected());                 // only one direction of one channel so far

    // the game configures its own end of the control channel
    Feed(kb, Acl(0x0001, {0x04, 0x04, 0x04, 0x00, 0x40, 0x00, 0x00, 0x00}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);               // our Configuration Response, ACL ack
    if (pkts.size() == 2)
    {
        CHECK_EQ(pkts[0][9], 0x05);         // Configuration Response
        CHECK_EQ(pkts[0][13], 0x50);        // SCID: theirs
        CHECK_EQ(pkts[0][14], 0x00);
    }
    CHECK(!kb.Connected());                 // control channel is done, interrupt isn't yet

    // interrupt channel: our CID 0x0041, the game's 0x0051
    Feed(kb, Acl(0x0001, {0x03, 0x02, 0x08, 0x00, 0x51, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00}));
    Drain(kb);
    Feed(kb, Acl(0x0001, {0x05, 0x05, 0x06, 0x00, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00}));
    Drain(kb);
    CHECK(!kb.Connected());

    Feed(kb, Acl(0x0001, {0x04, 0x06, 0x04, 0x00, 0x41, 0x00, 0x00, 0x00}));
    Drain(kb);

    CHECK(kb.Connected());
}

// An L2CAP frame carries the receiver's CID, so data frames must use the game's
// CID for the channel, not ours. After a fresh registration the game numbers its
// channels from 0x0040 again while our counter carries on.
//
// Checks that every non-signalling ACL frame in pkts goes to cid, and that there is at least one.
static void CheckDataAddressedTo(const std::vector<std::vector<u8>>& pkts, u16 cid)
{
    int frames = 0;
    for (const std::vector<u8>& pkt : pkts)
    {
        if (pkt.size() < 9 || pkt[0] != 0x02) continue;
        u16 dest = (u16)(pkt[7] | (pkt[8] << 8));
        if (dest == 0x0001) continue;

        CHECK_EQ(dest, cid);
        frames++;
    }
    CHECK(frames > 0);
}

// the game's CIDs deliberately differ from ours
void testRepliesOnChannelsTheGameOpenedGoToItsCids()
{
    BTKeyboard kb;
    kb.Reset();

    u16 sdp  = OpenChannel(kb, 0x0001, 0x0060, 0x10);
    u16 ctrl = OpenChannel(kb, 0x0011, 0x0070, 0x20);
    u16 intr = OpenChannel(kb, 0x0013, 0x0071, 0x30);
    CHECK_EQ(sdp,  0x0040);
    CHECK_EQ(ctrl, 0x0041);
    CHECK_EQ(intr, 0x0042);

    Feed(kb, Acl(sdp, {0x08, 0x00, 0x09, 0x00, 0x00}));
    CheckDataAddressedTo(Drain(kb), 0x0060);

    // battery GET_REPORT, SET_REPORT, GET_PROTOCOL
    Feed(kb, Acl(ctrl, {0x41, 0x03}));
    CheckDataAddressedTo(Drain(kb), 0x0070);
    Feed(kb, Acl(ctrl, {0x52, 0x01}));
    CheckDataAddressedTo(Drain(kb), 0x0070);
    Feed(kb, Acl(ctrl, {0x60}));
    CheckDataAddressedTo(Drain(kb), 0x0070);
}

// reconnect: we open the HID channels and the game's Connection Responses carry its CIDs
void testReportsOnChannelsWeOpenedGoToTheGamesCids()
{
    BTKeyboard kb;
    kb.Reset();
    BringUpPagedLinkAndOpenHIDChannels(kb);     // ours: control 0x0040, interrupt 0x0041

    // the game's: 0x0050 and 0x0051, configured in both directions
    Feed(kb, Acl(0x0001, {0x03, 0x01, 0x08, 0x00, 0x50, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00}));
    Feed(kb, Acl(0x0001, {0x05, 0x03, 0x06, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}));
    Feed(kb, Acl(0x0001, {0x04, 0x04, 0x04, 0x00, 0x40, 0x00, 0x00, 0x00}));
    Feed(kb, Acl(0x0001, {0x03, 0x02, 0x08, 0x00, 0x51, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00}));
    Feed(kb, Acl(0x0001, {0x05, 0x05, 0x06, 0x00, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00}));
    Feed(kb, Acl(0x0001, {0x04, 0x06, 0x04, 0x00, 0x41, 0x00, 0x00, 0x00}));
    Drain(kb);
    CHECK(kb.Connected());

    // the battery request the game makes once the channels are up
    Feed(kb, Acl(0x0040, {0x41, 0x03}));
    CheckDataAddressedTo(Drain(kb), 0x0050);
}

// The DS accepts every page but never answers our HID Connection Requests. The
// page budget resets on each accept, so only the HID connect budget stops this.
void testHIDConnectRetryIsBounded()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x0C1A, {0x02}));      // Write_Scan_Enable: page scan on
    Drain(kb);

    int cyclesWithRequests = 0;
    for (int i = 0; i < 40; i++)
    {
        if (!kb.PageTick()) break;
        Drain(kb);

        Feed(kb, Command(0x0409, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00, 0x01}));
        auto pkts = Drain(kb);              // Command Status, Connection Complete, maybe 2x ConnReq
        if (pkts.size() == 4) cyclesWithRequests++;

        Feed(kb, Command(0x0406, {0x42, 0x00, 0x13}));   // Disconnect
        Drain(kb);
    }

    CHECK(cyclesWithRequests > 0);
    CHECK(cyclesWithRequests < 40);

    // budget spent: Command Status and Connection Complete only
    kb.PageTick();
    Drain(kb);
    Feed(kb, Command(0x0409, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00, 0x01}));
    CHECK_EQ(Drain(kb).size(), 2);
}

void testConnectionResponsePendingIsNotAReject()
{
    // On a bonded reconnect the game answers our HID Connection Requests with
    // pending (0x0001) before success. Dropping the channel on pending leaves
    // every loaded save without a keyboard.
    BTKeyboard kb;
    kb.Reset();
    BringUpPagedLinkAndOpenHIDChannels(kb);

    // control channel, our CID 0x0040, game's 0x0050
    Feed(kb, Acl(0x0001, {0x03, 0x01, 0x08, 0x00, 0x50, 0x00, 0x40, 0x00,
                          0x01, 0x00,             // result: connection pending
                          0x00, 0x00}));
    Drain(kb);

    Feed(kb, Acl(0x0001, {0x03, 0x01, 0x08, 0x00, 0x50, 0x00, 0x40, 0x00,
                          0x00, 0x00,             // result: success
                          0x00, 0x00}));
    auto pkts = Drain(kb);

    bool sawConfigReq = false;
    for (auto& p : pkts)
        if (p.size() >= 16 && p[9] == 0x04 && p[13] == 0x50)   // Config Request, DCID 0x0050
            sawConfigReq = true;
    CHECK(sawConfigReq);
}

void testBondedReconnectWithPendingResponsesConnects()
{
    // a loaded-save reconnect: each Connection Request gets pending, then success
    BTKeyboard kb;
    kb.Reset();
    BringUpPagedLinkAndOpenHIDChannels(kb);
    CHECK(!kb.Connected());

    // control channel (our 0x0040, game's 0x0050): pending, then success
    Feed(kb, Acl(0x0001, {0x03, 0x01, 0x08, 0x00, 0x50, 0x00, 0x40, 0x00, 0x01, 0x00, 0x00, 0x00}));
    Drain(kb);
    Feed(kb, Acl(0x0001, {0x03, 0x01, 0x08, 0x00, 0x50, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00}));
    Drain(kb);
    Feed(kb, Acl(0x0001, {0x05, 0x03, 0x06, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}));   // game answers our config req
    Drain(kb);
    Feed(kb, Acl(0x0001, {0x04, 0x04, 0x04, 0x00, 0x40, 0x00, 0x00, 0x00}));               // game configs its end
    Drain(kb);
    CHECK(!kb.Connected());

    // interrupt channel (our 0x0041, game's 0x0051): pending, then success
    Feed(kb, Acl(0x0001, {0x03, 0x02, 0x08, 0x00, 0x51, 0x00, 0x41, 0x00, 0x01, 0x00, 0x00, 0x00}));
    Drain(kb);
    Feed(kb, Acl(0x0001, {0x03, 0x02, 0x08, 0x00, 0x51, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00}));
    Drain(kb);
    Feed(kb, Acl(0x0001, {0x05, 0x05, 0x06, 0x00, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00}));
    Drain(kb);
    Feed(kb, Acl(0x0001, {0x04, 0x06, 0x04, 0x00, 0x41, 0x00, 0x00, 0x00}));
    Drain(kb);

    CHECK(kb.Connected());
}

// 64 KB of controller flash from 0xFF008000; the game uses 0xFF00Bxxx..0xFF014xxx
struct RecordingFlash : BTKeyboard::FlashBackend
{
    static constexpr u32 Base = 0xFF008000;
    u8 Mem[0x10000];
    int Reads = 0, Writes = 0, Erases = 0;
    u32 LastWriteAddr = 0, LastWriteLen = 0, LastEraseAddr = 0;

    RecordingFlash() { memset(Mem, 0xFF, sizeof(Mem)); }

    void Read(u32 addr, u8* out, u32 len) override
    {
        Reads++;
        for (u32 i = 0; i < len; i++)
        {
            u32 a = addr + i - Base;
            out[i] = (a < sizeof(Mem)) ? Mem[a] : 0xFF;
        }
    }
    void Write(u32 addr, const u8* in, u32 len) override
    {
        Writes++; LastWriteAddr = addr; LastWriteLen = len;
        for (u32 i = 0; i < len; i++)
        {
            u32 a = addr + i - Base;
            if (a < sizeof(Mem)) Mem[a] = in[i];
        }
    }
    void EraseSector(u32 addr) override
    {
        Erases++; LastEraseAddr = addr;
        u32 s = (addr - Base) & ~0xFFFu;
        if (s < sizeof(Mem)) memset(&Mem[s], 0xFF, 0x1000);
    }
};

// Broadcom Write_RAM/Read_RAM/erase parameters: 32-bit LE address, then data
// for a write or a length byte for a read
std::vector<u8> VendorParams(u32 addr, const std::vector<u8>& tail)
{
    std::vector<u8> p { (u8)addr, (u8)(addr >> 8), (u8)(addr >> 16), (u8)(addr >> 24) };
    p.insert(p.end(), tail.begin(), tail.end());
    return p;
}

void testWriteRamReachesTheFlashBackend()
{
    BTKeyboard kb;
    kb.Reset();
    RecordingFlash flash;
    kb.SetFlash(&flash);

    Feed(kb, Command(0xFC4C, VendorParams(0xFF00E000, {0xDE, 0xAD, 0xBE, 0xEF})));

    CHECK_EQ(flash.Writes, 1);
    CHECK_EQ(flash.LastWriteAddr, 0xFF00E000);
    CHECK_EQ(flash.LastWriteLen, 4);
    CHECK_EQ(flash.Mem[0xE000 - 0x8000 + 0], 0xDE);
    CHECK_EQ(flash.Mem[0xE000 - 0x8000 + 3], 0xEF);

    auto pkts = Drain(kb);
    CHECK_EQ(pkts.size(), 1);
    if (pkts.empty()) return;
    CHECK_EQ(pkts[0][1], 0x0E);          // Command Complete
    CHECK_EQ(pkts[0][4], 0x4C);          // opcode low
    CHECK_EQ(pkts[0][5], 0xFC);          // opcode high
    CHECK_EQ(pkts[0][6], 0x00);          // status: success
}

void testReadRamReturnsWhatTheBackendHolds()
{
    BTKeyboard kb;
    kb.Reset();
    RecordingFlash flash;
    flash.Mem[0xE000 - 0x8000 + 0] = 0x52;   // 'R'
    flash.Mem[0xE000 - 0x8000 + 1] = 0x41;   // 'A'
    flash.Mem[0xE000 - 0x8000 + 2] = 0x58;   // 'X'
    flash.Mem[0xE000 - 0x8000 + 3] = 0x54;   // 'T'
    kb.SetFlash(&flash);

    Feed(kb, Command(0xFC4D, VendorParams(0xFF00E000, {0x04})));

    auto pkts = Drain(kb);
    CHECK_EQ(pkts.size(), 1);
    if (pkts.empty()) return;
    const std::vector<u8>& e = pkts[0];
    CHECK_EQ(e[1], 0x0E);                // Command Complete
    CHECK_EQ(e[4], 0x4D);                // opcode low
    CHECK_EQ(e[5], 0xFC);                // opcode high
    CHECK_EQ(e[6], 0x00);                // status: success
    if (e.size() < 11) { CHECK(e.size() >= 11); return; }
    CHECK_EQ(e[7], 0x52);
    CHECK_EQ(e[8], 0x41);
    CHECK_EQ(e[9], 0x58);
    CHECK_EQ(e[10], 0x54);
    CHECK_EQ(flash.Reads, 1);
}

void testEraseSectorClearsTheBackend()
{
    BTKeyboard kb;
    kb.Reset();
    RecordingFlash flash;
    memset(&flash.Mem[0xE000 - 0x8000], 0x00, 0x1000);   // dirty the sector
    kb.SetFlash(&flash);

    Feed(kb, Command(0xFF5E, VendorParams(0xFF00E000, {})));
    Drain(kb);

    CHECK_EQ(flash.Erases, 1);
    CHECK_EQ(flash.LastEraseAddr, 0xFF00E000);
    CHECK_EQ(flash.Mem[0xE000 - 0x8000 + 0], 0xFF);
    CHECK_EQ(flash.Mem[0xE000 - 0x8000 + 0xFFF], 0xFF);
}

void testVendorFlashCommandsAreAcknowledgedWithNoBackend()
{
    // the game's driver hangs if these go unanswered
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0xFC4C, VendorParams(0xFF00E000, {0x01, 0x02})));
    CHECK_EQ(Drain(kb).size(), 1);
    Feed(kb, Command(0xFF5E, VendorParams(0xFF00E000, {})));
    CHECK_EQ(Drain(kb).size(), 1);
    Feed(kb, Command(0xFC4D, VendorParams(0xFF00E000, {0x04})));
    CHECK_EQ(Drain(kb).size(), 1);
}

// An event carries at most 255 parameter bytes, which leaves 251 for Read_RAM's data.
void testReadRamOfTheLargestLengthAnswers()
{
    BTKeyboard kb;
    kb.Reset();
    RecordingFlash flash;
    kb.SetFlash(&flash);

    Feed(kb, Command(0xFC4D, VendorParams(0xFF00E000, {251})));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 1);
    if (pkts.empty()) return;
    CHECK_EQ(pkts[0].size(), 3 + 255);
    CHECK_EQ(pkts[0][2], 255);           // parameter total length
    CHECK_EQ(pkts[0][6], 0x00);          // status: success
}

void testReadRamPastTheEventSizeFailsInsteadOfGoingUnanswered()
{
    BTKeyboard kb;
    kb.Reset();
    RecordingFlash flash;
    kb.SetFlash(&flash);

    Feed(kb, Command(0xFC4D, VendorParams(0xFF00E000, {252})));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 1);
    if (pkts.empty()) return;
    CHECK_EQ(pkts[0][1], 0x0E);          // Command Complete
    CHECK_EQ(pkts[0][2], 4);             // no data
    CHECK_EQ(pkts[0][6], 0x12);          // status: invalid HCI command parameters
    CHECK_EQ(flash.Reads, 0);
}

// An ACL link the game created, with both HID channels open on it.
void ConnectWithHIDChannels(BTKeyboard& kb)
{
    Feed(kb, Command(0x0405, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00,
                              0x18, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}));
    Drain(kb);
    OpenChannel(kb, 0x0011, 0x0042, 0x10);
    OpenChannel(kb, 0x0013, 0x0043, 0x20);
}

void testHciResetDropsTheLinkAndItsChannels()
{
    BTKeyboard kb;
    kb.Reset();
    ConnectWithHIDChannels(kb);
    CHECK(kb.Connected());

    Feed(kb, Command(0x0C03, {}));       // HCI_Reset
    CHECK_EQ(Drain(kb).size(), 1);       // its Command Complete
    CHECK(!kb.Connected());

    // with the link gone, page scan brings the keyboard back
    Feed(kb, Command(0x0C1A, {0x02}));   // Write_Scan_Enable: page scan on
    Drain(kb);
    CHECK(kb.PageTick());
}

void testHciResetDiscardsEventsNotYetRead()
{
    BTKeyboard kb;
    kb.Reset();

    // answered at once, since the keyboard is discoverable
    Feed(kb, Command(0x0401, {0x33, 0x8B, 0x9E, 0x0A, 0x00}));
    Feed(kb, Command(0x0C03, {}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 1);
    if (pkts.empty()) return;
    CHECK_EQ(pkts[0][1], 0x0E);          // Command Complete
    CHECK_EQ(pkts[0][4], 0x03);          // for HCI_Reset
    CHECK_EQ(pkts[0][5], 0x0C);
}

void testHciResetEndsInquiryAndPageScan()
{
    BTKeyboard kb;
    kb.Reset();
    kb.SetAutoPair(false);

    Feed(kb, Command(0x0401, {0x33, 0x8B, 0x9E, 0x0A, 0x00}));   // sat out
    Feed(kb, Command(0x0C1A, {0x02}));                            // page scan on
    Drain(kb);

    Feed(kb, Command(0x0C03, {}));
    Drain(kb);

    for (int i = 0; i < 10; i++)
        CHECK(!kb.InquiryTick());
    CHECK(!kb.PageTick());
}

void testHciDisconnectTakesTheChannelsWithIt()
{
    BTKeyboard kb;
    kb.Reset();
    ConnectWithHIDChannels(kb);
    CHECK(kb.Connected());

    Feed(kb, Command(0x0406, {0x42, 0x00, 0x13}));   // Disconnect
    Drain(kb);
    CHECK(!kb.Connected());
}

// After a controller reset the host starts over, so the retry budgets start over too.
void testHciResetRestoresThePagingBudget()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x0C1A, {0x02}));      // Write_Scan_Enable: page scan on
    Drain(kb);
    while (kb.PageTick()) {}
    Drain(kb);

    Feed(kb, Command(0x0C03, {}));          // HCI_Reset
    Feed(kb, Command(0x0C1A, {0x02}));
    Drain(kb);

    CHECK(kb.PageTick());
}

void testHciResetRestoresTheHIDConnectBudget()
{
    BTKeyboard kb;
    kb.Reset();

    Feed(kb, Command(0x0C1A, {0x02}));      // Write_Scan_Enable: page scan on
    Drain(kb);

    // the DS accepts every page but never answers the HID Connection Requests
    for (int i = 0; i < 40 && kb.PageTick(); i++)
    {
        Drain(kb);
        Feed(kb, Command(0x0409, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00, 0x01}));
        Drain(kb);
        Feed(kb, Command(0x0406, {0x42, 0x00, 0x13}));   // Disconnect
        Drain(kb);
    }

    Feed(kb, Command(0x0C03, {}));          // HCI_Reset
    Feed(kb, Command(0x0C1A, {0x02}));
    Drain(kb);

    CHECK(kb.PageTick());
    Drain(kb);
    Feed(kb, Command(0x0409, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00, 0x01}));
    CHECK_EQ(Drain(kb).size(), 4);          // Command Status, Connection Complete, both requests
}

// A savestate holds 64 channels, so the keyboard refuses a channel past that many.
void testConnectionRequestsPastTheChannelLimitAreRefused()
{
    BTKeyboard kb;
    kb.Reset();

    for (int i = 0; i < 64; i++)
    {
        Feed(kb, Acl(0x0001, {0x02, (u8)(i + 1), 0x04, 0x00, 0x01, 0x00, (u8)(0x40 + i), 0x00}));
        Drain(kb);
    }

    Feed(kb, Acl(0x0001, {0x02, 0x80, 0x04, 0x00, 0x01, 0x00, 0x90, 0x00}));
    auto pkts = Drain(kb);

    CHECK_EQ(pkts.size(), 2);               // the refusal, then the ACL ack
    if (pkts.size() != 2) return;

    const std::vector<u8>& rsp = pkts[0];
    CHECK_EQ(rsp[9], 0x03);                 // Connection Response
    CHECK_EQ(rsp[10], 0x80);                // same identifier
    CHECK_EQ(rsp[13], 0x00);                // destination CID: none
    CHECK_EQ(rsp[14], 0x00);
    CHECK_EQ(rsp[15], 0x90);                // source CID: theirs
    CHECK_EQ(rsp[16], 0x00);
    CHECK_EQ(rsp[17], 0x04);                // result: refused, no resources
    CHECK_EQ(rsp[18], 0x00);
}

// For the same reason, the keyboard opens no HID channel of its own past that many.
void testTheKeyboardOpensNoChannelPastTheChannelLimit()
{
    BTKeyboard kb;
    kb.Reset();

    for (int i = 0; i < 64; i++)
    {
        Feed(kb, Acl(0x0001, {0x02, (u8)(i + 1), 0x04, 0x00, 0x01, 0x00, (u8)(0x40 + i), 0x00}));
        Drain(kb);
    }

    // the game accepting our page, which would have us open both HID channels
    Feed(kb, Command(0x0409, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00, 0x01}));
    CHECK_EQ(Drain(kb).size(), 2);          // Command Status, Connection Complete
}

}

int runBTKeyboardTests()
{
    testResetAnswersCommandComplete();
    testInquiryReportsAKeyboard();
    testCreateConnectionReportsTheHandle();
    testClockOffsetVersionAndFeaturesComplete();
    testRemoteNameIsPaddedTo248Bytes();
    testAclIsAcknowledged();
    testAclWithAnOverlongL2capLengthIsDropped();
    testL2capConnectAndConfigure();
    testL2capConfigureForAnUnknownChannelIsRejected();
    testL2capDisconnectClosesTheChannel();
    testL2capDisconnectForAnUnknownChannelIsRejected();
    testL2capDisconnectWithTheWrongSourceCidIsIgnored();
    testL2capInformationRequest();
    testSdpAnswersTheCapturedRequest();
    testSdpReturnsOnlyTheAttributesAsked();
    testSdpRejectsAServiceItDoesNotOffer();
    testSdpContinuesAcrossRequests();
    testSdpRejectsAnUnsupportedPdu();
    testSdpClampsChunksToTheAdvertisedAclSize();
    testDisconnectAnswersWithCommandStatusThenDisconnectionComplete();
    testReadRemoteExtendedFeaturesAnswersWithCommandStatusThenCompleteEvent();
    testServiceSearchRequestFindsOurRecord();
    testServiceSearchRequestFindsNothingForAnUnknownService();
    testServiceAttributeRequestReturnsTheAttribute();
    testServiceAttributeRequestRejectsAnUnknownHandle();
    testHidChannelsMakeTheKeyboardConnected();
    testGetReportReturnsTheCurrentInputReport();
    testGetReportForBatteryIdReturnsTheBatteryPayload();
    testGetReportWithNoReportIdStillReturnsThePlainBootReport();
    testSetReportStoresTheLedState();
    testProtocolModeRoundTrips();
    testUnknownHidRequestIsRefused();
    testInquiryFindsNothingWithAutoPairOffUntilFnIsHeld();
    testFnDuringAPendingInquiryAnswersItAtOnce();
    testInquiryCancelDropsThePendingScan();
    testAutoPairMakesTheKeyboardDiscoverableFromTheStart();
    testAutoPairSurvivesResetButAPairingRequestDoesNot();
    testFnIsThePairingGestureOnlyUntilTheGameConnects();
    testAnAcceptedPageEndsThePairingGestureToo();
    testGameHasKeyboardFromPageScanOnwards();
    testNothingIsTypedOnTheGamesBehalf();
    testDeleteStoredLinkKeyReportsNoKeysDeleted();
    testAuthenticationRequestedAsksForLinkKey();
    testLinkKeyRequestReplyCompletesAuthentication();
    testLinkKeyRequestNegativeReplyAsksForPin();
    testPinCodeRequestReplyNotifiesLinkKeyThenCompletesAuthentication();
    testPinCodeRequestNegativeReplyFailsAuthentication();
    testFullNegativeReplyPairingPathCompletesAuthentication();
    testSetConnectionEncryptionReportsEncryptionChange();
    testSniffModeReportsModeChange();
    testReadRssiReportsAHealthyLink();
    testNoConnectionRequestWhileLinkIsUp();
    testPageScanAfterDisconnectSendsConnectionRequest();
    testAcceptConnectionRequestCompletesTheConnection();
    testRejectConnectionRequestFailsTheConnection();
    testPagingRetryIsBounded();
    testConnectionResponseIsAbsorbedAndChannelRecorded();
    testInitiatedHIDChannelsConfigureInBothDirections();
    testRepliesOnChannelsTheGameOpenedGoToItsCids();
    testReportsOnChannelsWeOpenedGoToTheGamesCids();
    testHIDConnectRetryIsBounded();
    testConnectionResponsePendingIsNotAReject();
    testBondedReconnectWithPendingResponsesConnects();
    testWriteRamReachesTheFlashBackend();
    testReadRamReturnsWhatTheBackendHolds();
    testEraseSectorClearsTheBackend();
    testVendorFlashCommandsAreAcknowledgedWithNoBackend();
    testReadRamOfTheLargestLengthAnswers();
    testReadRamPastTheEventSizeFailsInsteadOfGoingUnanswered();
    testHciResetDropsTheLinkAndItsChannels();
    testHciResetDiscardsEventsNotYetRead();
    testHciResetEndsInquiryAndPageScan();
    testHciDisconnectTakesTheChannelsWithIt();
    testHciResetRestoresThePagingBudget();
    testHciResetRestoresTheHIDConnectBudget();
    testConnectionRequestsPastTheChannelLimitAreRefused();
    testTheKeyboardOpensNoChannelPastTheChannelLimit();
    return 0;
}
