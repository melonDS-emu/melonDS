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

#include "CartRetailNAND.h"
#include "../NDS.h"
#include "../Utils.h"

// CartRetailNAND: retail NDS cartridge with NAND backup
// ROM and save memory are part of the same chip, and both use the ROM comm interface
// SPI is not used

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace NDSCart
{

CartRetailNAND::CartRetailNAND(const u8* rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata) :
    CartRetailNAND(CopyToUnique(rom, len), len, chipid, romparams, std::move(sram), sramlen, userdata)
{
}

CartRetailNAND::CartRetailNAND(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata) :
    CartRetail(std::move(rom), len, chipid, false, romparams, std::move(sram), sramlen, userdata, CartType::RetailNAND)
{
    BuildSRAMID();
}

CartRetailNAND::~CartRetailNAND() = default;

void CartRetailNAND::Reset()
{
    CartRetail::Reset();

    SRAMAddr = 0;
    SRAMStatus = 0x20;
    SRAMWindow = 0;

    // ROM header 94/96 = SRAM addr start / 0x20000
    SRAMBase = *(u16*)&ROM[0x96] << 17;

    memset(SRAMWriteBuffer, 0, 0x800);
}

void CartRetailNAND::DoSavestate(Savestate* file)
{
    CartRetail::DoSavestate(file);

    file->Var32(&SRAMBase);
    file->Var32(&SRAMWindow);

    file->VarArray(SRAMWriteBuffer, 0x800);
    file->Var32(&SRAMWritePos);
    file->Var32(&SRAMWriteLen);

    if (!file->Saving)
        BuildSRAMID();
}

void CartRetailNAND::SetSaveMemory(const u8* savedata, u32 savelen)
{
    CartRetail::SetSaveMemory(savedata, savelen);
    BuildSRAMID();
}

u32 CartRetailNAND::SRAMRead32()
{
    u32 addr = (SRAMAddr - SRAMBase) & (SRAMLength-1);
    u32 addrhi = addr & ~0xFFF;
    u32 addrlo = addr & 0xFFF;
    u32 ret;

    if (addrlo & 0x3)
    {
        ret = SRAM[addrhi | addrlo];
        addrlo = (addrlo + 1) & 0xFFF;
        ret |= (SRAM[addrhi | addrlo] << 8);
        addrlo = (addrlo + 1) & 0xFFF;
        ret |= (SRAM[addrhi | addrlo] << 16);
        addrlo = (addrlo + 1) & 0xFFF;
        ret |= (SRAM[addrhi | addrlo] << 24);
        addrlo = (addrlo + 1) & 0xFFF;
    }
    else
    {
        ret = *(u32*)&SRAM[addrhi | addrlo];
        addrlo = (addrlo + 4) & 0xFFF;
    }

    SRAMAddr = addrhi | addrlo;
    return ret;
}

void CartRetailNAND::ROMCommandStart(NDSCart::NDSCartSlot& cartslot, const u8* cmd)
{
    if (CmdEncMode != 2) return CartRetail::ROMCommandStart(cartslot, cmd);

    memcpy(ROMCmd, cmd, 8);

    switch (ROMCmd[0])
    {
    case 0x81: // write data
        if ((SRAMStatus & (1<<4)) && SRAMWindow >= SRAMBase && SRAMWindow < (SRAMBase+SRAMLength))
        {
            u32 addr = (ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4];

            if (addr >= SRAMWindow && addr < (SRAMWindow+0x20000))
            {
                // the command is issued 4 times, each with the same address
                // seems they use the one from the first command (CHECKME)
                if (!SRAMAddr)
                    SRAMAddr = addr;
            }
        }
        else
            SRAMAddr = 0;
        return;

    case 0x82: // commit write
        if (SRAMAddr && SRAMWriteLen)
        {
            if (SRAMLength && SRAMAddr < (SRAMBase+SRAMLength-0x20000))
            {
                u32 offset = SRAMAddr - SRAMBase;
                if ((offset + 0x800) > SRAMLength)
                {
                    u32 len1 = SRAMLength - offset;
                    u32 len2 = 0x800 - len1;

                    memcpy(&SRAM[offset], SRAMWriteBuffer, len1);
                    memcpy(&SRAM[0], &SRAMWriteBuffer[len1], len2);

                    Platform::WriteNDSSave(SRAM.get(), SRAMLength, offset, len1, UserData);
                    Platform::WriteNDSSave(SRAM.get(), SRAMLength, 0, len2, UserData);
                }
                else
                {
                    memcpy(&SRAM[offset], SRAMWriteBuffer, 0x800);
                    Platform::WriteNDSSave(SRAM.get(), SRAMLength, offset, 0x800, UserData);
                }
            }

            SRAMAddr = 0;
            SRAMWritePos = 0;
            SRAMWriteLen = 0;
        }
        SRAMStatus &= ~(1<<4);
        return;

    case 0x84: // discard write buffer
        SRAMAddr = 0;
        SRAMWritePos = 0;
        return;

    case 0x85: // write enable
        if (SRAMWindow)
        {
            SRAMStatus |= (1<<4);
            SRAMWritePos = 0;
        }
        return;

    case 0x8B: // revert to ROM read mode
        SRAMWindow = 0;
        return;

    case 0x94: // return ID data
        ROMAddr = 0;
        return;

    case 0xB2: // set window for accessing SRAM
        {
            u32 addr = (ROMCmd[1]<<24) | ((ROMCmd[2]&0xFE)<<16);

            // window is 0x20000 bytes, address is aligned to that boundary
            // NAND remains stuck 'busy' forever if this is less than the starting SRAM address
            // TODO.
            if (addr < SRAMBase)
                Log(LogLevel::Warn, "NAND: !! BAD ADDR %08X < %08X\n", addr, SRAMBase);
            if (addr >= (SRAMBase+SRAMLength))
                Log(LogLevel::Warn, "NAND: !! BAD ADDR %08X > %08X\n", addr, SRAMBase+SRAMLength);

            SRAMWindow = addr;
        }
        return;

    case 0xB7:
        {
            u32 addr = (ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4];

            if (SRAMWindow == 0)
            {
                // regular ROM mode
                return CartRetail::ROMCommandStart(cartslot, cmd);
            }
            else
            {
                // SRAM mode
                if (SRAMWindow >= SRAMBase && SRAMWindow < (SRAMBase+SRAMLength) &&
                    addr >= SRAMWindow && addr < (SRAMWindow+0x20000))
                {
                    SRAMAddr = addr;
                }
                else
                    SRAMAddr = 0;
            }
        }
        return;

    default:
        return CartRetail::ROMCommandStart(cartslot, cmd);
    }
}

u32 CartRetailNAND::ROMCommandReceive()
{
    if (CmdEncMode != 2) return CartRetail::ROMCommandReceive();

    switch (ROMCmd[0])
    {
    case 0x94: // return ID data
        {
            // TODO: check what the data really is. probably the NAND chip's ID.
            // also, might be different between different games or even between different carts.
            // this was taken from a Jam with the Band cart.

            if (ROMAddr >= 0x30)
                return 0;

            u32 ret = *(u32*)&SRAMID[ROMAddr];
            ROMAddr += 4;
            return ret;
        }

    case 0xB7:
        if (SRAMWindow == 0)
        {
            // regular ROM mode
            if (ROMAddr >= SRAMBase && ROMAddr < (SRAMBase+SRAMLength))
                return 0xFFFFFFFF;

            return CartRetail::ROMCommandReceive();
        }
        else
        {
            // SRAM mode
            if (!SRAMAddr)
                return 0xFFFFFFFF;

            return SRAMRead32();
        }

    case 0xD6: // read NAND status
        // status bits
        // bit5: ready
        // bit4: write enable
        return SRAMStatus * 0x01010101;

    default:
        return CartRetail::ROMCommandReceive();
    }
}

void CartRetailNAND::ROMCommandTransmit(u32 val)
{
    if (CmdEncMode != 2) return CartRetail::ROMCommandTransmit(val);

    switch (ROMCmd[0])
    {
    case 0x81: // write SRAM data
        if (SRAMAddr)
        {
            *(u32*)&SRAMWriteBuffer[SRAMWritePos] = val;

            // TODO verify what happens when writing more than 0x800 bytes (or less)
            SRAMWritePos += 4;
            SRAMWritePos &= 0x7FF;
            if (SRAMWriteLen < 0x800)
                SRAMWriteLen += 4;
        }
        return;

    default:
        return CartRetail::ROMCommandTransmit(val);
    }
}

void CartRetailNAND::BuildSRAMID()
{
    // the last 128K of the SRAM are read-only.
    // most of it is FF, except for the NAND ID at the beginning
    // of the last 0x800 bytes.

    if (SRAMLength > 0x20000)
    {
        memset(&SRAM[SRAMLength - 0x20000], 0xFF, 0x20000);

        // TODO: check what the data is all about!
        // this was pulled from a Jam with the Band cart. may be different on other carts.
        // WarioWare DIY may have different data or not have this at all.
        // the ID data is also found in the response to command 94, and JwtB checks it.
        // WarioWare doesn't seem to care.
        // there is also more data here, but JwtB doesn't seem to care.
        u8 iddata[0x10] = {0xEC, 0x00, 0x9E, 0xA1, 0x51, 0x65, 0x34, 0x35, 0x30, 0x35, 0x30, 0x31, 0x19, 0x19, 0x02, 0x0A};
        memcpy(&SRAM[SRAMLength - 0x800], iddata, 16);

        u8 iddata2[0x30] = {
                0xEC, 0xF1, 0x00, 0x95, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        memcpy(SRAMID, iddata2, 0x30);
        memcpy(&SRAMID[0x18], iddata, 16);
    }
}


}

}
