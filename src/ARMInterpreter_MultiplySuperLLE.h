#ifndef ARMINTERPRETER_MULTIPLYSUPERLLE_H
#define ARMINTERPRETER_MULTIPLYSUPERLLE_H

#include "types.h"

using namespace melonDS;

/*
    Copyright (c) 2024 zaydlang

    This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

        1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software.
           If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source distribution.
*/




// code taken from: (also features a few alternative implementations that could maybe be worth looking at?)
// https://github.com/calc84maniac/multiplication-algorithm/blob/master/impl_opt.h
// based on research that can be found here: https://bmchtech.github.io/post/multiply/

// the code in this file is dedicated to handling the calculation of the carry flag for multiplication (S variant) instructions on the ARM7TDMI.


// Takes a multiplier between -0x01000000 and 0x00FFFFFF, cycles between 0 and 2
static inline bool booths_multiplication32_opt(u32 multiplicand, u32 multiplier, u32 accumulator) {
    // Set the low bit of the multiplicand to cause negation to invert the upper bits, this bit can't propagate to bit 31
    multiplicand |= 1;

    // Optimized first iteration
    u32 booth = (s32)(multiplier << 31) >> 31;
    u32 carry = booth * multiplicand;
    // Pre-populate accumulator for output
    u32 output = accumulator;

    u32 sum = output + carry;
    int shift = 29;
    do {
        for (int i = 0; i < 4; i++, shift -= 2) {
            // Get next booth factor (-2 to 2, shifted left by 30-shift)
            u32 next_booth = (s32)(multiplier << shift) >> shift;
            u32 factor = next_booth - booth;
            booth = next_booth;
            // Get scaled value of booth addend
            u32 addend = multiplicand * factor;
            // Combine the addend with the CSA
            // Not performing any masking seems to work because the lower carries can't propagate to bit 31
            output ^= carry ^ addend;
            sum += addend;
            carry = sum - output;
        }
    } while (booth != multiplier);

    return carry >> 31;
}

// Takes a multiplicand shifted right by 6 and a multiplier shifted right by 26 (zero or sign extended)
static inline bool booths_multiplication64_opt(u32 multiplicand, u32 multiplier, u32 accum_hi) {
    // Skipping the first 14 iterations seems to work because the lower carries can't propagate to bit 63
    // This means only magic bits 62-61 are needed (which requires decoding 3 booth chunks),
    // and only the last two booth iterations are needed

    // Set the low bit of the multiplicand to cause negation to invert the upper bits
    multiplicand |= 1;

    // Pre-populate magic bit 61 for carry
    u32 carry = ~accum_hi & UINT32_C(0x20000000);
    // Pre-populate magic bits 63-60 for output (with carry magic pre-added in)
    u32 output = accum_hi - UINT32_C(0x08000000);

    // Get factors from the top 3 booth chunks
    u32 booth0 = (s32)(multiplier << 27) >> 27;
    u32 booth1 = (s32)(multiplier << 29) >> 29;
    u32 booth2 = (s32)(multiplier << 31) >> 31;
    u32 factor0 = multiplier - booth0;
    u32 factor1 = booth0 - booth1;
    u32 factor2 = booth1 - booth2;

    // Get scaled value of the 3rd top booth addend
    u32 addend = multiplicand * factor2;
    // Finalize bits 61-60 of output magic using its sign
    output -= addend & UINT32_C(0x10000000);
    // Get scaled value of the 2nd top booth addend
    addend = multiplicand * factor1;
    // Finalize bits 63-62 of output magic using its sign
    output -= addend & UINT32_C(0x40000000);

    // Get the carry from the CSA in bit 61 and propagate it to bit 62, which is not processed in this iteration
    u32 sum = output + (addend & UINT32_C(0x20000000));
    // Subtract out the carry magic to get the actual output magic
    output -= carry;

    // Get scaled value of the 1st top booth addend
    addend = multiplicand * factor0;
    // Add to bit 62 and propagate the carry
    sum += addend & UINT32_C(0x40000000);

    // Cancel out the output magic bit 63 to get the carry bit 63
    return (sum ^ output) >> 31;
}


// also for MLAS and MUL (thumb ver.)
inline bool MULSCarry(s32 rm, s32 rs, u32 rn, bool lastcycle)
{
    if (lastcycle)
        return (rs >> 30) == -2;
    else
        return booths_multiplication32_opt(rm, rs, rn);
}

// also for UMLALS
inline bool UMULLSCarry(u64 rd, u32 rm, u32 rs, bool lastcycle)
{
    if (lastcycle)
        return booths_multiplication64_opt(rm >> 6, rs >> 26, rd >> 32);
    else
        return booths_multiplication32_opt(rm, rs, rd & 0xFFFFFFFF);
}

// also for SMLALS
inline bool SMULLSCarry(u64 rd, s32 rm, s32 rs, bool lastcycle)
{
    if (lastcycle)
        return booths_multiplication64_opt(rm >> 6, rs >> 26, rd >> 32);
    else
        return booths_multiplication32_opt(rm, rs, rd & 0xFFFFFFFF);
}

#endif
