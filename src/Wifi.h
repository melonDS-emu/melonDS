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

#ifndef WIFI_H
#define WIFI_H

#include "Savestate.h"

namespace melonDS
{
class WifiAP;
class NDS;
class Wifi
{
public:

    enum
    {
        W_ID = 0x000,

        W_ModeReset = 0x004,
        W_ModeWEP = 0x006,
        W_TXStatCnt = 0x008,
        W_IF = 0x010,
        W_IE = 0x012,

        W_MACAddr0 = 0x018,
        W_MACAddr1 = 0x01A,
        W_MACAddr2 = 0x01C,
        W_BSSID0 = 0x020,
        W_BSSID1 = 0x022,
        W_BSSID2 = 0x024,
        W_AIDLow = 0x028,
        W_AIDFull = 0x02A,

        W_TXRetryLimit = 0x02C,
        W_RXCnt = 0x030,
        W_WEPCnt = 0x032,

        W_TRXPower = 0x034,
        W_PowerUS = 0x036,
        W_PowerTX = 0x038,
        W_PowerState = 0x03C,
        W_PowerForce = 0x040,
        W_PowerDownCtrl = 0x48,

        W_Random = 0x044,

        W_RXBufBegin = 0x050,
        W_RXBufEnd = 0x052,
        W_RXBufWriteCursor = 0x054,
        W_RXBufWriteAddr = 0x056,
        W_RXBufReadAddr = 0x058,
        W_RXBufReadCursor = 0x05A,
        W_RXBufCount = 0x05C,
        W_RXBufDataRead = 0x060,
        W_RXBufGapAddr = 0x062,
        W_RXBufGapSize = 0x064,

        W_TXBufWriteAddr = 0x068,
        W_TXBufCount = 0x06C,
        W_TXBufDataWrite = 0x070,
        W_TXBufGapAddr = 0x074,
        W_TXBufGapSize = 0x076,

        W_TXSlotBeacon = 0x080,
        W_TXBeaconTIM = 0x084,
        W_ListenCount = 0x088,
        W_BeaconInterval = 0x08C,
        W_ListenInterval = 0x08E,
        W_TXSlotCmd = 0x090,
        W_TXSlotReply1 = 0x094,
        W_TXSlotReply2 = 0x098,
        W_TXSlotLoc1 = 0x0A0,
        W_TXSlotLoc2 = 0x0A4,
        W_TXSlotLoc3 = 0x0A8,
        W_TXReqReset = 0x0AC,
        W_TXReqSet = 0x0AE,
        W_TXReqRead = 0x0B0,
        W_TXSlotReset = 0x0B4,
        W_TXBusy = 0x0B6,
        W_TXStat = 0x0B8,
        W_Preamble = 0x0BC,
        W_CmdTotalTime = 0x0C0,
        W_CmdReplyTime = 0x0C4,
        W_RXFilter = 0x0D0,
        W_RXLenCrop = 0x0DA,
        W_RXFilter2 = 0x0E0,

        W_USCountCnt = 0x0E8,
        W_USCompareCnt = 0x0EA,
        W_CmdCountCnt = 0x0EE,

        W_USCount0 = 0x0F8,
        W_USCount1 = 0x0FA,
        W_USCount2 = 0x0FC,
        W_USCount3 = 0x0FE,
        W_USCompare0 = 0x0F0,
        W_USCompare1 = 0x0F2,
        W_USCompare2 = 0x0F4,
        W_USCompare3 = 0x0F6,

        W_ContentFree = 0x10C,
        W_PreBeacon = 0x110,
        W_CmdCount = 0x118,
        W_BeaconCount1 = 0x11C,
        W_BeaconCount2 = 0x134,

        W_BBCnt = 0x158,
        W_BBWrite = 0x15A,
        W_BBRead = 0x15C,
        W_BBBusy = 0x15E,
        W_BBMode = 0x160,
        W_BBPower = 0x168,

        W_RFData2 = 0x17C,
        W_RFData1 = 0x17E,
        W_RFBusy = 0x180,
        W_RFCnt = 0x184,

        W_TXHeaderCnt = 0x194,
        W_RFPins = 0x19C,

        W_RXStatIncIF = 0x1A8,
        W_RXStatIncIE = 0x1AA,
        W_RXStatHalfIF = 0x1AC,
        W_RXStatHalfIE = 0x1AE,
        W_TXErrorCount = 0x1C0,
        W_RXCount = 0x1C4,

        W_CMDStat0 = 0x1D0,
        W_CMDStat1 = 0x1D2,
        W_CMDStat2 = 0x1D4,
        W_CMDStat3 = 0x1D6,
        W_CMDStat4 = 0x1D8,
        W_CMDStat5 = 0x1DA,
        W_CMDStat6 = 0x1DC,
        W_CMDStat7 = 0x1DE,

        W_TXSeqNo = 0x210,
        W_RFStatus = 0x214,
        W_IFSet = 0x21C,
        W_RXTXAddr = 0x268,
    };

    Wifi(melonDS::NDS& nds);
    ~Wifi();
    void Reset();
    void DoSavestate(Savestate* file);

    void SetPowerCnt(u32 val);

    void USTimer(u32 param);

    u16 Read(u32 addr);
    void Write(u32 addr, u16 val);

    const u8* GetMAC() const;
    const u8* GetBSSID() const;

private:
    melonDS::NDS& NDS;
    u8 RAM[0x2000];
    u16 IO[0x1000>>1];

    static const u8 MPCmdMAC[6];
    static const u8 MPReplyMAC[6];
    static const u8 MPAckMAC[6];

    static const int kTimerInterval = 8;
    static const u32 kTimeCheckMask = ~(kTimerInterval - 1);

    bool Enabled;
    bool PowerOn;

    s32 TimerError;

    u16 Random;

    // general, always-on microsecond counter
    u64 USTimestamp;

    u64 USCounter;
    u64 USCompare;
    bool BlockBeaconIRQ14;

    u32 CmdCounter;

    u8 BBRegs[0x100];
    u8 BBRegsRO[0x100];

    u8 RFVersion;
    u32 RFRegs[0x40];

    u32 RFChannelIndex[2];
    u32 RFChannelData[14][2];
    int CurChannel;

    struct TXSlot
    {
        bool Valid;
        u16 Addr;
        u16 Length;
        u8 Rate;
        u8 CurPhase;
        int CurPhaseTime;
        u32 HalfwordTimeMask;
    };

    TXSlot TXSlots[6];
    u8 TXBuffer[0x2000];

    u8 RXBuffer[2048];
    u32 RXBufferPtr;
    int RXTime;
    u32 RXHalfwordTimeMask;

    u32 ComStatus; // 0=waiting for packets  1=receiving  2=sending
    u32 TXCurSlot;
    u32 RXCounter;

    int MPReplyTimer;
    u16 MPClientMask, MPClientFail;

    u8 MPClientReplies[15*1024];

    u16 MPLastSeqno;

    bool MPInited;
    bool LANInited;

    int USUntilPowerOn;

    // MULTIPLAYER SYNC APPARATUS
    bool IsMP;
    bool IsMPClient;
    u64 NextSync;           // for clients: timestamp for next sync point
    u64 RXTimestamp;

    class WifiAP* WifiAP;

    void ScheduleTimer(bool first);
    void UpdatePowerOn();

    void CheckIRQ(u16 oldflags);
    void SetIRQ(u32 irq);
    void SetIRQ13();
    void SetIRQ14(int source);
    void SetIRQ15();

    void SetStatus(u32 status);

    void UpdatePowerStatus(int power);

    int PreambleLen(int rate) const;
    u32 NumClients(u16 bitmask) const;
    void IncrementTXCount(const TXSlot* slot);
    void ReportMPReplyErrors(u16 clientfail);

    void TXSendFrame(const TXSlot* slot, int num);
    void StartTX_LocN(int nslot, int loc);
    void StartTX_Cmd();
    void StartTX_Beacon();
    void FireTX();
    void SendMPDefaultReply();
    void SendMPReply(u16 clienttime, u16 clientmask);
    void SendMPAck(u16 cmdcount, u16 clientfail);
    bool ProcessTX(TXSlot* slot, int num);

    void IncrementRXAddr(u16& addr, u16 inc = 2);
    void StartRX();
    void FinishRX();
    void MPClientReplyRX(int client);
    bool CheckRX(int type);

    void MSTimer();

    void ChangeChannel();

    void RFTransfer_Type2();
    void RFTransfer_Type3();
};

}
#endif
