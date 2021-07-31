# AHBM

## MMIO Layout

```
Status register
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00E0    |   |   |   |   |   |   |   |   |   |   |   |   |   |RNE|   |   |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
Applications wait for all bits to be 0 before connecting AHBM to DMA
RNE: 1 when the burst queue is not empty

Channel config (N = 0, 1, 2)
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00E2+N*6|       -       |   |   |   |   |   | - | TYPE  |   | BURST | R |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00E4+N*6|           -           | E | W |   |   |   |   |   |   |   |   |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00E6+N*6|               -               | D7| D6| D5| D4| D3| D2| D1| D0|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
R: Applications set this to 1 if BURST is non-zero
BURST: burst type
 - 0: x1
 - 1: x4
 - 2: x8
 - 3: ?
TYPE: data type
 - 0: 8 bit
 - 1: 16 bit
 - 2: 32 bit
 - 3: ?
W: Transfer direction
 - 0: read from external memory
 - 1: write to external memory
E: Applications always set this.
D0..D7: Connects to DMA channel 0..7 if set to one
```
