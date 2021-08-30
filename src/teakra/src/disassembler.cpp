#include <iomanip>
#include <sstream>
#include <type_traits>
#include <vector>
#include "common_types.h"
#include "crash.h"
#include "decoder.h"
#include "operand.h"
#include "teakra/disassembler.h"

namespace Teakra::Disassembler {

template <typename T>
std::string ToHex(T i) {
    u64 v = i;
    std::stringstream stream;
    stream << "0x" << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex << v;
    return stream.str();
}

template <unsigned bits>
std::string Dsm(Imm<bits> a) {
    std::string eight_mark = bits == 8 ? "u8" : "";
    return ToHex(a.Unsigned16()) + eight_mark;
}

template <unsigned bits>
std::string Dsm(Imms<bits> a) {
    u16 value = a.Signed16();
    bool negative = (value >> 15) != 0;
    if (negative) {
        value = (~value) + 1;
    }
    return (negative ? "-" : "+") + ToHex(value);
}

std::string Dsm(MemImm8 a) {
    return "[page:" + Dsm((Imm8)a) + "]";
}

std::string Dsm(MemImm16 a) {
    return "[" + Dsm((Imm16)a) + "]";
}

std::string Dsm(MemR7Imm16 a) {
    return "[r7+" + Dsm((Imm16)a) + "]";
}
std::string Dsm(MemR7Imm7s a) {
    return "[r7" + Dsm((Imm7s)a) + "s7]";
}

std::string DsmReg(RegName a) {
    switch (a) {
    case RegName::a0:
        return "a0";
    case RegName::a0l:
        return "a0l";
    case RegName::a0h:
        return "a0h";
    case RegName::a0e:
        return "a0e";
    case RegName::a1:
        return "a1";
    case RegName::a1l:
        return "a1l";
    case RegName::a1h:
        return "a1h";
    case RegName::a1e:
        return "a1e";
    case RegName::b0:
        return "b0";
    case RegName::b0l:
        return "b0l";
    case RegName::b0h:
        return "b0h";
    case RegName::b0e:
        return "b0e";
    case RegName::b1:
        return "b1";
    case RegName::b1l:
        return "b1l";
    case RegName::b1h:
        return "b1h";
    case RegName::b1e:
        return "b1e";

    case RegName::r0:
        return "r0";
    case RegName::r1:
        return "r1";
    case RegName::r2:
        return "r2";
    case RegName::r3:
        return "r3";
    case RegName::r4:
        return "r4";
    case RegName::r5:
        return "r5";
    case RegName::r6:
        return "r6";
    case RegName::r7:
        return "r7";

    case RegName::ar0:
        return "ar0";
    case RegName::ar1:
        return "ar1";
    case RegName::arp0:
        return "arp0";
    case RegName::arp1:
        return "arp1";
    case RegName::arp2:
        return "arp2";
    case RegName::arp3:
        return "arp3";
    case RegName::stt0:
        return "stt0";
    case RegName::stt1:
        return "stt1";
    case RegName::stt2:
        return "stt2";
    case RegName::mod0:
        return "mod0";
    case RegName::mod1:
        return "mod1";
    case RegName::mod2:
        return "mod2";
    case RegName::mod3:
        return "mod3";
    case RegName::cfgi:
        return "cfgi";
    case RegName::cfgj:
        return "cfgj";

    case RegName::y0:
        return "y0";
    case RegName::p:
        return "p*";
    case RegName::pc:
        return "pc";
    case RegName::sp:
        return "sp";
    case RegName::sv:
        return "sv";
    case RegName::lc:
        return "lc";

    case RegName::st0:
        return "st0";
    case RegName::st1:
        return "st1";
    case RegName::st2:
        return "st2";
    default:
        return "[ERROR]" + std::to_string((int)a);
    }
}

template <typename RegT>
std::string R(RegT a) {
    return DsmReg(a.GetName());
}

template <>
std::string R(Px a) {
    return "p" + std::to_string(a.Index());
}

std::string Dsm(Alm alm) {
    switch (alm.GetName()) {
    case AlmOp::Or:
        return "or";
    case AlmOp::And:
        return "and";
    case AlmOp::Xor:
        return "xor";
    case AlmOp::Add:
        return "add";
    case AlmOp::Tst0:
        return "tst0";
    case AlmOp::Tst1:
        return "tst1";
    case AlmOp::Cmp:
        return "cmp";
    case AlmOp::Sub:
        return "sub";
    case AlmOp::Msu:
        return "msu";
    case AlmOp::Addh:
        return "addh";
    case AlmOp::Addl:
        return "addl";
    case AlmOp::Subh:
        return "subh";
    case AlmOp::Subl:
        return "subl";
    case AlmOp::Sqr:
        return "sqr";
    case AlmOp::Sqra:
        return "sqra";
    case AlmOp::Cmpu:
        return "cmpu";
    default:
        return "[ERROR]";
    }
}

std::string Dsm(Alu alu) {
    switch (alu.GetName()) {
    case AlmOp::Or:
        return "or";
    case AlmOp::And:
        return "and";
    case AlmOp::Xor:
        return "xor";
    case AlmOp::Add:
        return "add";
    case AlmOp::Cmp:
        return "cmp";
    case AlmOp::Sub:
        return "sub";
    default:
        return "[ERROR]";
    }
}

std::string Dsm(Alb alb) {
    switch (alb.GetName()) {
    case AlbOp::Set:
        return "set";
    case AlbOp::Rst:
        return "rst";
    case AlbOp::Chng:
        return "chng";
    case AlbOp::Addv:
        return "addv";
    case AlbOp::Tst0:
        return "tst0";
    case AlbOp::Tst1:
        return "tst1";
    case AlbOp::Cmpv:
        return "cmpv";
    case AlbOp::Subv:
        return "subv";
    default:
        return "[ERROR]";
    }
}

std::string Dsm(Moda4 moda4) {
    switch (moda4.GetName()) {
    case ModaOp::Shr:
        return "shr";
    case ModaOp::Shr4:
        return "shr4";
    case ModaOp::Shl:
        return "shl";
    case ModaOp::Shl4:
        return "shl4";
    case ModaOp::Ror:
        return "ror";
    case ModaOp::Rol:
        return "rol";
    case ModaOp::Clr:
        return "clr";
    case ModaOp::Not:
        return "not";
    case ModaOp::Neg:
        return "neg";
    case ModaOp::Rnd:
        return "rnd";
    case ModaOp::Pacr:
        return "pacr";
    case ModaOp::Clrr:
        return "clrr";
    case ModaOp::Inc:
        return "inc";
    case ModaOp::Dec:
        return "dec";
    case ModaOp::Copy:
        return "copy";
    default:
        return "[ERROR]";
    }
}

std::string Dsm(Moda3 moda3) {
    switch (moda3.GetName()) {
    case ModaOp::Shr:
        return "shr";
    case ModaOp::Shr4:
        return "shr4";
    case ModaOp::Shl:
        return "shl";
    case ModaOp::Shl4:
        return "shl4";
    case ModaOp::Ror:
        return "ror";
    case ModaOp::Rol:
        return "rol";
    case ModaOp::Clr:
        return "clr";
    case ModaOp::Clrr:
        return "clrr";
    default:
        return "[ERROR]";
    }
}

std::string Dsm(Mul3 mul) {
    switch (mul.GetName()) {
    case MulOp::Mpy:
        return "mpy";
    case MulOp::Mpysu:
        return "mpysu";
    case MulOp::Mac:
        return "mac";
    case MulOp::Macus:
        return "macus";
    case MulOp::Maa:
        return "maa";
    case MulOp::Macuu:
        return "macuu";
    case MulOp::Macsu:
        return "macsu";
    case MulOp::Maasu:
        return "maasu";
    default:
        return "[ERROR]";
    }
}

std::string Dsm(Mul2 mul) {
    switch (mul.GetName()) {
    case MulOp::Mpy:
        return "mpy";
    case MulOp::Mac:
        return "mac";
    case MulOp::Maa:
        return "maa";
    case MulOp::Macsu:
        return "macsu";
    default:
        return "[ERROR]";
    }
}

std::string Dsm(Cond cond) {
    switch (cond.GetName()) {
    case CondValue::True:
        return "always";
    case CondValue::Eq:
        return "eq";
    case CondValue::Neq:
        return "neq";
    case CondValue::Gt:
        return "gt";
    case CondValue::Ge:
        return "ge";
    case CondValue::Lt:
        return "lt";
    case CondValue::Le:
        return "le";
    case CondValue::Nn:
        return "mn";
    case CondValue::C:
        return "c";
    case CondValue::V:
        return "v";
    case CondValue::E:
        return "e";
    case CondValue::L:
        return "l";
    case CondValue::Nr:
        return "nr";
    case CondValue::Niu0:
        return "niu0";
    case CondValue::Iu0:
        return "iu0";
    case CondValue::Iu1:
        return "iu1";
    default:
        return "[ERROR]";
    }
}

std::string Dsm(Address16 addr) {
    return ToHex(addr.Address32());
}

std::string A18(Address18_16 addr_low, Address18_2 addr_high) {
    return ToHex(Address32(addr_low, addr_high));
}

std::string Dsm(StepZIDS step) {
    switch (step.GetName()) {
    case StepValue::Zero:
        return "";
    case StepValue::Increase:
        return "++";
    case StepValue::Decrease:
        return "--";
    case StepValue::PlusStep:
        return "++s";
    default:
        return "[ERROR]";
    }
}

std::string Dsm(std::string t) {
    return t;
}

template <typename RegT>
std::string MemR(RegT reg, StepZIDS step) {
    return "[" + R(reg) + Dsm(step) + "]";
}

template <typename Reg>
std::string MemG(Reg reg) {
    return "[" + R(reg) + "]";
}

std::string Dsm(CbsCond c) {
    switch (c.GetName()) {
    case CbsCondValue::Ge:
        return "ge";
    case CbsCondValue::Gt:
        return "gt";
    default:
        return "[ERROR]";
    }
}

template <typename... T>
std::vector<std::string> D(T... t) {
    return std::vector<std::string>{Dsm(t)...};
}

std::string Mul(bool x_sign, bool y_sign) {
    return std::string("mpy") + (x_sign ? "sx" : "ux") + (y_sign ? "sy" : "uy");
}

std::string PA(SumBase base, bool sub_p0, bool p0_align, bool sub_p1, bool p1_align) {
    std::string result;
    switch (base) {
    case SumBase::Zero:
        result = "0";
        break;
    case SumBase::Acc:
        result = "acc";
        break;
    case SumBase::Sv:
        result = "sv";
        break;
    case SumBase::SvRnd:
        result = "svr";
        break;
    }
    result += sub_p0 ? "-" : "+";
    result += "p0";
    result += p0_align ? "a" : "";
    result += sub_p1 ? "-" : "+";
    result += "p1";
    result += p1_align ? "a" : "";
    return result;
}

class Disassembler {
public:
    using instruction_return_type = std::vector<std::string>;

    std::vector<std::string> undefined(u16 opcode) {
        return D("[ERROR]");
    }

    std::vector<std::string> nop() {
        return D("nop");
    }

    std::vector<std::string> norm(Ax a, Rn b, StepZIDS bs) {
        return D("norm", R(a), MemR(b, bs));
    }
    std::vector<std::string> swap(SwapType swap) {
        std::string desc;
        switch (swap.GetName()) {
        case SwapTypeValue::a0b0:
            desc = "a0<->b0";
            break;
        case SwapTypeValue::a0b1:
            desc = "a0<->b1";
            break;
        case SwapTypeValue::a1b0:
            desc = "a1<->b0";
            break;
        case SwapTypeValue::a1b1:
            desc = "a1<->b1";
            break;
        case SwapTypeValue::a0b0a1b1:
            desc = "a<->b";
            break;
        case SwapTypeValue::a0b1a1b0:
            desc = "a-x-b";
            break;
        case SwapTypeValue::a0b0a1:
            desc = "a0->b0->a1";
            break;
        case SwapTypeValue::a0b1a1:
            desc = "a0->b1->a1";
            break;
        case SwapTypeValue::a1b0a0:
            desc = "a1->b0->a0";
            break;
        case SwapTypeValue::a1b1a0:
            desc = "a1->b1->a0";
            break;
        case SwapTypeValue::b0a0b1:
            desc = "b0->a0->b1";
            break;
        case SwapTypeValue::b0a1b1:
            desc = "b0->a1->b1";
            break;
        case SwapTypeValue::b1a0b0:
            desc = "b1->a0->b0";
            break;
        case SwapTypeValue::b1a1b0:
            desc = "b1->a1->b0";
            break;
        default:
            desc = "[ERROR]";
        }
        return D("swap", desc);
    }
    std::vector<std::string> trap() {
        return D("trap");
    }

    std::vector<std::string> alm(Alm op, MemImm8 a, Ax b) {
        return D(op, a, R(b));
    }
    std::vector<std::string> alm(Alm op, Rn a, StepZIDS as, Ax b) {
        return D(op, MemR(a, as), R(b));
    }
    std::vector<std::string> alm(Alm op, Register a, Ax b) {
        return D(op, R(a), R(b));
    }
    std::vector<std::string> alm_r6(Alm op, Ax b) {
        return D(op, "r6", R(b));
    }

    std::vector<std::string> alu(Alu op, MemImm16 a, Ax b) {
        return D(op, a, R(b));
    }
    std::vector<std::string> alu(Alu op, MemR7Imm16 a, Ax b) {
        return D(op, a, R(b));
    }
    std::vector<std::string> alu(Alu op, Imm16 a, Ax b) {
        return D(op, a, R(b));
    }
    std::vector<std::string> alu(Alu op, Imm8 a, Ax b) {
        return D(op, a, R(b));
    }
    std::vector<std::string> alu(Alu op, MemR7Imm7s a, Ax b) {
        return D(op, a, R(b));
    }

    std::vector<std::string> or_(Ab a, Ax b, Ax c) {
        return D("or", R(a), R(b), R(c));
    }
    std::vector<std::string> or_(Ax a, Bx b, Ax c) {
        return D("or", R(a), R(b), R(c));
    }
    std::vector<std::string> or_(Bx a, Bx b, Ax c) {
        return D("or", R(a), R(b), R(c));
    }

    std::vector<std::string> alb(Alb op, Imm16 a, MemImm8 b) {
        return D(op, a, b);
    }
    std::vector<std::string> alb(Alb op, Imm16 a, Rn b, StepZIDS bs) {
        return D(op, a, MemR(b, bs));
    }
    std::vector<std::string> alb(Alb op, Imm16 a, Register b) {
        return D(op, a, R(b));
    }
    std::vector<std::string> alb_r6(Alb op, Imm16 a) {
        return D(op, a, "r6");
    }
    std::vector<std::string> alb(Alb op, Imm16 a, SttMod b) {
        return D(op, a, R(b));
    }

    std::vector<std::string> add(Ab a, Bx b) {
        return D("add", R(a), R(b));
    }
    std::vector<std::string> add(Bx a, Ax b) {
        return D("add", R(a), R(b));
    }
    std::vector<std::string> add_p1(Ax b) {
        return D("add", "p1", R(b));
    }
    std::vector<std::string> add(Px a, Bx b) {
        return D("add", R(a), R(b));
    }

    std::vector<std::string> sub(Ab a, Bx b) {
        return D("sub", R(a), R(b));
    }
    std::vector<std::string> sub(Bx a, Ax b) {
        return D("sub", R(a), R(b));
    }
    std::vector<std::string> sub_p1(Ax b) {
        return D("sub", "p1", R(b));
    }
    std::vector<std::string> sub(Px a, Bx b) {
        return D("sub", R(a), R(b));
    }

    std::vector<std::string> app(Ab c, SumBase base, bool sub_p0, bool p0_align, bool sub_p1,
                                 bool p1_align) {
        return D(PA(base, sub_p0, p0_align, sub_p1, p1_align), R(c));
    }

    std::vector<std::string> add_add(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return D("add||add", MemARPSJ(a, asj), MemARPSI(a, asi), R(b));
    }
    std::vector<std::string> add_sub(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return D("add||sub", MemARPSJ(a, asj), MemARPSI(a, asi), R(b));
    }
    std::vector<std::string> sub_add(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return D("sub||add", MemARPSJ(a, asj), MemARPSI(a, asi), R(b));
    }
    std::vector<std::string> sub_sub(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return D("sub||sub", MemARPSJ(a, asj), MemARPSI(a, asi), R(b));
    }

    std::vector<std::string> add_sub_sv(ArRn1 a, ArStep1 as, Ab b) {
        return D("add||sub", MemARS(a, as), "sv", R(b));
    }
    std::vector<std::string> sub_add_sv(ArRn1 a, ArStep1 as, Ab b) {
        return D("sub||add", MemARS(a, as), "sv", R(b));
    }

    std::vector<std::string> sub_add_i_mov_j_sv(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return D("sub||add", MemARPSI(a, asi), R(b), "||mov", MemARPSJ(a, asj), "sv");
    }
    std::vector<std::string> sub_add_j_mov_i_sv(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return D("sub||add", MemARPSJ(a, asj), R(b), "||mov", MemARPSI(a, asi), "sv");
    }
    std::vector<std::string> add_sub_i_mov_j(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return D("add_sub", MemARPSI(a, asi), R(b), "||mov", R(b), MemARPSJ(a, asj));
    }
    std::vector<std::string> add_sub_j_mov_i(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return D("add_sub", MemARPSJ(a, asj), R(b), "||mov", R(b), MemARPSI(a, asi));
    }

    std::vector<std::string> moda4(Moda4 op, Ax a, Cond cond) {
        return D(op, R(a), cond);
    }

    std::vector<std::string> moda3(Moda3 op, Bx a, Cond cond) {
        return D(op, R(a), cond);
    }

    std::vector<std::string> pacr1(Ax a) {
        return D("pacr p1", R(a));
    }
    std::vector<std::string> clr(Ab a, Ab b) {
        return D("clr", R(a), R(b));
    }
    std::vector<std::string> clrr(Ab a, Ab b) {
        return D("clrr", R(a), R(b));
    }

    std::vector<std::string> bkrep(Imm8 a, Address16 addr) {
        return D("bkrep", a, addr);
    }
    std::vector<std::string> bkrep(Register a, Address18_16 addr_low, Address18_2 addr_high) {
        return D("bkrep", R(a), A18(addr_low, addr_high));
    }
    std::vector<std::string> bkrep_r6(Address18_16 addr_low, Address18_2 addr_high) {
        return D("bkrep", "r6", A18(addr_low, addr_high));
    }
    std::vector<std::string> bkreprst(ArRn2 a) {
        return D("bkreprst", MemAR(a));
    }
    std::vector<std::string> bkreprst_memsp() {
        return D("bkreprst", "[sp]");
    }
    std::vector<std::string> bkrepsto(ArRn2 a) {
        return D("bkrepsto", MemAR(a));
    }
    std::vector<std::string> bkrepsto_memsp() {
        return D("bkrepsto", "[sp]");
    }

    std::vector<std::string> banke(BankFlags flags) {
        std::vector<std::string> s{"banke"};
        if (flags.R0())
            s.push_back("r0");
        if (flags.R1())
            s.push_back("r1");
        if (flags.R4())
            s.push_back("r4");
        if (flags.Cfgi())
            s.push_back("cfgi");
        if (flags.R7())
            s.push_back("r7");
        if (flags.Cfgj())
            s.push_back("cfgj");
        return s;
    }
    std::vector<std::string> bankr() {
        return D("bankr");
    }
    std::vector<std::string> bankr(Ar a) {
        return D("bankr", R(a));
    }
    std::vector<std::string> bankr(Ar a, Arp b) {
        return D("bankr", R(a), R(b));
    }
    std::vector<std::string> bankr(Arp a) {
        return D("bankr", R(a));
    }

    std::vector<std::string> bitrev(Rn a) {
        return D("bitrev", R(a));
    }
    std::vector<std::string> bitrev_dbrv(Rn a) {
        return D("bitrev", R(a), "dbrv");
    }
    std::vector<std::string> bitrev_ebrv(Rn a) {
        return D("bitrev", R(a), "ebrv");
    }

    std::vector<std::string> br(Address18_16 addr_low, Address18_2 addr_high, Cond cond) {
        return D("br", A18(addr_low, addr_high), cond);
    }

    std::vector<std::string> brr(RelAddr7 addr, Cond cond) {
        return D("brr", ToHex((u16)addr.Relative32()), cond);
    }

    std::vector<std::string> break_() {
        return D("break");
    }

    std::vector<std::string> call(Address18_16 addr_low, Address18_2 addr_high, Cond cond) {
        return D("call", A18(addr_low, addr_high), cond);
    }
    std::vector<std::string> calla(Axl a) {
        return D("calla", R(a));
    }
    std::vector<std::string> calla(Ax a) {
        return D("calla", R(a));
    }
    std::vector<std::string> callr(RelAddr7 addr, Cond cond) {
        return D("callr", ToHex((u16)addr.Relative32()), cond);
    }

    std::vector<std::string> cntx_s() {
        return D("cntx", "s");
    }
    std::vector<std::string> cntx_r() {
        return D("cntx", "r");
    }

    std::vector<std::string> ret(Cond c) {
        return D("ret", c);
    }
    std::vector<std::string> retd() {
        return D("retd");
    }
    std::vector<std::string> reti(Cond c) {
        return D("reti", c);
    }
    std::vector<std::string> retic(Cond c) {
        return D("retic", c);
    }
    std::vector<std::string> retid() {
        return D("retid");
    }
    std::vector<std::string> retidc() {
        return D("retidc");
    }
    std::vector<std::string> rets(Imm8 a) {
        return D("rets", a);
    }

    std::vector<std::string> load_ps(Imm2 a) {
        return D("load", a, "ps");
    }
    std::vector<std::string> load_stepi(Imm7s a) {
        return D("load", a, "stepi");
    }
    std::vector<std::string> load_stepj(Imm7s a) {
        return D("load", a, "stepj");
    }
    std::vector<std::string> load_page(Imm8 a) {
        return D("load", a, "page");
    }
    std::vector<std::string> load_modi(Imm9 a) {
        return D("load", a, "modi");
    }
    std::vector<std::string> load_modj(Imm9 a) {
        return D("load", a, "modj");
    }
    std::vector<std::string> load_movpd(Imm2 a) {
        return D("load", a, "movpd");
    }
    std::vector<std::string> load_ps01(Imm4 a) {
        return D("load", a, "ps01");
    }

    std::vector<std::string> push(Imm16 a) {
        return D("push", a);
    }
    std::vector<std::string> push(Register a) {
        return D("push", R(a));
    }
    std::vector<std::string> push(Abe a) {
        return D("push", R(a));
    }
    std::vector<std::string> push(ArArpSttMod a) {
        return D("push", R(a));
    }
    std::vector<std::string> push_prpage() {
        return D("push", "prpage");
    }
    std::vector<std::string> push(Px a) {
        return D("push", R(a));
    }
    std::vector<std::string> push_r6() {
        return D("push", "r6");
    }
    std::vector<std::string> push_repc() {
        return D("push", "repc");
    }
    std::vector<std::string> push_x0() {
        return D("push", "x0");
    }
    std::vector<std::string> push_x1() {
        return D("push", "x1");
    }
    std::vector<std::string> push_y1() {
        return D("push", "y1");
    }
    std::vector<std::string> pusha(Ax a) {
        return D("pusha", R(a));
    }
    std::vector<std::string> pusha(Bx a) {
        return D("pusha", R(a));
    }

    std::vector<std::string> pop(Register a) {
        return D("pop", R(a));
    }
    std::vector<std::string> pop(Abe a) {
        return D("pop", R(a));
    }
    std::vector<std::string> pop(ArArpSttMod a) {
        return D("pop", R(a));
    }
    std::vector<std::string> pop(Bx a) {
        return D("pop", R(a));
    }
    std::vector<std::string> pop_prpage() {
        return D("pop", "prpage");
    }
    std::vector<std::string> pop(Px a) {
        return D("pop", R(a));
    }
    std::vector<std::string> pop_r6() {
        return D("pop", "r6");
    }
    std::vector<std::string> pop_repc() {
        return D("pop", "repc");
    }
    std::vector<std::string> pop_x0() {
        return D("pop", "x0");
    }
    std::vector<std::string> pop_x1() {
        return D("pop", "x1");
    }
    std::vector<std::string> pop_y1() {
        return D("pop", "y1");
    }
    std::vector<std::string> popa(Ab a) {
        return D("popa", R(a));
    }

    std::vector<std::string> rep(Imm8 a) {
        return D("rep", a);
    }
    std::vector<std::string> rep(Register a) {
        return D("rep", R(a));
    }
    std::vector<std::string> rep_r6() {
        return D("rep", "r6");
    }

    std::vector<std::string> shfc(Ab a, Ab b, Cond cond) {
        return D("shfc", R(a), R(b), cond);
    }
    std::vector<std::string> shfi(Ab a, Ab b, Imm6s s) {
        return D("shfi", R(a), R(b), s);
    }

    std::vector<std::string> tst4b(ArRn2 b, ArStep2 bs) {
        return D("tst4b", "a0l", MemARS(b, bs));
    }
    std::vector<std::string> tst4b(ArRn2 b, ArStep2 bs, Ax c) {
        return D("tst4b", "a0l", MemARS(b, bs), R(c));
    }
    std::vector<std::string> tstb(MemImm8 a, Imm4 b) {
        return D("tstb", a, b);
    }
    std::vector<std::string> tstb(Rn a, StepZIDS as, Imm4 b) {
        return D("tstb", MemR(a, as), b);
    }
    std::vector<std::string> tstb(Register a, Imm4 b) {
        return D("tstb", R(a), b);
    }
    std::vector<std::string> tstb_r6(Imm4 b) {
        return D("tstb", "r6", b);
    }
    std::vector<std::string> tstb(SttMod a, Imm16 b) {
        return D("tstb", R(a), b);
    }

    std::vector<std::string> and_(Ab a, Ab b, Ax c) {
        return D("and", R(a), R(b), R(c));
    }

    std::vector<std::string> dint() {
        return D("dint");
    }
    std::vector<std::string> eint() {
        return D("eint");
    }

    std::vector<std::string> mul(Mul3 op, Rn y, StepZIDS ys, Imm16 x, Ax a) {
        return D(op, MemR(y, ys), x, R(a));
    }
    std::vector<std::string> mul_y0(Mul3 op, Rn x, StepZIDS xs, Ax a) {
        return D(op, "y0", MemR(x, xs), R(a));
    }
    std::vector<std::string> mul_y0(Mul3 op, Register x, Ax a) {
        return D(op, "y0", R(x), R(a));
    }
    std::vector<std::string> mul(Mul3 op, R45 y, StepZIDS ys, R0123 x, StepZIDS xs, Ax a) {
        return D(op, MemR(y, ys), MemR(x, xs), R(a));
    }
    std::vector<std::string> mul_y0_r6(Mul3 op, Ax a) {
        return D(op, "y0", "r6", R(a));
    }
    std::vector<std::string> mul_y0(Mul2 op, MemImm8 x, Ax a) {
        return D(op, "y0", x, R(a));
    }

    std::vector<std::string> mpyi(Imm8s x) {
        return D("mpyi", "y0", x);
    }

    std::vector<std::string> msu(R45 y, StepZIDS ys, R0123 x, StepZIDS xs, Ax a) {
        return D("msu", MemR(y, ys), MemR(x, xs), R(a));
    }
    std::vector<std::string> msu(Rn y, StepZIDS ys, Imm16 x, Ax a) {
        return D("msu", MemR(y, ys), x, R(a));
    }
    std::vector<std::string> msusu(ArRn2 x, ArStep2 xs, Ax a) {
        return D("msusu", "y0", MemARS(x, xs), R(a));
    }
    std::vector<std::string> mac_x1to0(Ax a) {
        return D("mac", "y0", "x1->x0", R(a));
    }
    std::vector<std::string> mac1(ArpRn1 xy, ArpStep1 xis, ArpStep1 yjs, Ax a) {
        return D("mac1", MemARPSJ(xy, yjs), MemARPSI(xy, xis), R(a));
    }

    std::vector<std::string> modr(Rn a, StepZIDS as) {
        return D("modr", MemR(a, as));
    }
    std::vector<std::string> modr_dmod(Rn a, StepZIDS as) {
        return D("modr", MemR(a, as), "dmod");
    }
    std::vector<std::string> modr_i2(Rn a) {
        return D("modr", R(a), "+2");
    }
    std::vector<std::string> modr_i2_dmod(Rn a) {
        return D("modr", R(a), "+2", "dmod");
    }
    std::vector<std::string> modr_d2(Rn a) {
        return D("modr", R(a), "-2");
    }
    std::vector<std::string> modr_d2_dmod(Rn a) {
        return D("modr", R(a), "-2", "dmod");
    }
    std::vector<std::string> modr_eemod(ArpRn2 a, ArpStep2 asi, ArpStep2 asj) {
        return D("modr", MemARPSI(a, asi), MemARPSJ(a, asj), "eemod");
    }
    std::vector<std::string> modr_edmod(ArpRn2 a, ArpStep2 asi, ArpStep2 asj) {
        return D("modr", MemARPSI(a, asi), MemARPSJ(a, asj), "edmod");
    }
    std::vector<std::string> modr_demod(ArpRn2 a, ArpStep2 asi, ArpStep2 asj) {
        return D("modr", MemARPSI(a, asi), MemARPSJ(a, asj), "demod");
    }
    std::vector<std::string> modr_ddmod(ArpRn2 a, ArpStep2 asi, ArpStep2 asj) {
        return D("modr", MemARPSI(a, asi), MemARPSJ(a, asj), "ddmod");
    }

    std::vector<std::string> movd(R0123 a, StepZIDS as, R45 b, StepZIDS bs) {
        return D("mov d->p", MemR(a, as), MemR(b, bs));
    }
    std::vector<std::string> movp(Axl a, Register b) {
        return D("mov p->r", MemG(a), R(b));
    }
    std::vector<std::string> movp(Ax a, Register b) {
        return D("mov p->r", MemG(a), R(b));
    }
    std::vector<std::string> movp(Rn a, StepZIDS as, R0123 b, StepZIDS bs) {
        return D("mov p->d", MemR(a, as), MemR(b, bs));
    }
    std::vector<std::string> movpdw(Ax a) {
        return D("mov p->pc", MemG(a));
    }

    std::vector<std::string> mov(Ab a, Ab b) {
        return D("mov", R(a), R(b));
    }
    std::vector<std::string> mov_dvm(Abl a) {
        return D("mov", R(a), "dvm");
    }
    std::vector<std::string> mov_x0(Abl a) {
        return D("mov", R(a), "x0");
    }
    std::vector<std::string> mov_x1(Abl a) {
        return D("mov", R(a), "x1");
    }
    std::vector<std::string> mov_y1(Abl a) {
        return D("mov", R(a), "y1");
    }
    std::vector<std::string> mov(Ablh a, MemImm8 b) {
        return D("mov", R(a), b);
    }
    std::vector<std::string> mov(Axl a, MemImm16 b) {
        return D("mov", R(a), b);
    }
    std::vector<std::string> mov(Axl a, MemR7Imm16 b) {
        return D("mov", R(a), b);
    }
    std::vector<std::string> mov(Axl a, MemR7Imm7s b) {
        return D("mov", R(a), b);
    }
    std::vector<std::string> mov(MemImm16 a, Ax b) {
        return D("mov", a, R(b));
    }
    std::vector<std::string> mov(MemImm8 a, Ab b) {
        return D("mov", a, R(b));
    }
    std::vector<std::string> mov(MemImm8 a, Ablh b) {
        return D("mov", a, R(b));
    }
    std::vector<std::string> mov_eu(MemImm8 a, Axh b) {
        return D("mov", a, R(b), "eu");
    }
    std::vector<std::string> mov(MemImm8 a, RnOld b) {
        return D("mov", a, R(b));
    }
    std::vector<std::string> mov_sv(MemImm8 a) {
        return D("mov", a, "sv");
    }
    std::vector<std::string> mov_dvm_to(Ab b) {
        return D("mov", "dvm", R(b));
    }
    std::vector<std::string> mov_icr_to(Ab b) {
        return D("mov", "icr", R(b));
    }
    std::vector<std::string> mov(Imm16 a, Bx b) {
        return D("mov", a, R(b));
    }
    std::vector<std::string> mov(Imm16 a, Register b) {
        return D("mov", a, R(b));
    }
    std::vector<std::string> mov_icr(Imm5 a) {
        return D("mov", a, "icr");
    }
    std::vector<std::string> mov(Imm8s a, Axh b) {
        return D("mov", a, R(b));
    }
    std::vector<std::string> mov(Imm8s a, RnOld b) {
        return D("mov", a, R(b));
    }
    std::vector<std::string> mov_sv(Imm8s a) {
        return D("mov", a, "sv");
    }
    std::vector<std::string> mov(Imm8 a, Axl b) {
        return D("mov", a, R(b));
    }
    std::vector<std::string> mov(MemR7Imm16 a, Ax b) {
        return D("mov", a, R(b));
    }
    std::vector<std::string> mov(MemR7Imm7s a, Ax b) {
        return D("mov", a, R(b));
    }
    std::vector<std::string> mov(Rn a, StepZIDS as, Bx b) {
        return D("mov", MemR(a, as), R(b));
    }
    std::vector<std::string> mov(Rn a, StepZIDS as, Register b) {
        return D("mov", MemR(a, as), R(b));
    }
    std::vector<std::string> mov_memsp_to(Register b) {
        return D("mov", "[sp]", R(b));
    }
    std::vector<std::string> mov_mixp_to(Register b) {
        return D("mov", "mixp", R(b));
    }
    std::vector<std::string> mov(RnOld a, MemImm8 b) {
        return D("mov", R(a), b);
    }
    std::vector<std::string> mov_icr(Register a) {
        return D("mov", R(a), "icr");
    }
    std::vector<std::string> mov_mixp(Register a) {
        return D("mov", R(a), "mixp");
    }
    std::vector<std::string> mov(Register a, Rn b, StepZIDS bs) {
        return D("mov", R(a), MemR(b, bs));
    }
    std::vector<std::string> mov(Register a, Bx b) {
        std::string a_mark;
        if (a.GetName() == RegName::a0 || a.GetName() == RegName::a1) {
            a_mark = "?";
        }
        return D("mov" + a_mark, R(a), R(b));
    }
    std::vector<std::string> mov(Register a, Register b) {
        return D("mov", R(a), R(b));
    }
    std::vector<std::string> mov_repc_to(Ab b) {
        return D("mov", "repc", R(b));
    }
    std::vector<std::string> mov_sv_to(MemImm8 b) {
        return D("mov", "sv", b);
    }
    std::vector<std::string> mov_x0_to(Ab b) {
        return D("mov", "x0", R(b));
    }
    std::vector<std::string> mov_x1_to(Ab b) {
        return D("mov", "x1", R(b));
    }
    std::vector<std::string> mov_y1_to(Ab b) {
        return D("mov", "y1", R(b));
    }
    std::vector<std::string> mov(Imm16 a, ArArp b) {
        return D("mov", a, R(b));
    }
    std::vector<std::string> mov_r6(Imm16 a) {
        return D("mov", a, "r6");
    }
    std::vector<std::string> mov_repc(Imm16 a) {
        return D("mov", a, "repc");
    }
    std::vector<std::string> mov_stepi0(Imm16 a) {
        return D("mov", a, "stepi0");
    }
    std::vector<std::string> mov_stepj0(Imm16 a) {
        return D("mov", a, "stepj0");
    }
    std::vector<std::string> mov(Imm16 a, SttMod b) {
        return D("mov", a, R(b));
    }
    std::vector<std::string> mov_prpage(Imm4 a) {
        return D("mov", a, "prpage");
    }

    std::vector<std::string> mov_a0h_stepi0() {
        return D("mov", "a0h", "stepi0");
    }
    std::vector<std::string> mov_a0h_stepj0() {
        return D("mov", "a0h", "stepj0");
    }
    std::vector<std::string> mov_stepi0_a0h() {
        return D("mov", "stepi0", "a0h");
    }
    std::vector<std::string> mov_stepj0_a0h() {
        return D("mov", "stepj0", "a0h");
    }

    std::vector<std::string> mov_prpage(Abl a) {
        return D("mov", R(a), "prpage");
    }
    std::vector<std::string> mov_repc(Abl a) {
        return D("mov", R(a), "repc");
    }
    std::vector<std::string> mov(Abl a, ArArp b) {
        return D("mov", R(a), R(b));
    }
    std::vector<std::string> mov(Abl a, SttMod b) {
        return D("mov", R(a), R(b));
    }

    std::vector<std::string> mov_prpage_to(Abl b) {
        return D("mov", "prpage", R(b));
    }
    std::vector<std::string> mov_repc_to(Abl b) {
        return D("mov", "repc", R(b));
    }
    std::vector<std::string> mov(ArArp a, Abl b) {
        return D("mov", R(a), R(b));
    }
    std::vector<std::string> mov(SttMod a, Abl b) {
        return D("mov", R(a), R(b));
    }

    std::vector<std::string> mov_repc_to(ArRn1 b, ArStep1 bs) {
        return D("mov", "repc", MemARS(b, bs));
    }
    std::vector<std::string> mov(ArArp a, ArRn1 b, ArStep1 bs) {
        return D("mov", R(a), MemARS(b, bs));
    }
    std::vector<std::string> mov(SttMod a, ArRn1 b, ArStep1 bs) {
        return D("mov", R(a), MemARS(b, bs));
    }

    std::vector<std::string> mov_repc(ArRn1 a, ArStep1 as) {
        return D("mov", MemARS(a, as), "repc");
    }
    std::vector<std::string> mov(ArRn1 a, ArStep1 as, ArArp b) {
        return D("mov", MemARS(a, as), R(b));
    }
    std::vector<std::string> mov(ArRn1 a, ArStep1 as, SttMod b) {
        return D("mov", MemARS(a, as), R(b));
    }

    std::vector<std::string> mov_repc_to(MemR7Imm16 b) {
        return D("mov", "repc", b);
    }
    std::vector<std::string> mov(ArArpSttMod a, MemR7Imm16 b) {
        return D("mov", R(a), b);
    }

    std::vector<std::string> mov_repc(MemR7Imm16 a) {
        return D("mov", a, "repc");
    }
    std::vector<std::string> mov(MemR7Imm16 a, ArArpSttMod b) {
        return D("mov", a, R(b));
    }

    std::vector<std::string> mov_pc(Ax a) {
        return D("mov", R(a), "pc");
    }
    std::vector<std::string> mov_pc(Bx a) {
        return D("mov", R(a), "pc");
    }

    std::vector<std::string> mov_mixp_to(Bx b) {
        return D("mov", "mixp", R(b));
    }
    std::vector<std::string> mov_mixp_r6() {
        return D("mov", "mixp", "r6");
    }
    std::vector<std::string> mov_p0h_to(Bx b) {
        return D("mov", "p0h", R(b));
    }
    std::vector<std::string> mov_p0h_r6() {
        return D("mov", "p0h", "r6");
    }
    std::vector<std::string> mov_p0h_to(Register b) {
        return D("mov", "p0h", R(b));
    }
    std::vector<std::string> mov_p0(Ab a) {
        return D("mov", R(a), "p0");
    }
    std::vector<std::string> mov_p1_to(Ab b) {
        return D("mov", "p1", R(b));
    }

    std::vector<std::string> mov2(Px a, ArRn2 b, ArStep2 bs) {
        return D("mov", R(a), MemARS(b, bs));
    }
    std::vector<std::string> mov2s(Px a, ArRn2 b, ArStep2 bs) {
        return D("mov s", R(a), MemARS(b, bs));
    }
    std::vector<std::string> mov2(ArRn2 a, ArStep2 as, Px b) {
        return D("mov", MemARS(a, as), R(b));
    }
    std::vector<std::string> mova(Ab a, ArRn2 b, ArStep2 bs) {
        return D("mov", R(a), MemARS(b, bs));
    }
    std::vector<std::string> mova(ArRn2 a, ArStep2 as, Ab b) {
        return D("mov", MemARS(a, as), R(b));
    }

    std::vector<std::string> mov_r6_to(Bx b) {
        return D("mov", "r6", R(b));
    }
    std::vector<std::string> mov_r6_mixp() {
        return D("mov", "r6", "mixp");
    }
    std::vector<std::string> mov_r6_to(Register b) {
        return D("mov", "r6", R(b));
    }
    std::vector<std::string> mov_r6(Register a) {
        return D("mov", R(a), "r6");
    }
    std::vector<std::string> mov_memsp_r6() {
        return D("mov", "[sp]", "r6");
    }
    std::vector<std::string> mov_r6_to(Rn b, StepZIDS bs) {
        return D("mov", "r6", MemR(b, bs));
    }
    std::vector<std::string> mov_r6(Rn a, StepZIDS as) {
        return D("mov", MemR(a, as), "r6");
    }

    std::vector<std::string> movs(MemImm8 a, Ab b) {
        return D("movs", a, R(b));
    }
    std::vector<std::string> movs(Rn a, StepZIDS as, Ab b) {
        return D("movs", MemR(a, as), R(b));
    }
    std::vector<std::string> movs(Register a, Ab b) {
        return D("movs", R(a), R(b));
    }
    std::vector<std::string> movs_r6_to(Ax b) {
        return D("movs", "r6", R(b));
    }
    std::vector<std::string> movsi(RnOld a, Ab b, Imm5s s) {
        return D("movsi", R(a), R(b), s);
    }

    std::vector<std::string> mov2_axh_m_y0_m(Axh a, ArRn2 b, ArStep2 bs) {
        return D("mov||mov", R(a), "y0", MemARS(b, bs));
    }
    std::vector<std::string> mov2_ax_mij(Ab a, ArpRn1 b, ArpStep1 bsi, ArpStep1 bsj) {
        return D("mov hilj", R(a), MemARPSI(b, bsi), MemARPSJ(b, bsj));
    }
    std::vector<std::string> mov2_ax_mji(Ab a, ArpRn1 b, ArpStep1 bsi, ArpStep1 bsj) {
        return D("mov lihj", R(a), MemARPSI(b, bsi), MemARPSJ(b, bsj));
    }
    std::vector<std::string> mov2_mij_ax(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return D("mov hilj", MemARPSI(a, asi), MemARPSJ(a, asj), R(b));
    }
    std::vector<std::string> mov2_mji_ax(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return D("mov lihj", MemARPSI(a, asi), MemARPSJ(a, asj), R(b));
    }
    std::vector<std::string> mov2_abh_m(Abh ax, Abh ay, ArRn1 b, ArStep1 bs) {
        return D("mov||mov", R(ax), R(ay), MemARS(b, bs));
    }
    std::vector<std::string> exchange_iaj(Axh a, ArpRn2 b, ArpStep2 bsi, ArpStep2 bsj) {
        return D("exchange i->a->j", R(a), MemARPSI(b, bsi), MemARPSJ(b, bsj));
    }
    std::vector<std::string> exchange_riaj(Axh a, ArpRn2 b, ArpStep2 bsi, ArpStep2 bsj) {
        return D("exchange ri->a->j", R(a), MemARPSI(b, bsi), MemARPSJ(b, bsj));
    }
    std::vector<std::string> exchange_jai(Axh a, ArpRn2 b, ArpStep2 bsi, ArpStep2 bsj) {
        return D("exchange j->a->i", R(a), MemARPSI(b, bsi), MemARPSJ(b, bsj));
    }
    std::vector<std::string> exchange_rjai(Axh a, ArpRn2 b, ArpStep2 bsi, ArpStep2 bsj) {
        return D("exchange rj->a->i", R(a), MemARPSI(b, bsi), MemARPSJ(b, bsj));
    }

    std::vector<std::string> movr(ArRn2 a, ArStep2 as, Abh b) {
        return D("movr", MemARS(a, as), R(b));
    }
    std::vector<std::string> movr(Rn a, StepZIDS as, Ax b) {
        return D("movr", MemR(a, as), R(b));
    }
    std::vector<std::string> movr(Register a, Ax b) {
        return D("movr", R(a), R(b));
    }
    std::vector<std::string> movr(Bx a, Ax b) {
        return D("movr", R(a), R(b));
    }
    std::vector<std::string> movr_r6_to(Ax b) {
        return D("movr", "r6", R(b));
    }

    std::vector<std::string> exp(Bx a) {
        return D("exp", R(a));
    }
    std::vector<std::string> exp(Bx a, Ax b) {
        return D("exp", R(a), R(b));
    }
    std::vector<std::string> exp(Rn a, StepZIDS as) {
        return D("exp", MemR(a, as));
    }
    std::vector<std::string> exp(Rn a, StepZIDS as, Ax b) {
        return D("exp", MemR(a, as), R(b));
    }
    std::vector<std::string> exp(Register a) {
        return D("exp", R(a));
    }
    std::vector<std::string> exp(Register a, Ax b) {
        return D("exp", R(a), R(b));
    }
    std::vector<std::string> exp_r6() {
        return D("exp", "r6");
    }
    std::vector<std::string> exp_r6(Ax b) {
        return D("exp", "r6", R(b));
    }

    std::vector<std::string> lim(Ax a, Ax b) {
        return D("lim", R(a), R(b));
    }

    std::vector<std::string> vtrclr0() {
        return D("vtrclr0");
    }
    std::vector<std::string> vtrclr1() {
        return D("vtrclr1");
    }
    std::vector<std::string> vtrclr() {
        return D("vtrclr");
    }
    std::vector<std::string> vtrmov0(Axl a) {
        return D("vtrmov0", R(a));
    }
    std::vector<std::string> vtrmov1(Axl a) {
        return D("vtrmov1", R(a));
    }
    std::vector<std::string> vtrmov(Axl a) {
        return D("vtrmov", R(a));
    }
    std::vector<std::string> vtrshr() {
        return D("vtrshr");
    }

    std::vector<std::string> clrp0() {
        return D("clr0");
    }
    std::vector<std::string> clrp1() {
        return D("clr1");
    }
    std::vector<std::string> clrp() {
        return D("clr");
    }

    std::vector<std::string> max_ge(Ax a, StepZIDS bs) {
        return D("max_ge", R(a), "^", "r0", bs);
    }
    std::vector<std::string> max_gt(Ax a, StepZIDS bs) {
        return D("max_gt", R(a), "^", "r0", bs);
    }
    std::vector<std::string> min_le(Ax a, StepZIDS bs) {
        return D("min_le", R(a), "^", "r0", bs);
    }
    std::vector<std::string> min_lt(Ax a, StepZIDS bs) {
        return D("min_lt", R(a), "^", "r0", bs);
    }

    std::vector<std::string> max_ge_r0(Ax a, StepZIDS bs) {
        return D("max_ge", R(a), "[r0]", bs);
    }
    std::vector<std::string> max_gt_r0(Ax a, StepZIDS bs) {
        return D("max_gt", R(a), "[r0]", bs);
    }
    std::vector<std::string> min_le_r0(Ax a, StepZIDS bs) {
        return D("min_le", R(a), "[r0]", bs);
    }
    std::vector<std::string> min_lt_r0(Ax a, StepZIDS bs) {
        return D("min_lt", R(a), "[r0]", bs);
    }
    std::vector<std::string> divs(MemImm8 a, Ax b) {
        return D("divs", a, R(b));
    }

    std::vector<std::string> sqr_sqr_add3(Ab a, Ab b) {
        return D("sqr h||l", R(a), "||add3", R(b));
    }
    std::vector<std::string> sqr_sqr_add3(ArRn2 a, ArStep2 as, Ab b) {
        return D("sqr h||l", MemARS(a, as), "||add3", R(b));
    }
    std::vector<std::string> sqr_mpysu_add3a(Ab a, Ab b) {
        return D("sqr h||mpysu hl", R(a), "||add3a", R(b));
    }

    std::vector<std::string> cmp(Ax a, Bx b) {
        return D("cmp", R(a), R(b));
    }
    std::vector<std::string> cmp_b0_b1() {
        return D("cmp", "b0", "b1");
    }
    std::vector<std::string> cmp_b1_b0() {
        return D("cmp", "b1", "b0");
    }
    std::vector<std::string> cmp(Bx a, Ax b) {
        return D("cmp", R(a), R(b));
    }
    std::vector<std::string> cmp_p1_to(Ax b) {
        return D("cmp", "p1", R(b));
    }

    std::vector<std::string> max2_vtr(Ax a) {
        return D("max h||l", R(a), "||vtrshr");
    }
    std::vector<std::string> min2_vtr(Ax a) {
        return D("min h||l", R(a), "||vtrshr");
    }
    std::vector<std::string> max2_vtr(Ax a, Bx b) {
        return D("max h||l", R(a), R(b), "||vtrshr");
    }
    std::vector<std::string> min2_vtr(Ax a, Bx b) {
        return D("min h||l", R(a), R(b), "||vtrshr");
    }
    std::vector<std::string> max2_vtr_movl(Ax a, Bx b, ArRn1 c, ArStep1 cs) {
        return D("max h||l", R(a), R(b), "||vtrshr", "||mov^l", R(a), MemARS(c, cs));
    }
    std::vector<std::string> max2_vtr_movh(Ax a, Bx b, ArRn1 c, ArStep1 cs) {
        return D("max h||l", R(a), R(b), "||vtrshr", "||mov^h", R(a), MemARS(c, cs));
    }
    std::vector<std::string> max2_vtr_movl(Bx a, Ax b, ArRn1 c, ArStep1 cs) {
        return D("max h||l", R(a), R(b), "||vtrshr", "||mov^l", R(a), MemARS(c, cs));
    }
    std::vector<std::string> max2_vtr_movh(Bx a, Ax b, ArRn1 c, ArStep1 cs) {
        return D("max h||l", R(a), R(b), "||vtrshr", "||mov^h", R(a), MemARS(c, cs));
    }
    std::vector<std::string> min2_vtr_movl(Ax a, Bx b, ArRn1 c, ArStep1 cs) {
        return D("min h||l", R(a), R(b), "||vtrshr", "||mov^l", R(a), MemARS(c, cs));
    }
    std::vector<std::string> min2_vtr_movh(Ax a, Bx b, ArRn1 c, ArStep1 cs) {
        return D("min h||l", R(a), R(b), "||vtrshr", "||mov^h", R(a), MemARS(c, cs));
    }
    std::vector<std::string> min2_vtr_movl(Bx a, Ax b, ArRn1 c, ArStep1 cs) {
        return D("min h||l", R(a), R(b), "||vtrshr", "||mov^l", R(a), MemARS(c, cs));
    }
    std::vector<std::string> min2_vtr_movh(Bx a, Ax b, ArRn1 c, ArStep1 cs) {
        return D("min h||l", R(a), R(b), "||vtrshr", "||mov^h", R(a), MemARS(c, cs));
    }
    std::vector<std::string> max2_vtr_movij(Ax a, Bx b, ArpRn1 c, ArpStep1 csi, ArpStep1 csj) {
        return D("max h||l", R(a), R(b), "||vtrshr", "||mov^hilj", R(a), MemARPSI(c, csi),
                 MemARPSJ(c, csj));
    }
    std::vector<std::string> max2_vtr_movji(Ax a, Bx b, ArpRn1 c, ArpStep1 csi, ArpStep1 csj) {
        return D("max h||l", R(a), R(b), "||vtrshr", "||mov^hjli", R(a), MemARPSI(c, csi),
                 MemARPSJ(c, csj));
    }
    std::vector<std::string> min2_vtr_movij(Ax a, Bx b, ArpRn1 c, ArpStep1 csi, ArpStep1 csj) {
        return D("min h||l", R(a), R(b), "||vtrshr", "||mov^hilj", R(a), MemARPSI(c, csi),
                 MemARPSJ(c, csj));
    }
    std::vector<std::string> min2_vtr_movji(Ax a, Bx b, ArpRn1 c, ArpStep1 csi, ArpStep1 csj) {
        return D("min h||l", R(a), R(b), "||vtrshr", "||mov^hjli", R(a), MemARPSI(c, csi),
                 MemARPSJ(c, csj));
    }

    template <typename ArpStepX>
    std::vector<std::string> mov_sv_app(ArRn1 a, ArpStepX as, Bx b, SumBase base, bool sub_p0,
                                        bool p0_align, bool sub_p1, bool p1_align) {
        return D("mov", MemARS(a, as), "sv", PA(base, sub_p0, p0_align, sub_p1, p1_align), R(b));
    }

    std::vector<std::string> cbs(Axh a, CbsCond c) {
        return D("cbs", R(a), "r0", c);
    }
    std::vector<std::string> cbs(Axh a, Bxh b, CbsCond c) {
        return D("cbs", R(a), R(b), "r0", c);
    }
    std::vector<std::string> cbs(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, CbsCond c) {
        return D("cbs", MemARPSI(a, asi), MemARPSJ(a, asj), c);
    }

    std::vector<std::string> mma(RegName a, bool x0_sign, bool y0_sign, bool x1_sign, bool y1_sign,
                                 SumBase base, bool sub_p0, bool p0_align, bool sub_p1,
                                 bool p1_align) {
        return D("x0<->x1", PA(base, sub_p0, p0_align, sub_p1, p1_align), DsmReg(a),
                 Mul(x0_sign, y0_sign), Mul(x1_sign, y1_sign));
    }

    template <typename ArpRnX, typename ArpStepX>
    std::vector<std::string> mma(ArpRnX xy, ArpStepX i, ArpStepX j, bool dmodi, bool dmodj,
                                 RegName a, bool x0_sign, bool y0_sign, bool x1_sign, bool y1_sign,
                                 SumBase base, bool sub_p0, bool p0_align, bool sub_p1,
                                 bool p1_align) {
        return D("xy<-", MemARPSI(xy, i), MemARPSJ(xy, j),
                 PA(base, sub_p0, p0_align, sub_p1, p1_align), DsmReg(a), Mul(x0_sign, y0_sign),
                 Mul(x1_sign, y1_sign), dmodi ? "dmodi" : "", dmodj ? "dmodj" : "");
    }

    std::vector<std::string> mma_mx_xy(ArRn1 y, ArStep1 ys, RegName a, bool x0_sign, bool y0_sign,
                                       bool x1_sign, bool y1_sign, SumBase base, bool sub_p0,
                                       bool p0_align, bool sub_p1, bool p1_align) {
        return D("x0<->x1, y0<-", MemARS(y, ys), PA(base, sub_p0, p0_align, sub_p1, p1_align),
                 DsmReg(a), Mul(x0_sign, y0_sign), Mul(x1_sign, y1_sign));
    }

    std::vector<std::string> mma_xy_mx(ArRn1 y, ArStep1 ys, RegName a, bool x0_sign, bool y0_sign,
                                       bool x1_sign, bool y1_sign, SumBase base, bool sub_p0,
                                       bool p0_align, bool sub_p1, bool p1_align) {
        return D("x0<->x1, y1<-", MemARS(y, ys), PA(base, sub_p0, p0_align, sub_p1, p1_align),
                 DsmReg(a), Mul(x0_sign, y0_sign), Mul(x1_sign, y1_sign));
    }

    std::vector<std::string> mma_my_my(ArRn1 x, ArStep1 xs, RegName a, bool x0_sign, bool y0_sign,
                                       bool x1_sign, bool y1_sign, SumBase base, bool sub_p0,
                                       bool p0_align, bool sub_p1, bool p1_align) {
        return D("x<-", MemARS(x, xs), PA(base, sub_p0, p0_align, sub_p1, p1_align), DsmReg(a),
                 Mul(x0_sign, y0_sign), Mul(x1_sign, y1_sign));
    }

    std::vector<std::string> mma_mov(Axh u, Bxh v, ArRn1 w, ArStep1 ws, RegName a, bool x0_sign,
                                     bool y0_sign, bool x1_sign, bool y1_sign, SumBase base,
                                     bool sub_p0, bool p0_align, bool sub_p1, bool p1_align) {
        return D("mov", R(u), R(v), MemARS(w, ws), "x0<->x1",
                 PA(base, sub_p0, p0_align, sub_p1, p1_align), DsmReg(a), Mul(x0_sign, y0_sign),
                 Mul(x1_sign, y1_sign));
    }

    std::vector<std::string> mma_mov(ArRn2 w, ArStep1 ws, RegName a, bool x0_sign, bool y0_sign,
                                     bool x1_sign, bool y1_sign, SumBase base, bool sub_p0,
                                     bool p0_align, bool sub_p1, bool p1_align) {
        return D("mov,^", DsmReg(a), MemARS(w, ws), "x0<->x1",
                 PA(base, sub_p0, p0_align, sub_p1, p1_align), DsmReg(a), Mul(x0_sign, y0_sign),
                 Mul(x1_sign, y1_sign));
    }

    std::vector<std::string> addhp(ArRn2 a, ArStep2 as, Px b, Ax c) {
        return D("addhp", MemARS(a, as), R(b), R(c));
    }

    std::vector<std::string> mov_ext0(Imm8s a) {
        return D("mov", a, "ext0");
    }
    std::vector<std::string> mov_ext1(Imm8s a) {
        return D("mov", a, "ext1");
    }
    std::vector<std::string> mov_ext2(Imm8s a) {
        return D("mov", a, "ext2");
    }
    std::vector<std::string> mov_ext3(Imm8s a) {
        return D("mov", a, "ext3");
    }

    void SetArArp(std::optional<ArArpSettings> ar_arp) {
        this->ar_arp = ar_arp;
    }

private:
    template <typename ArRn>
    std::string DsmArRn(ArRn a) {
        if (ar_arp) {
            return "%r" +
                   std::to_string((ar_arp->ar[a.Index() / 2] >> (13 - 3 * (a.Index() % 2))) & 7);
        }
        return "arrn" + std::to_string(a.Index());
    }

    std::string ConvertArStepAndOffset(u16 v) {
        static const std::array<std::string, 8> step_names{{
            "++0",
            "++1",
            "--1",
            "++s",
            "++2",
            "--2",
            "++2*",
            "--2*",
        }};

        static const std::array<std::string, 4> offset_names{{
            "+0",
            "+1",
            "-1",
            "-1*",
        }};

        return offset_names[v >> 3] + step_names[v & 7];
    }

    template <typename ArStep>
    std::string DsmArStep(ArStep a) {
        if (ar_arp) {
            u16 s = (ar_arp->ar[a.Index() / 2] >> (5 - 5 * (a.Index() % 2))) & 31;
            return ConvertArStepAndOffset(s);
        }
        return "+ars" + std::to_string(a.Index());
    }

    template <typename ArpRn>
    std::string DsmArpRni(ArpRn a) {
        if (ar_arp) {
            return "%r" + std::to_string((ar_arp->arp[a.Index()] >> 10) & 3);
        }
        return "arprni" + std::to_string(a.Index());
    }

    template <typename ArpStep>
    std::string DsmArpStepi(ArpStep a) {
        if (ar_arp) {
            u16 s = ar_arp->arp[a.Index()] & 31;
            return ConvertArStepAndOffset(s);
        }
        return "+arpsi" + std::to_string(a.Index());
    }

    template <typename ArpRn>
    std::string DsmArpRnj(ArpRn a) {
        if (ar_arp) {
            return "%r" + std::to_string(((ar_arp->arp[a.Index()] >> 13) & 3) + 4);
        }
        return "arprnj" + std::to_string(a.Index());
    }

    template <typename ArpStep>
    std::string DsmArpStepj(ArpStep a) {
        if (ar_arp) {
            u16 s = (ar_arp->arp[a.Index()] >> 5) & 31;
            return ConvertArStepAndOffset(s);
        }
        return "+arpsj" + std::to_string(a.Index());
    }

    template <typename ArRn, typename ArStep>
    std::string MemARS(ArRn reg, ArStep step) {
        return "[" + DsmArRn(reg) + DsmArStep(step) + "]";
    }

    template <typename ArpRn, typename ArpStep>
    std::string MemARPSI(ArpRn reg, ArpStep step) {
        return "[" + DsmArpRni(reg) + DsmArpStepi(step) + "]";
    }

    template <typename ArpRn, typename ArpStep>
    std::string MemARPSJ(ArpRn reg, ArpStep step) {
        return "[" + DsmArpRnj(reg) + DsmArpStepj(step) + "]";
    }

    template <typename ArRn>
    std::string MemAR(ArRn reg) {
        return "[" + DsmArRn(reg) + "]";
    }

    std::optional<ArArpSettings> ar_arp;
};

bool NeedExpansion(std::uint16_t opcode) {
    auto decoder = Decode<Disassembler>(opcode);
    return decoder.NeedExpansion();
}

std::vector<std::string> GetTokenList(std::uint16_t opcode, std::uint16_t expansion,
                                      std::optional<ArArpSettings> ar_arp) {
    Disassembler dsm;
    dsm.SetArArp(ar_arp);
    auto decoder = Decode<Disassembler>(opcode);
    auto v = decoder.call(dsm, opcode, expansion);
    return v;
}

std::string Do(std::uint16_t opcode, std::uint16_t expansion, std::optional<ArArpSettings> ar_arp) {
    auto v = GetTokenList(opcode, expansion, ar_arp);
    std::string last = v.back();
    std::string result;
    v.pop_back();
    for (const auto& s : v) {
        result += s + "    ";
    }
    return result + last;
}

} // namespace Teakra::Disassembler
