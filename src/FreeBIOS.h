/*  Custom NDS ARM7/ARM9 BIOS replacement
    Copyright (c) 2013, Gilead Kutnick
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
     modification, are permitted provided that the following conditions are met:

    1) Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
    2) Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef FREEBIOS_H
#define FREEBIOS_H

#include <array>
#include "MemConstants.h"

namespace melonDS
{
extern std::array<u8, ARM7BIOSSize> bios_arm7_bin;
extern std::array<u8, ARM9BIOSSize> bios_arm9_bin;
}

#endif // FREEBIOS_H
