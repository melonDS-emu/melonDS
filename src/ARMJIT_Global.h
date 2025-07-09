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

#ifndef ARMJIT_GLOBAL_H
#define ARMJIT_GLOBAL_H

#include "types.h"

#include <stdlib.h>

namespace melonDS
{

namespace ARMJIT_Global
{

static constexpr size_t CodeMemorySliceSize = 1024*1024*32;

void Init();
void DeInit();

void* AllocateCodeMem();
void FreeCodeMem(void* codeMem);

}

}

#endif