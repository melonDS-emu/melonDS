// This file is under the public domain.

#pragma once

#include <cstddef>
#include <bitset>
#include <initializer_list>
#include <type_traits>
#include "../types.h"

namespace Common
{
using namespace melonDS;
#if defined(__GNUC__) || defined(__clang__)
__attribute((always_inline)) static constexpr int CountSetBits(u8 val)
{
  return __builtin_popcount(val);
}
__attribute((always_inline)) static constexpr int CountSetBits(u16 val)
{
  return __builtin_popcount(val);
}
__attribute((always_inline)) static constexpr int CountSetBits(u32 val)
{
  return __builtin_popcount(val);
}
__attribute((always_inline)) static constexpr int CountSetBits(u64 val)
{
  return __builtin_popcountll(val);
}
__attribute((always_inline)) static constexpr int LeastSignificantSetBit(u8 val)
{
  return __builtin_ctz(val);
}
__attribute((always_inline)) static constexpr int LeastSignificantSetBit(u16 val)
{
  return __builtin_ctz(val);
}
__attribute((always_inline)) static constexpr int LeastSignificantSetBit(u32 val)
{
  return __builtin_ctz(val);
}
__attribute((always_inline)) static constexpr int LeastSignificantSetBit(u64 val)
{
  return __builtin_ctzll(val);
}
#elif defined(_MSC_VER)
#include <intrin.h>
// MSVC __popcnt doesn't switch between hardware availability like gcc does, can't use it, let C++ implementation handle it
__forceinline static int CountSetBits(u8 val)
{
  return std::bitset<8>(val).count();
}
__forceinline static int CountSetBits(u16 val)
{
  return std::bitset<16>(val).count();
}
__forceinline static int CountSetBits(u32 val)
{
  return std::bitset<32>(val).count();
}
__forceinline static int CountSetBits(u64 val)
{
  return std::bitset<64>(val).count();
}
__forceinline static int LeastSignificantSetBit(u8 val)
{
  unsigned long count;
  _BitScanForward(&count, val);
  return count;
}
__forceinline static int LeastSignificantSetBit(u16 val)
{
  unsigned long count;
  _BitScanForward(&count, val);
  return count;
}
__forceinline static int LeastSignificantSetBit(u32 val)
{
  unsigned long count;
  _BitScanForward(&count, val);
  return count;
}
__forceinline static int LeastSignificantSetBit(u64 val)
{
#if defined(_WIN64)
  unsigned long count;
  _BitScanForward64(&count, val);
  return count;
#else
  unsigned long tmp;
  _BitScanForward(&tmp, (u32)(val & 0x00000000FFFFFFFFull));
  if (tmp)
    return tmp;
  _BitScanForward(&tmp, (u32)((val & 0xFFFFFFFF00000000ull) >> 32));
  return tmp ? tmp + 32 : 0;
#endif
}
#endif

// Similar to std::bitset, this is a class which encapsulates a bitset, i.e.
// using the set bits of an integer to represent a set of integers.  Like that
// class, it acts like an array of bools:
//     BitSet32 bs;
//     bs[1] = true;
// but also like the underlying integer ([0] = least significant bit):
//     BitSet32 bs2 = ...;
//     bs = (bs ^ bs2) & BitSet32(0xffff);
// The following additional functionality is provided:
// - Construction using an initializer list.
//     BitSet bs { 1, 2, 4, 8 };
// - Efficiently iterating through the set bits:
//     for (int i : bs)
//         [i is the *index* of a set bit]
//   (This uses the appropriate CPU instruction to find the next set bit in one
//   operation.)
// - Counting set bits using .Count() - see comment on that method.

// TODO: use constexpr when MSVC gets out of the Dark Ages

template <typename IntTy>
class BitSet
{
  static_assert(!std::is_signed<IntTy>::value, "BitSet should not be used with signed types");

public:
  // A reference to a particular bit, returned from operator[].
  class Ref
  {
  public:
    constexpr Ref(Ref&& other) : m_bs(other.m_bs), m_mask(other.m_mask) {}
    constexpr Ref(BitSet* bs, IntTy mask) : m_bs(bs), m_mask(mask) {}
    constexpr operator bool() const { return (m_bs->m_val & m_mask) != 0; }
    bool operator=(bool set)
    {
      m_bs->m_val = (m_bs->m_val & ~m_mask) | (set ? m_mask : 0);
      return set;
    }

  private:
    BitSet* m_bs;
    IntTy m_mask;
  };

  // A STL-like iterator is required to be able to use range-based for loops.
  class Iterator
  {
  public:
    constexpr Iterator(const Iterator& other) : m_val(other.m_val), m_bit(other.m_bit) {}
    constexpr Iterator(IntTy val, int bit) : m_val(val), m_bit(bit) {}
    Iterator& operator=(Iterator other)
    {
      new (this) Iterator(other);
      return *this;
    }
    Iterator& operator++()
    {
      if (m_val == 0)
      {
        m_bit = -1;
      }
      else
      {
        int bit = LeastSignificantSetBit(m_val);
        m_val &= ~(1 << bit);
        m_bit = bit;
      }
      return *this;
    }
    Iterator operator++(int)
    {
      Iterator other(*this);
      ++*this;
      return other;
    }
    constexpr int operator*() const { return m_bit; }
    constexpr bool operator==(Iterator other) const { return m_bit == other.m_bit; }
    constexpr bool operator!=(Iterator other) const { return m_bit != other.m_bit; }

  private:
    IntTy m_val;
    int m_bit;
  };

  constexpr BitSet() : m_val(0) {}
  constexpr explicit BitSet(IntTy val) : m_val(val) {}
  BitSet(std::initializer_list<int> init)
  {
    m_val = 0;
    for (int bit : init)
      m_val |= (IntTy)1 << bit;
  }

  constexpr static BitSet AllTrue(size_t count)
  {
    return BitSet(count == sizeof(IntTy) * 8 ? ~(IntTy)0 : (((IntTy)1 << count) - 1));
  }

  Ref operator[](size_t bit) { return Ref(this, (IntTy)1 << bit); }
  constexpr const Ref operator[](size_t bit) const { return (*const_cast<BitSet*>(this))[bit]; }
  constexpr bool operator==(BitSet other) const { return m_val == other.m_val; }
  constexpr bool operator!=(BitSet other) const { return m_val != other.m_val; }
  constexpr bool operator<(BitSet other) const { return m_val < other.m_val; }
  constexpr bool operator>(BitSet other) const { return m_val > other.m_val; }
  constexpr BitSet operator|(BitSet other) const { return BitSet(m_val | other.m_val); }
  constexpr BitSet operator&(BitSet other) const { return BitSet(m_val & other.m_val); }
  constexpr BitSet operator^(BitSet other) const { return BitSet(m_val ^ other.m_val); }
  constexpr BitSet operator~() const { return BitSet(~m_val); }
  constexpr BitSet operator<<(IntTy shift) const { return BitSet(m_val << shift); }
  constexpr BitSet operator>>(IntTy shift) const { return BitSet(m_val >> shift); }
  constexpr explicit operator bool() const { return m_val != 0; }
  BitSet& operator|=(BitSet other) { return *this = *this | other; }
  BitSet& operator&=(BitSet other) { return *this = *this & other; }
  BitSet& operator^=(BitSet other) { return *this = *this ^ other; }
  BitSet& operator<<=(IntTy shift) { return *this = *this << shift; }
  BitSet& operator>>=(IntTy shift) { return *this = *this >> shift; }
  constexpr unsigned int Count() const { return CountSetBits(m_val); }
  constexpr Iterator begin() const { return ++Iterator(m_val, 0); }
  constexpr Iterator end() const { return Iterator(m_val, -1); }
  IntTy m_val;
};

using BitSet8 = BitSet<u8>;
using BitSet16 = BitSet<u16>;
using BitSet32 = BitSet<u32>;
using BitSet64 = BitSet<u64>;
}  // namespace Common

