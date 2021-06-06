# Teakra

[![Build Status](https://travis-ci.com/wwylele/teakra.svg?branch=master)](https://travis-ci.com/wwylele/teakra)
[![Build status](https://ci.appveyor.com/api/projects/status/mxr5tg4v8dafyqec/branch/master?svg=true)](https://ci.appveyor.com/project/wwylele/teakra/branch/master)

Emulator, (dis-)assembler, tools and documentation for XpertTeak, the DSP used by DSi/3DS.

Many thanks to Martin Korth and many other contributers for their help and their [excellent GBATEK doc](http://problemkaputt.de/gbatek.htm#dsixpertteakdsp)!

## Contents
Please refer to README.md in the following directories for their detail.
 - `src` contains main source code for compiling portable libraries/tools. [Detailed documentation](src/README.md) for the Teak architecture and for peripherals is also here.
 - `include` contains the header for the emulator and the disassembler libraries.
 - `dsptester` contains the source code of a 3DS tool that tests processor instructions and registers
 - `dspmemorytester` contains the source code of another 3DS tool that tests memory read/write, MMIO and DMA.

## General Information of the XpertTeak

The XpertTeak DSP consists of a Teak-family architecture processor, and peripheral components including DMA, interrupt controller and audio input/output ports etc. The exact architecture of the processor is still unclear. GBATEK states that the architecture is TeakLite II, the successor of the TeakLite architecture. Their evidence is the TeakLite II disassembler bundled in RealView Developer Suite. However, a Teak family debugger from [here](https://www.lauterbach.com) shows that the "TEAK(-REVA, -REVB, DEV-A0, -RTL2_0)" contains very similar registers and instruction set described in GBATEK, while the "TeakLite-II" contains very different registers and instructions. This shows that the architecture is likely the original Teak, introduced along with TeakLite as a "non-Lite" expansion to it.

DSi and 3DS both include XpertTeak. However, their uses of XpertTeak are pretty different. Most DSi games don't use it at all. It's used by the "Nintendo DSi Sound" and "Nintendo Zone" system utilities, and by the "Cooking Coach" cartridge (according to GBATEK), where it appears to be intended for audio/video decoding. On the contrary, 3DS games all use XpertTeak for audio decoding and output.
