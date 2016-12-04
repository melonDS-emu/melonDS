#include <stdio.h>
#include "NDS.h"
#include "SPI.h"


namespace SPI_Firmware
{

u8* Firmware;
u32 FirmwareLength;

u32 Hold;
u8 CurCmd;
u32 DataPos;
u8 Data;

u32 Addr;

void Init()
{
    Firmware = NULL;
}

void Reset()
{
    if (Firmware) delete[] Firmware;

    FILE* f = fopen("firmware.bin", "rb");
    fseek(f, 0, SEEK_END);
    FirmwareLength = (u32)ftell(f);
    Firmware = new u8[FirmwareLength];

    fseek(f, 0, SEEK_SET);
    fread(Firmware, FirmwareLength, 1, f);

    fclose(f);

    Hold = 0;
    CurCmd = 0;
    Data = 0;
}

u8 Read()
{
    return Data;
}

void Write(u8 val, u32 hold)
{
    if (!hold)
    {
        Hold = 0;
    }

    if (hold && (!Hold))
    {
        CurCmd = val;
        Hold = 1;
        DataPos = 1;
        Addr = 0;
        printf("firmware SPI command %02X\n", CurCmd);
        return;
    }

    switch (CurCmd)
    {
    case 0x03: // read
        {
            if (DataPos < 4)
            {
                Addr <<= 8;
                Addr |= val;
                Data = 0;

                if (DataPos == 3) printf("firmware SPI read %08X\n", Addr);
            }
            else
            {
                if (Addr >= FirmwareLength)
                    Data = 0;
                else
                    Data = Firmware[Addr];

                Addr++;
            }

            DataPos++;
        }
        break;

    default:
        printf("unknown firmware SPI command %02X\n", CurCmd);
        break;
    }
}

}


namespace SPI
{

u16 CNT;

u32 CurDevice;


void Init()
{
    SPI_Firmware::Init();
}

void Reset()
{
    CNT = 0;

    SPI_Firmware::Reset();
}


u16 ReadCnt()
{
    return CNT;
}

void WriteCnt(u16 val)
{
    CNT = val & 0xCF03;
    if (val & 0x0400) printf("!! CRAPOED 16BIT SPI MODE\n");
}

u8 ReadData()
{
    if (!(CNT & (1<<15))) return 0;

    switch (CNT & 0x0300)
    {
    case 0x0100: return SPI_Firmware::Read();
    default: return 0;
    }
}

void WriteData(u8 val)
{
    if (!(CNT & (1<<15))) return;

    // TODO: take delays into account

    switch (CNT & 0x0300)
    {
    case 0x0100: SPI_Firmware::Write(val, CNT&(1<<11)); break;
    }

    if (CNT & (1<<14))
        NDS::TriggerIRQ(1, NDS::IRQ_SPI);
}

}
