# ICU

## MMIO Layout

```
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0200    |IPF|IPE|IPD|IPC|IPB|IPA|IP9|IP8|IP7|IP6|IP5|IP4|IP3|IP2|IP1|IP0|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0202    |IAF|IAE|IAD|IAC|IAB|IAA|IA9|IA8|IA7|IA6|IA5|IA4|IA3|IA2|IA1|IA0|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0204    |ITF|ITE|ITD|ITC|ITB|ITA|IT9|IT8|IT7|IT6|IT5|IT4|IT3|IT2|IT1|IT0|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0206    |I0F|I0E|I0D|I0C|I0B|I0A|I09|I08|I07|I06|I05|I04|I03|I02|I01|I00|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0208    |I1F|I1E|I1D|I1C|I1B|I1A|I19|I18|I17|I16|I15|I14|I13|I12|I11|I10|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x020A    |I2F|I2E|I2D|I2C|I2B|I2A|I29|I28|I27|I26|I25|I24|I23|I22|I21|I20|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x020C    |IVF|IVE|IVD|IVC|IVB|IVA|IV9|IV8|IV7|IV6|IV5|IV4|IV3|IV2|IV1|IV0|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x020E    |TPF|TPE|TPD|TPC|TPB|TPA|TP9|TP8|TP7|TP6|TP5|TP4|TP3|TP2|TP1|TP0|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0210    |PLF|PLE|PLD|PLC|PLB|PLA|PL9|PL8|PL7|PL6|PL5|PL4|PL3|PL2|PL1|PL0|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#

N = 0..15
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0212+N*4|VIC|                                                   |VADDR_H|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0214+N*4|                            VADDR_L                            |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#

IP0..IPF: IRQ pending flag
IA0..IAF: IRQ acknowledge, 1 to clears pending flag
IT0..ITF: triggers IRQ manually
I00..I0F: connects IRQ to core interrupt 0
I10..I1F: connects IRQ to core interrupt 1
I20..I2F: connects IRQ to core interrupt 2
IV0..IVF: connects IRQ to core vectored interrupt
TP0..TPF: IRQ trigger mode? 0: pulse, 1: sticky
PL0..PLF: polarity of IRQ signal?
 * I might have got PLx and TPx swapped
VADDR_H, VADDR_L: address of interrupt handler for vectored interrupt
VIC: 1 to enable context switch on vectored interrupt
```

## IRQ signal

All bit fields in MMIO correspond to 16 IRQ signal. Some of them are known to connect to other peripherals as shown below. There might be more undiscovered components that have associated IRQ
```
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|           | *F| *E| *D| *C| *B| *A| *9| *8| *7| *6| *5| *4| *3| *2| *1| *0|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
              |   |   |   |   |   |   |
DMA   --------*   |   |   |   |   |   |
APBP  ------------*   |   |   |   |   |
ICU?  ----------------*   |   |   |   |
      --------------------*   |   |   |
BTDMP ------------------------*   |   |
TIMER0----------------------------*   |
TIMER1--------------------------------*
```

## IRQ-to-interrupt translator

The main job of ICU is to translate 16 IRQ signals to 4 processor interrupt signals (int0, int1, int2 and vint), specified by `I0x`, `I1x`, `I2x` and `IVx` registers. When an IRQ is signaled, ICU will generate interrupt signal and the processor starts to execute interrupt handling procedure if the core interrupt is enabled. The procedure is supposed check `IPx` reigster to know the exact IRQ index, and to set `IAx` registers to clear the IRQ signal.

## Software interrupt

The processor can writes to `ITx` to generate a software interrupt manually. A use case for this in many 3DS games is that IRQ 0 is used as the reschedule procedure for multithreading. When a thread wants to yield, it triggers IRQ 0, which switches thread context and resume another thread.
