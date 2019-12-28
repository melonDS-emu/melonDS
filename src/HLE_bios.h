/*
    Copyright 2016-2019 Arisotura

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

#ifndef HLE_BIOS_H
#define HLE_BIOS_H

#include "types.h"
#include "ARM.h"

extern char _binary_fakeBios7_bin_start[];
extern char _binary_fakeBios7_bin_end[];
extern char _binary_fakeBios9_bin_start[];
extern char _binary_fakeBios9_bin_end[];

extern u32 (*ARMSVCTable[2][32])(ARM *cpu);

#endif // HLE_BIOS_H