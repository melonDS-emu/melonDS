
#ifndef SPI_H
#define SPI_H

namespace SPI
{

void Init();
void Reset();

u16 ReadCnt();
void WriteCnt(u16 val);

u8 ReadData();
void WriteData(u8 val);

}

#endif
