
#ifndef HEXUTIL_GDB_H_
#define HEXUTIL_GDB_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

inline static uint8_t hex2nyb(uint8_t v)
{
	if (v >= '0' && v <= '9') return v - '0';
	else if (v >= 'A' && v <= 'F') return v - 'A' + 0xa;
	else if (v >= 'a' && v <= 'f') return v - 'a' + 0xa;
	else
	{
		__builtin_trap();
		return 0xcc;
	}
}
inline static uint8_t nyb2hex(uint8_t v)
{
	v &= 0xf;
	if (v >= 0xa) return v - 0xa + 'a';
	else return v - 0x0 + '0';
}

inline static void hexfmt8(uint8_t* dst, uint8_t v)
{
	dst[0] = nyb2hex(v>>4);
	dst[1] = nyb2hex(v>>0);
}
inline static uint8_t unhex8(const uint8_t* src)
{
	return (hex2nyb(src[0]) << 4) | hex2nyb(src[1]);
}

inline static void hexfmt16(uint8_t* dst, uint16_t v)
{
	dst[0] = nyb2hex(v>> 4);
	dst[1] = nyb2hex(v>> 0);
	dst[2] = nyb2hex(v>>12);
	dst[3] = nyb2hex(v>> 8);
}
inline static uint16_t unhex16(const uint8_t* src)
{
	return unhex8(&src[0*2]) | ((uint16_t)unhex8(&src[1*2]) << 8);
}

inline static void hexfmt32(uint8_t* dst, uint32_t v)
{
	for (size_t i = 0; i < 4; ++i, v >>= 8)
	{
		dst[2*i+0] = nyb2hex(v>>4);
		dst[2*i+1] = nyb2hex(v>>0);
	}
}
inline static uint32_t unhex32(const uint8_t* src)
{
	uint32_t v = 0;
	for (size_t i = 0; i < 4; ++i)
	{
		v |= (uint32_t)unhex8(&src[i*2]) << (i*8);
	}
	return v;
}

#ifdef __cplusplus
}
#endif

#endif

