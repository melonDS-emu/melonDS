## Features of the Architecture

Most things in the processor is 16-bit. There is no 8-bit byte, and the smallest addressable memory unit is 16-bit. Many registers and data buses are also 16-bit. Exceptions are 40-bit accumulators, 33-bit multiplication results and 18-bit program counter. From now on, we call 16-bit "a word".

Teak separate instruction and data address space. The instruction address is (18 + 4)-bit long, where 18 being the lower bits directly addressable by the program counter, and 4 being the higher bits specified by the `prpage` register. i.e. the program memory can be 16 pages with each page up to 0x40000 words. However, DSi/3DS seem to only support half of one program page (0x20000 words), and the register `prpage` is always 0. The data address is 16-bit long, meaning the data memory space contains 0x10000 words. However, DSi/3DS provides 0x20000-words long data memory to DSP. Accessing the larger space of data memory is achieved by memory bank switching. See [memory interface](miu.md) for details

Although the program/data memory is viewed as arrays of 16-bit words from the Teak processor with no byte-order concept, it can be also accessed from the CPU (ARM) side and viewed as byte arrays. It appears that the 16-bit words is little-endian when represented in byte arrays.

Each instruction is either one word or two words. The encoding of the instruction set is very messy. See [decoder](decoder.md) for details.

The clock rate of the processor is unclear. Disassembly of 3DS DSP binary shows that it is likely about 134MHz (134,060,000 being the number used in the code). This also matches claimed sample rate from 3dbrew (32728Hz ≈ 134MHz / 4100, where 4100 is also a number for configuring audio output timer found in disassembly). Most instructions can complete in a single clock cycle, except for double-word instructions, multiple data read/write instructions and flow control instructions.
