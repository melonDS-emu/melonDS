#pragma once

#include <algorithm>
#include <array>
#include <memory>
#include <vector>
#include "common_types.h"
#include "crash.h"
#include "operand.h"

namespace Teakra {

struct RegisterState {
    void Reset() {
        *this = RegisterState();
    }

    /** Program control unit **/

    u32 pc = 0;     // 18-bit, program counter
    u16 prpage = 0; // 4-bit, program page
    u16 cpc = 1;    // 1-bit, change word order when push/pop pc

    u16 repc = 0;     // 16-bit rep loop counter
    u16 repcs = 0;    // repc shadow
    bool rep = false; // true when in rep loop
    u16 crep = 1;     // 1-bit. If clear, store/restore repc to shadows on context switch

    u16 bcn = 0; // 3-bit, nest loop counter
    u16 lp = 0;  // 1-bit, set when in a loop

    struct BlockRepeatFrame {
        u32 start = 0;
        u32 end = 0;
        u16 lc = 0;
    };

    std::array<BlockRepeatFrame, 4> bkrep_stack;
    u16& Lc() {
        if (lp)
            return bkrep_stack[bcn - 1].lc;
        return bkrep_stack[0].lc;
    }

    /** Computation unit **/

    // 40-bit 2's comp accumulators.
    // Use 64-bit 2's comp here. The upper 24 bits are always sign extension
    std::array<u64, 2> a{};
    std::array<u64, 2> b{};

    u64 a1s = 0, b1s = 0; // shadows for a1 and b1
    u16 ccnta = 1;        // 1-bit. If clear, store/restore a1/b1 to shadows on context switch

    u16 sat = 0;  // 1-bit, disable saturation when moving from acc
    u16 sata = 1; // 1-bit, disable saturation when moving to acc
    u16 s = 0;    // 1-bit, shift mode. 0 - arithmetic, 1 - logic
    u16 sv = 0;   // 16-bit two's complement shift value

    // 1-bit flags
    u16 fz = 0;  // zero flag
    u16 fm = 0;  // negative flag
    u16 fn = 0;  // normalized flag
    u16 fv = 0;  // overflow flag
    u16 fe = 0;  // extension flag
    u16 fc0 = 0; // carry flag
    u16 fc1 = 0; // another carry flag
    u16 flm = 0; // set on saturation
    u16 fvl = 0; // latching fv
    u16 fr = 0;  // Rn zero flag

    // Viterbi
    u16 vtr0 = 0;
    u16 vtr1 = 0;

    /** Multiplication unit **/

    std::array<u16, 2> x{};  // factor
    std::array<u16, 2> y{};  // factor
    u16 hwm = 0;             // 2-bit, half word mode, modify y on multiplication
    std::array<u32, 2> p{};  // product
    std::array<u16, 2> pe{}; // 1-bit product extension
    std::array<u16, 2> ps{}; // 2-bit, product shift mode
    u16 p0h_cbs = 0;         // 16-bit hidden state for codebook search (CBS) opcode

    /** Address unit **/

    std::array<u16, 8> r{}; // 16-bit general and address registers
    u16 mixp = 0;           // 16-bit, stores result of min/max instructions
    u16 sp = 0;             // 16-bit stack pointer
    u16 page = 0;           // 8-bit, higher part of MemImm8 address
    u16 pcmhi = 0;          // 2-bit, higher part of program address for movp/movd

    // shadows for bank exchange;
    u16 r0b = 0, r1b = 0, r4b = 0, r7b = 0;

    /** Address step/mod unit **/

    // step/modulo
    u16 stepi = 0, stepj = 0;   // 7-bit step
    u16 modi = 0, modj = 0;     // 9-bit mod
    u16 stepi0 = 0, stepj0 = 0; // 16-bit step

    // shadows for bank exchange
    u16 stepib = 0, stepjb = 0;
    u16 modib = 0, modjb = 0;
    u16 stepi0b = 0, stepj0b = 0;

    std::array<u16, 8> m{};  // 1-bit each, enable modulo arithmetic for Rn
    std::array<u16, 8> br{}; // 1-bit each, use bit-reversed value from Rn as address
    u16 stp16 = 0; // 1 bit. If set, stepi0/j0 will be exchanged along with cfgi/j in banke, and use
                   // stepi0/j0 for steping
    u16 cmd = 1;   // 1-bit, step/mod method. 0 - Teak; 1 - TeakLite
    u16 epi = 0;   // 1-bit. If set, cause r3 = 0 when steping r3
    u16 epj = 0;   // 1-bit. If set, cause r7 = 0 when steping r7

    /** Indirect address unit **/

    // 3 bits each
    // 0: +0
    // 1: +1
    // 2: -1
    // 3: +s
    // 4: +2
    // 5: -2
    // 6: +2*
    // 7: -2*
    std::array<u16, 4> arstep{{1, 4, 5, 3}}, arpstepi{{1, 4, 5, 3}}, arpstepj{{1, 4, 5, 3}};

    // 2 bits each
    // 0: +0
    // 1: +1
    // 2: -1
    // 3: -1*
    std::array<u16, 4> aroffset{{0, 1, 2, 0}}, arpoffseti{{0, 1, 2, 0}}, arpoffsetj{{0, 1, 2, 0}};

    // 3 bits each, represent r0~r7
    std::array<u16, 4> arrn{{0, 4, 2, 6}};

    // 2 bits each. for i represent r0~r3, for j represents r4~r7
    std::array<u16, 4> arprni{{0, 1, 2, 3}}, arprnj{{0, 1, 2, 3}};

    /** Interrupt unit **/

    // interrupt pending bit
    std::array<u16, 3> ip{};
    u16 ipv = 0;

    // interrupt enable bit
    std::array<u16, 3> im{};
    u16 imv = 0;

    // interrupt context switching bit
    std::array<u16, 3> ic{};
    u16 nimc = 0;

    // interrupt enable master bit
    u16 ie = 0;

    /** Extension unit **/

    std::array<u16, 5> ou{}; // user output pins
    std::array<u16, 2> iu{}; // user input pins
    std::array<u16, 4> ext{};

    u16 mod0_unk_const = 1; // 3-bit

    /** Shadow registers **/

    template <u16 RegisterState::*origin>
    class ShadowRegister {
    public:
        void Store(RegisterState* self) {
            shadow = self->*origin;
        }
        void Restore(RegisterState* self) {
            self->*origin = shadow;
        }

    private:
        u16 shadow = 0;
    };

    template <std::size_t size, std::array<u16, size> RegisterState::*origin>
    class ShadowArrayRegister {
    public:
        void Store(RegisterState* self) {
            shadow = self->*origin;
        }
        void Restore(RegisterState* self) {
            self->*origin = shadow;
        }

    private:
        std::array<u16, size> shadow{};
    };

    template <typename... ShadowRegisters>
    class ShadowRegisterList : private ShadowRegisters... {
    public:
        void Store(RegisterState* self) {
            (ShadowRegisters::Store(self), ...);
        }
        void Restore(RegisterState* self) {
            (ShadowRegisters::Restore(self), ...);
        }
    };

    template <u16 RegisterState::*origin>
    class ShadowSwapRegister {
    public:
        void Swap(RegisterState* self) {
            std::swap(self->*origin, shadow);
        }

    private:
        u16 shadow = 0;
    };

    template <std::size_t size, std::array<u16, size> RegisterState::*origin>
    class ShadowSwapArrayRegister {
    public:
        void Swap(RegisterState* self) {
            std::swap(self->*origin, shadow);
        }

    private:
        std::array<u16, size> shadow{};
    };

    template <typename... ShadowSwapRegisters>
    class ShadowSwapRegisterList : private ShadowSwapRegisters... {
    public:
        void Swap(RegisterState* self) {
            (ShadowSwapRegisters::Swap(self), ...);
        }
    };

    // clang-format off

    ShadowRegisterList<
        ShadowRegister<&RegisterState::flm>,
        ShadowRegister<&RegisterState::fvl>,
        ShadowRegister<&RegisterState::fe>,
        ShadowRegister<&RegisterState::fc0>,
        ShadowRegister<&RegisterState::fc1>,
        ShadowRegister<&RegisterState::fv>,
        ShadowRegister<&RegisterState::fn>,
        ShadowRegister<&RegisterState::fm>,
        ShadowRegister<&RegisterState::fz>,
        ShadowRegister<&RegisterState::fr>
    > shadow_registers;

    ShadowSwapRegisterList<
        ShadowSwapRegister<&RegisterState::pcmhi>,
        ShadowSwapRegister<&RegisterState::sat>,
        ShadowSwapRegister<&RegisterState::sata>,
        ShadowSwapRegister<&RegisterState::hwm>,
        ShadowSwapRegister<&RegisterState::s>,
        ShadowSwapArrayRegister<2, &RegisterState::ps>,
        ShadowSwapRegister<&RegisterState::page>,
        ShadowSwapRegister<&RegisterState::stp16>,
        ShadowSwapRegister<&RegisterState::cmd>,
        ShadowSwapArrayRegister<8, &RegisterState::m>,
        ShadowSwapArrayRegister<8, &RegisterState::br>,
        ShadowSwapArrayRegister<3, &RegisterState::im>, // ?
        ShadowSwapRegister<&RegisterState::imv>,        // ?
        ShadowSwapRegister<&RegisterState::epi>,
        ShadowSwapRegister<&RegisterState::epj>
    > shadow_swap_registers;

    // clang-format on

    template <unsigned index>
    class ShadowSwapAr {
    public:
        void Swap(RegisterState* self) {
            std::swap(self->arrn[index * 2], rni);
            std::swap(self->arrn[index * 2 + 1], rnj);
            std::swap(self->arstep[index * 2], stepi);
            std::swap(self->arstep[index * 2 + 1], stepj);
            std::swap(self->aroffset[index * 2], offseti);
            std::swap(self->aroffset[index * 2 + 1], offsetj);
        }

    private:
        u16 rni, rnj, stepi, stepj, offseti, offsetj;
    };

    template <unsigned index>
    class ShadowSwapArp {
    public:
        void Swap(RegisterState* self) {
            std::swap(self->arprni[index], rni);
            std::swap(self->arprnj[index], rnj);
            std::swap(self->arpstepi[index], stepi);
            std::swap(self->arpstepj[index], stepj);
            std::swap(self->arpoffseti[index], offseti);
            std::swap(self->arpoffsetj[index], offsetj);
        }

    private:
        u16 rni, rnj, stepi, stepj, offseti, offsetj;
    };

    ShadowSwapAr<0> shadow_swap_ar0;
    ShadowSwapAr<1> shadow_swap_ar1;
    ShadowSwapArp<0> shadow_swap_arp0;
    ShadowSwapArp<1> shadow_swap_arp1;
    ShadowSwapArp<2> shadow_swap_arp2;
    ShadowSwapArp<3> shadow_swap_arp3;

    void ShadowStore() {
        shadow_registers.Store(this);
    }

    void ShadowRestore() {
        shadow_registers.Restore(this);
    }

    void SwapAllArArp() {
        shadow_swap_ar0.Swap(this);
        shadow_swap_ar1.Swap(this);
        shadow_swap_arp0.Swap(this);
        shadow_swap_arp1.Swap(this);
        shadow_swap_arp2.Swap(this);
        shadow_swap_arp3.Swap(this);
    }

    void ShadowSwap() {
        shadow_swap_registers.Swap(this);
        SwapAllArArp();
    }

    void SwapAr(u16 index) {
        switch (index) {
        case 0:
            shadow_swap_ar0.Swap(this);
            break;
        case 1:
            shadow_swap_ar1.Swap(this);
            break;
        }
    }

    void SwapArp(u16 index) {
        switch (index) {
        case 0:
            shadow_swap_arp0.Swap(this);
            break;
        case 1:
            shadow_swap_arp1.Swap(this);
            break;
        case 2:
            shadow_swap_arp2.Swap(this);
            break;
        case 3:
            shadow_swap_arp3.Swap(this);
            break;
        }
    }

    bool ConditionPass(Cond cond) const {
        switch (cond.GetName()) {
        case CondValue::True:
            return true;
        case CondValue::Eq:
            return fz == 1;
        case CondValue::Neq:
            return fz == 0;
        case CondValue::Gt:
            return fz == 0 && fm == 0;
        case CondValue::Ge:
            return fm == 0;
        case CondValue::Lt:
            return fm == 1;
        case CondValue::Le:
            return fm == 1 || fz == 1;
        case CondValue::Nn:
            return fn == 0;
        case CondValue::C:
            return fc0 == 1;
        case CondValue::V:
            return fv == 1;
        case CondValue::E:
            return fe == 1;
        case CondValue::L:
            return flm == 1 || fvl == 1;
        case CondValue::Nr:
            return fr == 0;
        case CondValue::Niu0:
            return iu[0] == 0;
        case CondValue::Iu0:
            return iu[0] == 1;
        case CondValue::Iu1:
            return iu[1] == 1;
        default:
            UNREACHABLE();
        }
    }

    template <typename PseudoRegisterT>
    u16 Get() const {
        return PseudoRegisterT::Get(this);
    }

    template <typename PseudoRegisterT>
    void Set(u16 value) {
        PseudoRegisterT::Set(this, value);
    }
};

template <u16 RegisterState::*target>
struct Redirector {
    static u16 Get(const RegisterState* self) {
        return self->*target;
    }
    static void Set(RegisterState* self, u16 value) {
        self->*target = value;
    }
};

template <std::size_t size, std::array<u16, size> RegisterState::*target, std::size_t index>
struct ArrayRedirector {
    static u16 Get(const RegisterState* self) {
        return (self->*target)[index];
    }
    static void Set(RegisterState* self, u16 value) {
        (self->*target)[index] = value;
    }
};

template <u16 RegisterState::*target0, u16 RegisterState::*target1>
struct DoubleRedirector {
    static u16 Get(const RegisterState* self) {
        return self->*target0 | self->*target1;
    }
    static void Set(RegisterState* self, u16 value) {
        self->*target0 = self->*target1 = value;
    }
};

template <u16 RegisterState::*target>
struct RORedirector {
    static u16 Get(const RegisterState* self) {
        return self->*target;
    }
    static void Set(RegisterState*, u16) {
        // no
    }
};

template <std::size_t size, std::array<u16, size> RegisterState::*target, std::size_t index>
struct ArrayRORedirector {
    static u16 Get(const RegisterState* self) {
        return (self->*target)[index];
    }
    static void Set(RegisterState*, u16) {
        // no
    }
};

template <unsigned index>
struct AccEProxy {
    static u16 Get(const RegisterState* self) {
        return (u16)((self->a[index] >> 32) & 0xF);
    }
    static void Set(RegisterState* self, u16 value) {
        u32 value32 = SignExtend<4>((u32)value);
        self->a[index] &= 0xFFFFFFFF;
        self->a[index] |= (u64)value32 << 32;
    }
};

struct LPRedirector {
    static u16 Get(const RegisterState* self) {
        return self->lp;
    }
    static void Set(RegisterState* self, u16 value) {
        if (value != 0) {
            self->lp = 0;
            self->bcn = 0;
        }
    }
};

template <typename Proxy, unsigned position, unsigned length>
struct ProxySlot {
    using proxy = Proxy;
    static constexpr unsigned pos = position;
    static constexpr unsigned len = length;
    static_assert(length < 16, "Error");
    static_assert(position + length <= 16, "Error");
    static constexpr u16 mask = ((1 << length) - 1) << position;
};

template <typename... ProxySlots>
struct PseudoRegister {
    static_assert(NoOverlap<u16, ProxySlots::mask...>, "Error");
    static u16 Get(const RegisterState* self) {
        return ((ProxySlots::proxy::Get(self) << ProxySlots::pos) | ...);
    }
    static void Set(RegisterState* self, u16 value) {
        (ProxySlots::proxy::Set(self, (value >> ProxySlots::pos) & ((1 << ProxySlots::len) - 1)),
         ...);
    }
};

// clang-format off

using cfgi = PseudoRegister<
    ProxySlot<Redirector<&RegisterState::stepi>, 0, 7>,
    ProxySlot<Redirector<&RegisterState::modi>, 7, 9>
>;

using cfgj = PseudoRegister<
    ProxySlot<Redirector<&RegisterState::stepj>, 0, 7>,
    ProxySlot<Redirector<&RegisterState::modj>, 7, 9>
>;

using stt0 = PseudoRegister<
    ProxySlot<Redirector<&RegisterState::flm>, 0, 1>,
    ProxySlot<Redirector<&RegisterState::fvl>, 1, 1>,
    ProxySlot<Redirector<&RegisterState::fe>, 2, 1>,
    ProxySlot<Redirector<&RegisterState::fc0>, 3, 1>,
    ProxySlot<Redirector<&RegisterState::fv>, 4, 1>,
    ProxySlot<Redirector<&RegisterState::fn>, 5, 1>,
    ProxySlot<Redirector<&RegisterState::fm>, 6, 1>,
    ProxySlot<Redirector<&RegisterState::fz>, 7, 1>,
    ProxySlot<Redirector<&RegisterState::fc1>, 11, 1>
>;
using stt1 = PseudoRegister<
    ProxySlot<Redirector<&RegisterState::fr>, 4, 1>,
    ProxySlot<ArrayRORedirector<2, &RegisterState::iu, 0>, 10, 1>,
    ProxySlot<ArrayRORedirector<2, &RegisterState::iu, 1>, 11, 1>,
    ProxySlot<ArrayRedirector<2, &RegisterState::pe, 0>, 14, 1>,
    ProxySlot<ArrayRedirector<2, &RegisterState::pe, 1>, 15, 1>
>;
using stt2 = PseudoRegister<
    ProxySlot<ArrayRORedirector<3, &RegisterState::ip, 0>, 0, 1>,
    ProxySlot<ArrayRORedirector<3, &RegisterState::ip, 1>, 1, 1>,
    ProxySlot<ArrayRORedirector<3, &RegisterState::ip, 2>, 2, 1>,
    ProxySlot<RORedirector<&RegisterState::ipv>, 3, 1>,

    ProxySlot<Redirector<&RegisterState::pcmhi>, 6, 2>,

    ProxySlot<RORedirector<&RegisterState::bcn>, 12, 3>,
    ProxySlot<LPRedirector, 15, 1>
>;
using mod0 = PseudoRegister<
    ProxySlot<Redirector<&RegisterState::sat>, 0, 1>,
    ProxySlot<Redirector<&RegisterState::sata>, 1, 1>,
    ProxySlot<RORedirector<&RegisterState::mod0_unk_const>, 2, 3>,
    ProxySlot<Redirector<&RegisterState::hwm>, 5, 2>,
    ProxySlot<Redirector<&RegisterState::s>, 7, 1>,
    ProxySlot<ArrayRedirector<5, &RegisterState::ou, 0>, 8, 1>,
    ProxySlot<ArrayRedirector<5, &RegisterState::ou, 1>, 9, 1>,
    ProxySlot<ArrayRedirector<2, &RegisterState::ps, 0>, 10, 2>,

    ProxySlot<ArrayRedirector<2, &RegisterState::ps, 1>, 13, 2>
>;
using mod1 = PseudoRegister<
    ProxySlot<Redirector<&RegisterState::page>, 0, 8>,

    ProxySlot<Redirector<&RegisterState::stp16>, 12, 1>,
    ProxySlot<Redirector<&RegisterState::cmd>, 13, 1>,
    ProxySlot<Redirector<&RegisterState::epi>, 14, 1>,
    ProxySlot<Redirector<&RegisterState::epj>, 15, 1>
>;
using mod2 = PseudoRegister<
    ProxySlot<ArrayRedirector<8, &RegisterState::m, 0>, 0, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::m, 1>, 1, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::m, 2>, 2, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::m, 3>, 3, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::m, 4>, 4, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::m, 5>, 5, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::m, 6>, 6, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::m, 7>, 7, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::br, 0>, 8, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::br, 1>, 9, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::br, 2>, 10, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::br, 3>, 11, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::br, 4>, 12, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::br, 5>, 13, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::br, 6>, 14, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::br, 7>, 15, 1>
>;
using mod3 = PseudoRegister<
    ProxySlot<Redirector<&RegisterState::nimc>, 0, 1>,
    ProxySlot<ArrayRedirector<3, &RegisterState::ic, 0>, 1, 1>,
    ProxySlot<ArrayRedirector<3, &RegisterState::ic, 1>, 2, 1>,
    ProxySlot<ArrayRedirector<3, &RegisterState::ic, 2>, 3, 1>,
    ProxySlot<ArrayRedirector<5, &RegisterState::ou, 2>, 4, 1>,
    ProxySlot<ArrayRedirector<5, &RegisterState::ou, 3>, 5, 1>,
    ProxySlot<ArrayRedirector<5, &RegisterState::ou, 4>, 6, 1>,
    ProxySlot<Redirector<&RegisterState::ie>, 7, 1>,
    ProxySlot<ArrayRedirector<3, &RegisterState::im, 0>, 8, 1>,
    ProxySlot<ArrayRedirector<3, &RegisterState::im, 1>, 9, 1>,
    ProxySlot<ArrayRedirector<3, &RegisterState::im, 2>, 10, 1>,
    ProxySlot<Redirector<&RegisterState::imv>, 11, 1>,

    ProxySlot<Redirector<&RegisterState::ccnta>, 13, 1>,
    ProxySlot<Redirector<&RegisterState::cpc>, 14, 1>,
    ProxySlot<Redirector<&RegisterState::crep>, 15, 1>
>;

using st0 = PseudoRegister<
    ProxySlot<Redirector<&RegisterState::sat>, 0, 1>,
    ProxySlot<Redirector<&RegisterState::ie>, 1, 1>,
    ProxySlot<ArrayRedirector<3, &RegisterState::im, 0>, 2, 1>,
    ProxySlot<ArrayRedirector<3, &RegisterState::im, 1>, 3, 1>,
    ProxySlot<Redirector<&RegisterState::fr>, 4, 1>,
    ProxySlot<DoubleRedirector<&RegisterState::flm, &RegisterState::fvl>, 5, 1>,
    ProxySlot<Redirector<&RegisterState::fe>, 6, 1>,
    ProxySlot<Redirector<&RegisterState::fc0>, 7, 1>,
    ProxySlot<Redirector<&RegisterState::fv>, 8, 1>,
    ProxySlot<Redirector<&RegisterState::fn>, 9, 1>,
    ProxySlot<Redirector<&RegisterState::fm>, 10, 1>,
    ProxySlot<Redirector<&RegisterState::fz>, 11, 1>,
    ProxySlot<AccEProxy<0>, 12, 4>
>;
using st1 = PseudoRegister<
    ProxySlot<Redirector<&RegisterState::page>, 0, 8>,
    // 8, 9: reserved
    ProxySlot<ArrayRedirector<2, &RegisterState::ps, 0>, 10, 2>,
    ProxySlot<AccEProxy<1>, 12, 4>
>;
using st2 = PseudoRegister<
    ProxySlot<ArrayRedirector<8, &RegisterState::m, 0>, 0, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::m, 1>, 1, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::m, 2>, 2, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::m, 3>, 3, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::m, 4>, 4, 1>,
    ProxySlot<ArrayRedirector<8, &RegisterState::m, 5>, 5, 1>,
    ProxySlot<ArrayRedirector<3, &RegisterState::im, 2>, 6, 1>,
    ProxySlot<Redirector<&RegisterState::s>, 7, 1>,
    ProxySlot<ArrayRedirector<5, &RegisterState::ou, 0>, 8, 1>,
    ProxySlot<ArrayRedirector<5, &RegisterState::ou, 1>, 9, 1>,
    ProxySlot<ArrayRORedirector<2, &RegisterState::iu, 0>, 10, 1>,
    ProxySlot<ArrayRORedirector<2, &RegisterState::iu, 1>, 11, 1>,
    // 12: reserved
    ProxySlot<ArrayRORedirector<3, &RegisterState::ip, 2>, 13, 1>, // Note the index order!
    ProxySlot<ArrayRORedirector<3, &RegisterState::ip, 0>, 14, 1>,
    ProxySlot<ArrayRORedirector<3, &RegisterState::ip, 1>, 15, 1>
>;
using icr = PseudoRegister<
    ProxySlot<Redirector<&RegisterState::nimc>, 0, 1>,
    ProxySlot<ArrayRedirector<3, &RegisterState::ic, 0>, 1, 1>,
    ProxySlot<ArrayRedirector<3, &RegisterState::ic, 1>, 2, 1>,
    ProxySlot<ArrayRedirector<3, &RegisterState::ic, 2>, 3, 1>,
    ProxySlot<LPRedirector, 4, 1>,
    ProxySlot<RORedirector<&RegisterState::bcn>, 5, 3>
>;

template<unsigned index>
using ar = PseudoRegister<
    ProxySlot<ArrayRedirector<4, &RegisterState::arstep, index * 2 + 1>, 0, 3>,
    ProxySlot<ArrayRedirector<4, &RegisterState::aroffset, index * 2 + 1>, 3, 2>,
    ProxySlot<ArrayRedirector<4, &RegisterState::arstep, index * 2>, 5, 3>,
    ProxySlot<ArrayRedirector<4, &RegisterState::aroffset, index * 2>, 8, 2>,
    ProxySlot<ArrayRedirector<4, &RegisterState::arrn, index * 2 + 1>, 10, 3>,
    ProxySlot<ArrayRedirector<4, &RegisterState::arrn, index * 2>, 13, 3>
>;

using ar0 = ar<0>;
using ar1 = ar<1>;

template<unsigned index>
using arp = PseudoRegister<
    ProxySlot<ArrayRedirector<4, &RegisterState::arpstepi, index>, 0, 3>,
    ProxySlot<ArrayRedirector<4, &RegisterState::arpoffseti, index>, 3, 2>,
    ProxySlot<ArrayRedirector<4, &RegisterState::arpstepj, index>, 5, 3>,
    ProxySlot<ArrayRedirector<4, &RegisterState::arpoffsetj, index>, 8, 2>,
    ProxySlot<ArrayRedirector<4, &RegisterState::arprni, index>, 10, 2>,
    // bit 12 reserved
    ProxySlot<ArrayRedirector<4, &RegisterState::arprnj, index>, 13, 2>
    // bit 15 reserved
>;

using arp0 = arp<0>;
using arp1 = arp<1>;
using arp2 = arp<2>;
using arp3 = arp<3>;

// clang-format on

} // namespace Teakra
