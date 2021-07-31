# MMIO

The core processor communicates with peripherals (and indirectly external hardware such as CPU) via MMIO. The MMIO region occupies 0x0800-word region in the 16-bit address space, initially starting at 0x8000. The location of MMIO region can be configured in MIU (...yes, via MMIO). MMIO registers usually use even addresses only, which seems to be a hardware optimization as there is a register in MIU that enables forcing this rule. Each peripheral corresponds to a sub-region in MMIO, listed below. The detail of registers are in their corresponding peripheral documents. The register addresses in these documents are offsets to the start of the MMIO region.

 - `+0x0000` ?
 - `+0x0004` JAM
 - `+0x0010` GLUE
 - `+0x0020` [Timer](timer.md)
 - `+0x0050` [SIO](sio.md), Serial IO
 - `+0x0060` [OCEM](ocem.md), On-chip Emulation Module
 - `+0x0080` [PMU](pmu.md), Power Management Unit
 - `+0x00C0` [APBP](apbp.md), Advanced Peripheral Bus Port?
 - `+0x00E0` [AHBM](ahbm.md), Advanced High Performance Bus Master
 - `+0x0100` [MIU](miu.md), Memory Interface Unit
 - `+0x0140` [CRU](cru.md), Code Replacement Unit
 - `+0x0180` [DMA](dma.md), Direct Memory Access
 - `+0x0200` [ICU](icu.md), Interrupt Control Unit
 - `+0x0280` [BTDMP](btdmp.md), Buffered Time Division Multiplexing Port
