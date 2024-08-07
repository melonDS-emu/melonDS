/*
    Copyright 2016-2024 melonDS team

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

#ifndef MPINTERFACE_H
#define MPINTERFACE_H

#include "types.h"

namespace melonDS
{

class MPInterface
{
public:
    [[nodiscard]] int GetRecvTimeout() const noexcept { return RecvTimeout; }
    void SetRecvTimeout(int timeout) noexcept { RecvTimeout = timeout; }

    virtual void Begin(int inst) = 0;
    virtual void End(int inst) = 0;

    virtual int SendPacket(int inst, u8* data, int len, u64 timestamp) = 0;
    virtual int RecvPacket(int inst, u8* data, u64* timestamp) = 0;
    virtual int SendCmd(int inst, u8* data, int len, u64 timestamp) = 0;
    virtual int SendReply(int inst, u8* data, int len, u64 timestamp, u16 aid) = 0;
    virtual int SendAck(int inst, u8* data, int len, u64 timestamp) = 0;
    virtual int RecvHostPacket(int inst, u8* data, u64* timestamp) = 0;
    virtual u16 RecvReplies(int inst, u8* data, u64 timestamp, u16 aidmask) = 0;

protected:
    int RecvTimeout = 25;
};

}

#endif // MPINTERFACE_H
