# PMU

## MMIO Layout

The following MMIO definition is extracted from Lauterbach's Teak debugger and is not tested at all.

```
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0080    |                             PLLMUL                            |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0082    |                                                           |PLO|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0084    |PLB|                               |            CLKD           |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0086    |                   |SDJ|SD1|SD0|SDO|   |SDA|SDH|SDG|SDS|SDD|SDC|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0088    |                   |I0J|I01|I00|I0O|   |I0A|I0H|I0G|I0S|I0D|I0C|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x008A    |                   |I1J|I11|I10|I1O|   |I1A|I1H|I1G|I1S|I1D|I1C|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x008C    |                   |I2J|I21|I20|I2O|   |I2A|I2H|I2G|I2S|I2D|I2C|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x008E    |                   |IVJ|IV1|IV0|IVO|   |IVA|IVH|IVG|IVS|IVD|IVC|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0090    |                   |T0J|T01|   |T0O|   |T0A|T0H|T0G|T0S|T0D|T0C|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0092    |                   |T1J|   |T10|T1O|   |T1A|T1H|T1G|T1S|T1D|T1C|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0094    |                   |NMJ|NM1|NM0|NMO|   |NMA|NMH|NMG|NMS|NMD|NMC|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0096    |                   |   |   |   |   |   |   |   |   |   |EXD|   |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0098    |                   |   |BM1|BM0|   |   |   |   |   |   |BMD|   |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x009A    |                                               |      SDB      |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x009C    |                                               |      I0B      |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x009E    |                                               |      I1B      |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00A0    |                                               |      I2B      |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00A2    |                                               |      IVB      |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00A4    |                                               |      T0B      |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x00A6    |                                               |      T1B      |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#

PLLMUL: Configuration of the PLL clock multiplication
PLO: PLL power-on configuration value for PLL use
CLKD: Clock Division Factor. When writing a value of 0 or 1 to CLKD, then the clock frequency is divided by 1
PLB: 1 to bypass PLL

SDx: 1 to shut down module x
I0x: 1 to enable module x recovery on interrupt 0
I1x: 1 to enable module x recovery on interrupt 1
I2x: 1 to enable module x recovery on interrupt 2
IVx: 1 to enable module x recovery on vectored interrupt
T0x: 1 to enable module x recovery on timer 0
T1x: 1 to enable module x recovery on timer 1
NMx: 1 to enable module x recovery on non-maskable interrupt
EXx: 1 to enable module x recovery on external signal
BMx: 1 to enable module x breakpoint mask

The module "x" in registers above means
 - C: core
 - D: DMA
 - S: SIO
 - G: GLUE
 - H: APBP/HPI
 - A: AHBM
 - O: OCEM
 - 0: Timer 0
 - 1: Timer 1
 - J: JAM
 - B: BTDMP 0/1/2/3, for bit 0/1/2/3 respectively
```
