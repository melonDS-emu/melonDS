/*
    Copyright 2016-2025 melonDS team

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

#ifndef GRAPHICSUCODE_H
#define GRAPHICSUCODE_H

#include <functional>

#include "UcodeBase.h"
#include "../Savestate.h"

namespace melonDS
{
namespace DSP_HLE
{

class GraphicsUcode : public UcodeBase
{
public:
    GraphicsUcode(melonDS::DSi& dsi, int version);
    ~GraphicsUcode();
    void Reset() override;
    void DoSavestate(Savestate* file) override;

    typedef std::function<void()> fnReplyReadCb;

    //void SetRecvDataHandler(u8 index, std::function<void()> func);
    //void SetSemaphoreHandler(std::function<void()> func);

    void SendData(u8 index, u16 val) override;

protected:
    void RunUcodeCmd();
    void OnUcodeCmdFinish(u32 param);
    void UcodeCmd_Scaling(u16* pipe);
};

}
}

#endif // GRAPHICSUCODE_H
