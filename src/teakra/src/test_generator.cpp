#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <random>
#include <unordered_set>
#include "common_types.h"
#include "decoder.h"
#include "operand.h"
#include "test.h"

namespace Teakra::Test {

enum class RegConfig { Any = 0, Memory = 1 };

enum class ExpandConfig {
    None = 0, // or Zero
    Any = 1,
    Memory = 2,
};

namespace Random {
namespace {
std::mt19937 gen(std::random_device{}());

u64 uniform(u64 a, u64 b) {
    std::uniform_int_distribution<u64> dist(a, b);
    return dist(gen);
}

u64 bit40() {
    static std::uniform_int_distribution<u64> dist(0, 0xFF'FFFF'FFFF);
    static std::uniform_int_distribution<int> dist2(0, 4);
    u64 v = dist(gen);
    switch (dist2(gen)) {
    case 0:
        v = SignExtend<40>(v);
        break;
    case 1:
        v &= 0xFFFF;
        break;
    case 2:
        v &= 0xFFFF'FFFF;
        break;
    case 3:
        v = SignExtend<32>(v);
        break;
    case 4:
        v = SignExtend<16>(v);
        break;
    }
    return v;
}

u32 bit32() {
    static std::uniform_int_distribution<u32> dist;
    return dist(gen);
}

u16 bit16() {
    static std::uniform_int_distribution<u16> dist;
    return dist(gen);
}
} // Anonymous namespace
} // namespace Random

namespace {
struct Config {
    bool enable = false;
    bool lock_page = false;
    bool lock_r7 = false;
    std::array<RegConfig, 8> r{};
    std::array<RegConfig, 4> ar{};
    std::array<RegConfig, 4> arp{};

    ExpandConfig expand = ExpandConfig::None;

    Config WithAnyExpand() {
        Config copy = *this;
        copy.expand = ExpandConfig::Any;
        return copy;
    }

    Config WithMemoryExpand() {
        Config copy = *this;
        copy.expand = ExpandConfig::Memory;
        return copy;
    }

    State GenerateRandomState() {
        State state;
        state.stepi0 = Random::bit16();
        state.stepj0 = Random::bit16();
        state.mixp = Random::bit16();
        state.sv = Random::bit16();
        if (Random::uniform(0, 1) == 1) {
            state.sv &= 128;
            state.sv = SignExtend<7>(state.sv);
        }
        state.repc = Random::bit16();
        state.lc = Random::bit16();
        state.cfgi = Random::bit16();
        state.cfgj = Random::bit16();
        state.stt0 = Random::bit16();
        state.stt1 = Random::bit16();
        state.stt2 = Random::bit16();
        state.mod0 = Random::bit16();
        state.mod1 = Random::bit16();
        state.mod2 = Random::bit16();
        if (lock_page) {
            state.mod1 &= 0xFF00;
            state.mod1 |= TestSpaceX >> 8;
        }
        std::generate(state.ar.begin(), state.ar.end(), Random::bit16);
        std::generate(state.arp.begin(), state.arp.end(), Random::bit16);
        std::generate(state.x.begin(), state.x.end(), Random::bit16);
        std::generate(state.y.begin(), state.y.end(), Random::bit16);
        std::generate(state.p.begin(), state.p.end(), Random::bit32);
        std::generate(state.a.begin(), state.a.end(), Random::bit40);
        std::generate(state.b.begin(), state.b.end(), Random::bit40);
        std::generate(state.r.begin(), state.r.end(), Random::bit16);
        std::generate(state.test_space_x.begin(), state.test_space_x.end(), Random::bit16);
        std::generate(state.test_space_y.begin(), state.test_space_y.end(), Random::bit16);

        auto rp = r;
        for (std::size_t i = 0; i < ar.size(); ++i) {
            if (ar[i] == RegConfig::Memory) {
                rp[(state.ar[i / 2] >> (13 - 3 * (i % 2))) & 7] = RegConfig::Memory;
            }
        }
        for (std::size_t i = 0; i < arp.size(); ++i) {
            if (arp[i] == RegConfig::Memory) {
                rp[(state.arp[i] >> 10) & 3] = RegConfig::Memory;
                rp[((state.arp[i] >> 13) & 3) + 4] = RegConfig::Memory;
            }
        }
        for (std::size_t i = 0; i < rp.size(); ++i) {
            if (rp[i] == RegConfig::Memory) {
                state.r[i] = (i < 4 ? TestSpaceX : TestSpaceY) +
                             (u16)Random::uniform(10, TestSpaceSize - 10);
                if (!((state.mod2 >> i) & 1) && (((state.mod2 >> (i + 8)) & 1))) {
                    state.r[i] = BitReverse(state.r[i]);
                }
            }
        }
        if (lock_r7) {
            state.r[7] = TestSpaceY + (u16)Random::uniform(10, TestSpaceSize - 10);
        }
        return state;
    }
};

Config DisabledConfig{false};
Config AnyConfig{true};

Config ConfigWithAddress(Rn r) {
    Config config{true};
    config.r[r.Index()] = RegConfig::Memory;
    return config;
}

template <typename ArRnX>
Config ConfigWithArAddress(ArRnX r) {
    static_assert(std::is_same_v<ArRnX, ArRn1> || std::is_same_v<ArRnX, ArRn2>);
    Config config{true};
    config.ar[r.Index()] = RegConfig::Memory;
    return config;
}

template <typename ArpRnX>
Config ConfigWithArpAddress(ArpRnX r) {
    static_assert(std::is_same_v<ArpRnX, ArpRn1> || std::is_same_v<ArpRnX, ArpRn2>);
    Config config{true};
    config.arp[r.Index()] = RegConfig::Memory;
    return config;
}

Config ConfigWithImm8(Imm8 imm) {
    std::unordered_set<u16> random_pos = {
        0,
        35,
        0xE3,
    };
    if (random_pos.count(imm.Unsigned16())) {
        return AnyConfig;
    } else {
        return DisabledConfig;
    }
}

Config ConfigWithMemImm8(MemImm8 mem) {
    std::unordered_set<u16> random_pos = {
        0,
        0x88,
        0xFF,
    };
    if (random_pos.count(mem.Unsigned16())) {
        Config c = AnyConfig;
        c.lock_page = true;
        return c;
    } else {
        return DisabledConfig;
    }
}

Config ConfigWithMemR7Imm16() {
    Config c = AnyConfig;
    c.lock_r7 = true;
    return c;
}

Config ConfigWithMemR7Imm7s(MemR7Imm7s mem) {
    std::unordered_set<u16> random_pos = {
        0,
        4,
        0xFFFE,
    };
    if (random_pos.count(mem.Signed16())) {
        Config c = AnyConfig;
        c.lock_r7 = true;
        return c;
    } else {
        return DisabledConfig;
    }
}

class TestGenerator {
public:
    bool IsUnimplementedRegister(RegName r) {
        return r == RegName::pc || r == RegName::undefine || r == RegName::st0 ||
               r == RegName::st1 || r == RegName::st2 || r == RegName::stt2 || r == RegName::ext0 ||
               r == RegName::ext1 || r == RegName::ext2 || r == RegName::ext3 ||
               r == RegName::mod3 || r == RegName::sp;
    }

    using instruction_return_type = Config;

    Config undefined(u16 opcode) {
        return DisabledConfig;
    }

    Config nop() {
        return DisabledConfig;
    }

    Config norm(Ax a, Rn b, StepZIDS bs) {
        return AnyConfig;
    }
    Config swap(SwapType swap) {
        if (swap.GetName() == SwapTypeValue::reserved0 ||
            swap.GetName() == SwapTypeValue::reserved1)
            return DisabledConfig;
        return AnyConfig;
    }
    Config trap() {
        return DisabledConfig;
    }

    Config alm(Alm op, MemImm8 a, Ax b) {
        if (op.GetName() == AlmOp::Sqr && b.GetName() != RegName::a0) {
            return DisabledConfig;
        }
        return ConfigWithMemImm8(a);
    }
    Config alm(Alm op, Rn a, StepZIDS as, Ax b) {
        if (op.GetName() == AlmOp::Sqr && b.GetName() != RegName::a0) {
            return DisabledConfig;
        }
        return ConfigWithAddress(a);
    }
    Config alm(Alm op, Register a, Ax b) {
        if (op.GetName() == AlmOp::Sqr && b.GetName() != RegName::a0) {
            return DisabledConfig;
        }

        if (IsUnimplementedRegister(a.GetName())) {
            return DisabledConfig;
        }

        switch (a.GetName()) {
        case RegName::p:
        case RegName::a0:
        case RegName::a1: {
            static const std::unordered_set<AlmOp> allowed_instruction{
                AlmOp::Or, AlmOp::And, AlmOp::Xor, AlmOp::Add, AlmOp::Cmp, AlmOp::Sub,
            };
            if (allowed_instruction.count(op.GetName()) != 0)
                return AnyConfig;
            else
                return DisabledConfig;
        }
        default:
            return AnyConfig;
        }
    }
    Config alm_r6(Alm op, Ax b) {
        // op = sqr, b = a1 is excluded by decoder
        return AnyConfig;
    }

    Config alu(Alu op, MemImm16 a, Ax b) {
        // reserved op excluded by decoder
        return AnyConfig.WithMemoryExpand();
    }
    Config alu(Alu op, MemR7Imm16 a, Ax b) {
        return ConfigWithMemR7Imm16();
    }
    Config alu(Alu op, Imm16 a, Ax b) {
        return AnyConfig.WithAnyExpand();
    }
    Config alu(Alu op, Imm8 a, Ax b) {
        return ConfigWithImm8(a);
    }
    Config alu(Alu op, MemR7Imm7s a, Ax b) {
        return ConfigWithMemR7Imm7s(a);
    }

    Config or_(Ab a, Ax b, Ax c) {
        return AnyConfig;
    }
    Config or_(Ax a, Bx b, Ax c) {
        return AnyConfig;
    }
    Config or_(Bx a, Bx b, Ax c) {
        return AnyConfig;
    }

    Config alb(Alb op, Imm16 a, MemImm8 b) {
        return ConfigWithMemImm8(b).WithAnyExpand();
    }
    Config alb(Alb op, Imm16 a, Rn b, StepZIDS bs) {
        return ConfigWithAddress(b).WithAnyExpand();
    }
    Config alb(Alb op, Imm16 a, Register b) {
        if (IsUnimplementedRegister(b.GetName())) {
            return DisabledConfig;
        }
        switch (b.GetName()) {
        case RegName::a0:
        case RegName::a1:
            return DisabledConfig;
        default:
            return AnyConfig.WithAnyExpand();
        }
    }
    Config alb_r6(Alb op, Imm16 a) {
        return AnyConfig.WithAnyExpand();
    }
    Config alb(Alb op, Imm16 a, SttMod b) {
        if (IsUnimplementedRegister(b.GetName())) {
            return DisabledConfig;
        }
        return AnyConfig.WithAnyExpand();
    }

    Config add(Ab a, Bx b) {
        return AnyConfig;
    }
    Config add(Bx a, Ax b) {
        return AnyConfig;
    }
    Config add_p1(Ax b) {
        return AnyConfig;
    }
    Config add(Px a, Bx b) {
        return AnyConfig;
    }

    Config sub(Ab a, Bx b) {
        return AnyConfig;
    }
    Config sub(Bx a, Ax b) {
        return AnyConfig;
    }
    Config sub_p1(Ax b) {
        return AnyConfig;
    }
    Config sub(Px a, Bx b) {
        return AnyConfig;
    }

    Config app(Ab c, SumBase base, bool sub_p0, bool p0_align, bool sub_p1, bool p1_align) {
        return AnyConfig;
    }

    Config add_add(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return ConfigWithArpAddress(a);
    }
    Config add_sub(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return ConfigWithArpAddress(a);
    }
    Config sub_add(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return ConfigWithArpAddress(a);
    }
    Config sub_sub(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return ConfigWithArpAddress(a);
    }

    Config add_sub_sv(ArRn1 a, ArStep1 as, Ab b) {
        return ConfigWithArAddress(a);
    }
    Config sub_add_sv(ArRn1 a, ArStep1 as, Ab b) {
        return ConfigWithArAddress(a);
    }

    Config sub_add_i_mov_j_sv(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return ConfigWithArpAddress(a);
    }
    Config sub_add_j_mov_i_sv(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return ConfigWithArpAddress(a);
    }
    Config add_sub_i_mov_j(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return ConfigWithArpAddress(a);
    }
    Config add_sub_j_mov_i(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return ConfigWithArpAddress(a);
    }

    Config moda4(Moda4 op, Ax a, Cond cond) {
        // reserved op excluded by decoder
        return AnyConfig;
    }

    Config moda3(Moda3 op, Bx a, Cond cond) {
        return AnyConfig;
    }

    Config pacr1(Ax a) {
        return AnyConfig;
    }
    Config clr(Ab a, Ab b) {
        return AnyConfig;
    }
    Config clrr(Ab a, Ab b) {
        return AnyConfig;
    }

    Config bkrep(Imm8 a, Address16 addr) {
        return DisabledConfig;
    }
    Config bkrep(Register a, Address18_16 addr_low, Address18_2 addr_high) {
        return DisabledConfig;
    }
    Config bkrep_r6(Address18_16 addr_low, Address18_2 addr_high) {
        return DisabledConfig;
    }
    Config bkreprst(ArRn2 a) {
        return DisabledConfig;
    }
    Config bkreprst_memsp() {
        return DisabledConfig;
    }
    Config bkrepsto(ArRn2 a) {
        return DisabledConfig;
    }
    Config bkrepsto_memsp() {
        return DisabledConfig;
    }

    Config banke(BankFlags flags) {
        return DisabledConfig;
    }
    Config bankr() {
        return DisabledConfig;
    }
    Config bankr(Ar a) {
        return DisabledConfig;
    }
    Config bankr(Ar a, Arp b) {
        return DisabledConfig;
    }
    Config bankr(Arp a) {
        return DisabledConfig;
    }

    Config bitrev(Rn a) {
        return AnyConfig;
    }
    Config bitrev_dbrv(Rn a) {
        return AnyConfig;
    }
    Config bitrev_ebrv(Rn a) {
        return AnyConfig;
    }

    Config br(Address18_16 addr_low, Address18_2 addr_high, Cond cond) {
        return DisabledConfig;
    }

    Config brr(RelAddr7 addr, Cond cond) {
        return DisabledConfig;
    }

    Config break_() {
        return DisabledConfig;
    }

    Config call(Address18_16 addr_low, Address18_2 addr_high, Cond cond) {
        return DisabledConfig;
    }
    Config calla(Axl a) {
        return DisabledConfig;
    }
    Config calla(Ax a) {
        return DisabledConfig;
    }
    Config callr(RelAddr7 addr, Cond cond) {
        return DisabledConfig;
    }

    Config cntx_s() {
        return DisabledConfig;
    }
    Config cntx_r() {
        return DisabledConfig;
    }

    Config ret(Cond c) {
        return DisabledConfig;
    }
    Config retd() {
        return DisabledConfig;
    }
    Config reti(Cond c) {
        return DisabledConfig;
    }
    Config retic(Cond c) {
        return DisabledConfig;
    }
    Config retid() {
        return DisabledConfig;
    }
    Config retidc() {
        return DisabledConfig;
    }
    Config rets(Imm8 a) {
        return DisabledConfig;
    }

    Config load_ps(Imm2 a) {
        return AnyConfig;
    }
    Config load_stepi(Imm7s a) {
        return AnyConfig;
    }
    Config load_stepj(Imm7s a) {
        return AnyConfig;
    }
    Config load_page(Imm8 a) {
        return AnyConfig;
    }
    Config load_modi(Imm9 a) {
        return AnyConfig;
    }
    Config load_modj(Imm9 a) {
        return AnyConfig;
    }
    Config load_movpd(Imm2 a) {
        return DisabledConfig;
    }
    Config load_ps01(Imm4 a) {
        return AnyConfig;
    }

    Config push(Imm16 a) {
        return DisabledConfig;
    }
    Config push(Register a) {
        return DisabledConfig;
    }
    Config push(Abe a) {
        return DisabledConfig;
    }
    Config push(ArArpSttMod a) {
        return DisabledConfig;
    }
    Config push_prpage() {
        return DisabledConfig;
    }
    Config push(Px a) {
        return DisabledConfig;
    }
    Config push_r6() {
        return DisabledConfig;
    }
    Config push_repc() {
        return DisabledConfig;
    }
    Config push_x0() {
        return DisabledConfig;
    }
    Config push_x1() {
        return DisabledConfig;
    }
    Config push_y1() {
        return DisabledConfig;
    }
    Config pusha(Ax a) {
        return DisabledConfig;
    }
    Config pusha(Bx a) {
        return DisabledConfig;
    }

    Config pop(Register a) {
        return DisabledConfig;
    }
    Config pop(Abe a) {
        return DisabledConfig;
    }
    Config pop(ArArpSttMod a) {
        return DisabledConfig;
    }
    Config pop(Bx a) {
        return DisabledConfig;
    }
    Config pop_prpage() {
        return DisabledConfig;
    }
    Config pop(Px a) {
        return DisabledConfig;
    }
    Config pop_r6() {
        return DisabledConfig;
    }
    Config pop_repc() {
        return DisabledConfig;
    }
    Config pop_x0() {
        return DisabledConfig;
    }
    Config pop_x1() {
        return DisabledConfig;
    }
    Config pop_y1() {
        return DisabledConfig;
    }
    Config popa(Ab a) {
        return DisabledConfig;
    }

    Config rep(Imm8 a) {
        return DisabledConfig;
    }
    Config rep(Register a) {
        return DisabledConfig;
    }
    Config rep_r6() {
        return DisabledConfig;
    }

    Config shfc(Ab a, Ab b, Cond cond) {
        return AnyConfig;
    }
    Config shfi(Ab a, Ab b, Imm6s s) {
        return AnyConfig;
    }

    Config tst4b(ArRn2 b, ArStep2 bs) {
        return ConfigWithArAddress(b);
    }
    Config tst4b(ArRn2 b, ArStep2 bs, Ax c) {
        return ConfigWithArAddress(b);
    }
    Config tstb(MemImm8 a, Imm4 b) {
        return ConfigWithMemImm8(a);
    }
    Config tstb(Rn a, StepZIDS as, Imm4 b) {
        return ConfigWithAddress(a);
    }
    Config tstb(Register a, Imm4 b) {
        if (IsUnimplementedRegister(a.GetName()))
            return DisabledConfig;
        return AnyConfig;
    }
    Config tstb_r6(Imm4 b) {
        return AnyConfig;
    }
    Config tstb(SttMod a, Imm16 b) {
        // TODO deal with the expansion
        return DisabledConfig;
    }

    Config and_(Ab a, Ab b, Ax c) {
        return AnyConfig;
    }

    Config dint() {
        return DisabledConfig;
    }
    Config eint() {
        return DisabledConfig;
    }

    Config mul(Mul3 op, Rn y, StepZIDS ys, Imm16 x, Ax a) {
        return ConfigWithAddress(y);
    }
    Config mul_y0(Mul3 op, Rn x, StepZIDS xs, Ax a) {
        return ConfigWithAddress(x);
    }
    Config mul_y0(Mul3 op, Register x, Ax a) {
        if (IsUnimplementedRegister(x.GetName()))
            return DisabledConfig;
        return AnyConfig;
    }
    Config mul(Mul3 op, R45 y, StepZIDS ys, R0123 x, StepZIDS xs, Ax a) {
        return DisabledConfig;
    }
    Config mul_y0_r6(Mul3 op, Ax a) {
        return AnyConfig;
    }
    Config mul_y0(Mul2 op, MemImm8 x, Ax a) {
        return ConfigWithMemImm8(x);
    }

    Config mpyi(Imm8s x) {
        return AnyConfig;
    }

    Config msu(R45 y, StepZIDS ys, R0123 x, StepZIDS xs, Ax a) {
        return DisabledConfig;
    }
    Config msu(Rn y, StepZIDS ys, Imm16 x, Ax a) {
        return ConfigWithAddress(y).WithAnyExpand();
    }
    Config msusu(ArRn2 x, ArStep2 xs, Ax a) {
        return ConfigWithArAddress(x);
    }
    Config mac_x1to0(Ax a) {
        return AnyConfig;
    }
    Config mac1(ArpRn1 xy, ArpStep1 xis, ArpStep1 yjs, Ax a) {
        return ConfigWithArpAddress(xy);
    }

    Config modr(Rn a, StepZIDS as) {
        return AnyConfig;
    }
    Config modr_dmod(Rn a, StepZIDS as) {
        return AnyConfig;
    }
    Config modr_i2(Rn a) {
        return AnyConfig;
    }
    Config modr_i2_dmod(Rn a) {
        return AnyConfig;
    }
    Config modr_d2(Rn a) {
        return AnyConfig;
    }
    Config modr_d2_dmod(Rn a) {
        return AnyConfig;
    }
    Config modr_eemod(ArpRn2 a, ArpStep2 asi, ArpStep2 asj) {
        return AnyConfig;
    }
    Config modr_edmod(ArpRn2 a, ArpStep2 asi, ArpStep2 asj) {
        return AnyConfig;
    }
    Config modr_demod(ArpRn2 a, ArpStep2 asi, ArpStep2 asj) {
        return AnyConfig;
    }
    Config modr_ddmod(ArpRn2 a, ArpStep2 asi, ArpStep2 asj) {
        return AnyConfig;
    }

    Config movd(R0123 a, StepZIDS as, R45 b, StepZIDS bs) {
        return DisabledConfig;
    }
    Config movp(Axl a, Register b) {
        return DisabledConfig;
    }
    Config movp(Ax a, Register b) {
        return DisabledConfig;
    }
    Config movp(Rn a, StepZIDS as, R0123 b, StepZIDS bs) {
        return DisabledConfig;
    }
    Config movpdw(Ax a) {
        return DisabledConfig;
    }

    Config mov(Ab a, Ab b) {
        return AnyConfig;
    }
    Config mov_dvm(Abl a) {
        return DisabledConfig;
    }
    Config mov_x0(Abl a) {
        return AnyConfig;
    }
    Config mov_x1(Abl a) {
        return AnyConfig;
    }
    Config mov_y1(Abl a) {
        return AnyConfig;
    }
    Config mov(Ablh a, MemImm8 b) {
        return ConfigWithMemImm8(b);
    }
    Config mov(Axl a, MemImm16 b) {
        return AnyConfig.WithMemoryExpand();
    }
    Config mov(Axl a, MemR7Imm16 b) {
        return ConfigWithMemR7Imm16();
    }
    Config mov(Axl a, MemR7Imm7s b) {
        return ConfigWithMemR7Imm7s(b);
    }
    Config mov(MemImm16 a, Ax b) {
        return AnyConfig.WithMemoryExpand();
    }
    Config mov(MemImm8 a, Ab b) {
        return ConfigWithMemImm8(a);
    }
    Config mov(MemImm8 a, Ablh b) {
        return ConfigWithMemImm8(a);
    }
    Config mov_eu(MemImm8 a, Axh b) {
        return DisabledConfig;
    }
    Config mov(MemImm8 a, RnOld b) {
        return ConfigWithMemImm8(a);
    }
    Config mov_sv(MemImm8 a) {
        return ConfigWithMemImm8(a);
    }
    Config mov_dvm_to(Ab b) {
        return DisabledConfig;
    }
    Config mov_icr_to(Ab b) {
        return DisabledConfig;
    }
    Config mov(Imm16 a, Bx b) {
        return AnyConfig.WithAnyExpand();
    }
    Config mov(Imm16 a, Register b) {
        if (IsUnimplementedRegister(b.GetName()))
            return DisabledConfig;
        return AnyConfig.WithAnyExpand();
    }
    Config mov_icr(Imm5 a) {
        return DisabledConfig;
    }
    Config mov(Imm8s a, Axh b) {
        return AnyConfig;
    }
    Config mov(Imm8s a, RnOld b) {
        return AnyConfig;
    }
    Config mov_sv(Imm8s a) {
        return AnyConfig;
    }
    Config mov(Imm8 a, Axl b) {
        return AnyConfig;
    }
    Config mov(MemR7Imm16 a, Ax b) {
        return ConfigWithMemR7Imm16();
    }
    Config mov(MemR7Imm7s a, Ax b) {
        return ConfigWithMemR7Imm7s(a);
    }
    Config mov(Rn a, StepZIDS as, Bx b) {
        return ConfigWithAddress(a);
    }
    Config mov(Rn a, StepZIDS as, Register b) {
        if (IsUnimplementedRegister(b.GetName()))
            return DisabledConfig;
        return ConfigWithAddress(a);
    }
    Config mov_memsp_to(Register b) {
        return DisabledConfig;
    }
    Config mov_mixp_to(Register b) {
        if (IsUnimplementedRegister(b.GetName()))
            return DisabledConfig;
        return AnyConfig;
    }
    Config mov(RnOld a, MemImm8 b) {
        return ConfigWithMemImm8(b);
    }
    Config mov_icr(Register a) {
        return DisabledConfig;
    }
    Config mov_mixp(Register a) {
        if (IsUnimplementedRegister(a.GetName()))
            return DisabledConfig;
        return AnyConfig;
    }
    Config mov(Register a, Rn b, StepZIDS bs) {
        if (IsUnimplementedRegister(a.GetName()))
            return DisabledConfig;
        return ConfigWithAddress(b);
    }
    Config mov(Register a, Bx b) {
        if (IsUnimplementedRegister(a.GetName()))
            return DisabledConfig;
        return AnyConfig;
    }
    Config mov(Register a, Register b) {
        if (IsUnimplementedRegister(a.GetName()))
            return DisabledConfig;
        if (IsUnimplementedRegister(b.GetName()))
            return DisabledConfig;
        return AnyConfig;
    }
    Config mov_repc_to(Ab b) {
        return AnyConfig;
    }
    Config mov_sv_to(MemImm8 b) {
        return ConfigWithMemImm8(b);
    }
    Config mov_x0_to(Ab b) {
        return AnyConfig;
    }
    Config mov_x1_to(Ab b) {
        return AnyConfig;
    }
    Config mov_y1_to(Ab b) {
        return AnyConfig;
    }
    Config mov(Imm16 a, ArArp b) {
        if (IsUnimplementedRegister(b.GetName())) {
            return DisabledConfig;
        }
        return AnyConfig.WithAnyExpand();
    }
    Config mov_r6(Imm16 a) {
        return AnyConfig.WithAnyExpand();
    }
    Config mov_repc(Imm16 a) {
        return AnyConfig.WithAnyExpand();
    }
    Config mov_stepi0(Imm16 a) {
        return AnyConfig.WithAnyExpand();
    }
    Config mov_stepj0(Imm16 a) {
        return AnyConfig.WithAnyExpand();
    }
    Config mov(Imm16 a, SttMod b) {
        if (IsUnimplementedRegister(b.GetName())) {
            return DisabledConfig;
        }
        return AnyConfig.WithAnyExpand();
    }
    Config mov_prpage(Imm4 a) {
        return DisabledConfig;
    }

    Config mov_a0h_stepi0() {
        return AnyConfig;
    }
    Config mov_a0h_stepj0() {
        return AnyConfig;
    }
    Config mov_stepi0_a0h() {
        return AnyConfig;
    }
    Config mov_stepj0_a0h() {
        return AnyConfig;
    }

    Config mov_prpage(Abl a) {
        return DisabledConfig;
    }
    Config mov_repc(Abl a) {
        return AnyConfig;
    }
    Config mov(Abl a, ArArp b) {
        if (IsUnimplementedRegister(b.GetName())) {
            return DisabledConfig;
        }
        return AnyConfig;
    }
    Config mov(Abl a, SttMod b) {
        if (IsUnimplementedRegister(b.GetName())) {
            return DisabledConfig;
        }
        return AnyConfig;
    }

    Config mov_prpage_to(Abl b) {
        return DisabledConfig;
    }
    Config mov_repc_to(Abl b) {
        return AnyConfig;
    }
    Config mov(ArArp a, Abl b) {
        if (IsUnimplementedRegister(a.GetName())) {
            return DisabledConfig;
        }
        return AnyConfig;
    }
    Config mov(SttMod a, Abl b) {
        if (IsUnimplementedRegister(a.GetName())) {
            return DisabledConfig;
        }
        return AnyConfig;
    }

    Config mov_repc_to(ArRn1 b, ArStep1 bs) {
        return ConfigWithArAddress(b);
    }
    Config mov(ArArp a, ArRn1 b, ArStep1 bs) {
        if (IsUnimplementedRegister(a.GetName())) {
            return DisabledConfig;
        }
        return ConfigWithArAddress(b);
    }
    Config mov(SttMod a, ArRn1 b, ArStep1 bs) {
        if (IsUnimplementedRegister(a.GetName())) {
            return DisabledConfig;
        }
        return ConfigWithArAddress(b);
    }

    Config mov_repc(ArRn1 a, ArStep1 as) {
        return ConfigWithArAddress(a);
    }
    Config mov(ArRn1 a, ArStep1 as, ArArp b) {
        if (IsUnimplementedRegister(b.GetName())) {
            return DisabledConfig;
        }
        return ConfigWithArAddress(a);
    }
    Config mov(ArRn1 a, ArStep1 as, SttMod b) {
        if (IsUnimplementedRegister(b.GetName())) {
            return DisabledConfig;
        }
        return ConfigWithArAddress(a);
    }

    Config mov_repc_to(MemR7Imm16 b) {
        return ConfigWithMemR7Imm16();
    }
    Config mov(ArArpSttMod a, MemR7Imm16 b) {
        if (IsUnimplementedRegister(a.GetName())) {
            return DisabledConfig;
        }
        return ConfigWithMemR7Imm16();
    }

    Config mov_repc(MemR7Imm16 a) {
        return ConfigWithMemR7Imm16();
    }
    Config mov(MemR7Imm16 a, ArArpSttMod b) {
        if (IsUnimplementedRegister(b.GetName())) {
            return DisabledConfig;
        }
        return ConfigWithMemR7Imm16();
    }

    Config mov_pc(Ax a) {
        return DisabledConfig;
    }
    Config mov_pc(Bx a) {
        return DisabledConfig;
    }

    Config mov_mixp_to(Bx b) {
        return AnyConfig;
    }
    Config mov_mixp_r6() {
        return AnyConfig;
    }
    Config mov_p0h_to(Bx b) {
        return AnyConfig;
    }
    Config mov_p0h_r6() {
        return AnyConfig;
    }
    Config mov_p0h_to(Register b) {
        if (IsUnimplementedRegister(b.GetName()))
            return DisabledConfig;
        return AnyConfig;
    }
    Config mov_p0(Ab a) {
        return AnyConfig;
    }
    Config mov_p1_to(Ab b) {
        return AnyConfig;
    }

    Config mov2(Px a, ArRn2 b, ArStep2 bs) {
        return ConfigWithArAddress(b);
    }
    Config mov2s(Px a, ArRn2 b, ArStep2 bs) {
        return ConfigWithArAddress(b);
    }
    Config mov2(ArRn2 a, ArStep2 as, Px b) {
        return ConfigWithArAddress(a);
    }
    Config mova(Ab a, ArRn2 b, ArStep2 bs) {
        return ConfigWithArAddress(b);
    }
    Config mova(ArRn2 a, ArStep2 as, Ab b) {
        return ConfigWithArAddress(a);
    }

    Config mov_r6_to(Bx b) {
        return AnyConfig;
    }
    Config mov_r6_mixp() {
        return AnyConfig;
    }
    Config mov_r6_to(Register b) {
        if (IsUnimplementedRegister(b.GetName()))
            return DisabledConfig;
        return AnyConfig;
    }
    Config mov_r6(Register a) {
        if (IsUnimplementedRegister(a.GetName()))
            return DisabledConfig;
        return AnyConfig;
    }
    Config mov_memsp_r6() {
        return DisabledConfig;
    }
    Config mov_r6_to(Rn b, StepZIDS bs) {
        return ConfigWithAddress(b);
    }
    Config mov_r6(Rn a, StepZIDS as) {
        return ConfigWithAddress(a);
    }

    Config movs(MemImm8 a, Ab b) {
        return ConfigWithMemImm8(a);
    }
    Config movs(Rn a, StepZIDS as, Ab b) {
        return ConfigWithAddress(a);
    }
    Config movs(Register a, Ab b) {
        if (IsUnimplementedRegister(a.GetName()))
            return DisabledConfig;
        return AnyConfig;
    }
    Config movs_r6_to(Ax b) {
        return AnyConfig;
    }
    Config movsi(RnOld a, Ab b, Imm5s s) {
        return AnyConfig;
    }

    Config mov2_axh_m_y0_m(Axh a, ArRn2 b, ArStep2 bs) {
        return ConfigWithArAddress(b);
    }
    Config mov2_ax_mij(Ab a, ArpRn1 b, ArpStep1 bsi, ArpStep1 bsj) {
        return ConfigWithArpAddress(b);
    }
    Config mov2_ax_mji(Ab a, ArpRn1 b, ArpStep1 bsi, ArpStep1 bsj) {
        return ConfigWithArpAddress(b);
    }
    Config mov2_mij_ax(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return ConfigWithArpAddress(a);
    }
    Config mov2_mji_ax(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, Ab b) {
        return ConfigWithArpAddress(a);
    }
    Config mov2_abh_m(Abh ax, Abh ay, ArRn1 b, ArStep1 bs) {
        return ConfigWithArAddress(b);
    }
    Config exchange_iaj(Axh a, ArpRn2 b, ArpStep2 bsi, ArpStep2 bsj) {
        return ConfigWithArpAddress(b);
    }
    Config exchange_riaj(Axh a, ArpRn2 b, ArpStep2 bsi, ArpStep2 bsj) {
        return ConfigWithArpAddress(b);
    }
    Config exchange_jai(Axh a, ArpRn2 b, ArpStep2 bsi, ArpStep2 bsj) {
        return ConfigWithArpAddress(b);
    }
    Config exchange_rjai(Axh a, ArpRn2 b, ArpStep2 bsi, ArpStep2 bsj) {
        return ConfigWithArpAddress(b);
    }

    Config movr(ArRn2 a, ArStep2 as, Abh b) {
        return ConfigWithArAddress(a);
    }
    Config movr(Rn a, StepZIDS as, Ax b) {
        return ConfigWithAddress(a);
    }
    Config movr(Register a, Ax b) {
        if (IsUnimplementedRegister(a.GetName()))
            return DisabledConfig;
        return AnyConfig;
    }
    Config movr(Bx a, Ax b) {
        return AnyConfig;
    }
    Config movr_r6_to(Ax b) {
        return AnyConfig;
    }

    Config exp(Bx a) {
        return AnyConfig;
    }
    Config exp(Bx a, Ax b) {
        return AnyConfig;
    }
    Config exp(Rn a, StepZIDS as) {
        return ConfigWithAddress(a);
    }
    Config exp(Rn a, StepZIDS as, Ax b) {
        return ConfigWithAddress(a);
    }
    Config exp(Register a) {
        if (IsUnimplementedRegister(a.GetName()))
            return DisabledConfig;
        return AnyConfig;
    }
    Config exp(Register a, Ax b) {
        if (IsUnimplementedRegister(a.GetName()))
            return DisabledConfig;
        return AnyConfig;
    }
    Config exp_r6() {
        return AnyConfig;
    }
    Config exp_r6(Ax b) {
        return AnyConfig;
    }

    Config lim(Ax a, Ax b) {
        return AnyConfig;
    }

    Config vtrclr0() {
        return DisabledConfig;
    }
    Config vtrclr1() {
        return DisabledConfig;
    }
    Config vtrclr() {
        return DisabledConfig;
    }
    Config vtrmov0(Axl a) {
        return DisabledConfig;
    }
    Config vtrmov1(Axl a) {
        return DisabledConfig;
    }
    Config vtrmov(Axl a) {
        return DisabledConfig;
    }
    Config vtrshr() {
        return DisabledConfig;
    }

    Config clrp0() {
        return AnyConfig;
    }
    Config clrp1() {
        return AnyConfig;
    }
    Config clrp() {
        return AnyConfig;
    }

    Config max_ge(Ax a, StepZIDS bs) {
        return AnyConfig;
    }
    Config max_gt(Ax a, StepZIDS bs) {
        return AnyConfig;
    }
    Config min_le(Ax a, StepZIDS bs) {
        return AnyConfig;
    }
    Config min_lt(Ax a, StepZIDS bs) {
        return AnyConfig;
    }

    Config max_ge_r0(Ax a, StepZIDS bs) {
        return ConfigWithAddress(Rn{0});
    }
    Config max_gt_r0(Ax a, StepZIDS bs) {
        return ConfigWithAddress(Rn{0});
    }
    Config min_le_r0(Ax a, StepZIDS bs) {
        return ConfigWithAddress(Rn{0});
    }
    Config min_lt_r0(Ax a, StepZIDS bs) {
        return ConfigWithAddress(Rn{0});
    }
    Config divs(MemImm8 a, Ax b) {
        return ConfigWithMemImm8(a);
    }

    Config sqr_sqr_add3(Ab a, Ab b) {
        return AnyConfig;
    }
    Config sqr_sqr_add3(ArRn2 a, ArStep2 as, Ab b) {
        return ConfigWithArAddress(a);
    }
    Config sqr_mpysu_add3a(Ab a, Ab b) {
        return AnyConfig;
    }

    Config cmp(Ax a, Bx b) {
        return AnyConfig;
    }
    Config cmp_b0_b1() {
        return AnyConfig;
    }
    Config cmp_b1_b0() {
        return AnyConfig;
    }
    Config cmp(Bx a, Ax b) {
        return AnyConfig;
    }
    Config cmp_p1_to(Ax b) {
        return AnyConfig;
    }

    Config max2_vtr(Ax a) {
        return AnyConfig;
    }
    Config min2_vtr(Ax a) {
        return AnyConfig;
    }
    Config max2_vtr(Ax a, Bx b) {
        return AnyConfig;
    }
    Config min2_vtr(Ax a, Bx b) {
        return AnyConfig;
    }
    Config max2_vtr_movl(Ax a, Bx b, ArRn1 c, ArStep1 cs) {
        return ConfigWithArAddress(c);
    }
    Config max2_vtr_movh(Ax a, Bx b, ArRn1 c, ArStep1 cs) {
        return ConfigWithArAddress(c);
    }
    Config max2_vtr_movl(Bx a, Ax b, ArRn1 c, ArStep1 cs) {
        return ConfigWithArAddress(c);
    }
    Config max2_vtr_movh(Bx a, Ax b, ArRn1 c, ArStep1 cs) {
        return ConfigWithArAddress(c);
    }
    Config min2_vtr_movl(Ax a, Bx b, ArRn1 c, ArStep1 cs) {
        return ConfigWithArAddress(c);
    }
    Config min2_vtr_movh(Ax a, Bx b, ArRn1 c, ArStep1 cs) {
        return ConfigWithArAddress(c);
    }
    Config min2_vtr_movl(Bx a, Ax b, ArRn1 c, ArStep1 cs) {
        return ConfigWithArAddress(c);
    }
    Config min2_vtr_movh(Bx a, Ax b, ArRn1 c, ArStep1 cs) {
        return ConfigWithArAddress(c);
    }
    Config max2_vtr_movij(Ax a, Bx b, ArpRn1 c, ArpStep1 csi, ArpStep1 csj) {
        return ConfigWithArpAddress(c);
    }
    Config max2_vtr_movji(Ax a, Bx b, ArpRn1 c, ArpStep1 csi, ArpStep1 csj) {
        return ConfigWithArpAddress(c);
    }
    Config min2_vtr_movij(Ax a, Bx b, ArpRn1 c, ArpStep1 csi, ArpStep1 csj) {
        return ConfigWithArpAddress(c);
    }
    Config min2_vtr_movji(Ax a, Bx b, ArpRn1 c, ArpStep1 csi, ArpStep1 csj) {
        return ConfigWithArpAddress(c);
    }

    template <typename ArpStepX>
    Config mov_sv_app(ArRn1 a, ArpStepX as, Bx b, SumBase base, bool sub_p0, bool p0_align,
                      bool sub_p1, bool p1_align) {
        return ConfigWithArAddress(a);
    }

    Config cbs(Axh a, CbsCond c) {
        // return AnyConfig;
        return DisabledConfig;
    }
    Config cbs(Axh a, Bxh b, CbsCond c) {
        // return AnyConfig;
        return DisabledConfig;
    }
    Config cbs(ArpRn1 a, ArpStep1 asi, ArpStep1 asj, CbsCond c) {
        // return ConfigWithArpAddress(a);
        return DisabledConfig;
    }

    Config mma(RegName a, bool x0_sign, bool y0_sign, bool x1_sign, bool y1_sign, SumBase base,
               bool sub_p0, bool p0_align, bool sub_p1, bool p1_align) {
        return AnyConfig;
    }

    template <typename ArpRnX, typename ArpStepX>
    Config mma(ArpRnX xy, ArpStepX i, ArpStepX j, bool dmodi, bool dmodj, RegName a, bool x0_sign,
               bool y0_sign, bool x1_sign, bool y1_sign, SumBase base, bool sub_p0, bool p0_align,
               bool sub_p1, bool p1_align) {
        return ConfigWithArpAddress(xy);
    }

    Config mma_mx_xy(ArRn1 y, ArStep1 ys, RegName a, bool x0_sign, bool y0_sign, bool x1_sign,
                     bool y1_sign, SumBase base, bool sub_p0, bool p0_align, bool sub_p1,
                     bool p1_align) {
        return ConfigWithArAddress(y);
    }

    Config mma_xy_mx(ArRn1 y, ArStep1 ys, RegName a, bool x0_sign, bool y0_sign, bool x1_sign,
                     bool y1_sign, SumBase base, bool sub_p0, bool p0_align, bool sub_p1,
                     bool p1_align) {
        return ConfigWithArAddress(y);
    }

    Config mma_my_my(ArRn1 x, ArStep1 xs, RegName a, bool x0_sign, bool y0_sign, bool x1_sign,
                     bool y1_sign, SumBase base, bool sub_p0, bool p0_align, bool sub_p1,
                     bool p1_align) {
        return ConfigWithArAddress(x);
    }

    Config mma_mov(Axh u, Bxh v, ArRn1 w, ArStep1 ws, RegName a, bool x0_sign, bool y0_sign,
                   bool x1_sign, bool y1_sign, SumBase base, bool sub_p0, bool p0_align,
                   bool sub_p1, bool p1_align) {
        return ConfigWithArAddress(w);
    }

    Config mma_mov(ArRn2 w, ArStep1 ws, RegName a, bool x0_sign, bool y0_sign, bool x1_sign,
                   bool y1_sign, SumBase base, bool sub_p0, bool p0_align, bool sub_p1,
                   bool p1_align) {
        return ConfigWithArAddress(w);
    }

    Config addhp(ArRn2 a, ArStep2 as, Px b, Ax c) {
        return ConfigWithArAddress(a);
    }

    Config mov_ext0(Imm8s a) {
        return DisabledConfig;
    }
    Config mov_ext1(Imm8s a) {
        return DisabledConfig;
    }
    Config mov_ext2(Imm8s a) {
        return DisabledConfig;
    }
    Config mov_ext3(Imm8s a) {
        return DisabledConfig;
    }
};
} // Anonymous namespace

bool GenerateTestCasesToFile(const char* path) {
    std::unique_ptr<std::FILE, decltype(&std::fclose)> f{std::fopen(path, "wb"), std::fclose};
    if (!f) {
        return false;
    }

    TestGenerator generator;
    for (u32 i = 0; i < 0x10000; ++i) {
        u16 opcode = (u16)i;
        auto decoded = Decode<TestGenerator>(opcode);
        Config config = decoded.call(generator, opcode, 0);
        if (!config.enable)
            continue;

        for (int j = 0; j < 4; ++j) {
            TestCase test_case{};
            test_case.before = config.GenerateRandomState();
            test_case.opcode = opcode;

            switch (config.expand) {
            case ExpandConfig::None:
                test_case.expand = 0;
                break;
            case ExpandConfig::Any:
                test_case.expand = Random::bit16();
                break;
            case ExpandConfig::Memory:
                test_case.expand = TestSpaceX + (u16)Random::uniform(10, TestSpaceSize - 10);
                break;
            }

            if (std::fwrite(&test_case, sizeof(test_case), 1, f.get()) == 0) {
                return false;
            }
        }
    }

    return true;
}

} // namespace Teakra::Test
