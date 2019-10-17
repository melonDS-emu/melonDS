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

#include "MemoryStream.h"

MemoryStream::MemoryStream()
{
	// Create new array of chunks, and an initial chunk.
	chunks = new u8*[MAX_CHUNKS];
	memset(chunks, 0, MAX_CHUNKS * sizeof(u8*));
	chunks[0] = new u8[CHUNK_SIZE];
	
	pos = 0;
	size = 0;
	firstNonsequentialChunk = 0;
}
MemoryStream::MemoryStream(u8* data, s32 len)
{
	chunks = NULL;
	Init(data, len);
}
MemoryStream::~MemoryStream()
{
	DeInit();
}

// Make chunks point to data; does not copy data.
void MemoryStream::Init(u8* data, s32 len)
{
	if (chunks) DeInit();

	if (len > CHUNK_SIZE * MAX_CHUNKS)
		throw "data array too big";

	chunks = new u8*[MAX_CHUNKS];
	memset(chunks, 0, MAX_CHUNKS * sizeof(u8*));
	for (int i = 0; i * CHUNK_SIZE < len; i++)
		chunks[i] = data + i * CHUNK_SIZE;

	pos = 0;
	size = len;
	firstNonsequentialChunk = size / CHUNK_SIZE + 1;
}
void MemoryStream::DeInit()
{
	for (int i = firstNonsequentialChunk; i < MAX_CHUNKS && chunks[i]; i++)
		delete chunks[i];
	delete chunks;
}

u8* MemoryStream::GetData()
{
	// Create a new array, and copy chunks to new array.
	u8* data = new u8[size];
	int i;
	for (i = 0; (i+1) * CHUNK_SIZE < size; i++)
		memcpy(data + i * CHUNK_SIZE, chunks[i], CHUNK_SIZE);
	memcpy(data + i * CHUNK_SIZE, chunks[i], size % CHUNK_SIZE);
	// Re-init with new array; this deletes old arrays.
	// Also means the new array will be deleted upon destruction of the MemoryStream.
	Init(data, size);
	return data;
}
s32 MemoryStream::GetLength()
{
	return size;
}

void MemoryStream::Write(const void* src, s32 len)
{
	if (pos + len + 1 > CHUNK_SIZE * MAX_CHUNKS)
		throw "exceeded maximum stream size";

	if ((pos + len - 1) / CHUNK_SIZE > pos / CHUNK_SIZE)
	{
		u32 new_len = CHUNK_SIZE - (pos % CHUNK_SIZE);
		Write(src, new_len);
		Write((u8*)src + new_len, len - new_len);
	}
	else
	{
		s32 chunk = pos / CHUNK_SIZE;
		s32 cPos = pos % CHUNK_SIZE;
		memcpy(chunks[chunk] + cPos, src, len);
		
		pos += len;
		if (chunk < pos / CHUNK_SIZE)
		{
			chunks[chunk+1] = new u8[CHUNK_SIZE];
			memset(chunks[chunk+1], 0, CHUNK_SIZE);
		}
		if (pos > size)
			size = pos;
	}
}
void MemoryStream::Read(void* dst, s32 len)
{
	if (pos + len > size)
		throw "attempted to read past end of stream";

	if ((pos + len - 1) / CHUNK_SIZE > pos / CHUNK_SIZE)
	{
		u32 new_len = CHUNK_SIZE - (pos % CHUNK_SIZE);
		Read(dst, new_len);
		Read((u8*)dst + new_len, len - new_len);
	}
	else
	{
		s32 chunk = pos / CHUNK_SIZE;
		s32 cPos = pos % CHUNK_SIZE;
		memcpy(dst, chunks[chunk] + cPos, len);
		pos += len;
	}
}

void MemoryStream::Seek(s32 pos, s32 origin)
{
	if (origin == SEEK_SET)
	{
		if (pos < 0 || pos > CHUNK_SIZE * MAX_CHUNKS)
			throw "Invalid seek operation.";
		if (pos > size)
		{
			// Ensure the chunk exists.
			s32 oldChunk = this->pos / CHUNK_SIZE;
			s32 newChunk = pos / CHUNK_SIZE;
			while (oldChunk < newChunk)
			{
				oldChunk++;
				chunks[oldChunk] = new u8[CHUNK_SIZE];
				memset(chunks[oldChunk], 0, CHUNK_SIZE);
			}
		}
		this->pos = pos;
	}
	else if (origin == SEEK_CUR)
		Seek(this->pos + pos, SEEK_SET);
	else if (origin == SEEK_END)
		Seek(size - pos, SEEK_SET);
}

s32 MemoryStream::Tell()
{
	return pos;
}
