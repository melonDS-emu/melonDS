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

#ifndef NDSCART_BTKEYBOARD_H
#define NDSCART_BTKEYBOARD_H

#include <deque>
#include <vector>

#include "../types.h"
#include "../Savestate.h"

namespace melonDS::NDSCart
{

// The Bluetooth keyboard Pokémon Typing Adventure shipped with. There is one peer
// and the game drives the conversation, so every answer is scripted to match
// what the game's driver asks for.
//
// Packets carry the HCI packet type in byte 0: 0x01 command, 0x02 ACL data,
// 0x04 event. The cart's SPI framing is handled by CartRetailBT.
class BTKeyboard
{
public:
    BTKeyboard() { Reset(); }

    void Reset() noexcept;

    /// Appends to the savestate section the caller already has open (the cart's)
    /// rather than opening its own.
    void DoSavestate(Savestate* file);

    // A savestate always holds this many channels and this many bytes of unread
    // packets, since frontends like melonDS DS need every state of a game to be
    // one size. The link never comes close to either.
    static constexpr u32 MaxSavedChannels = 64;
    static constexpr u32 SavedPacketBytes = 4096;

    /// Feed one HCI packet from the host.
    void HostPacket(const u8* data, u32 len);

    /// Take the next packet the controller wants to send.
    /// Returns false when there is nothing pending.
    bool NextPacket(std::vector<u8>& out);

    /// Whether both HID channels are up, which is when the game considers a
    /// keyboard present.
    [[nodiscard]] bool Connected() const noexcept;

    /// The "Automatically send Fn on start" setting: answer Inquiry from power-on,
    /// as if switched on with Fn held. A user setting, so Reset() leaves it alone.
    void SetAutoPair(bool enable) noexcept { AutoPair = enable; }

    /// The player's Fn gesture: answer Inquiry for the rest of this session.
    /// Returns false once the game has connected, when Fn is an ordinary key.
    bool EnterPairingMode() noexcept;

    /// Whether the keyboard currently answers Inquiry.
    [[nodiscard]] bool Discoverable() const noexcept { return AutoPair || PairingRequested; }

    /// Send the empty Inquiry Complete once an Inquiry the keyboard sat out has
    /// run its length. Driven by the cart's 2-second timer; returns whether it was sent.
    bool InquiryTick() noexcept;

    /// Whether the game's driver has this keyboard: a link has been up, or page scan is on.
    /// From then the key injector must not force the game's "keyboard present" flag.
    [[nodiscard]] bool GameHasKeyboard() const noexcept { return LinkEverUp || PageScanEnabled; }

    /// With page scan on and no link up, send a Connection_Request, the way a bonded
    /// keyboard reconnects. Driven by the cart's 2-second timer; returns whether one was sent.
    bool PageTick() noexcept;

    struct SDPAttribute
    {
        u16 ID;
        std::vector<u8> Value;
    };

    // Typing Adventure keeps its save in the controller's on-board flash, reached with
    // the Broadcom vendor commands Read_RAM (0xFC4D), Write_RAM (0xFC4C) and sector erase
    // (0xFF5E). Addresses arrive as on the wire. With no backend installed the commands
    // are acknowledged and their data discarded.
    struct FlashBackend
    {
        virtual ~FlashBackend() {}
        virtual void Read(u32 addr, u8* out, u32 len) = 0;
        virtual void Write(u32 addr, const u8* in, u32 len) = 0;
        virtual void EraseSector(u32 addr) = 0;
    };

    /// Install the cart's flash backend. Reset() and DoSavestate() leave it alone.
    void SetFlash(FlashBackend* flash) noexcept { Flash = flash; }

private:
    void RejectSavestate(Savestate* file) noexcept;

    void HandleCommand(u16 opcode, const u8* params, u32 plen);
    void HandleACL(const u8* data, u32 len);
    void HandleL2CAP(u16 cid, const u8* data, u32 len);
    void SendACL(u16 cid, const u8* payload, u32 plen);
    void CompletedPackets();

    void Event(u8 code, const u8* params, u32 plen);
    void CommandComplete(u16 opcode, const u8* ret, u32 retlen);
    void CommandStatus(u16 opcode, u8 status);

    void Inquiry(u8 length);
    void AnswerInquiry();
    void RemoteName();
    void Connect(u8 status = 0x00, bool weInitiated = false);

    struct Channel
    {
        u16 LocalCID;
        u16 RemoteCID;
        u16 PSM;
        bool Configured;
    };

    void Signal(u8 code, u8 id, const u8* data, u32 len);
    void RejectInvalidCID(u8 id, u16 localcid, u16 remotecid);
    Channel* FindChannelByLocal(u16 cid);
    void SendOnChannel(const Channel& ch, const u8* payload, u32 plen);

    void OpenHIDChannels();
    void OpenChannel(u16 psm);

    void HandleSDP(const Channel& ch, const u8* data, u32 len);
    void HandleServiceSearchAttributeRequest(const Channel& ch, u16 tid, const u8* p, u32 len);
    void HandleServiceSearchRequest(const Channel& ch, u16 tid, const u8* p, u32 len);
    void HandleServiceAttributeRequest(const Channel& ch, u16 tid, const u8* p, u32 len);
    void SDPError(const Channel& ch, u16 tid, u16 code);

    void HandleHID(const Channel& ch, const u8* data, u32 len);
    void Handshake(const Channel& ch, u8 result);

    std::vector<Channel> Channels;
    u16 NextLocalCID = 0;
    u8 NextSignalID = 0;

    // boot-keyboard input report: modifier byte, reserved byte, six key codes
    u8 InputReport[8] {};
    u8 OutputReport = 0;
    bool BootProtocol = true;

    // AutoPair mirrors the user setting; PairingRequested is the player's Fn
    // gesture this session. LinkEverUp is sticky for the session, unlike LinkUp.
    bool AutoPair = true;
    bool PairingRequested = false;
    bool LinkEverUp = false;

    // timer ticks left on an Inquiry the keyboard is sitting out, 0 if none
    u32 InquiryTicksLeft = 0;

    // PageScanEnabled is the page-scan bit of the last Write_Scan_Enable.
    // LinkUp is whether an ACL link is up right now, whichever side opened it.
    bool PageScanEnabled = false;
    bool LinkUp = false;
    u32 PageAttempts = 0;

    // Counts OpenHIDChannels() calls. Reset only once both HID channels are
    // configured, not on LinkUp, so a DS that accepts our page but never lets
    // the channels finish opening still hits the cap.
    u32 HIDConnectAttempts = 0;

    FlashBackend* Flash = nullptr;

    std::deque<std::vector<u8>> Outgoing;
};

}

#endif // NDSCART_BTKEYBOARD_H
