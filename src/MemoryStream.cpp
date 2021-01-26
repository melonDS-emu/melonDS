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

#include <vector>

#include "MemoryStream.h"
#include "types.h"

MemoryStream::MemoryStream(s32 capacity)
{
	buffer = std::vector<u8>(capacity);

	pos = 0;
	size = 0;
}
MemoryStream::MemoryStream(u8* data, s32 len)
{
	buffer = std::vector<u8>(data, data + len);

	pos = 0;
	size = len;
}

u8* MemoryStream::GetData()
{
	return buffer.data();
}
s32 MemoryStream::GetLength()
{
	return size;
}

void MemoryStream::Write(const u8* src, s32 len)
{
	while (pos + len + 1 > buffer.size())
		ExpandCapacity();

	std::copy(src, src + len, buffer.data() + pos);		
	pos += len;
	if (pos > size)
		size = pos;
}
void MemoryStream::ExpandCapacity()
{
	buffer.resize(buffer.size() * 2);
}

void MemoryStream::Read(u8* dst, s32 len)
{
	if (pos + len > size)
		return;

	std::copy(buffer.data() + pos, buffer.data() + pos + len, dst);
	pos += len;
}

void MemoryStream::Seek(s32 pos, s32 origin)
{
	if (origin == SEEK_SET)
	{
		if (pos < 0)
			return;
		while (pos > buffer.size())
			ExpandCapacity();
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
