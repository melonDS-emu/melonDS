#include <functional>
#include <string>
#include <type_traits>
#include <vector>
#include "ahbm.h"
#include "apbp.h"
#include "btdmp.h"
#include "dma.h"
#include "memory_interface.h"
#include "mmio.h"
#include "timer.h"

namespace Teakra {

auto NoSet(const std::string& debug_string) {
    return [debug_string](u16) { printf("Warning: NoSet on %s\n", debug_string.data()); };
}
auto NoGet(const std::string& debug_string) {
    return [debug_string]() -> u16 {
        printf("Warning: NoGet on %s\n", debug_string.data());
        return 0;
    };
}

struct BitFieldSlot {
    unsigned pos;
    unsigned length;
    std::function<void(u16)> set;
    std::function<u16(void)> get;

    template <typename T>
    static BitFieldSlot RefSlot(unsigned pos, unsigned length, T& var) {
        static_assert(
            std::is_same_v<u16,
                           typename std::conditional_t<std::is_enum_v<T>, std::underlying_type<T>,
                                                       std::enable_if<true, T>>::type>);
        BitFieldSlot slot{pos, length, {}, {}};
        slot.set = [&var](u16 value) { var = static_cast<T>(value); };
        slot.get = [&var]() -> u16 { return static_cast<u16>(var); };
        return slot;
    }
};

struct Cell {
    std::function<void(u16)> set;
    std::function<u16(void)> get;
    u16 index = 0;

    Cell(std::function<void(u16)> set, std::function<u16(void)> get)
        : set(std::move(set)), get(std::move(get)) {}
    Cell() {
        std::shared_ptr<u16> storage = std::make_shared<u16>(0);
        set = [storage, this](u16 value) {
            *storage = value;
            std::printf("MMIO: cell %04X set = %04X\n", index, value);
        };
        get = [storage, this]() -> u16 {
            std::printf("MMIO: cell %04X get\n", index);
            return *storage;
        };
    }
    static Cell ConstCell(u16 constant) {
        Cell cell({}, {});
        cell.set = NoSet("");
        cell.get = [constant]() -> u16 { return constant; };
        return cell;
    }
    static Cell RefCell(u16& var) {
        Cell cell({}, {});
        cell.set = [&var](u16 value) { var = value; };
        cell.get = [&var]() -> u16 { return var; };
        return cell;
    }

    static Cell MirrorCell(Cell* mirror) {
        Cell cell({}, {});
        cell.set = [mirror](u16 value) { mirror->set(value); };
        cell.get = [mirror]() -> u16 { return mirror->get(); };
        return cell;
    }

    static Cell BitFieldCell(const std::vector<BitFieldSlot>& slots) {
        Cell cell({}, {});
        std::shared_ptr<u16> storage = std::make_shared<u16>(0);
        cell.set = [storage, slots](u16 value) {
            for (const auto& slot : slots) {
                if (slot.set) {
                    slot.set((value >> slot.pos) & ((1 << slot.length) - 1));
                }
            }
            *storage = value;
        };
        cell.get = [storage, slots]() -> u16 {
            u16 value = *storage;
            for (const auto& slot : slots) {
                if (slot.get) {
                    value &= ~(((1 << slot.length) - 1) << slot.pos);
                    value |= slot.get() << slot.pos;
                }
            }
            return value;
        };
        return cell;
    }
};

class MMIORegion::Impl {
public:
    std::array<Cell, 0x800> cells{};
    Impl() {
        for (std::size_t i = 0; i < cells.size(); ++i) {
            cells[i].index = (u16)i;
        }
    }
};

MMIORegion::MMIORegion(MemoryInterfaceUnit& miu, ICU& icu, Apbp& apbp_from_cpu, Apbp& apbp_from_dsp,
                       std::array<Timer, 2>& timer, Dma& dma, Ahbm& ahbm,
                       std::array<Btdmp, 2>& btdmp)
    : impl(new Impl) {
    using namespace std::placeholders;

    impl->cells[0x01A] = Cell::ConstCell(0xC902); // chip detect

    // Timer
    for (unsigned i = 0; i < 2; ++i) {
        impl->cells[0x20 + i * 0x10] = Cell::BitFieldCell({
            // TIMERx_CFG
            BitFieldSlot::RefSlot(0, 2, timer[i].scale),       // TS
            BitFieldSlot::RefSlot(2, 3, timer[i].count_mode),  // CM
            BitFieldSlot{6, 1, {}, {}},                        // TP
            BitFieldSlot{7, 1, {}, {}},                        // CT
            BitFieldSlot::RefSlot(8, 1, timer[i].pause),       // PC
            BitFieldSlot::RefSlot(9, 1, timer[i].update_mmio), // MU
            BitFieldSlot{10, 1,
                         [&timer, i](u16 v) {
                             if (v)
                                 timer[i].Restart();
                         },
                         []() -> u16 { return 0; }}, // RES
            BitFieldSlot{11, 1, {}, {}},             // BP
            BitFieldSlot{12, 1, {}, {}},             // CS
            BitFieldSlot{13, 1, {}, {}},             // GP
            BitFieldSlot{14, 2, {}, {}},             // TM
        });

        impl->cells[0x22 + i * 0x10].set = [&timer, i](u16 v) {
            if (v)
                timer[i].TickEvent();
        }; // TIMERx_EW
        impl->cells[0x22 + i * 0x10].get = []() -> u16 { return 0; };
        impl->cells[0x24 + i * 0x10] = Cell::RefCell(timer[i].start_low);    // TIMERx_SCL
        impl->cells[0x26 + i * 0x10] = Cell::RefCell(timer[i].start_high);   // TIMERx_SCH
        impl->cells[0x28 + i * 0x10] = Cell::RefCell(timer[i].counter_low);  // TIMERx_CCL
        impl->cells[0x2A + i * 0x10] = Cell::RefCell(timer[i].counter_high); // TIMERx_CCH
        impl->cells[0x2C + i * 0x10] = Cell();                               // TIMERx_SPWMCL
        impl->cells[0x2E + i * 0x10] = Cell();                               // TIMERx_SPWMCH
    }

    // APBP
    for (unsigned i = 0; i < 3; ++i) {
        impl->cells[0x0C0 + i * 4].set = std::bind(&Apbp::SendData, &apbp_from_dsp, i, _1);
        impl->cells[0x0C0 + i * 4].get = std::bind(&Apbp::PeekData, &apbp_from_dsp, i);
        impl->cells[0x0C2 + i * 4].set = [](u16) {};
        impl->cells[0x0C2 + i * 4].get = std::bind(&Apbp::RecvData, &apbp_from_cpu, i);
    }
    impl->cells[0x0CC].set = std::bind(&Apbp::SetSemaphore, &apbp_from_dsp, _1);
    impl->cells[0x0CC].get = std::bind(&Apbp::GetSemaphore, &apbp_from_dsp);
    impl->cells[0x0CE].set = std::bind(&Apbp::MaskSemaphore, &apbp_from_cpu, _1);
    impl->cells[0x0CE].get = std::bind(&Apbp::GetSemaphoreMask, &apbp_from_cpu);
    impl->cells[0x0D0].set = std::bind(&Apbp::ClearSemaphore, &apbp_from_cpu, _1);
    impl->cells[0x0D0].get = []() -> u16 { return 0; };
    impl->cells[0x0D2].set = [](u16) {};
    impl->cells[0x0D2].get = std::bind(&Apbp::GetSemaphore, &apbp_from_cpu);
    impl->cells[0x0D4] = Cell::BitFieldCell({
        BitFieldSlot{2, 1, {}, {}}, // ARM side endianness flag
        BitFieldSlot{8, 1,
                     [&apbp_from_cpu](u16 v) { return apbp_from_cpu.SetDisableInterrupt(0, v); },
                     [&apbp_from_cpu]() -> u16 { return apbp_from_cpu.GetDisableInterrupt(0); }},
        BitFieldSlot{12, 1,
                     [&apbp_from_cpu](u16 v) { return apbp_from_cpu.SetDisableInterrupt(1, v); },
                     [&apbp_from_cpu]() -> u16 { return apbp_from_cpu.GetDisableInterrupt(1); }},
        BitFieldSlot{13, 1,
                     [&apbp_from_cpu](u16 v) { return apbp_from_cpu.SetDisableInterrupt(2, v); },
                     [&apbp_from_cpu]() -> u16 { return apbp_from_cpu.GetDisableInterrupt(2); }},
    });
    impl->cells[0x0D6] = Cell::BitFieldCell({
        BitFieldSlot{5, 1, {}, [&apbp_from_dsp]() -> u16 { return apbp_from_dsp.IsDataReady(0); }},
        BitFieldSlot{6, 1, {}, [&apbp_from_dsp]() -> u16 { return apbp_from_dsp.IsDataReady(1); }},
        BitFieldSlot{7, 1, {}, [&apbp_from_dsp]() -> u16 { return apbp_from_dsp.IsDataReady(2); }},
        BitFieldSlot{8, 1, {}, [&apbp_from_cpu]() -> u16 { return apbp_from_cpu.IsDataReady(0); }},
        BitFieldSlot{
            9, 1, {}, [&apbp_from_cpu]() -> u16 { return apbp_from_cpu.IsSemaphoreSignaled(); }},
        BitFieldSlot{12, 1, {}, [&apbp_from_cpu]() -> u16 { return apbp_from_cpu.IsDataReady(1); }},
        BitFieldSlot{13, 1, {}, [&apbp_from_cpu]() -> u16 { return apbp_from_cpu.IsDataReady(2); }},
    });

    // This register is a mirror of CPU side register DSP_PSTS
    impl->cells[0x0D8] = Cell::BitFieldCell({
        BitFieldSlot{
            9, 1, {}, [&apbp_from_cpu]() -> u16 { return apbp_from_cpu.IsSemaphoreSignaled(); }},
        BitFieldSlot{10, 1, {}, [&apbp_from_dsp]() -> u16 { return apbp_from_dsp.IsDataReady(0); }},
        BitFieldSlot{11, 1, {}, [&apbp_from_dsp]() -> u16 { return apbp_from_dsp.IsDataReady(1); }},
        BitFieldSlot{12, 1, {}, [&apbp_from_dsp]() -> u16 { return apbp_from_dsp.IsDataReady(2); }},
        BitFieldSlot{13, 1, {}, [&apbp_from_cpu]() -> u16 { return apbp_from_cpu.IsDataReady(0); }},
        BitFieldSlot{14, 1, {}, [&apbp_from_cpu]() -> u16 { return apbp_from_cpu.IsDataReady(1); }},
        BitFieldSlot{15, 1, {}, [&apbp_from_cpu]() -> u16 { return apbp_from_cpu.IsDataReady(2); }},
    });

    // AHBM
    impl->cells[0x0E0].set = NoSet("AHBM::BusyFlag");
    impl->cells[0x0E0].get = std::bind(&Ahbm::GetBusyFlag, &ahbm);
    for (u16 i = 0; i < 3; ++i) {
        impl->cells[0x0E2 + i * 6] = Cell::BitFieldCell({
            // BitFieldSlot{0, 1, ?, ?},
            BitFieldSlot{1, 2, std::bind(&Ahbm::SetBurstSize, &ahbm, i, _1),
                         std::bind(&Ahbm::GetBurstSize, &ahbm, i)},
            BitFieldSlot{4, 2, std::bind(&Ahbm::SetUnitSize, &ahbm, i, _1),
                         std::bind(&Ahbm::GetUnitSize, &ahbm, i)},
        });
        impl->cells[0x0E4 + i * 6] = Cell::BitFieldCell({
            BitFieldSlot{8, 1, std::bind(&Ahbm::SetDirection, &ahbm, i, _1),
                         std::bind(&Ahbm::GetDirection, &ahbm, i)},
            // BitFieldSlot{9, 1, ?, ?},
        });
        impl->cells[0x0E6 + i * 6].set = std::bind(&Ahbm::SetDmaChannel, &ahbm, i, _1);
        impl->cells[0x0E6 + i * 6].get = std::bind(&Ahbm::GetDmaChannel, &ahbm, i);
    }

    // MIU
    // impl->cells[0x100]; // MIU_WSCFG0
    // impl->cells[0x102]; // MIU_WSCFG1
    // impl->cells[0x104]; // MIU_Z0WSCFG
    // impl->cells[0x106]; // MIU_Z1WSCFG
    // impl->cells[0x108]; // MIU_Z2WSCFG
    // impl->cells[0x10C]; // MIU_Z3WSCFG
    impl->cells[0x10E] = Cell::RefCell(miu.x_page); // MIU_XPAGE
    impl->cells[0x110] = Cell::RefCell(miu.y_page); // MIU_YPAGE
    impl->cells[0x112] = Cell::RefCell(miu.z_page); // MIU_ZPAGE
    impl->cells[0x114] = Cell::BitFieldCell({
        // MIU_PAGE0CFG
        BitFieldSlot::RefSlot(0, 6, miu.x_size[0]),
        BitFieldSlot::RefSlot(8, 6, miu.y_size[0]),
    });
    impl->cells[0x116] = Cell::BitFieldCell({
        // MIU_PAGE1CFG
        BitFieldSlot::RefSlot(0, 6, miu.x_size[1]),
        BitFieldSlot::RefSlot(8, 6, miu.y_size[1]),
    });
    // impl->cells[0x118]; // MIU_OFFPAGECFG
    impl->cells[0x11A] = Cell::BitFieldCell({
        BitFieldSlot{0, 1, {}, {}},                 // PP
        BitFieldSlot{1, 1, {}, {}},                 // TESTP
        BitFieldSlot{2, 1, {}, {}},                 // INTP
        BitFieldSlot{4, 1, {}, {}},                 // ZSINGLEP
        BitFieldSlot::RefSlot(6, 1, miu.page_mode), // PAGEMODE
    });
    // impl->cells[0x11C]; // MIU_DLCFG
    impl->cells[0x11E] = Cell::RefCell(miu.mmio_base); // MIU_MMIOBASE
    // impl->cells[0x120]; // MIU_OBSCFG
    // impl->cells[0x122]; // MIU_POLARITY

    // DMA
    impl->cells[0x184].set = std::bind(&Dma::EnableChannel, &dma, _1);
    impl->cells[0x184].get = std::bind(&Dma::GetChannelEnabled, &dma);

    impl->cells[0x18C].get = []() -> u16 { return 0xFFFF; }; // SEOX ?

    impl->cells[0x1BE].set = std::bind(&Dma::ActivateChannel, &dma, _1);
    impl->cells[0x1BE].get = std::bind(&Dma::GetActiveChannel, &dma);
    impl->cells[0x1C0].set = std::bind(&Dma::SetAddrSrcLow, &dma, _1);
    impl->cells[0x1C0].get = std::bind(&Dma::GetAddrSrcLow, &dma);
    impl->cells[0x1C2].set = std::bind(&Dma::SetAddrSrcHigh, &dma, _1);
    impl->cells[0x1C2].get = std::bind(&Dma::GetAddrSrcHigh, &dma);
    impl->cells[0x1C4].set = std::bind(&Dma::SetAddrDstLow, &dma, _1);
    impl->cells[0x1C4].get = std::bind(&Dma::GetAddrDstLow, &dma);
    impl->cells[0x1C6].set = std::bind(&Dma::SetAddrDstHigh, &dma, _1);
    impl->cells[0x1C6].get = std::bind(&Dma::GetAddrDstHigh, &dma);
    impl->cells[0x1C8].set = std::bind(&Dma::SetSize0, &dma, _1);
    impl->cells[0x1C8].get = std::bind(&Dma::GetSize0, &dma);
    impl->cells[0x1CA].set = std::bind(&Dma::SetSize1, &dma, _1);
    impl->cells[0x1CA].get = std::bind(&Dma::GetSize1, &dma);
    impl->cells[0x1CC].set = std::bind(&Dma::SetSize2, &dma, _1);
    impl->cells[0x1CC].get = std::bind(&Dma::GetSize2, &dma);
    impl->cells[0x1CE].set = std::bind(&Dma::SetSrcStep0, &dma, _1);
    impl->cells[0x1CE].get = std::bind(&Dma::GetSrcStep0, &dma);
    impl->cells[0x1D0].set = std::bind(&Dma::SetDstStep0, &dma, _1);
    impl->cells[0x1D0].get = std::bind(&Dma::GetDstStep0, &dma);
    impl->cells[0x1D2].set = std::bind(&Dma::SetSrcStep1, &dma, _1);
    impl->cells[0x1D2].get = std::bind(&Dma::GetSrcStep1, &dma);
    impl->cells[0x1D4].set = std::bind(&Dma::SetDstStep1, &dma, _1);
    impl->cells[0x1D4].get = std::bind(&Dma::GetDstStep1, &dma);
    impl->cells[0x1D6].set = std::bind(&Dma::SetSrcStep2, &dma, _1);
    impl->cells[0x1D6].get = std::bind(&Dma::GetSrcStep2, &dma);
    impl->cells[0x1D8].set = std::bind(&Dma::SetDstStep2, &dma, _1);
    impl->cells[0x1D8].get = std::bind(&Dma::GetDstStep2, &dma);
    impl->cells[0x1DA] = Cell::BitFieldCell({
        BitFieldSlot{0, 4, std::bind(&Dma::SetSrcSpace, &dma, _1),
                     std::bind(&Dma::GetSrcSpace, &dma)},
        BitFieldSlot{4, 4, std::bind(&Dma::SetDstSpace, &dma, _1),
                     std::bind(&Dma::GetDstSpace, &dma)},
        // BitFieldSlot{9, 1, ?, ?},
        BitFieldSlot{10, 1, std::bind(&Dma::SetDwordMode, &dma, _1),
                     std::bind(&Dma::GetDwordMode, &dma)},
    });
    impl->cells[0x1DC].set = std::bind(&Dma::SetY, &dma, _1);
    impl->cells[0x1DC].get = std::bind(&Dma::GetY, &dma);
    impl->cells[0x1DE].set = std::bind(&Dma::SetZ, &dma, _1);
    impl->cells[0x1DE].get = std::bind(&Dma::GetZ, &dma);

    // ICU
    impl->cells[0x200].set = NoSet("ICU::GetRequest");
    impl->cells[0x200].get = std::bind(&ICU::GetRequest, &icu);
    impl->cells[0x202].set = std::bind(&ICU::Acknowledge, &icu, _1);
    impl->cells[0x202].get = std::bind(&ICU::GetAcknowledge, &icu);
    impl->cells[0x204].set = std::bind(&ICU::Trigger, &icu, _1);
    impl->cells[0x204].get = std::bind(&ICU::GetTrigger, &icu);
    impl->cells[0x206].set = std::bind(&ICU::SetEnable, &icu, 0, _1);
    impl->cells[0x206].get = std::bind(&ICU::GetEnable, &icu, 0);
    impl->cells[0x208].set = std::bind(&ICU::SetEnable, &icu, 1, _1);
    impl->cells[0x208].get = std::bind(&ICU::GetEnable, &icu, 1);
    impl->cells[0x20A].set = std::bind(&ICU::SetEnable, &icu, 2, _1);
    impl->cells[0x20A].get = std::bind(&ICU::GetEnable, &icu, 2);
    impl->cells[0x20C].set = std::bind(&ICU::SetEnableVectored, &icu, _1);
    impl->cells[0x20C].get = std::bind(&ICU::GetEnableVectored, &icu);
    // impl->cells[0x20E]; // polarity for each interrupt?
    // impl->cells[0x210]; // source type for each interrupt?
    for (unsigned i = 0; i < 16; ++i) {
        impl->cells[0x212 + i * 4] = Cell::BitFieldCell({
            BitFieldSlot::RefSlot(0, 2, icu.vector_high[i]),
            BitFieldSlot::RefSlot(15, 1, icu.vector_context_switch[i]),
        });
        impl->cells[0x214 + i * 4] = Cell::RefCell(icu.vector_low[i]);
    }

    // BTDMP
    for (u16 i = 0; i < 2; ++i) {
        impl->cells[0x2A2 + i * 0x80].set = std::bind(&Btdmp::SetTransmitClockConfig, &btdmp[i], _1);
        impl->cells[0x2A2 + i * 0x80].get = std::bind(&Btdmp::GetTransmitClockConfig, &btdmp[i]);
        impl->cells[0x2BE + i * 0x80].set = std::bind(&Btdmp::SetTransmitEnable, &btdmp[i], _1);
        impl->cells[0x2BE + i * 0x80].get = std::bind(&Btdmp::GetTransmitEnable, &btdmp[i]);
        impl->cells[0x2C2 + i * 0x80] = Cell::BitFieldCell({
            BitFieldSlot{3, 1, {}, std::bind(&Btdmp::GetTransmitFull, &btdmp[i])},
            BitFieldSlot{4, 1, {}, std::bind(&Btdmp::GetTransmitEmpty, &btdmp[i])},
        });
        impl->cells[0x2C6 + i * 0x80].set = std::bind(&Btdmp::Send, &btdmp[i], _1);
        impl->cells[0x2CA + i * 0x80].set = std::bind(&Btdmp::SetTransmitFlush, &btdmp[i], _1);
        impl->cells[0x2CA + i * 0x80].get = std::bind(&Btdmp::GetTransmitFlush, &btdmp[i]);
    }
}

MMIORegion::~MMIORegion() = default;

u16 MMIORegion::Read(u16 addr) {
    u16 value = impl->cells[addr].get();
    return value;
}
void MMIORegion::Write(u16 addr, u16 value) {
    impl->cells[addr].set(value);
}
} // namespace Teakra
