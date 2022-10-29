/*
    Copyright 2016-2022 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

namespace Teakra
{

void OP_UNDEFINED(Interpreter& cpu, u16 instr)
{
    cpu.undefined(instr);
}

void OP_GLUE_0000(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.nop();
}

void OP_GLUE_94C0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 8) & 0x1;
    Rn arg1{}; arg1.storage = instr & 0x7;
    StepZIDS arg2{}; arg2.storage = (instr >> 3) & 0x3;

    cpu.norm(arg0, arg1, arg2);
}

void OP_GLUE_4980(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    SwapType arg0{}; arg0.storage = instr & 0xF;

    cpu.swap(arg0);
}

void OP_GLUE_0020(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.trap();
}

void OP_GLUE_A000(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = (instr >> 9) & 0xF;
    MemImm8 arg1{}; arg1.storage = instr & 0xFF;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.alm(arg0, arg1, arg2);
}

void OP_GLUE_8080(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = (instr >> 9) & 0xF;
    Rn arg1{}; arg1.storage = instr & 0x7;
    StepZIDS arg2{}; arg2.storage = (instr >> 3) & 0x3;
    Ax arg3{}; arg3.storage = (instr >> 8) & 0x1;

    cpu.alm(arg0, arg1, arg2, arg3);
}

void OP_GLUE_80A0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = (instr >> 9) & 0xF;
    Register arg1{}; arg1.storage = instr & 0x1F;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.alm(arg0, arg1, arg2);
}

void OP_GLUE_D388(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 0;
    Ax arg1{}; arg1.storage = (instr >> 4) & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_D389(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 1;
    Ax arg1{}; arg1.storage = (instr >> 4) & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_D38A(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 2;
    Ax arg1{}; arg1.storage = (instr >> 4) & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_D38B(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 3;
    Ax arg1{}; arg1.storage = (instr >> 4) & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_D38C(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 4;
    Ax arg1{}; arg1.storage = (instr >> 4) & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_D38D(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 5;
    Ax arg1{}; arg1.storage = (instr >> 4) & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_D38E(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 6;
    Ax arg1{}; arg1.storage = (instr >> 4) & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_D38F(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 7;
    Ax arg1{}; arg1.storage = (instr >> 4) & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_9462(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 8;
    Ax arg1{}; arg1.storage = instr & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_9464(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 9;
    Ax arg1{}; arg1.storage = instr & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_9466(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 10;
    Ax arg1{}; arg1.storage = instr & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_5E23(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 11;
    Ax arg1{}; arg1.storage = (instr >> 8) & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_5E22(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 12;
    Ax arg1{}; arg1.storage = (instr >> 8) & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_5F41(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 13;
    Ax arg1{}; arg1.storage = 0;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_9062(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 14;
    Ax arg1{}; arg1.storage = (instr >> 8) & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_8A63(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alm arg0{}; arg0.storage = 15;
    Ax arg1{}; arg1.storage = (instr >> 3) & 0x1;

    cpu.alm_r6(arg0, arg1);
}

void OP_GLUE_D4F8(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Alu arg0{}; arg0.storage = instr & 0x7;
    MemImm16 arg1{}; arg1.storage = expand;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.alu(arg0, arg1, arg2);
}

void OP_GLUE_D4D8(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Alu arg0{}; arg0.storage = instr & 0x7;
    MemR7Imm16 arg1{}; arg1.storage = expand;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.alu(arg0, arg1, arg2);
}

void OP_GLUE_80C0(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Alu arg0{}; arg0.storage = (instr >> 9) & 0x7;
    Imm16 arg1{}; arg1.storage = expand;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.alu(arg0, arg1, arg2);
}

void OP_GLUE_C000(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alu arg0{}; arg0.storage = (instr >> 9) & 0x7;
    Imm8 arg1{}; arg1.storage = instr & 0xFF;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.alu(arg0, arg1, arg2);
}

void OP_GLUE_4000(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Alu arg0{}; arg0.storage = (instr >> 9) & 0x7;
    MemR7Imm7s arg1{}; arg1.storage = instr & 0x7F;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.alu(arg0, arg1, arg2);
}

void OP_GLUE_D291(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 10) & 0x3;
    Ax arg1{}; arg1.storage = (instr >> 6) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 5) & 0x1;

    cpu.or_(arg0, arg1, arg2);
}

void OP_GLUE_D4A4(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 8) & 0x1;
    Bx arg1{}; arg1.storage = (instr >> 1) & 0x1;
    Ax arg2{}; arg2.storage = instr & 0x1;

    cpu.or_(arg0, arg1, arg2);
}

void OP_GLUE_D3C4(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 10) & 0x1;
    Bx arg1{}; arg1.storage = (instr >> 1) & 0x1;
    Ax arg2{}; arg2.storage = instr & 0x1;

    cpu.or_(arg0, arg1, arg2);
}

void OP_GLUE_E100(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Alb arg0{}; arg0.storage = (instr >> 9) & 0x7;
    Imm16 arg1{}; arg1.storage = expand;
    MemImm8 arg2{}; arg2.storage = instr & 0xFF;

    cpu.alb(arg0, arg1, arg2);
}

void OP_GLUE_80E0(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Alb arg0{}; arg0.storage = (instr >> 9) & 0x7;
    Imm16 arg1{}; arg1.storage = expand;
    Rn arg2{}; arg2.storage = instr & 0x7;
    StepZIDS arg3{}; arg3.storage = (instr >> 3) & 0x3;

    cpu.alb(arg0, arg1, arg2, arg3);
}

void OP_GLUE_81E0(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Alb arg0{}; arg0.storage = (instr >> 9) & 0x7;
    Imm16 arg1{}; arg1.storage = expand;
    Register arg2{}; arg2.storage = instr & 0x1F;

    cpu.alb(arg0, arg1, arg2);
}

void OP_GLUE_47B8(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Alb arg0{}; arg0.storage = instr & 0x7;
    Imm16 arg1{}; arg1.storage = expand;

    cpu.alb_r6(arg0, arg1);
}

void OP_GLUE_43C8(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Alb arg0{}; arg0.storage = 0;
    Imm16 arg1{}; arg1.storage = expand;
    SttMod arg2{}; arg2.storage = instr & 0x7;

    cpu.alb(arg0, arg1, arg2);
}

void OP_GLUE_4388(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Alb arg0{}; arg0.storage = 1;
    Imm16 arg1{}; arg1.storage = expand;
    SttMod arg2{}; arg2.storage = instr & 0x7;

    cpu.alb(arg0, arg1, arg2);
}

void OP_GLUE_0038(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Alb arg0{}; arg0.storage = 2;
    Imm16 arg1{}; arg1.storage = expand;
    SttMod arg2{}; arg2.storage = instr & 0x7;

    cpu.alb(arg0, arg1, arg2);
}

void OP_GLUE_9470(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Alb arg0{}; arg0.storage = 4;
    Imm16 arg1{}; arg1.storage = expand;
    SttMod arg2{}; arg2.storage = instr & 0x7;

    cpu.alb(arg0, arg1, arg2);
}

void OP_GLUE_9478(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Alb arg0{}; arg0.storage = 5;
    Imm16 arg1{}; arg1.storage = expand;
    SttMod arg2{}; arg2.storage = instr & 0x7;

    cpu.alb(arg0, arg1, arg2);
}

void OP_GLUE_D2DA(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 10) & 0x3;
    Bx arg1{}; arg1.storage = instr & 0x1;

    cpu.add(arg0, arg1);
}

void OP_GLUE_5DF0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 1) & 0x1;
    Ax arg1{}; arg1.storage = instr & 0x1;

    cpu.add(arg0, arg1);
}

void OP_GLUE_D782(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = instr & 0x1;

    cpu.add_p1(arg0);
}

void OP_GLUE_5DF8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Px arg0{}; arg0.storage = (instr >> 1) & 0x1;
    Bx arg1{}; arg1.storage = instr & 0x1;

    cpu.add(arg0, arg1);
}

void OP_GLUE_8A61(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 3) & 0x3;
    Bx arg1{}; arg1.storage = (instr >> 8) & 0x1;

    cpu.sub(arg0, arg1);
}

void OP_GLUE_8861(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 4) & 0x1;
    Ax arg1{}; arg1.storage = (instr >> 3) & 0x1;

    cpu.sub(arg0, arg1);
}

void OP_GLUE_D4B9(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 8) & 0x1;

    cpu.sub_p1(arg0);
}

void OP_GLUE_8FD0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Px arg0{}; arg0.storage = (instr >> 1) & 0x1;
    Bx arg1{}; arg1.storage = instr & 0x1;

    cpu.sub(arg0, arg1);
}

void OP_GLUE_5DC0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 2) & 0x3;

    cpu.app(arg0, SumBase::Zero, false, false, false, false);
}

void OP_GLUE_5DC1(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 2) & 0x3;

    cpu.app(arg0, SumBase::Zero, false, false, false, true);
}

void OP_GLUE_4590(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 2) & 0x3;

    cpu.app(arg0, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_4592(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 2) & 0x3;

    cpu.app(arg0, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_4593(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 2) & 0x3;

    cpu.app(arg0, SumBase::Acc, false, true, false, true);
}

void OP_GLUE_5DC2(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 2) & 0x3;

    cpu.app(arg0, SumBase::Zero, false, false, true, false);
}

void OP_GLUE_5DC3(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 2) & 0x3;

    cpu.app(arg0, SumBase::Zero, false, false, true, true);
}

void OP_GLUE_80C6(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 10) & 0x3;

    cpu.app(arg0, SumBase::Acc, true, false, true, false);
}

void OP_GLUE_82C6(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 10) & 0x3;

    cpu.app(arg0, SumBase::Acc, true, false, true, true);
}

void OP_GLUE_83C6(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 10) & 0x3;

    cpu.app(arg0, SumBase::Acc, true, true, true, true);
}

void OP_GLUE_906C(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = instr & 0x3;

    cpu.app(arg0, SumBase::Acc, false, false, true, false);
}

void OP_GLUE_49C2(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 4) & 0x3;

    cpu.app(arg0, SumBase::Acc, true, false, false, false);
}

void OP_GLUE_916C(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = instr & 0x3;

    cpu.app(arg0, SumBase::Acc, false, false, true, true);
}

void OP_GLUE_49C3(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 4) & 0x3;

    cpu.app(arg0, SumBase::Acc, true, false, false, true);
}

void OP_GLUE_6F80(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 2) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ab arg3{}; arg3.storage = (instr >> 3) & 0x3;

    cpu.add_add(arg0, arg1, arg2, arg3);
}

void OP_GLUE_6FA0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 2) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ab arg3{}; arg3.storage = (instr >> 3) & 0x3;

    cpu.add_sub(arg0, arg1, arg2, arg3);
}

void OP_GLUE_6FC0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 2) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ab arg3{}; arg3.storage = (instr >> 3) & 0x3;

    cpu.sub_add(arg0, arg1, arg2, arg3);
}

void OP_GLUE_6FE0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 2) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ab arg3{}; arg3.storage = (instr >> 3) & 0x3;

    cpu.sub_sub(arg0, arg1, arg2, arg3);
}

void OP_GLUE_5DB0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 1) & 0x1;
    ArStep1 arg1{}; arg1.storage = instr & 0x1;
    Ab arg2{}; arg2.storage = (instr >> 2) & 0x3;

    cpu.add_sub_sv(arg0, arg1, arg2);
}

void OP_GLUE_5DE0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 1) & 0x1;
    ArStep1 arg1{}; arg1.storage = instr & 0x1;
    Ab arg2{}; arg2.storage = (instr >> 2) & 0x3;

    cpu.sub_add_sv(arg0, arg1, arg2);
}

void OP_GLUE_8064(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 8) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ab arg3{}; arg3.storage = (instr >> 3) & 0x3;

    cpu.sub_add_i_mov_j_sv(arg0, arg1, arg2, arg3);
}

void OP_GLUE_5D80(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 2) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ab arg3{}; arg3.storage = (instr >> 3) & 0x3;

    cpu.sub_add_j_mov_i_sv(arg0, arg1, arg2, arg3);
}

void OP_GLUE_9070(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 8) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ab arg3{}; arg3.storage = (instr >> 2) & 0x3;

    cpu.add_sub_i_mov_j(arg0, arg1, arg2, arg3);
}

void OP_GLUE_5E30(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 8) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ab arg3{}; arg3.storage = (instr >> 2) & 0x3;

    cpu.add_sub_j_mov_i(arg0, arg1, arg2, arg3);
}

void OP_GLUE_8000(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Mul3 arg0{}; arg0.storage = (instr >> 8) & 0x7;
    Rn arg1{}; arg1.storage = instr & 0x7;
    StepZIDS arg2{}; arg2.storage = (instr >> 3) & 0x3;
    Imm16 arg3{}; arg3.storage = expand;
    Ax arg4{}; arg4.storage = (instr >> 11) & 0x1;

    cpu.mul(arg0, arg1, arg2, arg3, arg4);
}

void OP_GLUE_8020(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Mul3 arg0{}; arg0.storage = (instr >> 8) & 0x7;
    Rn arg1{}; arg1.storage = instr & 0x7;
    StepZIDS arg2{}; arg2.storage = (instr >> 3) & 0x3;
    Ax arg3{}; arg3.storage = (instr >> 11) & 0x1;

    cpu.mul_y0(arg0, arg1, arg2, arg3);
}

void OP_GLUE_8040(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Mul3 arg0{}; arg0.storage = (instr >> 8) & 0x7;
    Register arg1{}; arg1.storage = instr & 0x1F;
    Ax arg2{}; arg2.storage = (instr >> 11) & 0x1;

    cpu.mul_y0(arg0, arg1, arg2);
}

void OP_GLUE_D000(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Mul3 arg0{}; arg0.storage = (instr >> 8) & 0x7;
    R45 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    StepZIDS arg2{}; arg2.storage = (instr >> 5) & 0x3;
    R0123 arg3{}; arg3.storage = instr & 0x3;
    StepZIDS arg4{}; arg4.storage = (instr >> 3) & 0x3;
    Ax arg5{}; arg5.storage = (instr >> 11) & 0x1;

    cpu.mul(arg0, arg1, arg2, arg3, arg4, arg5);
}

void OP_GLUE_5EA0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Mul3 arg0{}; arg0.storage = (instr >> 1) & 0x7;
    Ax arg1{}; arg1.storage = instr & 0x1;

    cpu.mul_y0_r6(arg0, arg1);
}

void OP_GLUE_E000(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Mul2 arg0{}; arg0.storage = (instr >> 9) & 0x3;
    MemImm8 arg1{}; arg1.storage = instr & 0xFF;
    Ax arg2{}; arg2.storage = (instr >> 11) & 0x1;

    cpu.mul_y0(arg0, arg1, arg2);
}

void OP_GLUE_0800(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm8s arg0{}; arg0.storage = instr & 0xFF;

    cpu.mpyi(arg0);
}

void OP_GLUE_D080(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    R45 arg0{}; arg0.storage = (instr >> 2) & 0x1;
    StepZIDS arg1{}; arg1.storage = (instr >> 5) & 0x3;
    R0123 arg2{}; arg2.storage = instr & 0x3;
    StepZIDS arg3{}; arg3.storage = (instr >> 3) & 0x3;
    Ax arg4{}; arg4.storage = (instr >> 8) & 0x1;

    cpu.msu(arg0, arg1, arg2, arg3, arg4);
}

void OP_GLUE_90C0(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;
    Imm16 arg2{}; arg2.storage = expand;
    Ax arg3{}; arg3.storage = (instr >> 8) & 0x1;

    cpu.msu(arg0, arg1, arg2, arg3);
}

void OP_GLUE_8264(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn2 arg0{}; arg0.storage = (instr >> 3) & 0x3;
    ArStep2 arg1{}; arg1.storage = instr & 0x3;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.msusu(arg0, arg1, arg2);
}

void OP_GLUE_4D84(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 1) & 0x1;

    cpu.mac_x1to0(arg0);
}

void OP_GLUE_5E28(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 2) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ax arg3{}; arg3.storage = (instr >> 8) & 0x1;

    cpu.mac1(arg0, arg1, arg2, arg3);
}

void OP_GLUE_6700(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Moda4 arg0{}; arg0.storage = (instr >> 4) & 0xF;
    Ax arg1{}; arg1.storage = (instr >> 12) & 0x1;
    Cond arg2{}; arg2.storage = instr & 0xF;

    cpu.moda4(arg0, arg1, arg2);
}

void OP_GLUE_6F00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Moda3 arg0{}; arg0.storage = (instr >> 4) & 0x7;
    Bx arg1{}; arg1.storage = (instr >> 12) & 0x1;
    Cond arg2{}; arg2.storage = instr & 0xF;

    cpu.moda3(arg0, arg1, arg2);
}

void OP_GLUE_D7C2(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = instr & 0x1;

    cpu.pacr1(arg0);
}

void OP_GLUE_8ED0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 2) & 0x3;
    Ab arg1{}; arg1.storage = instr & 0x3;

    cpu.clr(arg0, arg1);
}

void OP_GLUE_8DD0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 2) & 0x3;
    Ab arg1{}; arg1.storage = instr & 0x3;

    cpu.clrr(arg0, arg1);
}

void OP_GLUE_5C00(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Imm8 arg0{}; arg0.storage = instr & 0xFF;
    Address16 arg1{}; arg1.storage = expand;

    cpu.bkrep(arg0, arg1);
}

void OP_GLUE_5D00(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;
    Address18_16 arg1{}; arg1.storage = expand;
    Address18_2 arg2{}; arg2.storage = (instr >> 5) & 0x3;

    cpu.bkrep(arg0, arg1, arg2);
}

void OP_GLUE_8FDC(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Address18_16 arg0{}; arg0.storage = expand;
    Address18_2 arg1{}; arg1.storage = instr & 0x3;

    cpu.bkrep_r6(arg0, arg1);
}

void OP_GLUE_DA9C(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn2 arg0{}; arg0.storage = instr & 0x3;

    cpu.bkreprst(arg0);
}

void OP_GLUE_5F48(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.bkreprst_memsp();
}

void OP_GLUE_DADC(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn2 arg0{}; arg0.storage = instr & 0x3;

    cpu.bkrepsto(arg0);
}

void OP_GLUE_9468(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.bkrepsto_memsp();
}

void OP_GLUE_4B80(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    BankFlags arg0{}; arg0.storage = instr & 0x3F;

    cpu.banke(arg0);
}

void OP_GLUE_8CDF(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.bankr();
}

void OP_GLUE_8CDC(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ar arg0{}; arg0.storage = instr & 0x1;

    cpu.bankr(arg0);
}

void OP_GLUE_8CD0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ar arg0{}; arg0.storage = (instr >> 2) & 0x1;
    Arp arg1{}; arg1.storage = instr & 0x3;

    cpu.bankr(arg0, arg1);
}

void OP_GLUE_8CD8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Arp arg0{}; arg0.storage = instr & 0x3;

    cpu.bankr(arg0);
}

void OP_GLUE_5EB8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;

    cpu.bitrev(arg0);
}

void OP_GLUE_D7E8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;

    cpu.bitrev_dbrv(arg0);
}

void OP_GLUE_D7E0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;

    cpu.bitrev_ebrv(arg0);
}

void OP_GLUE_4180(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Address18_16 arg0{}; arg0.storage = expand;
    Address18_2 arg1{}; arg1.storage = (instr >> 4) & 0x3;
    Cond arg2{}; arg2.storage = instr & 0xF;

    cpu.br(arg0, arg1, arg2);
}

void OP_GLUE_5000(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    RelAddr7 arg0{}; arg0.storage = (instr >> 4) & 0x7F;
    Cond arg1{}; arg1.storage = instr & 0xF;

    cpu.brr(arg0, arg1);
}

void OP_GLUE_D3C0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.break_();
}

void OP_GLUE_41C0(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Address18_16 arg0{}; arg0.storage = expand;
    Address18_2 arg1{}; arg1.storage = (instr >> 4) & 0x3;
    Cond arg2{}; arg2.storage = instr & 0xF;

    cpu.call(arg0, arg1, arg2);
}

void OP_GLUE_D480(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axl arg0{}; arg0.storage = (instr >> 8) & 0x1;

    cpu.calla(arg0);
}

void OP_GLUE_D381(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 4) & 0x1;

    cpu.calla(arg0);
}

void OP_GLUE_1000(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    RelAddr7 arg0{}; arg0.storage = (instr >> 4) & 0x7F;
    Cond arg1{}; arg1.storage = instr & 0xF;

    cpu.callr(arg0, arg1);
}

void OP_GLUE_D380(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.cntx_s();
}

void OP_GLUE_D390(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.cntx_r();
}

void OP_GLUE_4580(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Cond arg0{}; arg0.storage = instr & 0xF;

    cpu.ret(arg0);
}

void OP_GLUE_D780(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.retd();
}

void OP_GLUE_45C0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Cond arg0{}; arg0.storage = instr & 0xF;

    cpu.reti(arg0);
}

void OP_GLUE_45D0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Cond arg0{}; arg0.storage = instr & 0xF;

    cpu.retic(arg0);
}

void OP_GLUE_D7C0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.retid();
}

void OP_GLUE_D3C3(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.retidc();
}

void OP_GLUE_0900(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm8 arg0{}; arg0.storage = instr & 0xFF;

    cpu.rets(arg0);
}

void OP_GLUE_4D80(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm2 arg0{}; arg0.storage = instr & 0x3;

    cpu.load_ps(arg0);
}

void OP_GLUE_DB80(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm7s arg0{}; arg0.storage = instr & 0x7F;

    cpu.load_stepi(arg0);
}

void OP_GLUE_DF80(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm7s arg0{}; arg0.storage = instr & 0x7F;

    cpu.load_stepj(arg0);
}

void OP_GLUE_0400(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm8 arg0{}; arg0.storage = instr & 0xFF;

    cpu.load_page(arg0);
}

void OP_GLUE_0200(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm9 arg0{}; arg0.storage = instr & 0x1FF;

    cpu.load_modi(arg0);
}

void OP_GLUE_0A00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm9 arg0{}; arg0.storage = instr & 0x1FF;

    cpu.load_modj(arg0);
}

void OP_GLUE_D7D8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm2 arg0{}; arg0.storage = (instr >> 1) & 0x3;

    cpu.load_movpd(arg0);
}

void OP_GLUE_0010(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm4 arg0{}; arg0.storage = instr & 0xF;

    cpu.load_ps01(arg0);
}

void OP_GLUE_5F40(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Imm16 arg0{}; arg0.storage = expand;

    cpu.push(arg0);
}

void OP_GLUE_5E40(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;

    cpu.push(arg0);
}

void OP_GLUE_D7C8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Abe arg0{}; arg0.storage = (instr >> 1) & 0x3;

    cpu.push(arg0);
}

void OP_GLUE_D3D0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArArpSttMod arg0{}; arg0.storage = instr & 0xF;

    cpu.push(arg0);
}

void OP_GLUE_D7FC(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.push_prpage();
}

void OP_GLUE_D78C(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Px arg0{}; arg0.storage = (instr >> 1) & 0x1;

    cpu.push(arg0);
}

void OP_GLUE_D4D7(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.push_r6();
}

void OP_GLUE_D7F8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.push_repc();
}

void OP_GLUE_D4D4(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.push_x0();
}

void OP_GLUE_D4D5(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.push_x1();
}

void OP_GLUE_D4D6(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.push_y1();
}

void OP_GLUE_4384(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 6) & 0x1;

    cpu.pusha(arg0);
}

void OP_GLUE_D788(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 1) & 0x1;

    cpu.pusha(arg0);
}

void OP_GLUE_5E60(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;

    cpu.pop(arg0);
}

void OP_GLUE_47B4(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Abe arg0{}; arg0.storage = instr & 0x3;

    cpu.pop(arg0);
}

void OP_GLUE_80C7(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArArpSttMod arg0{}; arg0.storage = (instr >> 8) & 0xF;

    cpu.pop(arg0);
}

void OP_GLUE_0006(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 5) & 0x1;

    cpu.pop(arg0);
}

void OP_GLUE_D7F4(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.pop_prpage();
}

void OP_GLUE_D496(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Px arg0{}; arg0.storage = instr & 0x1;

    cpu.pop(arg0);
}

void OP_GLUE_0024(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.pop_r6();
}

void OP_GLUE_D7F0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.pop_repc();
}

void OP_GLUE_D494(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.pop_x0();
}

void OP_GLUE_D495(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.pop_x1();
}

void OP_GLUE_0004(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.pop_y1();
}

void OP_GLUE_47B0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = instr & 0x3;

    cpu.popa(arg0);
}

void OP_GLUE_0C00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm8 arg0{}; arg0.storage = instr & 0xFF;

    cpu.rep(arg0);
}

void OP_GLUE_0D00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;

    cpu.rep(arg0);
}

void OP_GLUE_0002(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.rep_r6();
}

void OP_GLUE_D280(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 10) & 0x3;
    Ab arg1{}; arg1.storage = (instr >> 5) & 0x3;
    Cond arg2{}; arg2.storage = instr & 0xF;

    cpu.shfc(arg0, arg1, arg2);
}

void OP_GLUE_9240(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 10) & 0x3;
    Ab arg1{}; arg1.storage = (instr >> 7) & 0x3;
    Imm6s arg2{}; arg2.storage = instr & 0x3F;

    cpu.shfi(arg0, arg1, arg2);
}

void OP_GLUE_80C1(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn2 arg0{}; arg0.storage = (instr >> 10) & 0x3;
    ArStep2 arg1{}; arg1.storage = (instr >> 8) & 0x3;

    cpu.tst4b(arg0, arg1);
}

void OP_GLUE_4780(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn2 arg0{}; arg0.storage = (instr >> 2) & 0x3;
    ArStep2 arg1{}; arg1.storage = instr & 0x3;
    Ax arg2{}; arg2.storage = (instr >> 4) & 0x1;

    cpu.tst4b(arg0, arg1, arg2);
}

void OP_GLUE_F000(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    MemImm8 arg0{}; arg0.storage = instr & 0xFF;
    Imm4 arg1{}; arg1.storage = (instr >> 8) & 0xF;

    cpu.tstb(arg0, arg1);
}

void OP_GLUE_9020(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;
    Imm4 arg2{}; arg2.storage = (instr >> 8) & 0xF;

    cpu.tstb(arg0, arg1, arg2);
}

void OP_GLUE_9000(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;
    Imm4 arg1{}; arg1.storage = (instr >> 8) & 0xF;

    cpu.tstb(arg0, arg1);
}

void OP_GLUE_9018(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm4 arg0{}; arg0.storage = (instr >> 8) & 0xF;

    cpu.tstb_r6(arg0);
}

void OP_GLUE_0028(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    SttMod arg0{}; arg0.storage = instr & 0x7;
    Imm16 arg1{}; arg1.storage = expand;

    cpu.tstb(arg0, arg1);
}

void OP_GLUE_6770(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 2) & 0x3;
    Ab arg1{}; arg1.storage = instr & 0x3;
    Ax arg2{}; arg2.storage = (instr >> 12) & 0x1;

    cpu.and_(arg0, arg1, arg2);
}

void OP_GLUE_43C0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.dint();
}

void OP_GLUE_4380(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.eint();
}

void OP_GLUE_9460(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = instr & 0x1;

    cpu.exp(arg0);
}

void OP_GLUE_9060(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = instr & 0x1;
    Ax arg1{}; arg1.storage = (instr >> 8) & 0x1;

    cpu.exp(arg0, arg1);
}

void OP_GLUE_9C40(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;

    cpu.exp(arg0, arg1);
}

void OP_GLUE_9840(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.exp(arg0, arg1, arg2);
}

void OP_GLUE_9440(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;

    cpu.exp(arg0);
}

void OP_GLUE_9040(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;
    Ax arg1{}; arg1.storage = (instr >> 8) & 0x1;

    cpu.exp(arg0, arg1);
}

void OP_GLUE_D7C1(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.exp_r6();
}

void OP_GLUE_D382(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 4) & 0x1;

    cpu.exp_r6(arg0);
}

void OP_GLUE_0080(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;

    cpu.modr(arg0, arg1);
}

void OP_GLUE_00A0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;

    cpu.modr_dmod(arg0, arg1);
}

void OP_GLUE_4990(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;

    cpu.modr_i2(arg0);
}

void OP_GLUE_4998(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;

    cpu.modr_i2_dmod(arg0);
}

void OP_GLUE_5DA0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;

    cpu.modr_d2(arg0);
}

void OP_GLUE_5DA8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;

    cpu.modr_d2_dmod(arg0);
}

void OP_GLUE_D294(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn2 arg0{}; arg0.storage = (instr >> 10) & 0x3;
    ArpStep2 arg1{}; arg1.storage = instr & 0x3;
    ArpStep2 arg2{}; arg2.storage = (instr >> 5) & 0x3;

    cpu.modr_eemod(arg0, arg1, arg2);
}

void OP_GLUE_0D80(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn2 arg0{}; arg0.storage = (instr >> 5) & 0x3;
    ArpStep2 arg1{}; arg1.storage = (instr >> 1) & 0x3;
    ArpStep2 arg2{}; arg2.storage = (instr >> 3) & 0x3;

    cpu.modr_edmod(arg0, arg1, arg2);
}

void OP_GLUE_8464(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn2 arg0{}; arg0.storage = (instr >> 8) & 0x3;
    ArpStep2 arg1{}; arg1.storage = instr & 0x3;
    ArpStep2 arg2{}; arg2.storage = (instr >> 3) & 0x3;

    cpu.modr_demod(arg0, arg1, arg2);
}

void OP_GLUE_0D81(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn2 arg0{}; arg0.storage = (instr >> 5) & 0x3;
    ArpStep2 arg1{}; arg1.storage = (instr >> 1) & 0x3;
    ArpStep2 arg2{}; arg2.storage = (instr >> 3) & 0x3;

    cpu.modr_ddmod(arg0, arg1, arg2);
}

void OP_GLUE_D290(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 10) & 0x3;
    Ab arg1{}; arg1.storage = (instr >> 5) & 0x3;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_D298(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Abl arg0{}; arg0.storage = (instr >> 10) & 0x3;

    cpu.mov_dvm(arg0);
}

void OP_GLUE_D2D8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Abl arg0{}; arg0.storage = (instr >> 10) & 0x3;

    cpu.mov_x0(arg0);
}

void OP_GLUE_D394(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Abl arg0{}; arg0.storage = instr & 0x3;

    cpu.mov_x1(arg0);
}

void OP_GLUE_D384(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Abl arg0{}; arg0.storage = instr & 0x3;

    cpu.mov_y1(arg0);
}

void OP_GLUE_3000(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ablh arg0{}; arg0.storage = (instr >> 9) & 0x7;
    MemImm8 arg1{}; arg1.storage = instr & 0xFF;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_D4BC(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Axl arg0{}; arg0.storage = (instr >> 8) & 0x1;
    MemImm16 arg1{}; arg1.storage = expand;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_D49C(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Axl arg0{}; arg0.storage = (instr >> 8) & 0x1;
    MemR7Imm16 arg1{}; arg1.storage = expand;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_DC80(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axl arg0{}; arg0.storage = (instr >> 8) & 0x1;
    MemR7Imm7s arg1{}; arg1.storage = instr & 0x7F;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_D4B8(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    MemImm16 arg0{}; arg0.storage = expand;
    Ax arg1{}; arg1.storage = (instr >> 8) & 0x1;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_6100(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    MemImm8 arg0{}; arg0.storage = instr & 0xFF;
    Ab arg1{}; arg1.storage = (instr >> 11) & 0x3;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_6200(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    MemImm8 arg0{}; arg0.storage = instr & 0xFF;
    Ablh arg1{}; arg1.storage = (instr >> 10) & 0x7;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_6500(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    MemImm8 arg0{}; arg0.storage = instr & 0xFF;
    Axh arg1{}; arg1.storage = (instr >> 12) & 0x1;

    cpu.mov_eu(arg0, arg1);
}

void OP_GLUE_6000(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    MemImm8 arg0{}; arg0.storage = instr & 0xFF;
    RnOld arg1{}; arg1.storage = (instr >> 10) & 0x7;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_6D00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    MemImm8 arg0{}; arg0.storage = instr & 0xFF;

    cpu.mov_sv(arg0);
}

void OP_GLUE_D491(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 5) & 0x3;

    cpu.mov_dvm_to(arg0);
}

void OP_GLUE_D492(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 5) & 0x3;

    cpu.mov_icr_to(arg0);
}

void OP_GLUE_5E20(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Imm16 arg0{}; arg0.storage = expand;
    Bx arg1{}; arg1.storage = (instr >> 8) & 0x1;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_5E00(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Imm16 arg0{}; arg0.storage = expand;
    Register arg1{}; arg1.storage = instr & 0x1F;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_4F80(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm5 arg0{}; arg0.storage = instr & 0x1F;

    cpu.mov_icr(arg0);
}

void OP_GLUE_2500(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm8s arg0{}; arg0.storage = instr & 0xFF;
    Axh arg1{}; arg1.storage = (instr >> 12) & 0x1;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_2900(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm8s arg0{}; arg0.storage = instr & 0xFF;

    cpu.mov_ext0(arg0);
}

void OP_GLUE_2D00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm8s arg0{}; arg0.storage = instr & 0xFF;

    cpu.mov_ext1(arg0);
}

void OP_GLUE_3900(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm8s arg0{}; arg0.storage = instr & 0xFF;

    cpu.mov_ext2(arg0);
}

void OP_GLUE_3D00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm8s arg0{}; arg0.storage = instr & 0xFF;

    cpu.mov_ext3(arg0);
}

void OP_GLUE_2300(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm8s arg0{}; arg0.storage = instr & 0xFF;
    RnOld arg1{}; arg1.storage = (instr >> 10) & 0x7;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_0500(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm8s arg0{}; arg0.storage = instr & 0xFF;

    cpu.mov_sv(arg0);
}

void OP_GLUE_2100(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm8 arg0{}; arg0.storage = instr & 0xFF;
    Axl arg1{}; arg1.storage = (instr >> 12) & 0x1;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_D498(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    MemR7Imm16 arg0{}; arg0.storage = expand;
    Ax arg1{}; arg1.storage = (instr >> 8) & 0x1;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_D880(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    MemR7Imm7s arg0{}; arg0.storage = instr & 0x7F;
    Ax arg1{}; arg1.storage = (instr >> 8) & 0x1;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_98C0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;
    Bx arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.mov(arg0, arg1, arg2);
}

void OP_GLUE_1C00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;
    Register arg2{}; arg2.storage = (instr >> 5) & 0x1F;

    cpu.mov(arg0, arg1, arg2);
}

void OP_GLUE_47E0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;

    cpu.mov_memsp_to(arg0);
}

void OP_GLUE_47C0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;

    cpu.mov_mixp_to(arg0);
}

void OP_GLUE_2000(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    RnOld arg0{}; arg0.storage = (instr >> 9) & 0x7;
    MemImm8 arg1{}; arg1.storage = instr & 0xFF;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_4FC0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;

    cpu.mov_icr(arg0);
}

void OP_GLUE_5E80(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;

    cpu.mov_mixp(arg0);
}

void OP_GLUE_1800(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = (instr >> 5) & 0x1F;
    Rn arg1{}; arg1.storage = instr & 0x7;
    StepZIDS arg2{}; arg2.storage = (instr >> 3) & 0x3;

    cpu.mov(arg0, arg1, arg2);
}

void OP_GLUE_5EC0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;
    Bx arg1{}; arg1.storage = (instr >> 5) & 0x1;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_5800(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;
    Register arg1{}; arg1.storage = (instr >> 5) & 0x1F;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_D490(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 5) & 0x3;

    cpu.mov_repc_to(arg0);
}

void OP_GLUE_7D00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    MemImm8 arg0{}; arg0.storage = instr & 0xFF;

    cpu.mov_sv_to(arg0);
}

void OP_GLUE_D493(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 5) & 0x3;

    cpu.mov_x0_to(arg0);
}

void OP_GLUE_49C1(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 4) & 0x3;

    cpu.mov_x1_to(arg0);
}

void OP_GLUE_D299(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 10) & 0x3;

    cpu.mov_y1_to(arg0);
}

void OP_GLUE_0008(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Imm16 arg0{}; arg0.storage = expand;
    ArArp arg1{}; arg1.storage = instr & 0x7;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_0023(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Imm16 arg0{}; arg0.storage = expand;

    cpu.mov_r6(arg0);
}

void OP_GLUE_0001(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Imm16 arg0{}; arg0.storage = expand;

    cpu.mov_repc(arg0);
}

void OP_GLUE_8971(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Imm16 arg0{}; arg0.storage = expand;

    cpu.mov_stepi0(arg0);
}

void OP_GLUE_8979(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Imm16 arg0{}; arg0.storage = expand;

    cpu.mov_stepj0(arg0);
}

void OP_GLUE_0030(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    Imm16 arg0{}; arg0.storage = expand;
    SttMod arg1{}; arg1.storage = instr & 0x7;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_5DD0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Imm4 arg0{}; arg0.storage = instr & 0xF;

    cpu.mov_prpage(arg0);
}

void OP_GLUE_5F80(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    R0123 arg0{}; arg0.storage = instr & 0x3;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;
    R45 arg2{}; arg2.storage = (instr >> 2) & 0x1;
    StepZIDS arg3{}; arg3.storage = (instr >> 5) & 0x3;

    cpu.movd(arg0, arg1, arg2, arg3);
}

void OP_GLUE_0040(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axl arg0{}; arg0.storage = (instr >> 5) & 0x1;
    Register arg1{}; arg1.storage = instr & 0x1F;

    cpu.movp(arg0, arg1);
}

void OP_GLUE_0D40(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 5) & 0x1;
    Register arg1{}; arg1.storage = instr & 0x1F;

    cpu.movp(arg0, arg1);
}

void OP_GLUE_0600(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;
    R0123 arg2{}; arg2.storage = (instr >> 5) & 0x3;
    StepZIDS arg3{}; arg3.storage = (instr >> 7) & 0x3;

    cpu.movp(arg0, arg1, arg2, arg3);
}

void OP_GLUE_D499(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 8) & 0x1;

    cpu.movpdw(arg0);
}

void OP_GLUE_D49B(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.mov_a0h_stepi0();
}

void OP_GLUE_D59B(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.mov_a0h_stepj0();
}

void OP_GLUE_D482(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.mov_stepi0_a0h();
}

void OP_GLUE_D582(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.mov_stepj0_a0h();
}

void OP_GLUE_9164(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Abl arg0{}; arg0.storage = instr & 0x3;

    cpu.mov_prpage(arg0);
}

void OP_GLUE_9064(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Abl arg0{}; arg0.storage = instr & 0x3;

    cpu.mov_repc(arg0);
}

void OP_GLUE_9540(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Abl arg0{}; arg0.storage = (instr >> 3) & 0x3;
    ArArp arg1{}; arg1.storage = instr & 0x7;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_9C60(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Abl arg0{}; arg0.storage = (instr >> 3) & 0x3;
    SttMod arg1{}; arg1.storage = instr & 0x7;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_5EB0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Abl arg0{}; arg0.storage = instr & 0x3;

    cpu.mov_prpage_to(arg0);
}

void OP_GLUE_D2D9(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Abl arg0{}; arg0.storage = (instr >> 10) & 0x3;

    cpu.mov_repc_to(arg0);
}

void OP_GLUE_9560(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArArp arg0{}; arg0.storage = instr & 0x7;
    Abl arg1{}; arg1.storage = (instr >> 3) & 0x3;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_D2F8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    SttMod arg0{}; arg0.storage = instr & 0x7;
    Abl arg1{}; arg1.storage = (instr >> 10) & 0x3;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_D7D0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 1) & 0x1;
    ArStep1 arg1{}; arg1.storage = instr & 0x1;

    cpu.mov_repc_to(arg0, arg1);
}

void OP_GLUE_D488(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArArp arg0{}; arg0.storage = instr & 0x7;
    ArRn1 arg1{}; arg1.storage = (instr >> 8) & 0x1;
    ArStep1 arg2{}; arg2.storage = (instr >> 5) & 0x1;

    cpu.mov(arg0, arg1, arg2);
}

void OP_GLUE_49A0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    SttMod arg0{}; arg0.storage = instr & 0x7;
    ArRn1 arg1{}; arg1.storage = (instr >> 4) & 0x1;
    ArStep1 arg2{}; arg2.storage = (instr >> 3) & 0x1;

    cpu.mov(arg0, arg1, arg2);
}

void OP_GLUE_D7D4(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 1) & 0x1;
    ArStep1 arg1{}; arg1.storage = instr & 0x1;

    cpu.mov_repc(arg0, arg1);
}

void OP_GLUE_8062(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArArp arg2{}; arg2.storage = (instr >> 8) & 0x7;

    cpu.mov(arg0, arg1, arg2);
}

void OP_GLUE_8063(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    SttMod arg2{}; arg2.storage = (instr >> 8) & 0x7;

    cpu.mov(arg0, arg1, arg2);
}

void OP_GLUE_D3C8(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    MemR7Imm16 arg0{}; arg0.storage = expand;

    cpu.mov_repc_to(arg0);
}

void OP_GLUE_5F50(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    ArArpSttMod arg0{}; arg0.storage = instr & 0xF;
    MemR7Imm16 arg1{}; arg1.storage = expand;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_D2DC(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    MemR7Imm16 arg0{}; arg0.storage = expand;

    cpu.mov_repc(arg0);
}

void OP_GLUE_4D90(Interpreter& cpu, u16 instr)
{
    u16 expand = cpu.OpcodeFetch();
    cpu.HandleLoops();

    MemR7Imm16 arg0{}; arg0.storage = expand;
    ArArpSttMod arg1{}; arg1.storage = instr & 0xF;

    cpu.mov(arg0, arg1);
}

void OP_GLUE_886B(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 8) & 0x1;

    cpu.mov_pc(arg0);
}

void OP_GLUE_8863(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 8) & 0x1;

    cpu.mov_pc(arg0);
}

void OP_GLUE_8A73(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 3) & 0x1;

    cpu.mov_mixp_to(arg0);
}

void OP_GLUE_4381(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.mov_mixp_r6();
}

void OP_GLUE_4382(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = instr & 0x1;

    cpu.mov_p0h_to(arg0);
}

void OP_GLUE_D3C2(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.mov_p0h_r6();
}

void OP_GLUE_4B60(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;

    cpu.mov_p0h_to(arg0);
}

void OP_GLUE_8FD4(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = instr & 0x3;

    cpu.mov_p0(arg0);
}

void OP_GLUE_8FD8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = instr & 0x3;

    cpu.mov_p1_to(arg0);
}

void OP_GLUE_88D0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Px arg0{}; arg0.storage = (instr >> 1) & 0x1;
    ArRn2 arg1{}; arg1.storage = (instr >> 8) & 0x3;
    ArStep2 arg2{}; arg2.storage = (instr >> 2) & 0x3;

    cpu.mov2(arg0, arg1, arg2);
}

void OP_GLUE_88D1(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Px arg0{}; arg0.storage = (instr >> 1) & 0x1;
    ArRn2 arg1{}; arg1.storage = (instr >> 8) & 0x3;
    ArStep2 arg2{}; arg2.storage = (instr >> 2) & 0x3;

    cpu.mov2s(arg0, arg1, arg2);
}

void OP_GLUE_D292(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn2 arg0{}; arg0.storage = (instr >> 10) & 0x3;
    ArStep2 arg1{}; arg1.storage = (instr >> 5) & 0x3;
    Px arg2{}; arg2.storage = instr & 0x1;

    cpu.mov2(arg0, arg1, arg2);
}

void OP_GLUE_4DC0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 4) & 0x3;
    ArRn2 arg1{}; arg1.storage = (instr >> 2) & 0x3;
    ArStep2 arg2{}; arg2.storage = instr & 0x3;

    cpu.mova(arg0, arg1, arg2);
}

void OP_GLUE_4BC0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn2 arg0{}; arg0.storage = (instr >> 2) & 0x3;
    ArStep2 arg1{}; arg1.storage = instr & 0x3;
    Ab arg2{}; arg2.storage = (instr >> 4) & 0x3;

    cpu.mova(arg0, arg1, arg2);
}

void OP_GLUE_D481(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 8) & 0x1;

    cpu.mov_r6_to(arg0);
}

void OP_GLUE_43C1(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.mov_r6_mixp();
}

void OP_GLUE_5F00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;

    cpu.mov_r6_to(arg0);
}

void OP_GLUE_5F60(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;

    cpu.mov_r6(arg0);
}

void OP_GLUE_D29C(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.mov_memsp_r6();
}

void OP_GLUE_1B00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;

    cpu.mov_r6_to(arg0, arg1);
}

void OP_GLUE_1B20(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;

    cpu.mov_r6(arg0, arg1);
}

void OP_GLUE_6300(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    MemImm8 arg0{}; arg0.storage = instr & 0xFF;
    Ab arg1{}; arg1.storage = (instr >> 11) & 0x3;

    cpu.movs(arg0, arg1);
}

void OP_GLUE_0180(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;
    Ab arg2{}; arg2.storage = (instr >> 5) & 0x3;

    cpu.movs(arg0, arg1, arg2);
}

void OP_GLUE_0100(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;
    Ab arg1{}; arg1.storage = (instr >> 5) & 0x3;

    cpu.movs(arg0, arg1);
}

void OP_GLUE_5F42(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = instr & 0x1;

    cpu.movs_r6_to(arg0);
}

void OP_GLUE_4080(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    RnOld arg0{}; arg0.storage = (instr >> 9) & 0x7;
    Ab arg1{}; arg1.storage = (instr >> 5) & 0x3;
    Imm5s arg2{}; arg2.storage = instr & 0x1F;

    cpu.movsi(arg0, arg1, arg2);
}

void OP_GLUE_4390(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axh arg0{}; arg0.storage = (instr >> 6) & 0x1;
    ArRn2 arg1{}; arg1.storage = (instr >> 2) & 0x3;
    ArStep2 arg2{}; arg2.storage = instr & 0x3;

    cpu.mov2_axh_m_y0_m(arg0, arg1, arg2);
}

void OP_GLUE_43A0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 3) & 0x3;
    ArpRn1 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    ArpStep1 arg2{}; arg2.storage = instr & 0x1;
    ArpStep1 arg3{}; arg3.storage = (instr >> 1) & 0x1;

    cpu.mov2_ax_mij(arg0, arg1, arg2, arg3);
}

void OP_GLUE_43E0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 3) & 0x3;
    ArpRn1 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    ArpStep1 arg2{}; arg2.storage = instr & 0x1;
    ArpStep1 arg3{}; arg3.storage = (instr >> 1) & 0x1;

    cpu.mov2_ax_mji(arg0, arg1, arg2, arg3);
}

void OP_GLUE_80C4(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 9) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 8) & 0x1;
    Ab arg3{}; arg3.storage = (instr >> 10) & 0x3;

    cpu.mov2_mij_ax(arg0, arg1, arg2, arg3);
}

void OP_GLUE_D4C0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ab arg3{}; arg3.storage = (instr >> 2) & 0x3;

    cpu.mov2_mji_ax(arg0, arg1, arg2, arg3);
}

void OP_GLUE_9D40(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Abh arg0{}; arg0.storage = (instr >> 4) & 0x3;
    Abh arg1{}; arg1.storage = (instr >> 2) & 0x3;
    ArRn1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    ArStep1 arg3{}; arg3.storage = instr & 0x1;

    cpu.mov2_abh_m(arg0, arg1, arg2, arg3);
}

void OP_GLUE_8C60(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axh arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArpRn2 arg1{}; arg1.storage = (instr >> 8) & 0x3;
    ArpStep2 arg2{}; arg2.storage = instr & 0x3;
    ArpStep2 arg3{}; arg3.storage = (instr >> 2) & 0x3;

    cpu.exchange_iaj(arg0, arg1, arg2, arg3);
}

void OP_GLUE_7F80(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axh arg0{}; arg0.storage = (instr >> 6) & 0x1;
    ArpRn2 arg1{}; arg1.storage = (instr >> 4) & 0x3;
    ArpStep2 arg2{}; arg2.storage = instr & 0x3;
    ArpStep2 arg3{}; arg3.storage = (instr >> 2) & 0x3;

    cpu.exchange_riaj(arg0, arg1, arg2, arg3);
}

void OP_GLUE_4900(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axh arg0{}; arg0.storage = (instr >> 6) & 0x1;
    ArpRn2 arg1{}; arg1.storage = (instr >> 4) & 0x3;
    ArpStep2 arg2{}; arg2.storage = instr & 0x3;
    ArpStep2 arg3{}; arg3.storage = (instr >> 2) & 0x3;

    cpu.exchange_jai(arg0, arg1, arg2, arg3);
}

void OP_GLUE_4800(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axh arg0{}; arg0.storage = (instr >> 6) & 0x1;
    ArpRn2 arg1{}; arg1.storage = (instr >> 4) & 0x3;
    ArpStep2 arg2{}; arg2.storage = instr & 0x3;
    ArpStep2 arg3{}; arg3.storage = (instr >> 2) & 0x3;

    cpu.exchange_rjai(arg0, arg1, arg2, arg3);
}

void OP_GLUE_8864(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn2 arg0{}; arg0.storage = (instr >> 3) & 0x3;
    ArStep2 arg1{}; arg1.storage = instr & 0x3;
    Abh arg2{}; arg2.storage = (instr >> 8) & 0x3;

    cpu.movr(arg0, arg1, arg2);
}

void OP_GLUE_9CE0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Rn arg0{}; arg0.storage = instr & 0x7;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.movr(arg0, arg1, arg2);
}

void OP_GLUE_9CC0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Register arg0{}; arg0.storage = instr & 0x1F;
    Ax arg1{}; arg1.storage = (instr >> 8) & 0x1;

    cpu.movr(arg0, arg1);
}

void OP_GLUE_5DF4(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 1) & 0x1;
    Ax arg1{}; arg1.storage = instr & 0x1;

    cpu.movr(arg0, arg1);
}

void OP_GLUE_8961(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 3) & 0x1;

    cpu.movr_r6_to(arg0);
}

void OP_GLUE_49C0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 5) & 0x1;
    Ax arg1{}; arg1.storage = (instr >> 4) & 0x1;

    cpu.lim(arg0, arg1);
}

void OP_GLUE_5F45(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.vtrclr0();
}

void OP_GLUE_5F46(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.vtrclr1();
}

void OP_GLUE_5F47(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.vtrclr();
}

void OP_GLUE_D29A(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axl arg0{}; arg0.storage = instr & 0x1;

    cpu.vtrmov0(arg0);
}

void OP_GLUE_D69A(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axl arg0{}; arg0.storage = instr & 0x1;

    cpu.vtrmov1(arg0);
}

void OP_GLUE_D383(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axl arg0{}; arg0.storage = (instr >> 4) & 0x1;

    cpu.vtrmov(arg0);
}

void OP_GLUE_D781(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.vtrshr();
}

void OP_GLUE_5DFE(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.clrp0();
}

void OP_GLUE_5DFD(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.clrp1();
}

void OP_GLUE_5DFF(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.clrp();
}

void OP_GLUE_8460(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 8) & 0x1;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;

    cpu.max_ge(arg0, arg1);
}

void OP_GLUE_8660(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 8) & 0x1;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;

    cpu.max_gt(arg0, arg1);
}

void OP_GLUE_8860(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 8) & 0x1;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;

    cpu.min_le(arg0, arg1);
}

void OP_GLUE_8A60(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 8) & 0x1;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;

    cpu.min_lt(arg0, arg1);
}

void OP_GLUE_8060(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 8) & 0x1;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;

    cpu.max_ge_r0(arg0, arg1);
}

void OP_GLUE_8260(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 8) & 0x1;
    StepZIDS arg1{}; arg1.storage = (instr >> 3) & 0x3;

    cpu.max_gt_r0(arg0, arg1);
}

void OP_GLUE_47A0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 3) & 0x1;
    StepZIDS arg1{}; arg1.storage = instr & 0x3;

    cpu.min_le_r0(arg0, arg1);
}

void OP_GLUE_47A4(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 3) & 0x1;
    StepZIDS arg1{}; arg1.storage = instr & 0x3;

    cpu.min_lt_r0(arg0, arg1);
}

void OP_GLUE_0E00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    MemImm8 arg0{}; arg0.storage = instr & 0xFF;
    Ax arg1{}; arg1.storage = (instr >> 8) & 0x1;

    cpu.divs(arg0, arg1);
}

void OP_GLUE_D790(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 2) & 0x3;
    Ab arg1{}; arg1.storage = instr & 0x3;

    cpu.sqr_sqr_add3(arg0, arg1);
}

void OP_GLUE_4B00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn2 arg0{}; arg0.storage = (instr >> 4) & 0x3;
    ArStep2 arg1{}; arg1.storage = (instr >> 2) & 0x3;
    Ab arg2{}; arg2.storage = instr & 0x3;

    cpu.sqr_sqr_add3(arg0, arg1, arg2);
}

void OP_GLUE_49C4(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 4) & 0x3;
    Ab arg1{}; arg1.storage = instr & 0x3;

    cpu.sqr_mpysu_add3a(arg0, arg1);
}

void OP_GLUE_4D8C(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 1) & 0x1;
    Bx arg1{}; arg1.storage = instr & 0x1;

    cpu.cmp(arg0, arg1);
}

void OP_GLUE_D483(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.cmp_b0_b1();
}

void OP_GLUE_D583(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();


    cpu.cmp_b1_b0();
}

void OP_GLUE_DA9A(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 10) & 0x1;
    Ax arg1{}; arg1.storage = instr & 0x1;

    cpu.cmp(arg0, arg1);
}

void OP_GLUE_8B63(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 4) & 0x1;

    cpu.cmp_p1_to(arg0);
}

void OP_GLUE_5E21(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 8) & 0x1;

    cpu.max2_vtr(arg0);
}

void OP_GLUE_43C2(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = instr & 0x1;

    cpu.min2_vtr(arg0);
}

void OP_GLUE_D784(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 1) & 0x1;
    Bx arg1{}; arg1.storage = instr & 0x1;

    cpu.max2_vtr(arg0, arg1);
}

void OP_GLUE_D4BA(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 8) & 0x1;
    Bx arg1{}; arg1.storage = instr & 0x1;

    cpu.min2_vtr(arg0, arg1);
}

void OP_GLUE_4A40(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 3) & 0x1;
    Bx arg1{}; arg1.storage = (instr >> 4) & 0x1;
    ArRn1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    ArStep1 arg3{}; arg3.storage = instr & 0x1;

    cpu.max2_vtr_movl(arg0, arg1, arg2, arg3);
}

void OP_GLUE_4A44(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 3) & 0x1;
    Bx arg1{}; arg1.storage = (instr >> 4) & 0x1;
    ArRn1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    ArStep1 arg3{}; arg3.storage = instr & 0x1;

    cpu.max2_vtr_movh(arg0, arg1, arg2, arg3);
}

void OP_GLUE_4A60(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 4) & 0x1;
    Ax arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArRn1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    ArStep1 arg3{}; arg3.storage = instr & 0x1;

    cpu.max2_vtr_movl(arg0, arg1, arg2, arg3);
}

void OP_GLUE_4A64(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 4) & 0x1;
    Ax arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArRn1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    ArStep1 arg3{}; arg3.storage = instr & 0x1;

    cpu.max2_vtr_movh(arg0, arg1, arg2, arg3);
}

void OP_GLUE_4A00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 3) & 0x1;
    Bx arg1{}; arg1.storage = (instr >> 4) & 0x1;
    ArRn1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    ArStep1 arg3{}; arg3.storage = instr & 0x1;

    cpu.min2_vtr_movl(arg0, arg1, arg2, arg3);
}

void OP_GLUE_4A04(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 3) & 0x1;
    Bx arg1{}; arg1.storage = (instr >> 4) & 0x1;
    ArRn1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    ArStep1 arg3{}; arg3.storage = instr & 0x1;

    cpu.min2_vtr_movh(arg0, arg1, arg2, arg3);
}

void OP_GLUE_4A20(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 4) & 0x1;
    Ax arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArRn1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    ArStep1 arg3{}; arg3.storage = instr & 0x1;

    cpu.min2_vtr_movl(arg0, arg1, arg2, arg3);
}

void OP_GLUE_4A24(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 4) & 0x1;
    Ax arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArRn1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    ArStep1 arg3{}; arg3.storage = instr & 0x1;

    cpu.min2_vtr_movh(arg0, arg1, arg2, arg3);
}

void OP_GLUE_D590(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 6) & 0x1;
    Bx arg1{}; arg1.storage = (instr >> 5) & 0x1;
    ArpRn1 arg2{}; arg2.storage = (instr >> 2) & 0x1;
    ArpStep1 arg3{}; arg3.storage = instr & 0x1;
    ArpStep1 arg4{}; arg4.storage = (instr >> 1) & 0x1;

    cpu.max2_vtr_movij(arg0, arg1, arg2, arg3, arg4);
}

void OP_GLUE_45A0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 4) & 0x1;
    Bx arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpRn1 arg2{}; arg2.storage = (instr >> 2) & 0x1;
    ArpStep1 arg3{}; arg3.storage = instr & 0x1;
    ArpStep1 arg4{}; arg4.storage = (instr >> 1) & 0x1;

    cpu.max2_vtr_movji(arg0, arg1, arg2, arg3, arg4);
}

void OP_GLUE_D2B8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 11) & 0x1;
    Bx arg1{}; arg1.storage = (instr >> 10) & 0x1;
    ArpRn1 arg2{}; arg2.storage = (instr >> 2) & 0x1;
    ArpStep1 arg3{}; arg3.storage = instr & 0x1;
    ArpStep1 arg4{}; arg4.storage = (instr >> 1) & 0x1;

    cpu.min2_vtr_movij(arg0, arg1, arg2, arg3, arg4);
}

void OP_GLUE_45E0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 4) & 0x1;
    Bx arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpRn1 arg2{}; arg2.storage = (instr >> 2) & 0x1;
    ArpStep1 arg3{}; arg3.storage = instr & 0x1;
    ArpStep1 arg4{}; arg4.storage = (instr >> 1) & 0x1;

    cpu.min2_vtr_movji(arg0, arg1, arg2, arg3, arg4);
}

void OP_GLUE_4B40(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 3) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    Bx arg2{}; arg2.storage = instr & 0x1;

    cpu.mov_sv_app(arg0, arg1, arg2, SumBase::Sv, true, false, false, false);
}

void OP_GLUE_9960(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1Alt arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Bx arg2{}; arg2.storage = (instr >> 2) & 0x1;

    cpu.mov_sv_app(arg0, arg1, arg2, SumBase::Sv, true, false, false, false);
}

void OP_GLUE_4B42(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 3) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    Bx arg2{}; arg2.storage = instr & 0x1;

    cpu.mov_sv_app(arg0, arg1, arg2, SumBase::SvRnd, true, false, false, false);
}

void OP_GLUE_99E0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1Alt arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Bx arg2{}; arg2.storage = (instr >> 2) & 0x1;

    cpu.mov_sv_app(arg0, arg1, arg2, SumBase::SvRnd, true, false, false, false);
}

void OP_GLUE_5F4C(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 1) & 0x1;
    ArStep1 arg1{}; arg1.storage = instr & 0x1;
    Bx arg2{}; arg2.storage = 0;

    cpu.mov_sv_app(arg0, arg1, arg2, SumBase::Sv, true, false, true, false);
}

void OP_GLUE_8873(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 8) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Bx arg2{}; arg2.storage = 1;

    cpu.mov_sv_app(arg0, arg1, arg2, SumBase::Sv, true, false, true, false);
}

void OP_GLUE_9860(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1Alt arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Bx arg2{}; arg2.storage = (instr >> 2) & 0x1;

    cpu.mov_sv_app(arg0, arg1, arg2, SumBase::Sv, true, false, true, false);
}

void OP_GLUE_DE9C(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 1) & 0x1;
    ArStep1 arg1{}; arg1.storage = instr & 0x1;
    Bx arg2{}; arg2.storage = 0;

    cpu.mov_sv_app(arg0, arg1, arg2, SumBase::SvRnd, true, false, true, false);
}

void OP_GLUE_D4B4(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 1) & 0x1;
    ArStep1 arg1{}; arg1.storage = instr & 0x1;
    Bx arg2{}; arg2.storage = 1;

    cpu.mov_sv_app(arg0, arg1, arg2, SumBase::SvRnd, true, false, true, false);
}

void OP_GLUE_98E0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1Alt arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Bx arg2{}; arg2.storage = (instr >> 2) & 0x1;

    cpu.mov_sv_app(arg0, arg1, arg2, SumBase::SvRnd, true, false, true, false);
}

void OP_GLUE_9068(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axh arg0{}; arg0.storage = instr & 0x1;
    CbsCond arg1{}; arg1.storage = (instr >> 8) & 0x1;

    cpu.cbs(arg0, arg1);
}

void OP_GLUE_D49E(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axh arg0{}; arg0.storage = (instr >> 8) & 0x1;
    Bxh arg1{}; arg1.storage = (instr >> 5) & 0x1;
    CbsCond arg2{}; arg2.storage = instr & 0x1;

    cpu.cbs(arg0, arg1, arg2);
}

void OP_GLUE_D5C0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 2) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    CbsCond arg3{}; arg3.storage = (instr >> 3) & 0x1;

    cpu.cbs(arg0, arg1, arg2, arg3);
}

void OP_GLUE_4D88(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 1) & 0x1;

    cpu.mma(arg0.GetName(), true, true, true, true, SumBase::Zero, false, false, true, false);
}

void OP_GLUE_D49D(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 5) & 0x1;

    cpu.mma(arg0.GetName(), true, true, true, true, SumBase::Zero, false, false, true, false);
}

void OP_GLUE_5E24(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = instr & 0x3;

    cpu.mma(arg0.GetName(), true, true, true, true, SumBase::Zero, false, false, false, false);
}

void OP_GLUE_8061(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 8) & 0x3;

    cpu.mma(arg0.GetName(), true, true, true, true, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_8071(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 8) & 0x3;

    cpu.mma(arg0.GetName(), true, true, true, true, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_8461(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 8) & 0x3;

    cpu.mma(arg0.GetName(), true, true, true, true, SumBase::Acc, true, false, true, false);
}

void OP_GLUE_8471(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 8) & 0x3;

    cpu.mma(arg0.GetName(), true, true, true, true, SumBase::Acc, true, false, true, true);
}

void OP_GLUE_D484(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = instr & 0x3;

    cpu.mma(arg0.GetName(), true, true, true, true, SumBase::Acc, false, true, false, true);
}

void OP_GLUE_D4A0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = instr & 0x3;

    cpu.mma(arg0.GetName(), true, true, true, true, SumBase::Acc, false, false, true, false);
}

void OP_GLUE_4D89(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ax arg0{}; arg0.storage = (instr >> 1) & 0x1;

    cpu.mma(arg0.GetName(), true, true, true, false, SumBase::Zero, false, false, true, false);
}

void OP_GLUE_D59D(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Bx arg0{}; arg0.storage = (instr >> 5) & 0x1;

    cpu.mma(arg0.GetName(), true, true, true, false, SumBase::Zero, false, false, true, false);
}

void OP_GLUE_5F24(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = instr & 0x3;

    cpu.mma(arg0.GetName(), true, true, true, false, SumBase::Zero, false, false, false, false);
}

void OP_GLUE_8069(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 8) & 0x3;

    cpu.mma(arg0.GetName(), true, true, true, false, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_8079(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 8) & 0x3;

    cpu.mma(arg0.GetName(), true, true, true, false, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_8469(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 8) & 0x3;

    cpu.mma(arg0.GetName(), true, true, true, false, SumBase::Acc, true, false, true, false);
}

void OP_GLUE_8479(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = (instr >> 8) & 0x3;

    cpu.mma(arg0.GetName(), true, true, true, false, SumBase::Acc, true, false, true, true);
}

void OP_GLUE_D584(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = instr & 0x3;

    cpu.mma(arg0.GetName(), true, true, true, false, SumBase::Acc, false, true, false, true);
}

void OP_GLUE_D5A0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Ab arg0{}; arg0.storage = instr & 0x3;

    cpu.mma(arg0.GetName(), true, true, true, false, SumBase::Acc, false, false, true, false);
}

void OP_GLUE_CA00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), false, true, false, true, SumBase::Acc, true, false, true, true);
}

void OP_GLUE_CA01(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), false, true, true, false, SumBase::Acc, true, false, true, true);
}

void OP_GLUE_CA02(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), false, true, false, true, SumBase::Acc, true, true, true, true);
}

void OP_GLUE_CA03(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), false, true, true, false, SumBase::Acc, true, true, true, true);
}

void OP_GLUE_CA04(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), false, true, false, true, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_CA05(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), false, true, true, false, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_CA06(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), false, true, false, true, SumBase::Acc, false, true, false, true);
}

void OP_GLUE_CA07(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), false, true, true, false, SumBase::Acc, false, true, false, true);
}

void OP_GLUE_CB00(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, false, true, SumBase::Acc, true, false, true, false);
}

void OP_GLUE_CB01(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, false, SumBase::Acc, true, false, true, false);
}

void OP_GLUE_CB02(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, false, true, SumBase::Acc, true, false, true, true);
}

void OP_GLUE_CB03(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, false, SumBase::Acc, true, false, true, true);
}

void OP_GLUE_CB04(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, false, true, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_CB05(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, false, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_CB06(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, false, true, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_CB07(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 5) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, false, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_0D30(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 3) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 1) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 2) & 0x1;
    Ax arg5{}; arg5.storage = instr & 0x1;

    cpu.mma(arg0, arg1, arg2, false, true, arg5.GetName(), true, true, true, false, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_0D20(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 3) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 1) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 2) & 0x1;
    Ax arg5{}; arg5.storage = instr & 0x1;

    cpu.mma(arg0, arg1, arg2, true, false, arg5.GetName(), true, true, true, false, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_4B50(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 3) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 1) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 2) & 0x1;
    Ax arg5{}; arg5.storage = instr & 0x1;

    cpu.mma(arg0, arg1, arg2, true, true, arg5.GetName(), true, true, true, false, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_9861(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 3) & 0x1;
    Ax arg5{}; arg5.storage = (instr >> 8) & 0x1;

    cpu.mma(arg0, arg1, arg2, false, true, arg5.GetName(), true, true, true, true, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_9862(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 3) & 0x1;
    Ax arg5{}; arg5.storage = (instr >> 8) & 0x1;

    cpu.mma(arg0, arg1, arg2, true, false, arg5.GetName(), true, true, true, true, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_9863(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 3) & 0x1;
    Ax arg5{}; arg5.storage = (instr >> 8) & 0x1;

    cpu.mma(arg0, arg1, arg2, true, true, arg5.GetName(), true, true, true, true, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_98E1(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 3) & 0x1;
    Ax arg5{}; arg5.storage = (instr >> 8) & 0x1;

    cpu.mma(arg0, arg1, arg2, false, true, arg5.GetName(), true, true, true, true, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_98E2(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 3) & 0x1;
    Ax arg5{}; arg5.storage = (instr >> 8) & 0x1;

    cpu.mma(arg0, arg1, arg2, true, false, arg5.GetName(), true, true, true, true, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_98E3(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 3) & 0x1;
    Ax arg5{}; arg5.storage = (instr >> 8) & 0x1;

    cpu.mma(arg0, arg1, arg2, true, true, arg5.GetName(), true, true, true, true, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_80C8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 2) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 10) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, true, SumBase::Acc, false, false, true, false);
}

void OP_GLUE_81C8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 2) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 10) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, true, SumBase::Acc, false, false, true, true);
}

void OP_GLUE_82C8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 2) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 10) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, true, SumBase::Zero, false, false, false, false);
}

void OP_GLUE_83C8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 2) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 10) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, true, SumBase::Zero, false, false, false, true);
}

void OP_GLUE_80C2(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = instr & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 8) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 9) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 10) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, true, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_49C8(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 2) & 0x1;
    ArpStep1 arg1{}; arg1.storage = instr & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 4) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, true, SumBase::Acc, true, false, true, true);
}

void OP_GLUE_00C0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 3) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 1) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 2) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 4) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, true, SumBase::Zero, false, false, true, false);
}

void OP_GLUE_00C1(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 3) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 1) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 2) & 0x1;
    Ab arg5{}; arg5.storage = (instr >> 4) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, true, SumBase::Zero, false, false, true, true);
}

void OP_GLUE_D7A0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 3) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 1) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 2) & 0x1;
    Ax arg5{}; arg5.storage = (instr >> 4) & 0x1;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, true, SumBase::Sv, false, false, false, false);
}

void OP_GLUE_D7A1(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn1 arg0{}; arg0.storage = (instr >> 3) & 0x1;
    ArpStep1 arg1{}; arg1.storage = (instr >> 1) & 0x1;
    ArpStep1 arg2{}; arg2.storage = (instr >> 2) & 0x1;
    Ax arg5{}; arg5.storage = (instr >> 4) & 0x1;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, true, SumBase::SvRnd, false, false, false, false);
}

void OP_GLUE_C800(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn2 arg0{}; arg0.storage = (instr >> 4) & 0x3;
    ArpStep2 arg1{}; arg1.storage = instr & 0x3;
    ArpStep2 arg2{}; arg2.storage = (instr >> 2) & 0x3;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, true, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_C900(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArpRn2 arg0{}; arg0.storage = (instr >> 4) & 0x3;
    ArpStep2 arg1{}; arg1.storage = instr & 0x3;
    ArpStep2 arg2{}; arg2.storage = (instr >> 2) & 0x3;
    Ab arg5{}; arg5.storage = (instr >> 6) & 0x3;

    cpu.mma(arg0, arg1, arg2, false, false, arg5.GetName(), true, true, true, true, SumBase::Acc, true, false, true, false);
}

void OP_GLUE_D5E0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 1) & 0x1;
    ArStep1 arg1{}; arg1.storage = instr & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 3) & 0x1;

    cpu.mma_mx_xy(arg0, arg1, arg2.GetName(), true, true, true, true, SumBase::Acc, true, false, true, false);
}

void OP_GLUE_D5E4(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 1) & 0x1;
    ArStep1 arg1{}; arg1.storage = instr & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 3) & 0x1;

    cpu.mma_mx_xy(arg0, arg1, arg2.GetName(), true, true, true, true, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_8862(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.mma_xy_mx(arg0, arg1, arg2.GetName(), true, true, true, true, SumBase::Acc, true, false, true, false);
}

void OP_GLUE_8A62(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.mma_xy_mx(arg0, arg1, arg2.GetName(), true, true, true, true, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_4DA0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 3) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 4) & 0x1;

    cpu.mma_my_my(arg0, arg1, arg2.GetName(), true, true, true, false, SumBase::Acc, true, false, true, false);
}

void OP_GLUE_4DA1(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 3) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 4) & 0x1;

    cpu.mma_my_my(arg0, arg1, arg2.GetName(), true, true, true, false, SumBase::Acc, true, false, true, true);
}

void OP_GLUE_4DA2(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 3) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 4) & 0x1;

    cpu.mma_my_my(arg0, arg1, arg2.GetName(), true, true, true, false, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_4DA3(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 3) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 2) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 4) & 0x1;

    cpu.mma_my_my(arg0, arg1, arg2.GetName(), true, true, true, false, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_94E0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.mma_my_my(arg0, arg1, arg2.GetName(), true, true, true, true, SumBase::Acc, true, false, true, false);
}

void OP_GLUE_94E1(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.mma_my_my(arg0, arg1, arg2.GetName(), true, true, false, true, SumBase::Acc, true, false, true, false);
}

void OP_GLUE_94E2(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.mma_my_my(arg0, arg1, arg2.GetName(), true, true, true, true, SumBase::Acc, true, false, true, true);
}

void OP_GLUE_94E3(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.mma_my_my(arg0, arg1, arg2.GetName(), true, true, false, true, SumBase::Acc, true, false, true, true);
}

void OP_GLUE_94E4(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.mma_my_my(arg0, arg1, arg2.GetName(), true, true, true, true, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_94E5(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.mma_my_my(arg0, arg1, arg2.GetName(), true, true, false, true, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_94E6(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.mma_my_my(arg0, arg1, arg2.GetName(), true, true, true, true, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_94E7(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn1 arg0{}; arg0.storage = (instr >> 4) & 0x1;
    ArStep1 arg1{}; arg1.storage = (instr >> 3) & 0x1;
    Ax arg2{}; arg2.storage = (instr >> 8) & 0x1;

    cpu.mma_my_my(arg0, arg1, arg2.GetName(), true, true, false, true, SumBase::Acc, false, false, false, true);
}

void OP_GLUE_4FA0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axh arg0{}; arg0.storage = (instr >> 6) & 0x1;
    Bxh arg1{}; arg1.storage = (instr >> 2) & 0x1;
    ArRn1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    ArStep1 arg3{}; arg3.storage = instr & 0x1;
    Ab arg4{}; arg4.storage = (instr >> 3) & 0x3;

    cpu.mma_mov(arg0, arg1, arg2, arg3, arg4.GetName(), true, true, true, true, SumBase::Acc, false, false, false, false);
}

void OP_GLUE_D3A0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axh arg0{}; arg0.storage = (instr >> 6) & 0x1;
    Bxh arg1{}; arg1.storage = (instr >> 2) & 0x1;
    ArRn1 arg2{}; arg2.storage = (instr >> 1) & 0x1;
    ArStep1 arg3{}; arg3.storage = instr & 0x1;
    Ab arg4{}; arg4.storage = (instr >> 3) & 0x3;

    cpu.mma_mov(arg0, arg1, arg2, arg3, arg4.GetName(), true, true, true, true, SumBase::Acc, false, false, true, false);
}

void OP_GLUE_80D0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axh arg0{}; arg0.storage = (instr >> 9) & 0x1;
    Bxh arg1{}; arg1.storage = (instr >> 8) & 0x1;
    ArRn1 arg2{}; arg2.storage = (instr >> 3) & 0x1;
    ArStep1 arg3{}; arg3.storage = (instr >> 2) & 0x1;
    Ax arg4{}; arg4.storage = (instr >> 10) & 0x1;

    cpu.mma_mov(arg0, arg1, arg2, arg3, arg4.GetName(), true, true, true, true, SumBase::Sv, false, false, true, false);
}

void OP_GLUE_80D1(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axh arg0{}; arg0.storage = (instr >> 9) & 0x1;
    Bxh arg1{}; arg1.storage = (instr >> 8) & 0x1;
    ArRn1 arg2{}; arg2.storage = (instr >> 3) & 0x1;
    ArStep1 arg3{}; arg3.storage = (instr >> 2) & 0x1;
    Ax arg4{}; arg4.storage = (instr >> 10) & 0x1;

    cpu.mma_mov(arg0, arg1, arg2, arg3, arg4.GetName(), true, true, true, true, SumBase::SvRnd, false, false, true, false);
}

void OP_GLUE_80D2(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axh arg0{}; arg0.storage = (instr >> 9) & 0x1;
    Bxh arg1{}; arg1.storage = (instr >> 8) & 0x1;
    ArRn1 arg2{}; arg2.storage = (instr >> 3) & 0x1;
    ArStep1 arg3{}; arg3.storage = (instr >> 2) & 0x1;
    Ax arg4{}; arg4.storage = (instr >> 10) & 0x1;

    cpu.mma_mov(arg0, arg1, arg2, arg3, arg4.GetName(), true, true, true, true, SumBase::Sv, false, false, false, false);
}

void OP_GLUE_80D3(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    Axh arg0{}; arg0.storage = (instr >> 9) & 0x1;
    Bxh arg1{}; arg1.storage = (instr >> 8) & 0x1;
    ArRn1 arg2{}; arg2.storage = (instr >> 3) & 0x1;
    ArStep1 arg3{}; arg3.storage = (instr >> 2) & 0x1;
    Ax arg4{}; arg4.storage = (instr >> 10) & 0x1;

    cpu.mma_mov(arg0, arg1, arg2, arg3, arg4.GetName(), true, true, true, true, SumBase::SvRnd, false, false, false, false);
}

void OP_GLUE_5818(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn2 arg0{}; arg0.storage = (instr >> 7) & 0x3;
    ArStep1 arg1{}; arg1.storage = (instr >> 6) & 0x1;
    Ax arg2{}; arg2.storage = instr & 0x1;

    cpu.mma_mov(arg0, arg1, arg2.GetName(), true, true, true, true, SumBase::Sv, false, false, true, false);
}

void OP_GLUE_5838(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn2 arg0{}; arg0.storage = (instr >> 7) & 0x3;
    ArStep1 arg1{}; arg1.storage = (instr >> 6) & 0x1;
    Ax arg2{}; arg2.storage = instr & 0x1;

    cpu.mma_mov(arg0, arg1, arg2.GetName(), true, true, true, true, SumBase::SvRnd, false, false, true, false);
}

void OP_GLUE_90E0(Interpreter& cpu, u16 instr)
{
    cpu.HandleLoops();

    ArRn2 arg0{}; arg0.storage = (instr >> 2) & 0x3;
    ArStep2 arg1{}; arg1.storage = instr & 0x3;
    Px arg2{}; arg2.storage = (instr >> 4) & 0x1;
    Ax arg3{}; arg3.storage = (instr >> 8) & 0x1;

    cpu.addhp(arg0, arg1, arg2, arg3);
}


#define INSTRFUNC_PROTO(x)  void (*x)(Interpreter& cpu, u16 instr)
#include "InstrTable.h"
#undef INSTRFUNC_PROTO

}
