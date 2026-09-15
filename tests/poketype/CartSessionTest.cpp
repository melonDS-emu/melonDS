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

// Drives a real CartRetailBT over SPI: CartSPITest covers the routing rules,
// these check that the cart applies them and that save writes land and flush.

#include <memory>
#include <vector>

#include "NDS.h"
#include "NDSCart/CartRetailBT.h"
#include "NDSSupport.h"
#include "PlatformStub.h"
#include "TestSupport.h"

using melonDS::u8;
using melonDS::u16;
using melonDS::u32;
using melonDS::ROMListEntry;
using melonDS::NDSCart::CartRetailBT;

namespace
{

// CartCommon only copies the 4096-byte header out of the ROM, so a zeroed
// header is a whole cart here. SaveMemType 3 is a 64 KB EEPROM (SRAMType 2).
// Slot stays null; every use of it is guarded.
std::unique_ptr<CartRetailBT> MakeCart()
{
    auto rom = std::make_unique<u8[]>(4096);
    ROMListEntry params { 0, 4096, 3 };
    auto cart = std::make_unique<CartRetailBT>(std::move(rom), 4096, 0x00007FC2,
                                               params, nullptr, 0, nullptr);
    cart->Reset();
    ClearSaveWrites();
    return cart;
}

// one chipselect transaction, the unit routing is decided over
std::vector<u8> Session(CartRetailBT& cart, const std::vector<u8>& bytes)
{
    std::vector<u8> out;
    cart.SPISelect();
    for (u8 b : bytes)
        out.push_back(cart.SPITransmitReceive(b));
    cart.SPIRelease();
    return out;
}

// chipselect pulse with nothing clocked, as the driver sends via AUXSPICNT bit 13
void EmptyPulse(CartRetailBT& cart)
{
    cart.SPISelect();
    cart.SPIRelease();
}

// data follows the READ opcode and two address bytes, at index 3
std::vector<u8> ReadSave(CartRetailBT& cart, u32 addr, u32 count)
{
    std::vector<u8> req { 0x03, (u8)(addr >> 8), (u8)(addr & 0xFF) };
    for (u32 i = 0; i < count; i++)
        req.push_back(0x00);

    auto rsp = Session(cart, req);
    return std::vector<u8>(rsp.begin() + 3, rsp.end());
}

// Send an HCI command in the cart's SPI framing (01 00 | BE length | payload).
// SaveActive is false in a fresh cart, so the frame reaches the controller.
void SendBTCommand(CartRetailBT& cart, u16 opcode, const std::vector<u8>& params)
{
    std::vector<u8> hci { 0x01, (u8)(opcode & 0xFF), (u8)(opcode >> 8), (u8)params.size() };
    hci.insert(hci.end(), params.begin(), params.end());

    std::vector<u8> frame { 0x01, 0x00, (u8)(hci.size() >> 8), (u8)(hci.size() & 0xFF) };
    frame.insert(frame.end(), hci.begin(), hci.end());
    Session(cart, frame);
}

void testAWriteReachesSaveMemoryAndIsFlushed()
{
    auto cart = MakeCart();

    Session(*cart, {0x06});                                      // WREN
    Session(*cart, {0x02, 0x01, 0x00, 0xAA, 0xBB, 0xCC, 0xDD});  // WRITE @0x0100

    // CartRetail::SPIRelease only flushes while the write-enable latch is set,
    // so CartRetailBT must call it before clearing the latch
    CHECK_EQ(SaveWrites().size(), 1);
    if (SaveWrites().empty()) return;
    CHECK_EQ(SaveWrites()[0].Offset, 0x0100);
    CHECK_EQ(SaveWrites()[0].Len, 4);
}

void testWrittenBytesReadBack()
{
    auto cart = MakeCart();

    Session(*cart, {0x06});
    Session(*cart, {0x02, 0x01, 0x00, 0xAA, 0xBB, 0xCC, 0xDD});

    auto back = ReadSave(*cart, 0x0100, 4);
    if (back.size() < 4) return;
    CHECK_EQ(back[0], 0xAA);
    CHECK_EQ(back[1], 0xBB);
    CHECK_EQ(back[2], 0xCC);
    CHECK_EQ(back[3], 0xDD);
}

void testAnAddressOnlyWriteStillClosesTheWriteLatch()
{
    auto cart = MakeCart();

    Session(*cart, {0x06});                    // WREN
    Session(*cart, {0x02, 0x01, 0x00});        // WRITE, address only, no data

    // CartRetail only drops the latch for a write that stored bytes, so this
    // one relies on CartRetailBT's clear; otherwise the next write needs no WREN
    Session(*cart, {0x02, 0x01, 0x00, 0xAA, 0xBB});

    auto back = ReadSave(*cart, 0x0100, 2);
    if (back.size() < 2) return;
    CHECK_EQ(back[0], 0xFF);
    CHECK_EQ(back[1], 0xFF);
}

void testAnEmptyPulseLeavesTheWriteLatchArmed()
{
    auto cart = MakeCart();

    Session(*cart, {0x06});                    // WREN
    EmptyPulse(*cart);                         // no clock edges, no state change
    Session(*cart, {0x02, 0x02, 0x00, 0x11, 0x22});

    auto back = ReadSave(*cart, 0x0200, 2);
    if (back.size() < 2) return;
    CHECK_EQ(back[0], 0x11);
    CHECK_EQ(back[1], 0x22);
}

void testABluetoothSessionDisarmsTheWriteLatch()
{
    auto cart = MakeCart();

    // the sync byte, since with the latch armed a 01 frame routes to save memory
    Session(*cart, {0x06});                    // WREN
    Session(*cart, {0xFF});                    // Bluetooth sync byte
    Session(*cart, {0x02, 0x03, 0x00, 0x33, 0x44});

    auto back = ReadSave(*cart, 0x0300, 2);
    if (back.size() < 2) return;
    CHECK_EQ(back[0], 0xFF);
    CHECK_EQ(back[1], 0xFF);
}

void testTheBluetoothPathStillWorksWithTheSavePathLatched()
{
    auto cart = MakeCart();

    // RDSR latches SaveActive without writing anything
    Session(*cart, {0x05, 0x00});

    Session(*cart, {0x01, 0x00, 0x00, 0x04, 0x01, 0x03, 0x0C, 0x00});  // HCI_Reset

    // Command Complete for HCI_Reset: 01 00 | 00 07 | 04 0E 04 01 03 0C 00
    auto rsp = Session(*cart, {0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    if (rsp.size() < 11) return;
    CHECK_EQ(rsp[0], 0x01);
    CHECK_EQ(rsp[1], 0x00);
    CHECK_EQ(rsp[2], 0x00);
    CHECK_EQ(rsp[3], 0x07);
    CHECK_EQ(rsp[4], 0x04);     // HCI event packet
    CHECK_EQ(rsp[5], 0x0E);     // Command Complete
    CHECK_EQ(rsp[6], 0x04);     // parameter total length
    CHECK_EQ(rsp[7], 0x01);     // command packets the host may send
    CHECK_EQ(rsp[8], 0x03);     // opcode low
    CHECK_EQ(rsp[9], 0x0C);     // opcode high
    CHECK_EQ(rsp[10], 0x00);    // status: success
}

void testControllerFlashSaveIsWrittenAndPersisted()
{
    auto cart = MakeCart();

    // Write_RAM of the game's "RAXT" save signature to the gameplay save sector
    // at 0xFF00E000, which is SRAM offset 0x6000 (addr - 0xFF008000)
    SendBTCommand(*cart, 0xFC4C,
                  {0x00, 0xE0, 0x00, 0xFF, 0x52, 0x41, 0x58, 0x54});

    CHECK_EQ(SaveWrites().size(), 1);
    if (SaveWrites().empty()) return;
    CHECK_EQ(SaveWrites()[0].Offset, 0x6000);
    CHECK_EQ(SaveWrites()[0].Len, 4);

    // GetSaveMemory is what the frontend writes to disk
    const u8* mem = cart->GetSaveMemory();
    CHECK(mem != nullptr);
    if (!mem) return;
    CHECK_EQ(mem[0x6000 + 0], 0x52);
    CHECK_EQ(mem[0x6000 + 1], 0x41);
    CHECK_EQ(mem[0x6000 + 2], 0x58);
    CHECK_EQ(mem[0x6000 + 3], 0x54);
}

void testControllerFlashEraseClearsTheSectorToErased()
{
    auto cart = MakeCart();

    // an erased sector must read 0xFF for the game to see no save
    SendBTCommand(*cart, 0xFC4C, {0x00, 0xE0, 0x00, 0xFF, 0x52, 0x41, 0x58, 0x54});
    SendBTCommand(*cart, 0xFF5E, {0x00, 0xE0, 0x00, 0xFF});

    const u8* mem = cart->GetSaveMemory();
    if (!mem) { CHECK(mem != nullptr); return; }
    CHECK_EQ(mem[0x6000 + 0], 0xFF);
    CHECK_EQ(mem[0x6000 + 3], 0xFF);
}

// A savestate restores every scheduler event, whatever cart is inserted, so the
// page timer must point at the slot: a cart pointer would dangle once ejected.
void testThePageTimerBelongsToTheSlot()
{
    // megabytes of memory timing tables: too big for the stack
    melonDS::NDSArgs args;
    args.JIT = std::nullopt;
    auto nds = std::make_unique<melonDS::NDS>(std::move(args));

    melonDS::SchedEvent& evt = nds->SchedList[melonDS::Event_CartBTKeyboardTimer];
    CHECK(evt.That == &nds->NDSCartSlot);

    auto cart = MakeCart();
    const CartRetailBT* raw = cart.get();
    nds->SetNDSCart(std::move(cart));
    CHECK(evt.That == &nds->NDSCartSlot);
    CHECK(evt.That != raw);

    nds->EjectCart();

    // firing it with no cart inserted, as a loaded savestate can, does nothing
    if (evt.That == &nds->NDSCartSlot && evt.Funcs[evt.FuncID])
        evt.Funcs[evt.FuncID](evt.That, 0);
}

// Switching between DS and DSi keeps the cart and replaces the console under it,
// so a cart ejected from one console must never schedule on it again.
void testAnEjectedCartLeavesItsOldConsoleAlone()
{
    auto first = MakeNDS();
    auto second = MakeNDS();
    const u32 timer = 1u << melonDS::Event_CartBTKeyboardTimer;

    first->SetNDSCart(MakeCart());
    CHECK((first->EventMask() & timer) != 0);

    auto cart = first->EjectCart();
    CHECK((first->EventMask() & timer) == 0);

    second->SetNDSCart(std::move(cart));
    CHECK((first->EventMask() & timer) == 0);
    CHECK((second->EventMask() & timer) != 0);
}

// A 14.0 state of this game has no cart timer entry, so loading one must start the
// timer again: without it the keyboard never pages the DS back.
void testAStateFrom14_0RestartsTheCartTimer()
{
    const u32 timer = 1u << melonDS::Event_CartBTKeyboardTimer;

    auto saved = MakeNDS();
    saved->SetNDSCart(MakeCart());

    // values no other bytes of the state match, so MakeVersion14_0 finds the entry
    melonDS::SchedEvent& evt = saved->SchedList[melonDS::Event_CartBTKeyboardTimer];
    evt.Timestamp = 0x7172737475767778;
    evt.Param = 0x05060708;

    auto state = SaveNDS(*saved);
    CHECK(MakeVersion14_0(state, evt));

    auto loaded = MakeNDS();
    loaded->SetNDSCart(MakeCart());
    CHECK(LoadNDS(*loaded, state));
    CHECK((loaded->EventMask() & timer) != 0);
}

}

int runCartSessionTests()
{
    testAWriteReachesSaveMemoryAndIsFlushed();
    testWrittenBytesReadBack();
    testAnAddressOnlyWriteStillClosesTheWriteLatch();
    testAnEmptyPulseLeavesTheWriteLatchArmed();
    testABluetoothSessionDisarmsTheWriteLatch();
    testTheBluetoothPathStillWorksWithTheSavePathLatched();
    testControllerFlashSaveIsWrittenAndPersisted();
    testControllerFlashEraseClearsTheSectorToErased();
    testThePageTimerBelongsToTheSlot();
    testAnEjectedCartLeavesItsOldConsoleAlone();
    testAStateFrom14_0RestartsTheCartTimer();
    return 0;
}
