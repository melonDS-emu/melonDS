# OCEM

## MMIO Layout

The following MMIO definition is derived from Lauterbach's Teak debugger with  modification according to the Teak architecutre. The different layout around program address breakpoint is tested. Note that this is the only MMIO region that uses odd-address registers

```
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0060    |                              PFT                              |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0061    |                                                               |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0062    |                             PAB1_L                            |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0063    |               |      PAP1     |                       |PAB1_H |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0064    |                             PAB2_L                            |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0065    |               |      PAP2     |                       |PAB2_H |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0066    |                             PAB3_L                            |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0067    |               |      PAP3     |                       |PAB3_H |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0068    |                               |             PACNT1            |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0069    |                               |             PACNT2            |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x006A    |                               |             PACNT3            |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x006B    |                              DAM                              |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x006C    |                              DAB                              |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x006D    |SSE|ILE|BRE|TBE|INE|BRE|P3E|P2E|P1E|EXR|EXW|CDE|DAR|DAW|DVR|DVW|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x006E    |DBG|BOT|ERR|MVD|                                           |TRE|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x006F    |SFT|ILL|TBF|INT|BR |           |PA3|PA2|PA1|ABT|ERG|CD |DA |DV |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#

PFT: Program Flow Trace
PAB1_L, PAB1_H: Program Address Break Point #1
PAB2_L, PAB2_H: Program Address Break Point #2
PAB3_L, PAB3_H: Program Address Break Point #3
PAP1, PAP2, PAP3: Program page for Program Address Break Point #1/#2/#3 (?)
PACNT1, PACNT2, PACNT3: Program Address Break Point Counter #1/#2/#3
DAM: Data Address Mask
DAB: Data Address Break Point

DVW: 1 to enable data value break point on data write transaction
DVR: 1 to enable data value break point on data read transaction
DAW: 1 to enable data address break point as a result on data write transaction
DAR: 1 to enable data address break point as a result on data read transaction
CDE: 1 to enable break point as a result of simultaneous data address and data value match
EXW: 1 to enable break point as a result of external register write transaction
EXR: 1 to enable break point as a result of external register read transaction
P1E: 1 to enable program break point #1
P2E: 1 to enable program break point #2
P3E: 1 to enable program break point #3
BRE: 1 to enable break point every time the program jumps instead of executing the next address
INE: 1 to enable break point upon detection of interrupt service routine
TBE: 1 to enable break point as a result of program flow trace buffer full
BRE: 1 to enable break point when returning to the beginning of block repeat loop
ILE: 1 to enable break point on illegal condition
SSE: 1 to enable single step

DBG: 1 indicates the debug mode
BOT: 1 indicates the boot mode
ERR: 1 on detection of user reset that is being activated during execution of break point service routine
MVD: 1 on detection of MOVD instruction
TRE: 1 indicates that the current TRACE entry has to be combined with the next TRACE entry

SFT: 1 on detection of software trap
ILL: 1 on detection of illegal break point
TBF: 1 indicates program flow Trace Buffer Full
INT: 1 on detection of interrupt break point
BR: 1 on detection of branch break point
PA3: 1 on detection of program address break point #3
PA2: 1 on detection of program address break point #2
PA1: 1 on detection of program address break point #1
ABT: 1 on detection of break point due to an external event
ERG: 1 on detection of break point due to user defined register transaction
CD: 1 on detection of break point due to matched data value and matched data address
DA: 1 on detection of break point due to matched data address
DV: 1 on detection of break point due to matched data value
```
