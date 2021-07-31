# SIO

## MMIO Layout

The following MMIO definition is extracted from Lauterbach's Teak debugger and is not tested at all.

```
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0050    |   SIOSHIFT    |                       | IM| PH| CP| MS|CSO|CSP|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0052    |   |        CLKD2SIO       |   |             CLKD1SIO          |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0054    |                          SIODATA                              |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0056    |                                                           | SE|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0058    |                                                       |ERR|STS|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#

CSP: Chip Select Polarity
CSO: Chip Select Output
MS: Defines the operation mode of the SIO (Master or Slave).
 - 0: Master mode, the SIO clock is the Core clock frequency (with or without division)
 - 1: Slave mode, the clock is external
CP: Clock Polarity
PH: Phase, Determines whether SIODI/SIODO are sampled at the rising edge or at the falling edge
 - 0: SIODI on rising edge, SIODO on falling edge
 - 1: SIODI on falling edge, SIODO on rising edge
IM: Interrupt Mask Enable - Masks the SIO interrupt to the ICU. When set to 1, the SIO does not issue an interrupt at the end of the transfer
SIOSHIFT: Determines the length of bit shifts for every transfer (2 to 16). The value in the register is (shift - 1)

CLKD1SIO: Clock Division #1 Factor - Defines the division value of divider #1, which can range from 2 to 128. Writing 0 or 1 to this field causes the divider to be bypassed, and no division is performed.
CLKD2SIO: Clock Division #2 Factor, similar behaviour to CLKD1SIO.

SIODATA: The data register of the SIO. Writing to this register initiates a shift of SIOSHIFT + 1 bits into it and its data is output via SIODO. Data can be written to this register via the ZSI only, and only when no shift operation occurs.

SE: 1 to enable SIO operation
ERR: SIO Error, 1 when an error occurred. A shift overwrites a previous shift value before the Core has read it. The indication is automatically cleared when the Core reads the SIO_STS register
STS: SIO Status, sticky indication set at the end of a transfer and cleared automatically when read.
```
