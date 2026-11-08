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

#ifndef POKETYPE_BTKEYBOARDSUPPORT_H
#define POKETYPE_BTKEYBOARDSUPPORT_H

// Packet helpers for tests that drive a BTKeyboard over HCI and L2CAP.

#include <vector>

#include "NDSCart/BTKeyboard.h"

// Build an HCI command packet: type 01, opcode LE, parameter length, parameters.
inline std::vector<melonDS::u8> Command(melonDS::u16 opcode, const std::vector<melonDS::u8>& params)
{
    using melonDS::u8;

    std::vector<u8> pkt;
    pkt.push_back(0x01);
    pkt.push_back((u8)(opcode & 0xFF));
    pkt.push_back((u8)(opcode >> 8));
    pkt.push_back((u8)params.size());
    pkt.insert(pkt.end(), params.begin(), params.end());
    return pkt;
}

inline void Feed(melonDS::NDSCart::BTKeyboard& kb, const std::vector<melonDS::u8>& pkt)
{
    kb.HostPacket(pkt.data(), (melonDS::u32)pkt.size());
}

inline std::vector<std::vector<melonDS::u8>> Drain(melonDS::NDSCart::BTKeyboard& kb)
{
    std::vector<std::vector<melonDS::u8>> out;
    std::vector<melonDS::u8> pkt;
    while (kb.NextPacket(pkt))
        out.push_back(pkt);
    return out;
}

// Build an ACL packet on handle 0x0042 carrying one L2CAP frame.
inline std::vector<melonDS::u8> Acl(melonDS::u16 cid, const std::vector<melonDS::u8>& payload)
{
    using melonDS::u8;
    using melonDS::u16;

    std::vector<u8> pkt;
    pkt.push_back(0x02);                            // HCI ACL data packet
    pkt.push_back(0x42);                            // handle low, PB/BC flags 0
    pkt.push_back(0x00);                            // handle high
    u16 total = (u16)(payload.size() + 4);
    pkt.push_back((u8)(total & 0xFF));              // ACL data total length
    pkt.push_back((u8)(total >> 8));
    pkt.push_back((u8)(payload.size() & 0xFF));     // L2CAP length
    pkt.push_back((u8)(payload.size() >> 8));
    pkt.push_back((u8)(cid & 0xFF));                // L2CAP CID
    pkt.push_back((u8)(cid >> 8));
    pkt.insert(pkt.end(), payload.begin(), payload.end());
    return pkt;
}

// Open and configure one L2CAP channel for a PSM, returning the keyboard's CID.
inline melonDS::u16 OpenChannel(melonDS::NDSCart::BTKeyboard& kb, melonDS::u16 psm,
                                melonDS::u16 scid, melonDS::u8 id)
{
    using melonDS::u8;
    using melonDS::u16;

    // Connection Request
    Feed(kb, Acl(0x0001, {0x02, id, 0x04, 0x00,
                          (u8)(psm & 0xFF), (u8)(psm >> 8),
                          (u8)(scid & 0xFF), (u8)(scid >> 8)}));
    auto pkts = Drain(kb);
    if (pkts.empty()) return 0;

    // DCID from the Connection Response
    u16 local = (u16)(pkts[0][13] | (pkts[0][14] << 8));

    // Configure Request, no options
    Feed(kb, Acl(0x0001, {0x04, (u8)(id + 1), 0x04, 0x00,
                          (u8)(local & 0xFF), (u8)(local >> 8), 0x00, 0x00}));
    Drain(kb);
    return local;
}

#endif // POKETYPE_BTKEYBOARDSUPPORT_H
