/*
    Copyright 2016-2022 melonDS team, RSDuck

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

#ifndef ARMJIT_COMPILER_H
#define ARMJIT_COMPILER_H

#ifdef JIT_ENABLED

#if defined(__x86_64__)
#include "ARMJIT_x64/ARMJIT_Compiler.h"
#elif defined(__aarch64__)
#include "ARMJIT_A64/ARMJIT_Compiler.h"
#else
#error "The current target platform doesn't have a JIT backend"
#endif

#endif

#endif
