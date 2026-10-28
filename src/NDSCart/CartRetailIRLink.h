/*
    Copyright 2016-2026 melonDS team

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

#ifndef NDSCART_CARTRETAILIRLINK_H
#define NDSCART_CARTRETAILIRLINK_H

#include "../types.h"

/**
 * IR link interface for communicating with auxillary Nintendo DS devices
 * NTR-027 and NTR-032, via Stepdad or a similarly designed emulator
 * 
 * Receive and transmit locally via port 0xAAAA (127.0.0.1:43690) as a socket
 * client and feed the data back into the NDS cartridge for polling via SPI
 * 
 * Every record on the connection is 8 bytes:
 *   [0]    'B' for an IR byte, 'T' for the end of a burst (packet)
 *   [1]    the IR byte, for 'B' records
 *   [2..3] 'I', 'R', so a receiver can find the record boundaries again
 *   [4..7] the sender's clock in device CPU cycles (3.6864 MHz), big-endian
 */
namespace melonDS::NDSCart::IRLink
{

// drops packets waiting to be read
void Reset(void);

// sends one byte of a packet
void SendByte(u8 val);

// ends the packet being sent
void EndPacket(void);

// checks for incoming data, and returns the length of the oldest packet
// waiting to be read, or 0 if there is none
u32 Receive(void);

// returns a byte of the oldest packet waiting to be read
u8 ReceivedByte(u32 pos);

// drops the oldest packet waiting to be read, once the game has read it
void DropPacket(void);

}

#endif
