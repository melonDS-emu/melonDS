// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license_dolphin.txt file included.

#include "../types.h"
#include "x64ABI.h"
#include "x64Emitter.h"

namespace Gen
{
using namespace Common;
// Shared code between Win64 and Unix64

void XEmitter::ABI_CalculateFrameSize(BitSet32 mask, size_t rsp_alignment, size_t needed_frame_size,
                                      size_t* shadowp, size_t* subtractionp, size_t* xmm_offsetp)
{
  size_t shadow = 0;
#if defined(_WIN32)
  shadow = 0x20;
#endif

  int count = (mask & ABI_ALL_GPRS).Count();
  rsp_alignment -= count * 8;
  size_t subtraction = 0;
  int fpr_count = (mask & ABI_ALL_FPRS).Count();
  if (fpr_count)
  {
    // If we have any XMMs to save, we must align the stack here.
    subtraction = rsp_alignment & 0xf;
  }
  subtraction += 16 * fpr_count;
  size_t xmm_base_subtraction = subtraction;
  subtraction += needed_frame_size;
  subtraction += shadow;
  // Final alignment.
  rsp_alignment -= subtraction;
  subtraction += rsp_alignment & 0xf;

  *shadowp = shadow;
  *subtractionp = subtraction;
  *xmm_offsetp = subtraction - xmm_base_subtraction;
}

size_t XEmitter::ABI_PushRegistersAndAdjustStack(BitSet32 mask, size_t rsp_alignment,
                                                 size_t needed_frame_size)
{
  size_t shadow, subtraction, xmm_offset;
  ABI_CalculateFrameSize(mask, rsp_alignment, needed_frame_size, &shadow, &subtraction,
                         &xmm_offset);

  for (int r : mask& ABI_ALL_GPRS)
    PUSH((X64Reg)r);

  if (subtraction)
    SUB(64, R(RSP), subtraction >= 0x80 ? Imm32((u32)subtraction) : Imm8((u8)subtraction));

  for (int x : mask& ABI_ALL_FPRS)
  {
    MOVAPD(MDisp(RSP, (int)xmm_offset), (X64Reg)(x - 16));
    xmm_offset += 16;
  }

  return shadow;
}

void XEmitter::ABI_PopRegistersAndAdjustStack(BitSet32 mask, size_t rsp_alignment,
                                              size_t needed_frame_size)
{
  size_t shadow, subtraction, xmm_offset;
  ABI_CalculateFrameSize(mask, rsp_alignment, needed_frame_size, &shadow, &subtraction,
                         &xmm_offset);

  for (int x : mask& ABI_ALL_FPRS)
  {
    MOVAPD((X64Reg)(x - 16), MDisp(RSP, (int)xmm_offset));
    xmm_offset += 16;
  }

  if (subtraction)
    ADD(64, R(RSP), subtraction >= 0x80 ? Imm32((u32)subtraction) : Imm8((u8)subtraction));

  for (int r = 15; r >= 0; r--)
  {
    if (mask[r])
      POP((X64Reg)r);
  }
}

void XEmitter::MOVTwo(int bits, Gen::X64Reg dst1, Gen::X64Reg src1, s32 offset1, Gen::X64Reg dst2,
                      Gen::X64Reg src2)
{
  if (dst1 == src2 && dst2 == src1)
  {
    XCHG(bits, R(src1), R(src2));
    if (offset1)
      ADD(bits, R(dst1), Imm32(offset1));
  }
  else if (src2 != dst1)
  {
    if (dst1 != src1 && offset1)
      LEA(bits, dst1, MDisp(src1, offset1));
    else if (dst1 != src1)
      MOV(bits, R(dst1), R(src1));
    else if (offset1)
      ADD(bits, R(dst1), Imm32(offset1));
    if (dst2 != src2)
      MOV(bits, R(dst2), R(src2));
  }
  else
  {
    if (dst2 != src2)
      MOV(bits, R(dst2), R(src2));
    if (dst1 != src1 && offset1)
      LEA(bits, dst1, MDisp(src1, offset1));
    else if (dst1 != src1)
      MOV(bits, R(dst1), R(src1));
    else if (offset1)
      ADD(bits, R(dst1), Imm32(offset1));
  }
}
}