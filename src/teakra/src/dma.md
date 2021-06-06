# DMA

## MMIO Layout

```
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0180    |                               |   |   |   |   |   |   |   |   |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0182    |                               |   |   |   |   |   |   |   |   |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0184    |   |   |   |   |   |   |   |   |C7 |C6 |C5 |C4 |C3 |C2 |C1 |C0 |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0186    |                               |   |   |   |   |   |   |   |   |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0188    |                               |   |   |   |   |   |   |   |   |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x018A    |                               |   |   |   |   |   |   |   |   |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x018C    |                               |E7 |E6 |E5 |E4 |E3 |E2 |E1 |E0 |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x018E    |   |     S3    |   |     S2    |   |     S1    |   |     S0    |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0190    |   |     S7    |   |     S6    |   |     S5    |   |     S4    |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#

C0..C7: enable channel 0..7?
E0..E7: end of transfer flag for channel 0..7?
S0..S7: some sort of slots for channel 0..7? are initialized as value 0..7

+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01BE    |                                                   |  CHANNEL  |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01C0    |                         SRC_ADDR_LOW                          |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01C2    |                         SRC_ADDR_HIGH                         |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01C4    |                         DST_ADDR_LOW                          |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01C6    |                         DST_ADDR_HIGH                         |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01C8    |                             SIZE0                             |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01CA    |                             SIZE1                             |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01CC    |                             SIZE2                             |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01CE    |                           SRC_STEP0                           |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01D0    |                           DST_STEP0                           |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01D2    |                           SRC_STEP1                           |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01D4    |                           DST_STEP1                           |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01D6    |                           SRC_STEP2                           |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01D8    |                           DST_STEP2                           |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01DA    |               | - |DWM| ? |   |   DST_SPACE   |   SRC_SPACE   |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01DC    |     -     |   |   | ? | ? | ? |   |   |   |   | - |   |   |   |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x01DE    |RST|STR|                       | ? | ? |                       |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#

CHANNEL: set the channel to bind with register +0x01C0..+0x01DE
SRC_SPACE / DST_SPACE: the memory space associated with SRC_ADDR and DST_ADDR
 - 0: DSP main memory
 - 1: MMIO
 - 5: Program memory (only for DST_SPACE) (untested)
 - 7: connect to AHBM / external memory

```

## Data Transfer Engine

The engine is used for transferring data between memory locations. One transfer session is like copying a three-dimensional array, with configurable strides, from the specified source and destination. `SRC_ADDR` and `DST_ADDR` specify the starting address of data source and destination. `SIZE0`, `SIZE1` and `SIZE2` specify the element count for each dimension, where `SIZE0` specifies the finest dimension in the linear memory space. (`SRC_`/`DST_`)`STEP0`, `STEP1` and `STEP2` specify the address stepping between elements for each dimension. Below is an example showing how it works.

With the source address configuration:

```
SRC_ADDR = 0
SIZE0 = 3
SIZE1 = 5
SIZE2 = 2
SRC_STEP0 = 2
SRC_STEP1 = 1
SRC_STEP2 = 7
```
The data transfer copies data from the following addresses, where `,` represents dimension 0 stepping, `/` for dimension 1 and `||` for dimension 2.
```
 0,  2,  4/  5,  7,  9/ 10, 12, 14/ 15, 17, 19/ 20, 22, 24||
31, 33, 35/ 36, 38, 40/ 41, 43, 45/ 46, 48, 50/ 51, 53, 55
```
The same rule applies to the destination address as well. Note that `SIZEx` can be 0, in which case they have the same effect as being 1.

One element can be either a 16-bit(`DWM = 0`) word or a 32-bit double word (`DWM = 1`). In the 32-bit double word mode, the address stepping method is the same as described above, except for `SIZE0`, where one 32-bit element is counted as 2 for the dimension 0 counter, so effectively `SIZE0` value is twice as the actual count of 32-bit elements copied in a dimension 0 stride. The double word mode also automatically aligns down the addresses to 32-bit boundaries.

When the memory space is specified as 7 (AHBM), it performs data transfer from/to external memory. Take 3DS for example, the external memory is FCRAM, and the address is specified as FCRAM's physical address as-is. Because the external memory has 8-bit addressing, the address resolution mismatch between DSP memory and FCRAM can make it unintuitive. Just keep in mind that the `STEP` value is also added to the address as-is, so `STEP = 1` means stepping 8-bit for FCRAM, while stepping 16-bit for DSP memory. There are also more alignment requirement on the external memory address, but it is handled by AHBM. See [ahbm.md](ahbm.md) for detail.
