# MIU

## MMIO Layout

The following MMIO definition is extracted from Lauterbach's Teak debugger. Some of them are untested.
```
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0100    |     Z3WS      |     Z2WS      |     Z1WS      |     Z0WS      |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0102    |               |     PRMWS     |     XYWS      |   ZDEFAULTWS  |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0104    |   Z0WSPAGE    |        Z0WSEA         |        Z0WSSA         |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0106    |   Z1WSPAGE    |        Z1WSEA         |        Z1WSSA         |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0108    |   Z2WSPAGE    |        Z2WSEA         |        Z2WSSA         |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x010A    |   Z3WSPAGE    |        Z3WSEA         |        Z3WSSA         |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x010C    |                                                               |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x010E    |                               |             XPAGE             |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0110    |                                               |     YPAGE     |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0112    |                               |             ZPAGE             |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0114    |   |         YPAGE0CFG         |       |       XPAGE0CFG       |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0116    |   |         YPAGE1CFG         |       |       XPAGE1CFG       |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0118    |   |        YOFFPAGECFG        |       |      XOFFPAGECFG      |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x011A    |                                   |PGM|   |ZSP|   |INP|TSP|PP |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x011C    |                                   |PDP|    PDLPAGE    |SDL|DLP|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x011E    |       MMIOBASE        |                                       |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0120    |                                               |  OBSMOD   |OBS|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0122    |                                   |PRD|PZS|PXS|PXT|PZT|PZW|PZR|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#

ZxWS: the number of wait-states for the x-th off-chip Z block
ZDEFAULTWS: the default number of wait-states for Z off-chip transactions
XYWS: the number of wait-states for X/Y off-chip memory transactions
PRMWS: the number of wait-states for off-chip program-memory transactions
ZxWSSA: the start address of the Z wait-states block #0 with a resolution of 1K-word.
ZxWSEA: the end address of the Z wait-states block #0 with a resolution of 1K-word
ZxWSPACE: the four LSBs of the Z wait-states block #0 page

XPAGE: the X memory page when working in paging mode 1
YPAGE: the Y memory page when working in paging mode 1
ZPAGE:
 - in paging mode 0: the absolute data memory page
 - in paging mode 1: the Z memory page
XPAGE0CFG:
 - in paging mode 0: the X memory size for page #0
 - in paging mode 1:  the X memory size for all pages
YPAGE0CFG:
 - in paging mode 0: the Y memory size for page #0
 - in paging mode 1:  the Y memory size for all pages
XPAGE1CFG: in paging mode 0, the X memory size for page #1
YPAGE1CFG: in paging mode 0, the Y memory size for page #1
XOFFPAGECFG: in paging mode 0: the X memory size for all off-chip pages
YOFFPAGECFG: in paging mode 0: the Y memory size for all off-chip pages

PP: 1 to enable the program protection mechanism
TSP: "Test Program", 1 to latch the value of the DAZXE[1]/TESTP strap pin during reset. It configures the entire program space as off-chip or based on the INTP bit
INP: "Internal Program", when a breakpoint interrupt occurs, the MIU forces a jump to page #1 (off-chip page) (when set to 0) or to page #0 (on-chip page) (when set to 1) and reads the Monitor program from this page
ZSP: 1 to enable sngle access mode, where the Z data memory space's Core/DMA addresses use only the Z-even address bus
PGM: paging mode

DLP: "Download Memory Select", this bit is used to select between two, parallel Z-space data memories, including Regular and Download memories. 0 - ZRDEN, 1- ZBRDN
SDL: "Sticky Download Select", this bit is set one cycle after DLP is set. It is not cleared when clearing the DLP bit. It remains set until specifically writing low to it during a Trap routine
PDLPAGE: "Alternative Program Page", this field is an alternative paging register for program write (movd) and program read (movp) transactions
PDP: 1 to enable alternative program page

MMIOBASE: MMIO base address, with resolution of 512-word.

OBS: 1 to enable observability mode
OBSMOD: observability mode, defines which Core/DMA address/data buses are reflected on the off-chip XZ buses
 - 0: Core XZ address/data buses
 - 1: Core Y address/data buses
 - 2: Core P address/data buses
 - 3: DMA DST address/data buses
 - 4: DMA SRC address/data buses

PRD: Signal Polarity Bit - This bit defines the polarity of RD_WR
PZS: Z Select Polarity Bit - This bit defines the polarity of ZS
PXS: X Select Polarity Bit - This bit defines the polarity of XS
PXT: X Strobe Polarity Bit - This bit defines the polarity of XSTRB
PZT: Z Strobe Polarity Bit - This bit defines the polarity of ZSTRB
PZW: Z Write Polarity Bit - This bit defines the polarity of DWZON/DWZEN
PZR: Z Read Polarity Bit - This bit defines the polarity of DRZON/DRZEN
```
