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

#include "FreeBIOS.h"

namespace melonDS
{
#include "FreeBIOS_Data.h"

std::array<u8, ARM9BIOSSize> FreeBIOSGetNtrArm9(void) {
  std::array<u8, ARM9BIOSSize> result({0});
  std::copy(std::begin(bios_ntr_arm9), std::end(bios_ntr_arm9), std::begin(result));
  return result;
}

std::array<u8, ARM7BIOSSize> FreeBIOSGetNtrArm7(void) {
  std::array<u8, ARM7BIOSSize> result({0});
  std::copy(std::begin(bios_ntr_arm7), std::end(bios_ntr_arm7), std::begin(result));
  return result;
}

std::array<u8, DSiBIOSSize> FreeBIOSGetTwlArm9(void) {
  std::array<u8, DSiBIOSSize> result({0});
  std::copy(std::begin(bios_twl_arm9), std::end(bios_twl_arm9), std::begin(result));
  return result;
}

std::array<u8, DSiBIOSSize> FreeBIOSGetTwlArm7(void) {
  std::array<u8, DSiBIOSSize> result({0});
  std::copy(std::begin(bios_twl_arm7), std::end(bios_twl_arm7), std::begin(result));
  return result;
}
}