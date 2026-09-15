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

#include <assert.h>
#include <cstring>

#include "BTKeyboard.h"

namespace melonDS::NDSCart
{

// 00:1F:32:11:22:33, on Nintendo's OUI, little-endian as HCI carries it
static const u8 KeyboardAddr[6] = { 0x33, 0x22, 0x11, 0x32, 0x1F, 0x00 };

// Class of Device: Peripheral, Keyboard. The game's inquiry event filter only
// passes 000540 under mask FF0FFF.
static const u8 KeyboardClass[3] = { 0x40, 0x05, 0x00 };

// only displayed; the game never compares the name
static const char KeyboardName[] = "Nintendo Wireless Keyboard";

// any handle below 0x0F00 would do
static constexpr u16 ConnHandle = 0x0042;

// L2CAP channel IDs below 0x0040 are reserved; 0x0001 is the signalling channel.
static constexpr u16 SignalCID     = 0x0001;
static constexpr u16 FirstLocalCID = 0x0040;

// The two PSMs a Bluetooth HID device listens on.
static constexpr u16 PSM_HIDControl   = 0x0011;
static constexpr u16 PSM_HIDInterrupt = 0x0013;

// Handed out on PIN_Code_Request_Reply and accepted back on Link_Key_Request_Reply.
// Nothing verifies it, so any 16 bytes would do.
static const u8 LinkKey[16] =
{
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

// SendACL never fragments and Read Buffer Size advertises a 57-byte ACL data length.
// An SDP response adds 14 bytes of L2CAP and SDP framing to its chunk: 57 - 14 = 43.
static constexpr u32 MaxSDPChunk = 43;

void BTKeyboard::Reset() noexcept
{
    Outgoing.clear();
    Channels.clear();
    NextLocalCID = FirstLocalCID;
    NextSignalID = 1;

    memset(InputReport, 0, sizeof(InputReport));
    OutputReport = 0;
    BootProtocol = true;

    // AutoPair is a user setting and survives a reset; the Fn gesture does not
    PairingRequested = false;
    LinkEverUp = false;
    InquiryTicksLeft = 0;

    PageScanEnabled = false;
    LinkUp = false;
    PageAttempts = 0;
    HIDConnectAttempts = 0;
}

void BTKeyboard::DoSavestate(Savestate* file)
{
    u32 nchannels = (u32)Channels.size();
    file->Var32(&nchannels);
    if (!file->Saving)
    {
        if (nchannels > MaxSavedChannels) return RejectSavestate(file);
        Channels.resize(nchannels);
    }

    // every slot is written, used or not
    for (u32 i = 0; i < MaxSavedChannels; i++)
    {
        Channel spare {};
        Channel& ch = (i < nchannels) ? Channels[i] : spare;
        file->Var16(&ch.LocalCID);
        file->Var16(&ch.RemoteCID);
        file->Var16(&ch.PSM);
        file->VarBool(&ch.Configured);
    }

    file->Var16(&NextLocalCID);
    file->Var8(&NextSignalID);

    file->VarArray(InputReport, sizeof(InputReport));
    file->Var8(&OutputReport);
    file->VarBool(&BootProtocol);

    // AutoPair is a user setting, not session state; a savestate must not override it
    file->VarBool(&PairingRequested);
    file->VarBool(&LinkEverUp);

    file->Var32(&InquiryTicksLeft);

    file->VarBool(&PageScanEnabled);
    file->VarBool(&LinkUp);
    file->Var32(&PageAttempts);
    file->Var32(&HIDConnectAttempts);

    // unread packets, each a u16 length and its bytes, packed into a fixed block
    u32 npending = (u32)Outgoing.size();
    u8 pending[SavedPacketBytes] {};
    if (file->Saving)
    {
        u32 pos = 0;
        for (auto& pkt : Outgoing)
        {
            if (pkt.size() + 2 > SavedPacketBytes - pos)
            {
                file->Error = true;
                return;
            }

            u16 len = (u16)pkt.size();
            memcpy(&pending[pos], &len, 2);
            memcpy(&pending[pos + 2], pkt.data(), len);
            pos += 2 + len;
        }
    }

    file->Var32(&npending);
    file->VarArray(pending, sizeof(pending));
    if (file->Saving) return;

    Outgoing.clear();
    u32 pos = 0;
    for (u32 i = 0; i < npending; i++)
    {
        // every packet has at least its type byte
        u16 len = 0;
        if (SavedPacketBytes - pos >= 2) memcpy(&len, &pending[pos], 2);
        if (len == 0 || len + 2u > SavedPacketBytes - pos) return RejectSavestate(file);

        const u8* data = pending + pos + 2;
        Outgoing.emplace_back(data, data + len);
        pos += 2 + len;
    }
}

void BTKeyboard::RejectSavestate(Savestate* file) noexcept
{
    file->Error = true;
    Reset();
}

bool BTKeyboard::NextPacket(std::vector<u8>& out)
{
    if (Outgoing.empty()) return false;

    out = std::move(Outgoing.front());
    Outgoing.pop_front();
    return true;
}

void BTKeyboard::HostPacket(const u8* data, u32 len)
{
    if (len < 1) return;

    switch (data[0])
    {
    case 0x01: // command
        if (len < 4) return;
        {
            u16 opcode = (u16)(data[1] | (data[2] << 8));
            u8 plen = data[3];
            if (len < 4u + plen) return;
            HandleCommand(opcode, &data[4], plen);
        }
        return;

    case 0x02: // ACL data
        HandleACL(&data[1], len - 1);
        return;

    default:
        return;
    }
}

void BTKeyboard::Event(u8 code, const u8* params, u32 plen)
{
    if (plen > 255) return;

    std::vector<u8> pkt(3 + plen);
    pkt[0] = 0x04;                  // HCI event packet
    pkt[1] = code;
    pkt[2] = (u8)plen;              // parameter total length
    if (plen) memcpy(&pkt[3], params, plen);

    Outgoing.push_back(std::move(pkt));
}

void BTKeyboard::CommandComplete(u16 opcode, const u8* ret, u32 retlen)
{
    u8 params[259];
    if (retlen + 3 > sizeof(params)) return;

    params[0] = 0x01;               // command packets the host may send
    params[1] = (u8)(opcode & 0xFF);
    params[2] = (u8)(opcode >> 8);
    if (retlen) memcpy(&params[3], ret, retlen);

    Event(0x0E, params, 3 + retlen);
}

void BTKeyboard::CommandStatus(u16 opcode, u8 status)
{
    // commands that take time on air answer with a status first, and report their
    // outcome later in an event of their own
    u8 params[4];
    params[0] = status;
    params[1] = 0x01;               // command packets the host may send
    params[2] = (u8)(opcode & 0xFF);
    params[3] = (u8)(opcode >> 8);

    Event(0x0F, params, sizeof(params));
}

void BTKeyboard::Inquiry(u8 length)
{
    // Not switched on with Fn held, a real keyboard is invisible to inquiry. Complete
    // the Inquiry empty only once its length (units of 1.28 s) has run out, in 2-second
    // ticks rounded up: answered at once, the game's driver re-issues it ~30 times a second.
    if (!Discoverable())
    {
        InquiryTicksLeft = ((u32)length * 128 + 199) / 200;
        if (InquiryTicksLeft == 0) InquiryTicksLeft = 1;
        return;
    }

    AnswerInquiry();
}

bool BTKeyboard::InquiryTick() noexcept
{
    if (InquiryTicksLeft == 0) return false;
    if (--InquiryTicksLeft > 0) return false;

    u8 status = 0x00;
    Event(0x01, &status, 1);                // Inquiry Complete, nobody found
    return true;
}

void BTKeyboard::AnswerInquiry()
{
    InquiryTicksLeft = 0;

    u8 params[15];

    params[0] = 0x01;               // number of responses
    memcpy(&params[1], KeyboardAddr, 6);
    params[7] = 0x01;               // page scan repetition mode: R1
    params[8] = 0x00;               // reserved
    params[9] = 0x00;               // reserved
    memcpy(&params[10], KeyboardClass, 3);
    params[13] = 0x00; params[14] = 0x00;   // clock offset
    Event(0x02, params, sizeof(params));    // Inquiry Result

    u8 status = 0x00;
    Event(0x01, &status, 1);                // Inquiry Complete
}

void BTKeyboard::RemoteName()
{
    u8 params[255];
    memset(params, 0, sizeof(params));

    params[0] = 0x00;               // status: success
    memcpy(&params[1], KeyboardAddr, 6);
    memcpy(&params[7], KeyboardName, sizeof(KeyboardName) - 1);

    Event(0x07, params, sizeof(params));    // Remote Name Request Complete
}

void BTKeyboard::Connect(u8 status, bool weInitiated)
{
    u8 params[11];

    params[0] = status;
    params[1] = (u8)(ConnHandle & 0xFF);
    params[2] = (u8)(ConnHandle >> 8);
    memcpy(&params[3], KeyboardAddr, 6);
    params[9]  = 0x01;              // link type: ACL
    params[10] = 0x00;              // encryption not enabled

    Event(0x03, params, sizeof(params));    // Connection Complete

    if (status == 0x00)
    {
        // a link is up, whichever side opened it: reset the paging budget
        LinkUp = true;
        LinkEverUp = true;
        PageAttempts = 0;

        // weInitiated means the game accepted our page (Accept_Connection_Request).
        // It then leaves opening the HID channels to us, as our SDP record's
        // HIDReconnectInitiate (0x0205) says.
        if (weInitiated)
            OpenHIDChannels();
    }
}

// Without a cap, PageTick() would page a DS that never accepts on every tick.
// 30 tries is about a minute; Connect() resets the count whenever a link comes up.
static constexpr u32 MaxPageAttempts = 30;

bool BTKeyboard::PageTick() noexcept
{
    if (!PageScanEnabled || LinkUp) return false;
    if (PageAttempts >= MaxPageAttempts) return false;

    u8 params[10];
    memcpy(&params[0], KeyboardAddr, 6);
    memcpy(&params[6], KeyboardClass, 3);
    params[9] = 0x01;                // link type: ACL

    Event(0x04, params, sizeof(params));    // Connection_Request

    PageAttempts++;
    return true;
}

// Without a cap, every reconnect would retry HID channels the game refuses or
// never answers. The count resets once both channels are configured.
static constexpr u32 MaxHIDConnectAttempts = 10;

void BTKeyboard::OpenHIDChannels()
{
    if (HIDConnectAttempts >= MaxHIDConnectAttempts) return;
    HIDConnectAttempts++;

    OpenChannel(PSM_HIDControl);
    OpenChannel(PSM_HIDInterrupt);
}

void BTKeyboard::OpenChannel(u16 psm)
{
    // a savestate couldn't hold another
    if (Channels.size() >= MaxSavedChannels) return;

    Channel ch;
    ch.LocalCID   = NextLocalCID++;
    ch.RemoteCID  = 0;              // filled in once the Connection Response arrives
    ch.PSM        = psm;
    ch.Configured = false;
    Channels.push_back(ch);

    u8 req[4];
    req[0] = (u8)(psm & 0xFF);             // PSM
    req[1] = (u8)(psm >> 8);
    req[2] = (u8)(ch.LocalCID & 0xFF);     // SCID: ours
    req[3] = (u8)(ch.LocalCID >> 8);
    Signal(0x02, NextSignalID++, req, sizeof(req));    // Connection Request
}

void BTKeyboard::CompletedPackets()
{
    u8 params[5];
    params[0] = 0x01;               // number of handles
    params[1] = (u8)(ConnHandle & 0xFF);
    params[2] = (u8)(ConnHandle >> 8);
    params[3] = 0x01;               // packets completed
    params[4] = 0x00;

    Event(0x13, params, sizeof(params));    // Number Of Completed Packets
}

void BTKeyboard::SendACL(u16 cid, const u8* payload, u32 plen)
{
    std::vector<u8> pkt(9 + plen);

    pkt[0] = 0x02;                                  // HCI ACL data packet
    pkt[1] = (u8)(ConnHandle & 0xFF);
    pkt[2] = (u8)((ConnHandle >> 8) | 0x20);        // PB flag 2: first, flushable
    u16 total = (u16)(plen + 4);
    pkt[3] = (u8)(total & 0xFF);                    // ACL data total length
    pkt[4] = (u8)(total >> 8);
    pkt[5] = (u8)(plen & 0xFF);                     // L2CAP length
    pkt[6] = (u8)(plen >> 8);
    pkt[7] = (u8)(cid & 0xFF);                      // L2CAP CID
    pkt[8] = (u8)(cid >> 8);
    if (plen) memcpy(&pkt[9], payload, plen);

    Outgoing.push_back(std::move(pkt));
}

void BTKeyboard::SendOnChannel(const Channel& ch, const u8* payload, u32 plen)
{
    // L2CAP frames carry the receiver's CID. The two differ after a fresh
    // registration: the game resets its controller and numbers its channels from
    // 0x0040 again while NextLocalCID carries on.
    SendACL(ch.RemoteCID, payload, plen);
}

void BTKeyboard::HandleACL(const u8* data, u32 len)
{
    // ACL header: handle and flags (2, LE), data total length (2, LE), then one
    // L2CAP frame: length (2, LE), CID (2, LE), payload.
    if (len < 8) { CompletedPackets(); return; }

    u32 l2len = (u32)(data[4] | (data[5] << 8));
    u16 cid   = (u16)(data[6] | (data[7] << 8));

    if (8 + l2len <= len)
        HandleL2CAP(cid, &data[8], l2len);

    // the driver was told it has a ten-packet allowance; without this it stops
    // sending once it believes all ten are outstanding
    CompletedPackets();
}

BTKeyboard::Channel* BTKeyboard::FindChannelByLocal(u16 cid)
{
    for (Channel& ch : Channels)
        if (ch.LocalCID == cid) return &ch;

    return nullptr;
}

// A USB HID boot-keyboard report descriptor: an 8-byte input report of one
// modifier byte, one reserved byte and six key codes, plus a 1-byte LED output
// report. The game decodes reports itself and doesn't follow this: it expects
// report IDs, and this declares none.
static const u8 HIDReportDescriptor[] =
{
    0x05, 0x01,     // Usage Page (Generic Desktop)
    0x09, 0x06,     // Usage (Keyboard)
    0xA1, 0x01,     // Collection (Application)
    0x05, 0x07,     //   Usage Page (Key Codes)
    0x19, 0xE0,     //   Usage Minimum (224)
    0x29, 0xE7,     //   Usage Maximum (231)
    0x15, 0x00,     //   Logical Minimum (0)
    0x25, 0x01,     //   Logical Maximum (1)
    0x75, 0x01,     //   Report Size (1)
    0x95, 0x08,     //   Report Count (8)
    0x81, 0x02,     //   Input (Data, Variable, Absolute)   -- modifiers
    0x95, 0x01,     //   Report Count (1)
    0x75, 0x08,     //   Report Size (8)
    0x81, 0x01,     //   Input (Constant)                   -- reserved
    0x95, 0x05,     //   Report Count (5)
    0x75, 0x01,     //   Report Size (1)
    0x05, 0x08,     //   Usage Page (LEDs)
    0x19, 0x01,     //   Usage Minimum (1)
    0x29, 0x05,     //   Usage Maximum (5)
    0x91, 0x02,     //   Output (Data, Variable, Absolute)  -- LEDs
    0x95, 0x01,     //   Report Count (1)
    0x75, 0x03,     //   Report Size (3)
    0x91, 0x01,     //   Output (Constant)                  -- LED padding
    0x95, 0x06,     //   Report Count (6)
    0x75, 0x08,     //   Report Size (8)
    0x15, 0x00,     //   Logical Minimum (0)
    0x25, 0x65,     //   Logical Maximum (101)
    0x05, 0x07,     //   Usage Page (Key Codes)
    0x19, 0x00,     //   Usage Minimum (0)
    0x29, 0x65,     //   Usage Maximum (101)
    0x81, 0x00,     //   Input (Data, Array)                -- six key codes
    0xC0            // End Collection
};

// Every UUID the service record contains: HID, L2CAP and HIDP. Listed rather than
// scanned from the record, where the descriptor's 19 E0 would read as a UUID element.
static const u32 RecordUUIDs[] = { 0x1124, 0x0100, 0x0011 };

namespace
{

void PutSeq(std::vector<u8>& out, const std::vector<u8>& body)
{
    // sequence with an 8-bit length; a longer body needs the 16-bit form
    assert(body.size() <= 0xFF);
    out.push_back(0x35);
    out.push_back((u8)body.size());
    out.insert(out.end(), body.begin(), body.end());
}

void PutUint16(std::vector<u8>& out, u16 v)
{
    out.push_back(0x09);
    out.push_back((u8)(v >> 8));
    out.push_back((u8)(v & 0xFF));
}

void PutUUID16(std::vector<u8>& out, u16 v)
{
    out.push_back(0x19);
    out.push_back((u8)(v >> 8));
    out.push_back((u8)(v & 0xFF));
}

void PutUint8(std::vector<u8>& out, u8 v)
{
    out.push_back(0x08);
    out.push_back(v);
}

void PutBool(std::vector<u8>& out, bool v)
{
    out.push_back(0x28);
    out.push_back(v ? 0x01 : 0x00);
}

// a byte string with a 16-bit length, which the report descriptor needs
void PutBytes16(std::vector<u8>& out, const u8* data, u32 len)
{
    out.push_back(0x26);
    out.push_back((u8)(len >> 8));
    out.push_back((u8)(len & 0xFF));
    out.insert(out.end(), data, data + len);
}

}

// The one service this keyboard offers, attribute by attribute and in ascending
// ID order, which is the order SDP requires them to be returned in.
static const std::vector<BTKeyboard::SDPAttribute>& HIDServiceRecord()
{
    static const std::vector<BTKeyboard::SDPAttribute> record = []
    {
        std::vector<BTKeyboard::SDPAttribute> r;

        // 0x0000 ServiceRecordHandle
        {
            std::vector<u8> v;
            v.push_back(0x0A);                          // unsigned int, 32-bit
            v.push_back(0x00); v.push_back(0x01);
            v.push_back(0x00); v.push_back(0x00);
            r.push_back({0x0000, v});
        }

        // 0x0001 ServiceClassIDList: Human Interface Device
        {
            std::vector<u8> body, v;
            PutUUID16(body, 0x1124);
            PutSeq(v, body);
            r.push_back({0x0001, v});
        }

        // 0x0004 ProtocolDescriptorList: L2CAP on PSM 0x0011, then HIDP
        {
            std::vector<u8> l2cap, hidp, list, v;
            PutUUID16(l2cap, 0x0100);
            PutUint16(l2cap, 0x0011);
            PutSeq(list, l2cap);
            PutUUID16(hidp, 0x0011);
            PutSeq(list, hidp);
            PutSeq(v, list);
            r.push_back({0x0004, v});
        }

        // 0x0009 BluetoothProfileDescriptorList: HID, version 1.0
        {
            std::vector<u8> prof, list, v;
            PutUUID16(prof, 0x1124);
            PutUint16(prof, 0x0100);
            PutSeq(list, prof);
            PutSeq(v, list);
            r.push_back({0x0009, v});
        }

        // 0x000D AdditionalProtocolDescriptorLists: interrupt channel, PSM 0x0013
        {
            std::vector<u8> l2cap, hidp, inner, outer, v;
            PutUUID16(l2cap, 0x0100);
            PutUint16(l2cap, 0x0013);
            PutSeq(inner, l2cap);
            PutUUID16(hidp, 0x0011);
            PutSeq(inner, hidp);
            PutSeq(outer, inner);
            PutSeq(v, outer);
            r.push_back({0x000D, v});
        }

        { std::vector<u8> v; PutUint16(v, 0x0111); r.push_back({0x0201, v}); }  // HIDParserVersion 1.11
        { std::vector<u8> v; PutUint8(v, 0x40);    r.push_back({0x0202, v}); }  // HIDDeviceSubclass: keyboard
        { std::vector<u8> v; PutUint8(v, 0x00);    r.push_back({0x0203, v}); }  // HIDCountryCode: not localised
        { std::vector<u8> v; PutBool(v, true);     r.push_back({0x0204, v}); }  // HIDVirtualCable
        { std::vector<u8> v; PutBool(v, true);     r.push_back({0x0205, v}); }  // HIDReconnectInitiate

        // 0x0206 HIDDescriptorList: one descriptor, type 0x22 (report), then the bytes
        {
            std::vector<u8> desc, list, v;
            PutUint8(desc, 0x22);
            PutBytes16(desc, HIDReportDescriptor, (u32)sizeof(HIDReportDescriptor));
            PutSeq(list, desc);         // 68 bytes, so an 8-bit length still fits
            PutSeq(v, list);
            r.push_back({0x0206, v});
        }

        // 0x0207 HIDLANGIDBaseList
        {
            std::vector<u8> lang, list, v;
            PutUint16(lang, 0x0409);                    // English (US)
            PutUint16(lang, 0x0100);                    // base offset
            PutSeq(list, lang);
            PutSeq(v, list);
            r.push_back({0x0207, v});
        }

        { std::vector<u8> v; PutBool(v, true); r.push_back({0x020D, v}); }      // HIDBootDevice

        return r;
    }();

    return record;
}

// read from the record's own attribute 0x0000 so the two can't diverge
static u32 ServiceRecordHandle()
{
    for (const BTKeyboard::SDPAttribute& attr : HIDServiceRecord())
    {
        if (attr.ID == 0x0000 && attr.Value.size() == 5)
            return ((u32)attr.Value[1] << 24) | ((u32)attr.Value[2] << 16)
                 | ((u32)attr.Value[3] << 8)  |  (u32)attr.Value[4];
    }

    return 0;
}

namespace
{

// Measure one SDP data element. Fills in the header and payload sizes, and returns
// false if the element runs off the end of the buffer.
bool ElementSize(const u8* data, u32 len, u32& hdrlen, u32& datalen)
{
    if (len < 1) return false;

    u8 type = (u8)(data[0] >> 3);
    u8 size = (u8)(data[0] & 0x07);

    if (type == 0)                          // nil carries nothing
    {
        hdrlen = 1; datalen = 0;
        return true;
    }

    switch (size)
    {
    case 0: hdrlen = 1; datalen = 1;  break;
    case 1: hdrlen = 1; datalen = 2;  break;
    case 2: hdrlen = 1; datalen = 4;  break;
    case 3: hdrlen = 1; datalen = 8;  break;
    case 4: hdrlen = 1; datalen = 16; break;

    case 5:
        if (len < 2) return false;
        hdrlen = 2; datalen = data[1];
        break;

    case 6:
        if (len < 3) return false;
        hdrlen = 3; datalen = (u32)((data[1] << 8) | data[2]);
        break;

    default:
        if (len < 5) return false;
        hdrlen = 5;
        datalen = ((u32)data[1] << 24) | ((u32)data[2] << 16)
                | ((u32)data[3] << 8)  |  (u32)data[4];
        break;
    }

    return (u64)hdrlen + datalen <= len;
}

// Read the UUIDs out of a service search pattern. Only Bluetooth-assigned UUIDs can
// match, and a 128-bit one carries the assigned number in its first four bytes.
bool CollectUUIDs(const u8* data, u32 len, std::vector<u32>& out)
{
    u32 hdr = 0, dlen = 0;
    if (!ElementSize(data, len, hdr, dlen)) return false;
    if ((data[0] >> 3) != 6) return false;              // must be a sequence

    const u8* p = data + hdr;
    u32 left = dlen;

    while (left > 0)
    {
        u32 ehdr = 0, elen = 0;
        if (!ElementSize(p, left, ehdr, elen)) return false;
        if ((p[0] >> 3) != 3) return false;             // must be a UUID

        if (elen == 2)
            out.push_back((u32)((p[ehdr] << 8) | p[ehdr + 1]));
        else if (elen == 4 || elen == 16)
            out.push_back(((u32)p[ehdr] << 24) | ((u32)p[ehdr + 1] << 16)
                        | ((u32)p[ehdr + 2] << 8) |  (u32)p[ehdr + 3]);
        else
            return false;

        p += ehdr + elen;
        left -= ehdr + elen;
    }

    return true;
}

struct AttrRange { u16 First; u16 Last; };

// Read an attribute ID list: 16-bit unsigned integers are single attributes,
// 32-bit ones are inclusive ranges packed high word first.
bool CollectAttrRanges(const u8* data, u32 len, std::vector<AttrRange>& out)
{
    u32 hdr = 0, dlen = 0;
    if (!ElementSize(data, len, hdr, dlen)) return false;
    if ((data[0] >> 3) != 6) return false;              // must be a sequence

    const u8* p = data + hdr;
    u32 left = dlen;

    while (left > 0)
    {
        u32 ehdr = 0, elen = 0;
        if (!ElementSize(p, left, ehdr, elen)) return false;
        if ((p[0] >> 3) != 1) return false;             // must be an unsigned int

        if (elen == 2)
        {
            u16 id = (u16)((p[ehdr] << 8) | p[ehdr + 1]);
            out.push_back({id, id});
        }
        else if (elen == 4)
        {
            out.push_back({(u16)((p[ehdr] << 8) | p[ehdr + 1]),
                           (u16)((p[ehdr + 2] << 8) | p[ehdr + 3])});
        }
        else
        {
            return false;
        }

        p += ehdr + elen;
        left -= ehdr + elen;
    }

    return true;
}

}

void BTKeyboard::Signal(u8 code, u8 id, const u8* data, u32 len)
{
    // signalling packet: code (1), identifier (1), length (2, LE), payload
    std::vector<u8> pkt(4 + len);
    pkt[0] = code;
    pkt[1] = id;
    pkt[2] = (u8)(len & 0xFF);
    pkt[3] = (u8)(len >> 8);
    if (len) memcpy(&pkt[4], data, len);

    SendACL(SignalCID, pkt.data(), (u32)pkt.size());
}

void BTKeyboard::RejectInvalidCID(u8 id, u16 localcid, u16 remotecid)
{
    // Command Reject: both ends of the disputed channel, as we see them
    u8 rej[6];
    rej[0] = 0x02; rej[1] = 0x00;           // reason: invalid CID in request
    rej[2] = (u8)(localcid & 0xFF);
    rej[3] = (u8)(localcid >> 8);
    rej[4] = (u8)(remotecid & 0xFF);
    rej[5] = (u8)(remotecid >> 8);
    Signal(0x01, id, rej, sizeof(rej));
}

void BTKeyboard::SDPError(const Channel& ch, u16 tid, u16 code)
{
    u8 rsp[7];
    rsp[0] = 0x01;                          // ErrorResponse
    rsp[1] = (u8)(tid >> 8);
    rsp[2] = (u8)(tid & 0xFF);
    rsp[3] = 0x00; rsp[4] = 0x02;           // parameter length
    rsp[5] = (u8)(code >> 8);
    rsp[6] = (u8)(code & 0xFF);

    SendOnChannel(ch, rsp, sizeof(rsp));
}

void BTKeyboard::HandleSDP(const Channel& ch, const u8* data, u32 len)
{
    // PDU: id (1), transaction (2, big-endian), parameter length (2, big-endian)
    if (len < 5) return;

    u8  pdu  = data[0];
    u16 tid  = (u16)((data[1] << 8) | data[2]);
    u32 plen = (u32)((data[3] << 8) | data[4]);

    if (5 + plen > len) { SDPError(ch, tid, 0x0004); return; }   // invalid PDU size

    switch (pdu)
    {
    case 0x02: HandleServiceSearchRequest(ch, tid, &data[5], plen); return;
    case 0x04: HandleServiceAttributeRequest(ch, tid, &data[5], plen); return;
    case 0x06: HandleServiceSearchAttributeRequest(ch, tid, &data[5], plen); return;
    default:
        SDPError(ch, tid, 0x0003);          // invalid request syntax
        return;
    }
}

void BTKeyboard::HandleServiceSearchAttributeRequest(const Channel& ch, u16 tid,
                                                       const u8* p, u32 left)
{
    // ServiceSearchPattern
    std::vector<u32> uuids;
    u32 hdr = 0, dlen = 0;
    if (!ElementSize(p, left, hdr, dlen) || !CollectUUIDs(p, left, uuids))
    {
        SDPError(ch, tid, 0x0003);
        return;
    }
    p += hdr + dlen;
    left -= hdr + dlen;

    // MaximumAttributeByteCount
    if (left < 2) { SDPError(ch, tid, 0x0003); return; }
    u32 maxbytes = (u32)((p[0] << 8) | p[1]);
    p += 2;
    left -= 2;

    // AttributeIDList
    std::vector<AttrRange> ranges;
    if (!ElementSize(p, left, hdr, dlen) || !CollectAttrRanges(p, left, ranges))
    {
        SDPError(ch, tid, 0x0003);
        return;
    }
    p += hdr + dlen;
    left -= hdr + dlen;

    // ContinuationState: a length byte, then that many bytes. Ours is the two-byte
    // offset the previous response stopped at.
    if (left < 1) { SDPError(ch, tid, 0x0003); return; }
    u32 contlen = p[0];
    if (1 + contlen > left) { SDPError(ch, tid, 0x0003); return; }

    u32 offset = 0;
    if (contlen == 2)
        offset = (u32)((p[1] << 8) | p[2]);
    else if (contlen != 0)
    {
        SDPError(ch, tid, 0x0005);          // invalid continuation state
        return;
    }

    // the record matches only if it carries every UUID in the pattern
    bool matches = true;
    for (u32 want : uuids)
    {
        bool found = false;
        for (u32 have : RecordUUIDs)
            if (have == want) found = true;

        if (!found) { matches = false; break; }
    }

    // AttributeLists: a sequence of one attribute list per matching record, each a
    // sequence of alternating attribute ID and attribute value.
    std::vector<u8> attrlist;
    if (matches)
    {
        for (const SDPAttribute& attr : HIDServiceRecord())
        {
            bool wanted = false;
            for (const AttrRange& r : ranges)
                if (attr.ID >= r.First && attr.ID <= r.Last) wanted = true;

            if (!wanted) continue;

            PutUint16(attrlist, attr.ID);
            attrlist.insert(attrlist.end(), attr.Value.begin(), attr.Value.end());
        }
    }

    std::vector<u8> lists;
    if (matches && !attrlist.empty())
    {
        std::vector<u8> inner;
        inner.push_back(0x36);                          // sequence, 16-bit length
        inner.push_back((u8)(attrlist.size() >> 8));
        inner.push_back((u8)(attrlist.size() & 0xFF));
        inner.insert(inner.end(), attrlist.begin(), attrlist.end());

        lists.push_back(0x36);
        lists.push_back((u8)(inner.size() >> 8));
        lists.push_back((u8)(inner.size() & 0xFF));
        lists.insert(lists.end(), inner.begin(), inner.end());
    }
    else
    {
        lists.push_back(0x35);                          // an empty sequence
        lists.push_back(0x00);
    }

    if (offset > lists.size()) { SDPError(ch, tid, 0x0005); return; }
    if (maxbytes < 1) maxbytes = 1;
    if (maxbytes > MaxSDPChunk) maxbytes = MaxSDPChunk;

    u32 chunk = (u32)lists.size() - offset;
    if (chunk > maxbytes) chunk = maxbytes;

    bool more = (offset + chunk) < lists.size();

    // where the next request resumes; the host echoes it back as its offset
    u16 continuation = (u16)(offset + chunk);

    std::vector<u8> rsp;
    rsp.push_back(0x07);                                // ServiceSearchAttributeResponse
    rsp.push_back((u8)(tid >> 8));
    rsp.push_back((u8)(tid & 0xFF));

    u32 params = 2 + chunk + 1 + (more ? 2u : 0u);
    rsp.push_back((u8)(params >> 8));
    rsp.push_back((u8)(params & 0xFF));
    rsp.push_back((u8)(chunk >> 8));                    // AttributeListsByteCount
    rsp.push_back((u8)(chunk & 0xFF));
    rsp.insert(rsp.end(), lists.begin() + offset, lists.begin() + offset + chunk);

    if (more)
    {
        rsp.push_back(0x02);
        rsp.push_back((u8)(continuation >> 8));
        rsp.push_back((u8)(continuation & 0xFF));
    }
    else
    {
        rsp.push_back(0x00);
    }

    SendOnChannel(ch, rsp.data(), (u32)rsp.size());
}

void BTKeyboard::HandleServiceSearchRequest(const Channel& ch, u16 tid, const u8* p, u32 left)
{
    // ServiceSearchPattern
    std::vector<u32> uuids;
    u32 hdr = 0, dlen = 0;
    if (!ElementSize(p, left, hdr, dlen) || !CollectUUIDs(p, left, uuids))
    {
        SDPError(ch, tid, 0x0003);
        return;
    }
    p += hdr + dlen;
    left -= hdr + dlen;

    // MaximumServiceRecordCount
    if (left < 2) { SDPError(ch, tid, 0x0003); return; }
    u32 maxrecords = (u32)((p[0] << 8) | p[1]);
    p += 2;
    left -= 2;

    // ContinuationState: one 4-byte handle never needs continuing, so this is empty
    if (left < 1) { SDPError(ch, tid, 0x0003); return; }
    u32 contlen = p[0];
    if (1 + contlen > left) { SDPError(ch, tid, 0x0003); return; }
    if (contlen != 0) { SDPError(ch, tid, 0x0005); return; }   // invalid continuation state

    // the record matches only if it carries every UUID in the pattern
    bool matches = true;
    for (u32 want : uuids)
    {
        bool found = false;
        for (u32 have : RecordUUIDs)
            if (have == want) found = true;

        if (!found) { matches = false; break; }
    }

    // never offer more handles than fit in one MaxSDPChunk
    u32 maxcount = MaxSDPChunk / 4;
    if (maxrecords > maxcount) maxrecords = maxcount;

    u32 total = matches ? 1u : 0u;
    u32 current = (total < maxrecords) ? total : maxrecords;

    std::vector<u8> rsp;
    rsp.push_back(0x03);                                // ServiceSearchResponse
    rsp.push_back((u8)(tid >> 8));
    rsp.push_back((u8)(tid & 0xFF));

    u32 params = 2 + 2 + current * 4 + 1;
    rsp.push_back((u8)(params >> 8));
    rsp.push_back((u8)(params & 0xFF));
    rsp.push_back((u8)(total >> 8));                    // TotalServiceRecordCount
    rsp.push_back((u8)(total & 0xFF));
    rsp.push_back((u8)(current >> 8));                  // CurrentServiceRecordCount
    rsp.push_back((u8)(current & 0xFF));

    if (current)
    {
        // ServiceRecordHandleList: 4 bytes per handle, bare rather than a data element
        u32 handle = ServiceRecordHandle();
        rsp.push_back((u8)(handle >> 24));
        rsp.push_back((u8)(handle >> 16));
        rsp.push_back((u8)(handle >> 8));
        rsp.push_back((u8)(handle & 0xFF));
    }

    rsp.push_back(0x00);                                // no continuation

    SendOnChannel(ch, rsp.data(), (u32)rsp.size());
}

void BTKeyboard::HandleServiceAttributeRequest(const Channel& ch, u16 tid, const u8* p, u32 left)
{
    // ServiceRecordHandle
    if (left < 4) { SDPError(ch, tid, 0x0003); return; }
    u32 handle = ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
    p += 4;
    left -= 4;

    if (handle != ServiceRecordHandle())
    {
        SDPError(ch, tid, 0x0002);          // invalid service record handle
        return;
    }

    // MaximumAttributeByteCount
    if (left < 2) { SDPError(ch, tid, 0x0003); return; }
    u32 maxbytes = (u32)((p[0] << 8) | p[1]);
    p += 2;
    left -= 2;

    // AttributeIDList
    std::vector<AttrRange> ranges;
    u32 hdr = 0, dlen = 0;
    if (!ElementSize(p, left, hdr, dlen) || !CollectAttrRanges(p, left, ranges))
    {
        SDPError(ch, tid, 0x0003);
        return;
    }
    p += hdr + dlen;
    left -= hdr + dlen;

    // ContinuationState: as in HandleServiceSearchAttributeRequest
    if (left < 1) { SDPError(ch, tid, 0x0003); return; }
    u32 contlen = p[0];
    if (1 + contlen > left) { SDPError(ch, tid, 0x0003); return; }

    u32 offset = 0;
    if (contlen == 2)
        offset = (u32)((p[1] << 8) | p[2]);
    else if (contlen != 0)
    {
        SDPError(ch, tid, 0x0005);          // invalid continuation state
        return;
    }

    // AttributeList: unlike ServiceSearchAttributeResponse, this carries a single
    // sequence of alternating attribute ID and value -- not a sequence of lists.
    std::vector<u8> attrlist;
    for (const SDPAttribute& attr : HIDServiceRecord())
    {
        bool wanted = false;
        for (const AttrRange& r : ranges)
            if (attr.ID >= r.First && attr.ID <= r.Last) wanted = true;

        if (!wanted) continue;

        PutUint16(attrlist, attr.ID);
        attrlist.insert(attrlist.end(), attr.Value.begin(), attr.Value.end());
    }

    std::vector<u8> list;
    list.push_back(0x36);                               // sequence, 16-bit length
    list.push_back((u8)(attrlist.size() >> 8));
    list.push_back((u8)(attrlist.size() & 0xFF));
    list.insert(list.end(), attrlist.begin(), attrlist.end());

    if (offset > list.size()) { SDPError(ch, tid, 0x0005); return; }
    if (maxbytes < 1) maxbytes = 1;
    if (maxbytes > MaxSDPChunk) maxbytes = MaxSDPChunk;

    u32 chunk = (u32)list.size() - offset;
    if (chunk > maxbytes) chunk = maxbytes;

    bool more = (offset + chunk) < list.size();
    u16 next = more ? (u16)(offset + chunk) : 0;

    std::vector<u8> rsp;
    rsp.push_back(0x05);                                // ServiceAttributeResponse
    rsp.push_back((u8)(tid >> 8));
    rsp.push_back((u8)(tid & 0xFF));

    u32 params = 2 + chunk + 1 + (more ? 2u : 0u);
    rsp.push_back((u8)(params >> 8));
    rsp.push_back((u8)(params & 0xFF));
    rsp.push_back((u8)(chunk >> 8));                    // AttributeListByteCount
    rsp.push_back((u8)(chunk & 0xFF));
    rsp.insert(rsp.end(), list.begin() + offset, list.begin() + offset + chunk);

    if (more)
    {
        rsp.push_back(0x02);
        rsp.push_back((u8)(next >> 8));
        rsp.push_back((u8)(next & 0xFF));
    }
    else
    {
        rsp.push_back(0x00);
    }

    SendOnChannel(ch, rsp.data(), (u32)rsp.size());
}

bool BTKeyboard::Connected() const noexcept
{
    bool ctrl = false, intr = false;

    for (const Channel& ch : Channels)
    {
        if (!ch.Configured) continue;
        if (ch.PSM == PSM_HIDControl)   ctrl = true;
        if (ch.PSM == PSM_HIDInterrupt) intr = true;
    }

    return ctrl && intr;
}

void BTKeyboard::Handshake(const Channel& ch, u8 result)
{
    u8 rsp[1] = { result };
    SendOnChannel(ch, rsp, sizeof(rsp));
}

bool BTKeyboard::EnterPairingMode() noexcept
{
    // once a link has come up, Fn is an ordinary key
    if (LinkEverUp) return false;

    PairingRequested = true;

    // switched on mid-scan: answer the pending Inquiry now
    if (InquiryTicksLeft > 0)
        AnswerInquiry();

    return true;
}

// Input report ID 3, battery status. The game's driver asks for it with GET_REPORT
// on the control channel shortly after the link comes up; 0xFF reads as full.
static constexpr u8 BatteryReport[] = { 0xFF };

void BTKeyboard::HandleHID(const Channel& ch, const u8* data, u32 len)
{
    // HIDP transaction header: transaction type in the top nibble, its parameter
    // in the bottom one. For GET_REPORT and SET_REPORT the parameter's low two
    // bits are the report type: 1 input, 2 output, 3 feature.
    if (len < 1) return;

    u8 type  = (u8)(data[0] >> 4);
    u8 param = (u8)(data[0] & 0x0F);
    u8 rtype = (u8)(param & 0x03);

    switch (type)
    {
    case 0x4: // GET_REPORT
        if (rtype == 0x01)
        {
            // a report ID follows the header when the host asks for a specific
            // report; the game asks for ID 3 (battery status)
            if (len >= 2 && data[1] == 0x03)
            {
                u8 rsp[2 + sizeof(BatteryReport)];
                rsp[0] = 0xA1;                      // DATA, input report
                rsp[1] = 0x03;                       // report ID, echoed
                memcpy(&rsp[2], BatteryReport, sizeof(BatteryReport));
                SendOnChannel(ch, rsp, sizeof(rsp));
            }
            else if (len >= 2)
            {
                // no other report is known: still echo the ID, so the reply
                // matches the request
                u8 rsp[2 + sizeof(InputReport)];
                rsp[0] = 0xA1;
                rsp[1] = data[1];
                memcpy(&rsp[2], InputReport, sizeof(InputReport));
                SendOnChannel(ch, rsp, sizeof(rsp));
            }
            else
            {
                u8 rsp[9];
                rsp[0] = 0xA1;                          // DATA, input report
                memcpy(&rsp[1], InputReport, sizeof(InputReport));
                SendOnChannel(ch, rsp, sizeof(rsp));
            }
        }
        else if (rtype == 0x02)
        {
            u8 rsp[2] = { 0xA2, OutputReport };     // DATA, output report
            SendOnChannel(ch, rsp, sizeof(rsp));
        }
        else
        {
            Handshake(ch, 0x03);                    // unsupported request
        }
        return;

    case 0x5: // SET_REPORT
        if (rtype == 0x01 && len >= 1 + sizeof(InputReport))
        {
            memcpy(InputReport, &data[1], sizeof(InputReport));
            Handshake(ch, 0x00);
        }
        else if (rtype == 0x02 && len >= 2)
        {
            OutputReport = data[1];
            Handshake(ch, 0x00);
        }
        else
        {
            Handshake(ch, 0x04);                    // invalid parameter
        }
        return;

    case 0x6: // GET_PROTOCOL
        {
            u8 rsp[2] = { 0xA0, (u8)(BootProtocol ? 0x00 : 0x01) };
            SendOnChannel(ch, rsp, sizeof(rsp));
        }
        return;

    case 0x7: // SET_PROTOCOL
        BootProtocol = ((param & 0x01) == 0);
        Handshake(ch, 0x00);
        return;

    case 0x0: // HANDSHAKE from the host
    case 0x1: // HID_CONTROL
    case 0xA: // DATA
        return;

    default:
        Handshake(ch, 0x03);                        // unsupported request
        return;
    }
}

void BTKeyboard::HandleL2CAP(u16 cid, const u8* data, u32 len)
{
    if (cid != SignalCID)
    {
        Channel* ch = FindChannelByLocal(cid);
        if (!ch) return;

        if (ch->PSM == 0x0001)
            HandleSDP(*ch, data, len);
        else if (ch->PSM == PSM_HIDControl || ch->PSM == PSM_HIDInterrupt)
            HandleHID(*ch, data, len);

        return;
    }

    if (len < 4) return;

    u8 code = data[0];
    u8 id   = data[1];
    u32 plen = (u32)(data[2] | (data[3] << 8));
    const u8* params = &data[4];
    if (4 + plen > len) return;

    switch (code)
    {
    case 0x02: // Connection Request: PSM (2), SCID (2)
        {
            if (plen < 4) return;
            u16 psm  = (u16)(params[0] | (params[1] << 8));
            u16 scid = (u16)(params[2] | (params[3] << 8));

            // a savestate couldn't hold another
            if (Channels.size() >= MaxSavedChannels)
            {
                u8 rsp[8];
                rsp[0] = 0x00; rsp[1] = 0x00;           // destination CID: none
                rsp[2] = (u8)(scid & 0xFF);             // source CID: theirs
                rsp[3] = (u8)(scid >> 8);
                rsp[4] = 0x04; rsp[5] = 0x00;           // result: refused, no resources
                rsp[6] = 0x00; rsp[7] = 0x00;           // status: no further information
                Signal(0x03, id, rsp, sizeof(rsp));
                return;
            }

            Channel ch;
            ch.LocalCID  = NextLocalCID++;
            ch.RemoteCID = scid;
            ch.PSM       = psm;
            ch.Configured = false;
            Channels.push_back(ch);

            u8 rsp[8];
            rsp[0] = (u8)(ch.LocalCID & 0xFF);      // destination CID: ours
            rsp[1] = (u8)(ch.LocalCID >> 8);
            rsp[2] = (u8)(scid & 0xFF);             // source CID: theirs
            rsp[3] = (u8)(scid >> 8);
            rsp[4] = 0x00; rsp[5] = 0x00;           // result: connection successful
            rsp[6] = 0x00; rsp[7] = 0x00;           // status: no further information
            Signal(0x03, id, rsp, sizeof(rsp));

            // both ends configure; ask for the defaults by sending no options
            u8 req[4];
            req[0] = (u8)(scid & 0xFF);             // destination CID: theirs
            req[1] = (u8)(scid >> 8);
            req[2] = 0x00; req[3] = 0x00;           // flags: no continuation
            Signal(0x04, NextSignalID++, req, sizeof(req));
        }
        return;

    case 0x03: // Connection Response to our own request: DCID (2), SCID (2), result (2), status (2)
        {
            if (plen < 6) return;
            u16 dcid   = (u16)(params[0] | (params[1] << 8));
            u16 scid   = (u16)(params[2] | (params[3] << 8));
            u16 result = (u16)(params[4] | (params[5] << 8));

            Channel* ch = FindChannelByLocal(scid);
            if (!ch) return;

            // 0x0000 success, 0x0001 pending, anything else refused. On a bonded
            // reconnect the game answers pending before success, so pending
            // must keep the channel.
            if (result == 0x0001)
                return;

            if (result != 0x0000)
            {
                // refused: drop the channel, MaxHIDConnectAttempts bounds retries
                for (auto it = Channels.begin(); it != Channels.end(); ++it)
                    if (it->LocalCID == scid) { Channels.erase(it); break; }
                return;
            }

            ch->RemoteCID = dcid;

            // both ends configure; ask for the defaults by sending no options
            u8 req[4];
            req[0] = (u8)(dcid & 0xFF);             // destination CID: theirs
            req[1] = (u8)(dcid >> 8);
            req[2] = 0x00; req[3] = 0x00;           // flags: no continuation
            Signal(0x04, NextSignalID++, req, sizeof(req));
        }
        return;

    case 0x04: // Configuration Request: DCID (2), flags (2), options
        {
            if (plen < 4) return;
            u16 dcid = (u16)(params[0] | (params[1] << 8));

            Channel* ch = FindChannelByLocal(dcid);
            if (!ch)
            {
                // the request only names our end
                RejectInvalidCID(id, dcid, 0x0000);
                return;
            }

            ch->Configured = true;

            // both HID channels up: give OpenHIDChannels() its budget back
            if (Connected()) HIDConnectAttempts = 0;

            // accept whatever was asked for: result success, no options echoed
            u8 rsp[6];
            rsp[0] = (u8)(ch->RemoteCID & 0xFF);    // source CID: theirs
            rsp[1] = (u8)(ch->RemoteCID >> 8);
            rsp[2] = 0x00; rsp[3] = 0x00;           // flags: no continuation
            rsp[4] = 0x00; rsp[5] = 0x00;           // result: success
            Signal(0x05, id, rsp, sizeof(rsp));
        }
        return;

    case 0x05: // Configuration Response to our own request -- nothing to do
        return;

    case 0x06: // Disconnection Request: DCID (2), SCID (2)
        {
            if (plen < 4) return;
            u16 dcid = (u16)(params[0] | (params[1] << 8));
            u16 scid = (u16)(params[2] | (params[3] << 8));

            auto it = Channels.begin();
            while (it != Channels.end() && it->LocalCID != dcid)
                ++it;

            if (it == Channels.end())
            {
                RejectInvalidCID(id, dcid, scid);
                return;
            }

            // our channel named with someone else's CID: the spec says drop it silently
            if (it->RemoteCID != scid) return;

            Channels.erase(it);
            Signal(0x07, id, params, 4);            // Disconnection Response
        }
        return;

    case 0x0A: // Information Request: type (2)
        {
            if (plen < 2) return;

            u8 rsp[12];
            memset(rsp, 0, sizeof(rsp));
            rsp[0] = params[0];                     // type echoed
            rsp[1] = params[1];
            rsp[2] = 0x00; rsp[3] = 0x00;           // result: success
            // connectionless MTU and extended features both answer as zero, which
            // means "nothing beyond basic mode" -- all this link needs
            u32 datalen = (params[0] == 0x01) ? 2 : 4;
            Signal(0x0B, id, rsp, 4 + datalen);
        }
        return;

    default:
        // Command Reject: reason 0 (command not understood)
        {
            u8 rej[2] = { 0x00, 0x00 };
            Signal(0x01, id, rej, sizeof(rej));
        }
        return;
    }
}

void BTKeyboard::HandleCommand(u16 opcode, const u8* params, u32 plen)
{
    u8 ret[300];
    memset(ret, 0, sizeof(ret));
    // ret[0] is the status byte, and zero means success

    switch (opcode)
    {
    case 0x1001: // Read Local Version Information
        ret[1] = 0x04;                      // HCI version (Bluetooth 2.1)
        ret[2] = 0x1B; ret[3] = 0x00;       // HCI revision
        ret[4] = 0x04;                      // LMP version (Bluetooth 2.1)
        ret[5] = 0x0F; ret[6] = 0x00;       // manufacturer: Broadcom
        ret[7] = 0x1B; ret[8] = 0x00;       // LMP subversion
        CommandComplete(opcode, ret, 9);
        return;

    case 0x1003: // Read Local Supported Features
        memset(&ret[1], 0xFF, 8);
        CommandComplete(opcode, ret, 9);
        return;

    case 0x1005: // Read Buffer Size
        ret[1] = 0x39; ret[2] = 0x00;       // ACL data packet length
        ret[3] = 0x40;                      // synchronous data packet length
        ret[4] = 0x0A; ret[5] = 0x00;       // total ACL packets
        ret[6] = 0x08; ret[7] = 0x00;       // total synchronous packets
        CommandComplete(opcode, ret, 8);
        return;

    case 0x1009: // Read BD_ADDR
        ret[1] = 0x11; ret[2] = 0x22; ret[3] = 0x33;
        ret[4] = 0x44; ret[5] = 0x55; ret[6] = 0x66;
        CommandComplete(opcode, ret, 7);
        return;

    case 0x1405: // Read_RSSI: handle (2, LE)
        // the game polls this about every ten seconds while the link is up
        ret[1] = (u8)(ConnHandle & 0xFF);
        ret[2] = (u8)(ConnHandle >> 8);
        ret[3] = 0x00;                      // RSSI: inside the golden receive power range
        CommandComplete(opcode, ret, 4);
        return;

    case 0x0401: // Inquiry: LAP (3), inquiry length (1, units of 1.28 s), max responses (1)
        CommandStatus(opcode, 0x00);
        Inquiry((plen >= 4) ? params[3] : 0x0A);
        return;

    case 0x0402: // Inquiry_Cancel
        InquiryTicksLeft = 0;               // nothing left to complete
        CommandComplete(opcode, ret, 1);
        return;

    case 0x0419: // Remote Name Request
        CommandStatus(opcode, 0x00);
        RemoteName();
        return;

    case 0x0405: // Create Connection
        CommandStatus(opcode, 0x00);
        Connect();
        return;

    case 0x0406: // Disconnect: handle (2, LE), reason (1)
        CommandStatus(opcode, 0x00);
        // the channels go with the link; PageTick() brings the link back
        LinkUp = false;
        Channels.clear();
        {
            u8 reason = (plen >= 3) ? params[2] : 0x00;
            u8 ev[4];
            ev[0] = 0x00;                   // status: success
            ev[1] = (u8)(ConnHandle & 0xFF);
            ev[2] = (u8)(ConnHandle >> 8);
            ev[3] = reason;                 // reason echoed
            Event(0x05, ev, sizeof(ev));    // Disconnection Complete
        }
        return;

    case 0x0409: // Accept_Connection_Request: BD_ADDR (6), role (1)
        CommandStatus(opcode, 0x00);
        // the game accepting a page we sent from PageTick()
        Connect(0x00, true);
        return;

    case 0x040A: // Reject_Connection_Request: BD_ADDR (6), reason (1)
        CommandStatus(opcode, 0x00);
        {
            u8 reason = (plen >= 7) ? params[6] : 0x00;
            Connect(reason);             // Connection Complete, carrying the failure
        }
        return;

    case 0x041B: // Read Remote Supported Features
        CommandStatus(opcode, 0x00);
        {
            u8 ev[11];
            ev[0] = 0x00;                   // status: success
            ev[1] = (u8)(ConnHandle & 0xFF);
            ev[2] = (u8)(ConnHandle >> 8);
            memset(&ev[3], 0xFF, 8);        // LMP features
            Event(0x0B, ev, sizeof(ev));    // Read Remote Supported Features Complete
        }
        return;

    case 0x041C: // Read Remote Extended Features: handle (2, LE), page number (1)
        CommandStatus(opcode, 0x00);
        {
            u8 page = (plen >= 3) ? params[2] : 0x00;
            u8 ev[13];
            ev[0] = 0x00;                   // status: success
            ev[1] = (u8)(ConnHandle & 0xFF);
            ev[2] = (u8)(ConnHandle >> 8);
            ev[3] = page;                   // page number echoed
            ev[4] = 0x01;                   // maximum page number
            memset(&ev[5], 0x00, 8);        // features: none -- no Secure Simple Pairing
            Event(0x23, ev, sizeof(ev));    // Read Remote Extended Features Complete
        }
        return;

    case 0x041D: // Read Remote Version Information
        CommandStatus(opcode, 0x00);
        {
            u8 ev[8];
            ev[0] = 0x00;                   // status: success
            ev[1] = (u8)(ConnHandle & 0xFF);
            ev[2] = (u8)(ConnHandle >> 8);
            ev[3] = 0x04;                   // LMP version (Bluetooth 2.1)
            ev[4] = 0x0F; ev[5] = 0x00;     // manufacturer: Broadcom
            ev[6] = 0x1B; ev[7] = 0x00;     // LMP subversion
            Event(0x0C, ev, sizeof(ev));    // Read Remote Version Complete
        }
        return;

    case 0x041F: // Read Clock Offset
        CommandStatus(opcode, 0x00);
        {
            u8 ev[5];
            ev[0] = 0x00;                   // status: success
            ev[1] = (u8)(ConnHandle & 0xFF);
            ev[2] = (u8)(ConnHandle >> 8);
            ev[3] = 0x00; ev[4] = 0x00;     // clock offset
            Event(0x1C, ev, sizeof(ev));    // Read Clock Offset Complete
        }
        return;

    case 0x0C03: // HCI_Reset
        // The controller drops the link with everything on it, forgets its scans and
        // discards the events the host hasn't read. The host starts over, so the
        // keyboard's retry budgets do too.
        Outgoing.clear();
        Channels.clear();
        LinkUp = false;
        PageScanEnabled = false;
        InquiryTicksLeft = 0;
        PageAttempts = 0;
        HIDConnectAttempts = 0;
        CommandComplete(opcode, ret, 1);
        return;

    case 0x0C12: // Delete_Stored_Link_Key: BD_ADDR (6), delete-all flag (1)
        ret[1] = 0x00; ret[2] = 0x00;       // keys deleted: we store none
        CommandComplete(opcode, ret, 3);
        return;

    case 0x0C1A: // Write_Scan_Enable: scan enable (1) -- bit 0 inquiry scan, bit 1 page scan
        // page scan on: the DS is waiting to be paged, which PageTick() does
        PageScanEnabled = (plen >= 1) && ((params[0] & 0x02) != 0);
        CommandComplete(opcode, ret, 1);
        return;

    case 0x0411: // Authentication_Requested: handle (2, LE)
        CommandStatus(opcode, 0x00);
        Event(0x17, KeyboardAddr, sizeof(KeyboardAddr));    // Link_Key_Request
        return;

    case 0x040B: // Link_Key_Request_Reply: BD_ADDR (6), link key (16)
        {
            // the host claims to already hold our link key; we never check it
            memcpy(&ret[1], KeyboardAddr, 6);
            CommandComplete(opcode, ret, 7);

            u8 ev[3];
            ev[0] = 0x00;                   // status: success
            ev[1] = (u8)(ConnHandle & 0xFF);
            ev[2] = (u8)(ConnHandle >> 8);
            Event(0x06, ev, sizeof(ev));    // Authentication_Complete
        }
        return;

    case 0x040C: // Link_Key_Request_Negative_Reply: BD_ADDR (6)
        memcpy(&ret[1], KeyboardAddr, 6);
        CommandComplete(opcode, ret, 7);
        Event(0x16, KeyboardAddr, sizeof(KeyboardAddr));    // PIN_Code_Request
        return;

    case 0x040D: // PIN_Code_Request_Reply: BD_ADDR (6), PIN length (1), PIN (16)
        {
            // accept whatever PIN the host offers
            memcpy(&ret[1], KeyboardAddr, 6);
            CommandComplete(opcode, ret, 7);

            u8 notify[23];
            memcpy(&notify[0], KeyboardAddr, 6);
            memcpy(&notify[6], LinkKey, 16);
            notify[22] = 0x00;              // key type: combination key
            Event(0x18, notify, sizeof(notify));    // Link_Key_Notification

            u8 ev[3];
            ev[0] = 0x00;                   // status: success
            ev[1] = (u8)(ConnHandle & 0xFF);
            ev[2] = (u8)(ConnHandle >> 8);
            Event(0x06, ev, sizeof(ev));    // Authentication_Complete
        }
        return;

    case 0x040E: // PIN_Code_Request_Negative_Reply: BD_ADDR (6)
        {
            // the host declines to offer a PIN at all; there is no key to hand
            // out, so authentication simply fails
            memcpy(&ret[1], KeyboardAddr, 6);
            CommandComplete(opcode, ret, 7);

            u8 ev[3];
            ev[0] = 0x05;                   // status: authentication failure
            ev[1] = (u8)(ConnHandle & 0xFF);
            ev[2] = (u8)(ConnHandle >> 8);
            Event(0x06, ev, sizeof(ev));    // Authentication_Complete
        }
        return;

    case 0x0413: // Set_Connection_Encryption: handle (2, LE), enable flag (1)
        CommandStatus(opcode, 0x00);
        {
            u8 enable = (plen >= 3) ? params[2] : 0x00;
            u8 ev[4];
            ev[0] = 0x00;                   // status: success
            ev[1] = (u8)(ConnHandle & 0xFF);
            ev[2] = (u8)(ConnHandle >> 8);
            ev[3] = enable;                 // encryption enabled, echoed
            Event(0x08, ev, sizeof(ev));    // Encryption_Change
        }
        return;

    case 0x0803: // Sniff_Mode: handle (2, LE), max/min interval, attempt, timeout
                 // (2, LE each)
        CommandStatus(opcode, 0x00);
        {
            u8 maxlo = (plen >= 4) ? params[2] : 0x00;
            u8 maxhi = (plen >= 4) ? params[3] : 0x00;
            u8 ev[6];
            ev[0] = 0x00;                   // status: success
            ev[1] = (u8)(ConnHandle & 0xFF);
            ev[2] = (u8)(ConnHandle >> 8);
            ev[3] = 0x02;                   // current mode: sniff
            ev[4] = maxlo;                  // interval: the max interval asked for
            ev[5] = maxhi;
            Event(0x14, ev, sizeof(ev));    // Mode_Change
        }
        return;

    case 0xFC4C: // Broadcom vendor: Write_RAM (4-byte LE address, then data)
        {
            // the boot-time format and every save land here (see FlashBackend)
            if (Flash && plen > 4)
            {
                u32 addr = ((u32)params[0] | ((u32)params[1] << 8) | ((u32)params[2] << 16) | ((u32)params[3] << 24));
                Flash->Write(addr, &params[4], plen - 4);
            }
            CommandComplete(opcode, ret, 1);
        }
        return;

    case 0xFF5E: // Broadcom vendor: erase the flash sector at a 4-byte LE address
        {
            // the game erases a sector before rewriting it, pairing info and save alike
            if (Flash && plen >= 4)
            {
                u32 addr = ((u32)params[0] | ((u32)params[1] << 8) | ((u32)params[2] << 16) | ((u32)params[3] << 24));
                Flash->EraseSector(addr);
            }
            CommandComplete(opcode, ret, 1);
        }
        return;

    case 0xFC4D: // Broadcom vendor: Read_RAM (4-byte LE address, then a length)
        {
            // Read while applying the firmware patch at boot, and to load the save by
            // sweeping for its signature. The driver needs the byte count it asked
            // for, so with no backend this still answers, with zeros.
            u32 addr = (plen >= 4) ? ((u32)params[0] | ((u32)params[1] << 8) | ((u32)params[2] << 16) | ((u32)params[3] << 24)) : 0;
            u32 n = (plen >= 5) ? params[4] : 0;

            // an event's 255 parameter bytes leave room for 251 bytes of data
            if (n > 251)
            {
                ret[0] = 0x12;              // status: invalid HCI command parameters
                CommandComplete(opcode, ret, 1);
                return;
            }

            if (Flash && n) Flash->Read(addr, &ret[1], n);
            CommandComplete(opcode, ret, 1 + n);
        }
        return;

    default:
        // everything else -- the rest of the Broadcom vendor commands included --
        // just needs a success status
        CommandComplete(opcode, ret, 1);
        return;
    }
}

}
