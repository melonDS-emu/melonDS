/*
    Copyright 2016-2019 Arisotura

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

#ifndef MEMORYSTREAM_H
#define MEMORYSTREAM_H

#include <stdio.h>
#include <string.h>
#include "types.h"

#define CHUNK_SIZE (1024 * 1024)

class MemoryStream
{
public:
	MemoryStream();
	MemoryStream(u8* data, s32 len);
	~MemoryStream();

	u8* GetData();
	s32 GetLength();

	void Write(const void* src, s32 len);
	void Read(void* dst, s32 len);

	void Seek(s32 pos, s32 origin);

	s32 Tell();

private:
	u8** chunks;
	s32 firstNonsequentialChunk;
	bool ownsFirstChunk;
	
	s32 size;
	s32 pos;
	s32 numChunks;

	// Make chunks point to data; does not copy data.
	void Init(u8* data, s32 len);
	void DeInit();

	void ExpandCapacity();
};

#endif // MEMORYSTREAM_H