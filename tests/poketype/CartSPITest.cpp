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

#include "NDSCart/CartRetailBT.h"
#include "TestSupport.h"

using melonDS::u8;
using melonDS::NDSCart::ClassifySPISession;
using melonDS::NDSCart::SPIRoute;
using melonDS::NDSCart::SessionKeepsWriteEnable;

namespace
{

void testBackupCommandsAreAlwaysSaveMemory()
{
    // no Bluetooth meaning, so these are save memory even before SaveActive
    for (u8 cmd : {0x03, 0x04, 0x05, 0x06, 0x9F})
    {
        CHECK(ClassifySPISession(cmd, false, false) == SPIRoute::SaveMemory);
        CHECK(ClassifySPISession(cmd, true, true) == SPIRoute::SaveMemory);
    }
}

void testBluetoothFramingStaysBluetoothWhileTheSavePathIsDormant()
{
    // the three session shapes the game's Bluetooth driver sends
    CHECK(ClassifySPISession(0x01, false, false) == SPIRoute::Bluetooth);
    CHECK(ClassifySPISession(0x02, false, false) == SPIRoute::Bluetooth);
    CHECK(ClassifySPISession(0xFF, false, false) == SPIRoute::Bluetooth);
}

void testAmbiguousCommandsNeedBothLatches()
{
    // save path never used: Bluetooth, whatever the write-enable bit says
    CHECK(ClassifySPISession(0x01, false, true) == SPIRoute::Bluetooth);
    CHECK(ClassifySPISession(0x02, false, true) == SPIRoute::Bluetooth);

    // the chip is known to be in use, but no WREN has been issued
    CHECK(ClassifySPISession(0x01, true, false) == SPIRoute::Bluetooth);
    CHECK(ClassifySPISession(0x02, true, false) == SPIRoute::Bluetooth);

    // WREN then WRSR / WRITE, the order the Nitro SDK uses
    CHECK(ClassifySPISession(0x01, true, true) == SPIRoute::SaveMemory);
    CHECK(ClassifySPISession(0x02, true, true) == SPIRoute::SaveMemory);
}

void testTheSyncByteIsNeverSaveMemory()
{
    // 0xFF is the cart's wake byte, not an EEPROM opcode
    CHECK(ClassifySPISession(0xFF, true, true) == SPIRoute::Bluetooth);
}

void testEveryOtherByteIsBluetooth()
{
    for (unsigned v = 0; v < 256; v++)
    {
        u8 b = (u8)v;
        if (b == 0x01 || b == 0x02 || b == 0x03 || b == 0x04 ||
            b == 0x05 || b == 0x06 || b == 0x9F)
            continue;
        CHECK(ClassifySPISession(b, true, true) == SPIRoute::Bluetooth);
    }
}

void testWriteEnableSurvivesOnlyTheSessionsThatShouldKeepIt()
{
    // the WREN itself, and a status poll between it and the write it enables
    CHECK(SessionKeepsWriteEnable(SPIRoute::SaveMemory, 0x06, 1));
    CHECK(SessionKeepsWriteEnable(SPIRoute::SaveMemory, 0x05, 1));
    CHECK(SessionKeepsWriteEnable(SPIRoute::SaveMemory, 0x03, 1));
    CHECK(SessionKeepsWriteEnable(SPIRoute::SaveMemory, 0x9F, 1));

    // the two that complete a write, which a real chip closes the latch on
    CHECK(!SessionKeepsWriteEnable(SPIRoute::SaveMemory, 0x01, 1));
    CHECK(!SessionKeepsWriteEnable(SPIRoute::SaveMemory, 0x02, 1));

    // any Bluetooth session disarms it, or WREN, sync byte, read frame would
    // write the driver's dummy bytes into the save file
    CHECK(!SessionKeepsWriteEnable(SPIRoute::Bluetooth, 0xFF, 1));
    CHECK(!SessionKeepsWriteEnable(SPIRoute::Bluetooth, 0x01, 1));
    CHECK(!SessionKeepsWriteEnable(SPIRoute::Bluetooth, 0x02, 1));
    CHECK(!SessionKeepsWriteEnable(SPIRoute::Bluetooth, 0x06, 1));
}

void testAnEmptySessionLeavesTheWriteLatchAlone()
{
    // the driver pulses chipselect through AUXSPICNT bit 13 without clocking a
    // byte; a real chip sees no clock edges, whatever stale byte RxBuf holds
    CHECK(SessionKeepsWriteEnable(SPIRoute::Bluetooth, 0x00, 0));
    CHECK(SessionKeepsWriteEnable(SPIRoute::Bluetooth, 0xFF, 0));
    CHECK(SessionKeepsWriteEnable(SPIRoute::Bluetooth, 0x02, 0));
    CHECK(SessionKeepsWriteEnable(SPIRoute::SaveMemory, 0x02, 0));
}

}

int runCartSPITests()
{
    testBackupCommandsAreAlwaysSaveMemory();
    testBluetoothFramingStaysBluetoothWhileTheSavePathIsDormant();
    testAmbiguousCommandsNeedBothLatches();
    testTheSyncByteIsNeverSaveMemory();
    testEveryOtherByteIsBluetooth();
    testWriteEnableSurvivesOnlyTheSessionsThatShouldKeepIt();
    testAnEmptySessionLeavesTheWriteLatchAlone();
    return 0;
}
