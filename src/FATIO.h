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


#ifndef FATIO_H
#define FATIO_H

#include <functional>
#include "fatfs/ff.h"

// extra additions for interfacing with melonDS

using ff_disk_read_cb = std::function<UINT(BYTE*, LBA_t, UINT)>;
using ff_disk_write_cb = std::function<UINT(const BYTE*, LBA_t, UINT)>;

void ff_disk_open(const ff_disk_read_cb& readcb, const ff_disk_write_cb& writecb, LBA_t seccnt);
void ff_disk_close();

#endif // FATIO_H
