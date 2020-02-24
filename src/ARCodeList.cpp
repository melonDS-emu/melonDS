/*
    Copyright 2016-2020 Arisotura

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

#include <stdio.h>
#include "ARCodeList.h"

/*
    Action Replay code list format

    header:
    00 - magic MLAR
    04 - version major
    06 - version minor
    08 - length
    0C - number of codes

    code header:
    00 - magic MLCD
    04 - name length
    08 - code length
    0C - enable flag
    10 - code data (UTF8 name then actual code)
*/
