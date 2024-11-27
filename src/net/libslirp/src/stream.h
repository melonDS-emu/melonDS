/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef STREAM_H_
#define STREAM_H_

#include "libslirp.h"

typedef struct SlirpIStream {
    SlirpReadCb read_cb;
    void *opaque;
} SlirpIStream;

typedef struct SlirpOStream {
    SlirpWriteCb write_cb;
    void *opaque;
} SlirpOStream;

/* Read exactly size bytes from stream, return 1 if all ok, 0 otherwise */
bool slirp_istream_read(SlirpIStream *f, void *buf, size_t size);
/* Write exactly size bytes to stream, return 1 if all ok, 0 otherwise */
bool slirp_ostream_write(SlirpOStream *f, const void *buf, size_t size);

/* Read exactly one byte from stream, return it, otherwise return 0 */
uint8_t slirp_istream_read_u8(SlirpIStream *f);
/* Write exactly one byte to stream, return 1 if all ok, 0 otherwise */
bool slirp_ostream_write_u8(SlirpOStream *f, uint8_t b);

/* Read exactly two bytes from big-endian stream, return it, otherwise return 0 */
uint16_t slirp_istream_read_u16(SlirpIStream *f);
/* Write exactly two bytes to big-endian stream, return 1 if all ok, 0 otherwise */
bool slirp_ostream_write_u16(SlirpOStream *f, uint16_t b);

/* Read exactly four bytes from big-endian stream, return it, otherwise return 0 */
uint32_t slirp_istream_read_u32(SlirpIStream *f);
/* Write exactly four bytes to big-endian stream, return 1 if all ok, 0 otherwise */
bool slirp_ostream_write_u32(SlirpOStream *f, uint32_t b);

/* Read exactly two bytes from big-endian stream (signed), return it, otherwise return 0 */
int16_t slirp_istream_read_i16(SlirpIStream *f);
/* Write exactly two bytes to big-endian stream (signed), return 1 if all ok, 0 otherwise */
bool slirp_ostream_write_i16(SlirpOStream *f, int16_t b);

/* Read exactly four bytes from big-endian stream (signed), return it, otherwise return 0 */
int32_t slirp_istream_read_i32(SlirpIStream *f);
/* Write exactly four bytes to big-endian stream (signed), return 1 if all ok, 0 otherwise */
bool slirp_ostream_write_i32(SlirpOStream *f, int32_t b);

#endif /* STREAM_H_ */
