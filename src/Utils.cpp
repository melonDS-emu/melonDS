/*
    Copyright 2016-2023 melonDS team

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

#include "Utils.h"

#include <string.h>

namespace melonDS
{
std::pair<std::unique_ptr<u8[]>, u32> PadToPowerOf2(std::unique_ptr<u8[]>&& data, u32 len) noexcept
{
    if (data == nullptr || len == 0)
        return {nullptr, 0};

    if ((len & (len - 1)) == 0)
        return {std::move(data), len};

    u32 newlen = 1;
    while (newlen < len)
        newlen <<= 1;

    auto newdata = std::make_unique<u8[]>(newlen);
    memcpy(newdata.get(), data.get(), len);
    data = nullptr;
    return {std::move(newdata), newlen};
}

std::pair<std::unique_ptr<u8[]>, u32> PadToPowerOf2(const u8* data, u32 len) noexcept
{
    if (len == 0)
        return {nullptr, 0};

    u32 newlen = 1;
    while (newlen < len)
        newlen <<= 1;

    auto newdata = std::make_unique<u8[]>(newlen);
    memcpy(newdata.get(), data, len);
    return {std::move(newdata), newlen};
}

std::unique_ptr<u8[]> CopyToUnique(const u8* data, u32 len) noexcept
{
    if (data == nullptr || len == 0)
        return nullptr;

    auto newdata = std::make_unique<u8[]>(len);
    memcpy(newdata.get(), data, len);
    return newdata;
}
}