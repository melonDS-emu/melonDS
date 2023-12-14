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

#ifndef ARENGINE_H
#define ARENGINE_H

#include "ARCodeFile.h"

namespace melonDS
{
class NDS;
class AREngine
{
public:
    AREngine(melonDS::NDS& nds);

    ARCodeFile* GetCodeFile() { return CodeFile; }
    void SetCodeFile(ARCodeFile* file) { CodeFile = file; }

    void RunCheats();
    void RunCheat(const ARCode& arcode);
private:
    melonDS::NDS& NDS;
    ARCodeFile* CodeFile; // AR code file - frontend is responsible for managing this
};

}
#endif // ARENGINE_H
