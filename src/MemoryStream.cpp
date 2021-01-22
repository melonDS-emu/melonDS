/*
    Copyright 2016-2021 Arisotura

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
#include "types.h"

MemoryStream::MemoryStream()
{
	// Create new array of chunks, and an initial chunk.
	numChunks = 16;
	chunks = new u8*[numChunks];
	memset(chunks, 0, numChunks * sizeof(u8*));
	chunks[0] = new u8[CHUNK_SIZE];
	
	pos = 0;
	size = 0;
	firstNonsequentialChunk = 1;
	ownsFirstChunk = true;
}
// This does not necessarily copy all the data. If the original data will not out-live the MemoryStream, call GetData afterward
MemoryStream::MemoryStream(u8* data, s32 len)
{
	chunks = NULL;
	Init(data, len);
	ownsFirstChunk = false;
}
MemoryStream::~MemoryStream()
{
	DeInit();
}

// Make chunks point to data; does not copy all data.
void MemoryStream::Init(u8* data, s32 len)
{
	if (chunks) DeInit();

	numChunks = 16;
	while (len > CHUNK_SIZE * numChunks)
		numChunks = numChunks << 1;

	chunks = new u8*[numChunks];
	memset(chunks, 0, numChunks * sizeof(u8*));
	s32 numFullChunks = len / CHUNK_SIZE;
	s32 lenFull = numFullChunks * CHUNK_SIZE;
	for (int i = 0; i < numFullChunks; i++)
		chunks[i] = data + i * CHUNK_SIZE;
	chunks[numFullChunks] = new u8[CHUNK_SIZE];
	memcpy(chunks[numFullChunks], data + lenFull, len - lenFull);

	pos = 0;
	size = len;
	firstNonsequentialChunk = numFullChunks;
}
void MemoryStream::DeInit()
{
	if (ownsFirstChunk)
		delete chunks[0];
	for (int i = firstNonsequentialChunk; i < numChunks && chunks[i]; i++)
		delete chunks[i];
	delete chunks;
}

// Puts all of the stream's data into a single array and returns that array.
u8* MemoryStream::GetData()
{
	// Create a new array, and copy chunks to new array.
	u8* data = new u8[size];
	int i;
	for (i = 0; (i+1) * CHUNK_SIZE < size; i++)
		memcpy(data + i * CHUNK_SIZE, chunks[i], CHUNK_SIZE);
	memcpy(data + i * CHUNK_SIZE, chunks[i], size % CHUNK_SIZE);
	// Re-init with new array; this deletes old arrays.
	Init(data, size);
	// the new array will be deleted upon destruction of the MemoryStream.
	ownsFirstChunk = true;
	return data;
}
s32 MemoryStream::GetLength()
{
	return size;
}

void MemoryStream::Write(const void* src, s32 len)
{
	while (pos + len + 1 > CHUNK_SIZE * numChunks)
		ExpandCapacity();

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
			chunks[chunk+1] = new u8[CHUNK_SIZE];
		if (pos > size)
			size = pos;
	}
}
void MemoryStream::ExpandCapacity()
{
	if (!chunks)
		return;

	u8** newChunks = new u8*[numChunks << 1];
	memcpy(newChunks, chunks, numChunks * sizeof(u8*));
	memset(&newChunks[numChunks], 0, numChunks * sizeof(u8*));
	numChunks = numChunks << 1;

	delete[] chunks;
	chunks = newChunks;
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
		if (pos < 0 || pos > CHUNK_SIZE * numChunks)
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
