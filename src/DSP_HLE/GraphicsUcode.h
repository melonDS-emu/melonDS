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

namespace melonDS::DSP_HLE
{

class GraphicsUcode : public UcodeBase
{
public:
    GraphicsUcode(melonDS::DSi& dsi, int version);
    ~GraphicsUcode();
    void Reset() override;
    void DoSavestate(Savestate* file) override;

    void SendData(u8 index, u16 val) override;

protected:
    u8 CmdState;
    u16 CmdIndex;
    u16 CmdParams[14];

    void TryStartCmd();
    void FinishCmd(u32 param);

    void CmdScalingNearest();
    void CmdScalingBilinear();
    s32 CalcBicubicWeight(s32 x);
    void CmdScalingBicubic();
    void CmdScalingOneThird();

    void CmdYuvToRgb();
};

}

#endif // GRAPHICSUCODE_H
