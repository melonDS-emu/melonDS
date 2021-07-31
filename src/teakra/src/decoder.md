# Teak Instruction Set Encoding

Each opcode is one- or two-word wide. In this code base, two-word wide opcodes are generally referred as "expanded". The optional second word is used purely for parameters, so the opcode family can always be determined from the first word (which then determine whether there is a second word). The encoding is very messy. The general theme of the encoding are
 - The encoding seems first designed for TeakLite (which is already a mess), then additional opcodes from Teak squeezed in formerly unused locations.
 - While registers `r0`~`r7` are generally at equal status in the computation model, they are not always in the opcodes. Specifically, as `r6` doesn't present in TeakLite, and because of the opcode squeezing, many opcodes involving `r6` are encoded separately with seemingly random pattern.
 - Similarly, there are also opcodes that are in the same family but have different pattern for different arguments.
 - Some specific combination of arguments can be invalid for one opcode, and this specific pattern can be used for a totally different opcode.
 - Some opcodes have unused bits, where setting either 0 or 1 has no effect.

## Comparison between `decoder.h` and gbatek

`decoder.h` contains the full table of the instruction set. It is derived from the table in gbatek, but rearranged a little according to the opcode families and patterns. The notation of the two table are comparable. For example:

```
teakra: INST(push, 0xD7C8, At<Abe, 1>, Unused<0>)
gbatek: D7C8h TL2 push Abe@1, Unused1@0
```
This means that the opcode has a parameter `Abe` at bit 1 (which is 2-bit long, defined by operand `Abe`), and an unused bit at bit 0. The rest bit of the opcode (bit 3~15) should match the pattern `0xD7C8`. The assembly of this opcode would be like `push <Abe>`.

Sometimes there is a 18-bit address parameter that is split into two parts:

```
teakra: INST(br, 0x4180, At<Address18_16, 16>, At<Address18_2, 4>, At<Cond, 0>)
gbatek: 4180h TL  br   Address18@16and4, Cond@0
```
Note that the existence of `At<..., 16>` also indicates that this opcode is 2-word long. The pattern `0x4180` is only matched against the first word.

Some opcodes that have the same pattern are merged into one opcode in teakra. For example
```
teakra: INST(alm, 0xA000, At<Alm, 9>, At<MemImm8, 0>, At<Ax, 8>),
gbatek: {
        A000h TL  or   MemImm8@0, Ax@8
        A200h TL  and  MemImm8@0, Ax@8
        ...
        BE00h TL  cmpu MemImm8@0, Ax@8
}
```
Here the operation name is also treated as an operand `Alm` in teakra.

Opcodes with constants that has special encoding are marked in their opcode names:
```
teakra: INST(sub_p1, 0xD4B9, At<Ax, 8>),
gbatek: D4B9h TL2 sub  p1, Ax@8
```

However, if several opcodes with constants have different encoding but similar semantics, the constants are placed in the parameter list and delegate to the same opcode
```
teakra: {
        INST(alm_r6, 0xD388, Const<Alm, 0>, At<Ax, 4>),
        INST(alm_r6, 0xD389, Const<Alm, 1>, At<Ax, 4>),
        ...
        INST(alm_r6, 0x9462, Const<Alm, 8>, At<Ax, 0>),
        ...
        INST(alm_r6, 0x5F41, Const<Alm, 13>, Const<Ax, 0>),
        INST(alm_r6, 0x9062, Const<Alm, 14>, At<Ax, 8>, Unused<0>),
        INST(alm_r6, 0x8A63, Const<Alm, 15>, At<Ax, 3>),
}
gbatek: {
        D388h TL2 or   r6, Ax@4
        D389h TL2 and  r6, Ax@4
        ...
        9462h TL2 msu  y0, r6, Ax@0 // y0 is implied by msu
        ...
        5F41h TL2 sqr  r6 // Ax is implied/unused for sqr
        9062h TL2 sqra r6, Ax@8, Unused1@0
        8A63h TL2 cmpu r6, Ax@3
}
```