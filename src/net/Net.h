/*
    Copyright 2016-2025 melonDS team

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

#ifndef NET_H
#define NET_H

#include <memory>

#include "types.h"
#include "PacketDispatcher.h"
#include "NetDriver.h"

namespace melonDS
{

class Net
{
public:
    Net() noexcept = default;
    Net(const Net&) = delete;
    Net& operator=(const Net&) = delete;
    // Not movable because of callbacks that point to this object
    Net(Net&& other) = delete;
    Net& operator=(Net&& other) = delete;
    ~Net() noexcept = default;

    void RegisterInstance(int inst);
    void UnregisterInstance(int inst);

    void RXEnqueue(const void* buf, int len);

    int SendPacket(u8* data, int len, int inst);
    int RecvPacket(u8* data, int inst);

    void SetDriver(std::unique_ptr<NetDriver>&& driver) noexcept { Driver = std::move(driver); }
    [[nodiscard]] std::unique_ptr<NetDriver>& GetDriver() noexcept { return Driver; }
    [[nodiscard]] const std::unique_ptr<NetDriver>& GetDriver() const noexcept { return Driver; }

private:
    PacketDispatcher Dispatcher {};
    std::unique_ptr<NetDriver> Driver = nullptr;
};

}

#endif // NET_H
