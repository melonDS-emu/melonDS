# APBP

## MMIO Layout

```
Command/Reply registers
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00C0    |                             REPLY0                            |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00C2    |                              CMD0                             |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00C4    |                             REPLY1                            |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00C6    |                              CMD1                             |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00C8    |                             REPLY2                            |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00CA    |                              CMD2                             |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#

REPLY0..2: data sent from DSP to CPU
CMD0..2: data received from CPU to DSP

Semaphore registers
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00CC    |                         SET_SEMAPHORE                         |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00CE    |                         MASK_SEMAPHORE                        |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00D0    |                         ACK_SEMAPHORE                         |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00D2    |                         GET_SEMAPHORE                         |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#

SET_SEMAPHORE: sets semaphore sent from DSP to CPU
MASK_SEMAPHORE: masks semaphore interrupt received from CPU to DSP
ACK_SEMAPHORE: acknowledges/clears semaphore received from CPU to DSP
GET_SEMAPHORE: semaphore received from CPU to DSP

Config/status registers
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00D4    |       |CI2|CI1|           |CI0|                   |END|       |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00D6    |   |   | C2| C1|   |   | S | C0| R2| R1| R0|   |   |   |   |   |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00D8    | C2| C1| C0| R2| R1| R0| S'|WEM|WFL|RNE|RFL|       |PRS|WTU|RTU|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#

END: CPU-side registers endianness. 1 to swap byte order of CPU-side registers
R0..R2: 1 when there is data in REPLY0..2
C0..C2: 1 when there is data in CMD0..2
CI0...CI2: 1 to disable interrupt when CMD0..2 is written by CPU
S: 1 when (GET_SEMAPHORE & ~MASK_SEMAPHORE) is non-zero
S': similar to S, but for CPU side
RTU: CPU-side read transfer underway flag
WTU: CPU-side write transfer underway flag
PRS: peripheral reset flag
RFL: CPU-side read FIFO full flag
RNE: CPU-side read FIFO non-empty flag
WFL: CPU-side write FIFO full flag
WEM: CPU-side write FIFO empty flag
* Note 0x00D8 is a mirror of CPU-side register DSP_PSTS
```

## CPU-DSP communication

APBP is an important port for CPU and DSP to communicate with each other. It mainly contains 3 pairs of symmetrical data channels and a pair of 16-bit symmetrical semaphore channel.

### Data channels

When one side writes to a data channel, it sets the 1 to the "data ready" bit (`R0..R2` or `C0..C2`), and fires interrupt on the other side if enabled. When the othersides read from the channel register, it automatically clears the "data ready" bit. If new data is written before the previous one is read, the new data will overwrite the old data and fires another interrupt.

### Semaphore channels

There are two 16-bit semaphore channels for two directions, `CPU->DSP` and `DSP->CPU`. Writing to `SET_SEMAPHORE` from side A or `ACK_SEMAPHORE` from side B changes the semaphore value of direction `A->B`. Semaphore value change also changes the corresponding `S` bit (See the calculation above). Changing in mask is also reflected in the `S` bit immediately. Whenever a `0->1` transition for `S` is detected, interrupt is fired on B side.

## CPU side MMIO

```
Command/Reply registers
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0000    |                             PDATA                             |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0004    |                             PADDR                             |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0008    |   DATA_SPACE  |RI2|RI1|RI0|IWE|IWF|IRE|IRF|STR| BURST |INC|RST|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x000C    | C2| C1| C0| R2| R1| R0| S'|WEM|WFL|RNE|RFL|       |PRS|WTU|RTU|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0010    |                         SET_SEMAPHORE'                        |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0014    |                         MASK_SEMAPHORE'                       |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0018    |                         ACK_SEMAPHORE'                        |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x001C    |                         GET_SEMAPHORE'                        |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0020    |                              CMD0                             |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0024    |                             REPLY0                            |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0028    |                              CMD1                             |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x002C    |                             REPLY1                            |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0030    |                              CMD2                             |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0034    |                             REPLY2                            |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
```
