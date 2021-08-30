# Registers

Related code: [register.h](register.h)

## Basic Registers

The `RegisterState` class includes registers that program can directly access individually, such as `r0`~`r7` and accumulators, and registers that are not directly exposed to the program but affect execution. These registers have various bit length in hardware, but they are all represented as `u16` for consistency if possible(which makes it easier to define pseudo-registers, explained below). All extra bits are zero-padded. Exceptions are
 - program counter `pc`, as well as other program address registers, are 18-bit on hardware. they are zero-padded to `u32` here.
 - accumulators `a0`, `a1`, `b0` and `b1` are 40-bit on hardware. They are sign-extended to `u64` here.
 - Multiplication results `p0` and `p1` are 33-bit on hardware. They are stored in separate `u32` fields `p[*]` for lower bits and zero-padded one-bit `u16` fields `pe[*]` for the highest bit.

Note that many registers have signed-integer semantics with two's complement representation, but we still store them as unsigned integer. This is to cooperate with the interpreter policy where signed integer arithmetic is avoided in order to avoid undefined or implementation-defined behaviours.


## Shadow Registers

Many registers have "shadow" conterpart that are not directly accessible, but can be swapped with the main registers during context change. These shadow registers are classified into 4 groups:
 - bank exchange registers
 - Batch one-side shadow registers
 - Batch two-side shadow registers
 - shadow ar/arp

Bank exchange registers swap with there counterparts by the `banke` opcode. In the `banke` opcode, program can specify which registers to swap. These bank exchange registers are suffixed with `b` in `RegisterState`.

Batch shadow registers (both one-side and two-side) swap with coutnerparts by the `cntx r/s` opcode. They are always swapped together, so there is no need to provide ability to swap then individually. The difference of one-side and two-side is that for one-side registers, only main -> shadow transfer happens in `cntx s` and shadow -> main in `cntx r`, while the full swap between main <-> shadow happens in both `cntx s` and `cntx r` for two-side registers. In `RegisterState`, batch shadow registers are created using template `Shadow(Swap)(Array)Register`, where `Swap` templates are for two-side registers and `Array` templates are for register arrays. They are then added to the master templates `Shadow(Swap)RegisterList`, which includes all shadow register states by inheritance and handles value swapping together.

Shadow ar/arp registers behaves much like Batch two-side shadow registers for ar/arp registers, but they can be also swapped with smaller batch pieces by opcode `bankr`, so they are defined separately from `ShadowSwapRegisterList` as `ShadowSwapAr(p)`.

## Block Repeat Stack

Teak supports block repeat nested in 4 levels. It has a small 4 frame stack for this, with each frame storing the loop counter, the start and the end address. The `lc` (loop counter) register visible to the program is always the one in the stack top. It is implemented as a function `Lc()` instead of a normal `u16 lc` registers in order to emulate this behaviour.

## Pseudo-registers

These registers works like bit fields and are combination of basic registers. Program can read and write them in 16-bit words, while each bit of them affects the corresponding basic register. However, they are not implemented as bit fields here for the following reasons:
 - There are two sets of control/status pseudo-registers, many fields of which maps to the same basic registers but with different bit layout. One set is TeakLite-compatible and the other includes all Teak-exclusive registers.
 - Some bits have special behaviour other than simply store the states.


These pseudo-registers are defined as some C++ types with the same names, such as `cfgi`, `stt0` and `arp2`, and `RegisterState` provides two template functions `u16 Get<T>()` and `void Set<T>(u16)`, where `T` should be one of the predefined type names, for read from and write to pseudo-registers. Here is an example
```
u16 stt1_value = registers.Get<stt1>();
```
Internally, these predefined pseudo-register-types, using template, store pointers to basic registers and their bit position and length.

## Details of all registers

### Program control registers

#### `pc`, program counter

18-bit program counter, address for loading instructions from the program memory. It is increamented by 1 or 2, depending on the instruction length, on every instruction. It is post-incrementing from instructions' view, i.e. when referencing the `pc` value in an instruction, the value is always the address of the *next* instruction.

The value of `pc` can theoretically be accessed by all opcodes that accept operand `Register` as source. However, most of these opcodes and other registers in `Register` use 16-bit data bus, so it is unclear how the convertion works, or whether it works at all. Only the opcode `mov pc a0/a1` is well tested and implemented: it is full 18-bit transfer. Other code path that access `pc` via `Register` is currently marked as unreachable. On a side node, the TeakLite architecture only has a 16-bit `pc` register, where transferring `pc` value via 16-bit data base makes more sense.

`pc` value can be pushed to / popped from the stack as two words. The word order is specified by the `cpc` register.

Aside from increamenting on every instruction, `pc` value can also be modified by the following instructions and circumstances. Please refer to their sections for detail.
 - `mov a0/a1/b0/b1 pc` opcodes.
 - subroutine call: `call` opcodes family.
 - subroutine return: `ret` opcodes family.
 - branching: `br` opcodes family.
 - `movpdw [a0/a1] pc` opcode.
 - hitting the end of a repeating block indicated by opcode `bkrep`.
 - repeating instruction indicated by opcode `rep`.
 - on interrupt.

#### `prpage`, program page

4-bit register to select program page in the program memory, extending the entire possible program memory space to `4 + 18 = 22` bit. However XpertTeak/DSi/3DS only support single program page, so this register should remain 0.

#### `repc` repeat counter

16-bit counter that is set to `repeat count - 1` on `rep` opcodes (i.e. exactly the value passed in the opcodes) and decrements on every repeat until 0 (inclusive). During the repeat, program can read the register value by `mov` opcodes. Out side repeat loop, this register can be used as a general register by using `mov` opcodes to read and write. It can also be `push`ed to or `pop`ed from the stack, and swap with its shadow counterpart `repcs` on context store / restore if the configuration register `crep` is clear.

#### TODO: block repeat


### Arithmetic registers

#### `a0`/`a1`/`b0`/`b1`, accumulators

40-bit accumulators, stored as sign-extended 64-bit integer in Teakra. Accumulators each has three parts from the most significant bit to the least one: `8:16:16` as `e:h:l` (extension, high, and low). The `h` and `l` part are visible to many operations, while the `e` part can be only access by opcode `push/pop Abe`, and for `a0`/`a1`, the lower 4 bits of their `e` part is exposed in pseduo-registers `st0`/`st1`.

#### Saturation

#### Shifting

#### Flags

#### `vtr0`/`vtr1`, Viterbi registers

These two 16-bit registers are dedicated for [Viterbi decoding](https://en.wikipedia.org/wiki/Viterbi_decoder),and for path recovery to be specific. For every opcodes that contain a `vtrshr` part, status bits `fc0` and `fc1` are pushed into `vtr0` and `vtr1`, respectively. `vtrmov` opcodes can move the value from these registers to accumulators. `vtrclr` can clear these registers to zero. There is no way to directly access these registers.

### Multiplication registers

#### `x0`/`x1`/`y0`/`y1`, factor registers

16 bits each, used as factors for calculating multiplication, but can also be used for general purpose.

#### `p0`/`p1`, product registers

33 bits each, stores the result of multiplication operation when the instruction initiates one. `p0` stores `x0 * y0` and `p1` stores `x1 * y1`. The registers are split in to 32-bit parts `p[0]/p[1]` and sign bits `pe[0]/pe[1]` in the code.

### Address registers

#### `r0`..`r7`, general address registers

These 16-bit registers can be used for general purpose or as memory pointer. Most opcodes that support indirect data access uses these registers as address operand (e.g. `[r0]`). Specifically `r7` also supports supplying an additional immediate address offset in some opcodes, and is often used as stack frame pointer.

#### `sp`, stack pointer

16-bit register as a pointer to data memory, decrements on `push` / `call` / `bksto [sp]` opcodes and increments on `pop` / `ret` / `bkrst [sp]` opcodes. Can be also read and modified as a general register via all opcodes that accept `Register` operand.

#### `page`, data memory page

Used to specify the higher 8-bits for opcodes that use 8-bit immediate data address.

#### `pcmhi`, program memory transfer page

Used to specify the higher 2-bits for opcodes that use 16-bit program address.

### Advanced address registers

#### Step/mod configuration registers

#### Indirect address registers

### Interrupt registers

### Extension registers
