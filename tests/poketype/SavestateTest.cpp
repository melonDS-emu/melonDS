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

#include "BTKeyboardSupport.h"
#include "NDSCart/BTKeyboard.h"
#include "NDSSupport.h"
#include "Savestate.h"
#include "TestSupport.h"

using melonDS::u8;
using melonDS::u16;
using melonDS::u32;
using melonDS::u64;
using melonDS::Savestate;
using melonDS::NDSCart::BTKeyboard;

namespace
{

struct HIDChannels
{
    u16 Ctrl;
    u16 Intr;
};

// Connect as the game does: the HCI link first, then both HID channels. Returns
// the keyboard's CIDs.
HIDChannels Connect(BTKeyboard& kb)
{
    // HCI_Create_Connection
    Feed(kb, Command(0x0405, {0x33, 0x22, 0x11, 0x32, 0x1F, 0x00,
                              0x18, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}));
    Drain(kb);

    HIDChannels ch {};
    ch.Ctrl = OpenChannel(kb, 0x0011, 0x0042, 0x10);
    ch.Intr = OpenChannel(kb, 0x0013, 0x0043, 0x20);
    return ch;
}

std::vector<u8> SaveKeyboard(BTKeyboard& kb)
{
    Savestate save(1024 * 1024);
    save.Section("NDCS");
    kb.DoSavestate(&save);
    save.Finish();

    const u8* p = (const u8*)save.Buffer();
    return std::vector<u8>(p, p + save.Length());
}

void LoadKeyboard(BTKeyboard& kb, std::vector<u8>& state)
{
    Savestate load(state.data(), (u32)state.size(), false);
    load.Section("NDCS");
    kb.DoSavestate(&load);
    CHECK(!load.Error);
}

void testAConnectedKeyboardSurvivesARoundTrip()
{
    BTKeyboard saved;
    saved.Reset();
    Connect(saved);
    CHECK(saved.Connected());

    auto state = SaveKeyboard(saved);

    BTKeyboard loaded;
    loaded.Reset();
    CHECK(!loaded.Connected());

    LoadKeyboard(loaded, state);
    CHECK(loaded.Connected());
    CHECK(loaded.GameHasKeyboard());
}

void testPendingPacketsComeBackInOrder()
{
    BTKeyboard saved;
    saved.Reset();
    Connect(saved);

    // an Inquiry the keyboard answers at once: Command Status, Result, Complete
    Feed(saved, Command(0x0401, {0x33, 0x8B, 0x9E, 0x0A, 0x00}));
    auto expected = Drain(saved);
    CHECK_EQ(expected.size(), 3);

    // queue them again and save without draining
    Feed(saved, Command(0x0401, {0x33, 0x8B, 0x9E, 0x0A, 0x00}));

    auto state = SaveKeyboard(saved);

    BTKeyboard loaded;
    loaded.Reset();
    LoadKeyboard(loaded, state);

    auto actual = Drain(loaded);
    CHECK_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < actual.size() && i < expected.size(); i++)
    {
        CHECK_EQ(actual[i].size(), expected[i].size());
        for (size_t j = 0; j < actual[i].size() && j < expected[i].size(); j++)
            CHECK_EQ(actual[i][j], expected[i][j]);
    }
}

void testARestoredKeyboardAnswersTheNextPacketIdentically()
{
    BTKeyboard saved;
    saved.Reset();
    HIDChannels ch = Connect(saved);

    // SET_REPORT (input): left shift + A, so the probe below depends on restored
    // device state and not just restored channels
    Feed(saved, Acl(ch.Ctrl, {0x51, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}));
    Drain(saved);

    auto state = SaveKeyboard(saved);

    BTKeyboard loaded;
    loaded.Reset();
    LoadKeyboard(loaded, state);

    // GET_REPORT (input) on the control channel
    auto probe = Acl(ch.Ctrl, {0x41});
    Feed(saved, probe);
    Feed(loaded, probe);

    auto a = Drain(saved);
    auto b = Drain(loaded);
    CHECK(!a.empty());                      // guard against two empty replies
    CHECK_EQ(b.size(), a.size());
    for (size_t i = 0; i < a.size() && i < b.size(); i++)
    {
        CHECK_EQ(b[i].size(), a[i].size());
        for (size_t j = 0; j < a[i].size() && j < b[i].size(); j++)
            CHECK_EQ(b[i][j], a[i][j]);
    }
}

void testAutoPairIsNotRestoredFromAState()
{
    // AutoPair mirrors the "Automatically send Fn on start" setting, not session
    // state, so loading a state must not override it
    BTKeyboard saved;
    saved.Reset();
    saved.SetAutoPair(true);
    CHECK(saved.Discoverable());

    auto state = SaveKeyboard(saved);

    BTKeyboard loaded;
    loaded.Reset();
    loaded.SetAutoPair(false);
    LoadKeyboard(loaded, state);

    CHECK(!loaded.Discoverable());
}

// Frontends like melonDS DS size a savestate once per game and refuse any other
// size, so the keyboard writes the same number of bytes whatever the link is doing.
void testTheStateSizeDoesNotDependOnTheLink()
{
    BTKeyboard kb;
    kb.Reset();
    size_t idle = SaveKeyboard(kb).size();

    Connect(kb);                            // two HID channels
    CHECK_EQ(SaveKeyboard(kb).size(), idle);

    // an Inquiry the keyboard answers at once: three packets left unread
    Feed(kb, Command(0x0401, {0x33, 0x8B, 0x9E, 0x0A, 0x00}));
    CHECK_EQ(SaveKeyboard(kb).size(), idle);
}

// With a fixed-size state, a queue too long to fit must fail the save rather
// than write a state that loads back without some of its packets.
void testAQueueTooLongForAStateFailsTheSave()
{
    BTKeyboard kb;
    kb.Reset();

    // each Inquiry leaves three packets unread
    for (int i = 0; i < 1000; i++)
        Feed(kb, Command(0x0401, {0x33, 0x8B, 0x9E, 0x0A, 0x00}));

    Savestate save(1024 * 1024);
    save.Section("NDCS");
    kb.DoSavestate(&save);
    CHECK(save.Error);
}

// Marks on the scheduler and on the fields NDS::DoSavestate writes after it, so a
// load that reads the event entries out of step shows up in everything after them.
constexpr u64 CartTimerStamp = 0x7172737475767778;
constexpr u32 CartTimerFuncID = 0x01020304;
constexpr u32 CartTimerParam = 0x05060708;
constexpr u64 DSiEventStamp = 0x5152535455565758;
constexpr u32 SavedEventMask = 0x00000005;

void MarkScheduler(TestNDS& nds)
{
    melonDS::SchedEvent& cart = nds.SchedList[melonDS::Event_CartBTKeyboardTimer];
    cart.Timestamp = CartTimerStamp;
    cart.FuncID = CartTimerFuncID;
    cart.Param = CartTimerParam;

    nds.SchedList[melonDS::Event_DSi_SDMMCTransfer].Timestamp = DSiEventStamp;

    nds.EventMask() = SavedEventMask;
    nds.ARM9Timestamp = 0x0102030405060708;
    nds.ARM7Timestamp = 0x1112131415161718;
    nds.NumFrames = 0x21222324;
    nds.RCnt = 0x4142;
}

void CheckEverythingButTheCartTimer(TestNDS& nds)
{
    CHECK_EQ(nds.SchedList[melonDS::Event_DSi_SDMMCTransfer].Timestamp, DSiEventStamp);
    CHECK_EQ(nds.EventMask(), SavedEventMask);
    CHECK_EQ(nds.ARM9Timestamp, 0x0102030405060708);
    CHECK_EQ(nds.ARM7Timestamp, 0x1112131415161718);
    CHECK_EQ(nds.NumFrames, 0x21222324);
    CHECK_EQ(nds.RCnt, 0x4142);
}

// The scheduler mask is saved as bits numbered by event, so every event a 14.0
// state knows keeps its number, and the cart timer comes after them.
void testEventsFrom14_0KeepTheirNumbers()
{
    CHECK_EQ(melonDS::Event_DSi_Cart2Power, 24);
    CHECK_EQ(melonDS::Event_CartBTKeyboardTimer, 25);
}

void testTheCartTimerSurvivesAConsoleRoundTrip()
{
    auto saved = MakeNDS();
    MarkScheduler(*saved);
    auto state = SaveNDS(*saved);

    auto loaded = MakeNDS();
    CHECK(LoadNDS(*loaded, state));
    CheckEverythingButTheCartTimer(*loaded);

    const melonDS::SchedEvent& cart = loaded->SchedList[melonDS::Event_CartBTKeyboardTimer];
    CHECK_EQ(cart.Timestamp, CartTimerStamp);
    CHECK_EQ(cart.FuncID, CartTimerFuncID);
    CHECK_EQ(cart.Param, CartTimerParam);
}

// 14.1 added the cart timer's scheduler entry to the NDSG section. A 14.0 state,
// for any game, has every other entry and field in the same place.
void testAStateFrom14_0LoadsWithoutTheCartTimer()
{
    auto saved = MakeNDS();
    MarkScheduler(*saved);
    auto state = SaveNDS(*saved);
    CHECK(MakeVersion14_0(state, saved->SchedList[melonDS::Event_CartBTKeyboardTimer]));

    auto loaded = MakeNDS();
    CHECK(LoadNDS(*loaded, state));
    CheckEverythingButTheCartTimer(*loaded);
    CHECK_EQ(loaded->SchedList[melonDS::Event_CartBTKeyboardTimer].Timestamp, 0);
}

// A corrupt state must fail the load, not size the keyboard's buffers from its counts.
// NDCS is a SaveKeyboard() state's only section, so the keyboard's fields start at
// 0x20 with the channel count, and end with the pending count and the packet block.
void testAStateWithTooManyChannelsIsRejected()
{
    BTKeyboard saved;
    saved.Reset();
    Connect(saved);                         // two HID channels
    auto state = SaveKeyboard(saved);

    u32 nchannels = BTKeyboard::MaxSavedChannels + 1;
    memcpy(&state[0x20], &nchannels, 4);

    BTKeyboard loaded;
    loaded.Reset();
    Savestate load(state.data(), (u32)state.size(), false);
    load.Section("NDCS");
    loaded.DoSavestate(&load);

    CHECK(load.Error);
    CHECK(!loaded.Connected());
}

void testAStateWithAnOversizedPacketIsRejected()
{
    BTKeyboard saved;
    saved.Reset();
    Feed(saved, Command(0x0C03, {}));       // leaves one 7-byte Command Complete unread
    auto state = SaveKeyboard(saved);

    // the packet's length, at the start of the block, now runs past the block's end
    const size_t block = state.size() - BTKeyboard::SavedPacketBytes;
    u16 oversized = BTKeyboard::SavedPacketBytes - 1;
    memcpy(&state[block], &oversized, 2);

    BTKeyboard loaded;
    loaded.Reset();
    Savestate load(state.data(), (u32)state.size(), false);
    load.Section("NDCS");
    loaded.DoSavestate(&load);

    CHECK(load.Error);
    std::vector<u8> pkt;
    CHECK(!loaded.NextPacket(pkt));
}

void testAStateWithTooManyPacketsIsRejected()
{
    BTKeyboard saved;
    saved.Reset();
    Feed(saved, Command(0x0C03, {}));       // leaves one 7-byte Command Complete unread
    auto state = SaveKeyboard(saved);

    // the pending count, just before the block, now claims a second packet
    const size_t count = state.size() - BTKeyboard::SavedPacketBytes - 4;
    u32 npending = 2;
    memcpy(&state[count], &npending, 4);

    BTKeyboard loaded;
    loaded.Reset();
    Savestate load(state.data(), (u32)state.size(), false);
    load.Section("NDCS");
    loaded.DoSavestate(&load);

    CHECK(load.Error);
    std::vector<u8> pkt;
    CHECK(!loaded.NextPacket(pkt));
}

}

int runSavestateTests()
{
    testAConnectedKeyboardSurvivesARoundTrip();
    testPendingPacketsComeBackInOrder();
    testARestoredKeyboardAnswersTheNextPacketIdentically();
    testAutoPairIsNotRestoredFromAState();
    testTheStateSizeDoesNotDependOnTheLink();
    testAQueueTooLongForAStateFailsTheSave();
    testEventsFrom14_0KeepTheirNumbers();
    testTheCartTimerSurvivesAConsoleRoundTrip();
    testAStateFrom14_0LoadsWithoutTheCartTimer();
    testAStateWithTooManyChannelsIsRejected();
    testAStateWithAnOversizedPacketIsRejected();
    testAStateWithTooManyPacketsIsRejected();
    return 0;
}
