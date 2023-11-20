/*
    Copyright 2016-2023 melonDS team

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

#ifndef DSI_H
#define DSI_H

#include "NDS.h"
#include "DSi_NDMA.h"
#include "DSi_SD.h"
#include "DSi_I2CHost.h"
#include "DSi_DSP.h"
#include "DSi_AES.h"
#include "DSi_Camera.h"

namespace melonDS
{
namespace DSi_NAND
{
    class NANDImage;
}

class DSi final : public NDS
{
public:
    DSi() noexcept;
    ~DSi() noexcept override;
    DSi(const DSi&) = delete;
    DSi& operator=(const DSi&) = delete;
    DSi(DSi&&) = delete;
    DSi& operator=(DSi&&) = delete;

    void Reset() noexcept override;
    void Stop(Platform::StopReason reason) noexcept override;
    bool LoadCart(const u8* romdata, u32 romlen, const u8* savedata, u32 savelen) noexcept override;
    void EjectCart() noexcept override;
    bool NeedsDirectBoot() const noexcept override
    {
        // for now, DSi mode requires original BIOS/NAND
        return false;
    }
protected:
    void DoSavestateExtra(Savestate* file) noexcept override;
public:
    u16 SCFG_BIOS;
    u16 SCFG_Clock9;
    u32 SCFG_EXT[2];

    u8 ARM9iBIOS[0x10000];
    u8 ARM7iBIOS[0x10000];

    std::unique_ptr<DSi_NAND::NANDImage> NANDImage;
    DSi_SDHost SDMMC;
    DSi_SDHost SDIO;

    u8* NWRAM_A;
    u8* NWRAM_B;
    u8* NWRAM_C;

    u8* NWRAMMap_A[2][4];
    u8* NWRAMMap_B[3][8];
    u8* NWRAMMap_C[3][8];

    u32 NWRAMStart[2][3];
    u32 NWRAMEnd[2][3];
    u32 NWRAMMask[2][3];

    DSi_I2CHost I2C;
    DSi_CamModule CamModule;
    DSi_AES AES;
    DSi_DSP DSP;


    void CamInputFrame(int cam, const u32* data, int width, int height, bool rgb) noexcept override;


    void SetCartInserted(bool inserted) noexcept;

    void SetupDirectBoot() noexcept override;
    void SoftReset() noexcept;

    bool LoadNAND() noexcept;

    bool DMAsInMode(u32 cpu, u32 mode) const noexcept override;
    bool DMAsRunning(u32 cpu) const noexcept override;
    void StopDMAs(u32 cpu, u32 mode) noexcept override;
    void CheckDMAs(u32 cpu, u32 mode) noexcept override;

    void RunNDMAs(u32 cpu) noexcept;
    void StallNDMAs() noexcept;
    bool NDMAsInMode(u32 cpu, u32 mode) const noexcept;
    bool NDMAsRunning(u32 cpu) const noexcept;
    void CheckNDMAs(u32 cpu, u32 mode) noexcept;
    void StopNDMAs(u32 cpu, u32 mode) noexcept;

    void MapNWRAM_A(u32 num, u8 val) noexcept;
    void MapNWRAM_B(u32 num, u8 val) noexcept;
    void MapNWRAM_C(u32 num, u8 val) noexcept;
    void MapNWRAMRange(u32 cpu, u32 num, u32 val) noexcept;

    u8 ARM9Read8(u32 addr) noexcept override;
    u16 ARM9Read16(u32 addr) noexcept override;
    u32 ARM9Read32(u32 addr) noexcept override;
    void ARM9Write8(u32 addr, u8 val) noexcept override;
    void ARM9Write16(u32 addr, u16 val) noexcept override;
    void ARM9Write32(u32 addr, u32 val) noexcept override;

    bool ARM9GetMemRegion(u32 addr, bool write, MemRegion* region) noexcept override;

    u8 ARM7Read8(u32 addr) noexcept override;
    u16 ARM7Read16(u32 addr) noexcept override;
    u32 ARM7Read32(u32 addr) noexcept override;
    void ARM7Write8(u32 addr, u8 val) noexcept override;
    void ARM7Write16(u32 addr, u16 val) noexcept override;
    void ARM7Write32(u32 addr, u32 val) noexcept override;

    bool ARM7GetMemRegion(u32 addr, bool write, MemRegion* region) noexcept override;

    u8 ARM9IORead8(u32 addr) noexcept override;
    u16 ARM9IORead16(u32 addr) noexcept override;
    u32 ARM9IORead32(u32 addr) noexcept override;
    void ARM9IOWrite8(u32 addr, u8 val) noexcept override;
    void ARM9IOWrite16(u32 addr, u16 val) noexcept override;
    void ARM9IOWrite32(u32 addr, u32 val) noexcept override;

    u8 ARM7IORead8(u32 addr) noexcept override;
    u16 ARM7IORead16(u32 addr) noexcept override;
    u32 ARM7IORead32(u32 addr) noexcept override;
    void ARM7IOWrite8(u32 addr, u8 val) noexcept override;
    void ARM7IOWrite16(u32 addr, u16 val) noexcept override;
    void ARM7IOWrite32(u32 addr, u32 val) noexcept override;
public:
    u16 SCFG_Clock7;
    u32 SCFG_MC;
    u16 SCFG_RST;

    u32 MBK[2][9];


    u32 NDMACnt[2];
    std::array<DSi_NDMA, 8> NDMAs;

    // FIXME: these currently have no effect (and aren't stored in a savestate)
    //        ... not that they matter all that much
    u8 GPIO_Data;
    u8 GPIO_Dir;
    u8 GPIO_IEdgeSel;
    u8 GPIO_IE;
    u8 GPIO_WiFi;

private:
    void Set_SCFG_Clock9(u16 val) noexcept;
    void Set_SCFG_MC(u32 val) noexcept;
    void DecryptModcryptArea(u32 offset, u32 size, u8* iv) noexcept;
    void ApplyNewRAMSize(u32 size) noexcept;
};

}
#endif // DSI_H
