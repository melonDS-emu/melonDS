
#ifndef HEXUTIL_GDB_H_
#define HEXUTIL_GDB_H_

#include <stdint.h>

inline static uint8_t hex2nyb(uint8_t v) {
	if (v >= '0' && v <= '9') return v - '0';
	else if (v >= 'A' && v <= 'F') return v - 'A' + 0xa;
	else if (v >= 'a' && v <= 'f') return v - 'a' + 0xa;
	else return 0xcc;
}
inline static uint8_t nyb2hex(uint8_t v) {
	v &= 0xf;
	if (v >= 0xa) return v - 0xa + 'a';
	else return v - 0x0 + '0';
}

inline static void hexfmt8(uint8_t* dst, uint8_t v) {
	dst[0] = nyb2hex(v>>4);
	dst[1] = nyb2hex(v>>0);
}

inline static void hexfmt32(uint8_t* dst, uint32_t v) {
	for (size_t i = 0; i < 4; ++i, v >>= 8) {
		dst[2*i+0] = nyb2hex(v>>4);
		dst[2*i+1] = nyb2hex(v>>0);
	}
}

#endif

