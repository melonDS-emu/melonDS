#ifdef MELONPRIME_DS

#include "MelonPrimePatchShadowFreezeRuntimeHook.h"
#include "Config.h"
#include "NDS.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace MelonPrime {

namespace {

// ROM group order:
//   JP1_0=0, JP1_1=1, US1_0=2, US1_1=3, EU1_0=4, EU1_1=5, KR1_0=6
//
// Hook position:
//   The hook is placed at the conditional branch after the original dot/cos compare.
//   At this point, the original lateral-distance range check has already passed.
//   We skip the original conditional branch and choose hit/miss in C++ using the
//   full 3D between vector.
//
// Required live registers at the decision instruction:
//   r1 = cos threshold, sign-extended Q12
//   r6 = target CPlayer*
//   r7 = CBeamProjectile*
//
// Target positions:
//   Body       : player + 0x1C
//   Halfturret : *(player + 0xF24) + 0x30

static constexpr uint32_t kExpectedDecisionInstruction = 0xDA00000Au; // ble miss, all known versions

enum class IceWaveTargetKind : uint8_t
{
    Body,
    Halfturret,
};

struct IceWaveDecisionHook
{
    uint32_t DecisionAddress;
    uint32_t HitAddress;
    uint32_t MissAddress;
    IceWaveTargetKind Kind;
};

struct IceWaveRomHooks
{
    const IceWaveDecisionHook* Hooks;
    std::size_t Count;
};

static constexpr IceWaveDecisionHook kHooks_JP1_0[] = {
    {0x0203E5FCu, 0x0203E600u, 0x0203E62Cu, IceWaveTargetKind::Body},
    {0x0203E734u, 0x0203E738u, 0x0203E764u, IceWaveTargetKind::Halfturret},
};

static constexpr IceWaveDecisionHook kHooks_JP1_1[] = {
    {0x0203E5FCu, 0x0203E600u, 0x0203E62Cu, IceWaveTargetKind::Body},
    {0x0203E734u, 0x0203E738u, 0x0203E764u, IceWaveTargetKind::Halfturret},
};

static constexpr IceWaveDecisionHook kHooks_US1_0[] = {
    {0x0203E4B4u, 0x0203E4B8u, 0x0203E4E4u, IceWaveTargetKind::Body},
    {0x0203E5ECu, 0x0203E5F0u, 0x0203E61Cu, IceWaveTargetKind::Halfturret},
};

static constexpr IceWaveDecisionHook kHooks_US1_1[] = {
    {0x0203E3E4u, 0x0203E3E8u, 0x0203E414u, IceWaveTargetKind::Body},
    {0x0203E51Cu, 0x0203E520u, 0x0203E54Cu, IceWaveTargetKind::Halfturret},
};

static constexpr IceWaveDecisionHook kHooks_EU1_0[] = {
    {0x0203E3DCu, 0x0203E3E0u, 0x0203E40Cu, IceWaveTargetKind::Body},
    {0x0203E514u, 0x0203E518u, 0x0203E544u, IceWaveTargetKind::Halfturret},
};

static constexpr IceWaveDecisionHook kHooks_EU1_1[] = {
    {0x0203E3E4u, 0x0203E3E8u, 0x0203E414u, IceWaveTargetKind::Body},
    {0x0203E51Cu, 0x0203E520u, 0x0203E54Cu, IceWaveTargetKind::Halfturret},
};

static constexpr IceWaveDecisionHook kHooks_KR1_0[] = {
    {0x0203D95Cu, 0x0203D960u, 0x0203D98Cu, IceWaveTargetKind::Body},
    {0x0203DA84u, 0x0203DA88u, 0x0203DAB4u, IceWaveTargetKind::Halfturret},
};

static constexpr IceWaveRomHooks kRomHooks[7] = {
    {kHooks_JP1_0, sizeof(kHooks_JP1_0) / sizeof(kHooks_JP1_0[0])},
    {kHooks_JP1_1, sizeof(kHooks_JP1_1) / sizeof(kHooks_JP1_1[0])},
    {kHooks_US1_0, sizeof(kHooks_US1_0) / sizeof(kHooks_US1_0[0])},
    {kHooks_US1_1, sizeof(kHooks_US1_1) / sizeof(kHooks_US1_1[0])},
    {kHooks_EU1_0, sizeof(kHooks_EU1_0) / sizeof(kHooks_EU1_0[0])},
    {kHooks_EU1_1, sizeof(kHooks_EU1_1) / sizeof(kHooks_EU1_1[0])},
    {kHooks_KR1_0, sizeof(kHooks_KR1_0) / sizeof(kHooks_KR1_0[0])},
};

static bool IsMainRamAddress(uint32_t address)
{
    return address >= 0x02000000u && address <= 0x023FFFFFu;
}

static bool IsMainRamRange(uint32_t address, uint32_t size)
{
    if (size == 0)
        return false;

    const uint32_t last = address + size - 1u;
    if (last < address)
        return false;

    return IsMainRamAddress(address) && IsMainRamAddress(last);
}

static int32_t ReadS32(melonDS::NDS* nds, uint32_t address)
{
    return static_cast<int32_t>(nds->ARM9Read32(address));
}

static bool AddSquareChecked(uint64_t& sum, int64_t value)
{
    if (value == std::numeric_limits<int64_t>::min())
        return false;

    const uint64_t absValue = (value < 0)
        ? static_cast<uint64_t>(-value)
        : static_cast<uint64_t>(value);

    if (absValue != 0 && absValue > std::numeric_limits<uint64_t>::max() / absValue)
        return false;

    const uint64_t square = absValue * absValue;
    if (sum > std::numeric_limits<uint64_t>::max() - square)
        return false;

    sum += square;
    return true;
}

static bool AddI64Checked(int64_t& sum, int64_t value)
{
    if (value > 0 && sum > std::numeric_limits<int64_t>::max() - value)
        return false;
    if (value < 0 && sum < std::numeric_limits<int64_t>::min() - value)
        return false;

    sum += value;
    return true;
}

static bool MulI64Checked(int64_t left, int64_t right, int64_t& out)
{
    if (left == 0 || right == 0)
    {
        out = 0;
        return true;
    }

    const int64_t min = std::numeric_limits<int64_t>::min();
    const int64_t max = std::numeric_limits<int64_t>::max();

    if (left > 0)
    {
        if ((right > 0 && left > max / right) ||
            (right < 0 && right < min / left))
            return false;
    }
    else
    {
        if ((right > 0 && left < min / right) ||
            (right < 0 && left < max / right))
            return false;
    }

    out = left * right;
    return true;
}

static bool AddProductChecked(int64_t& sum, int64_t left, int64_t right)
{
    int64_t product = 0;
    return MulI64Checked(left, right, product) && AddI64Checked(sum, product);
}

static uint64_t IsqrtU64(uint64_t value)
{
    uint64_t result = 0;
    uint64_t bit = 1ull << 62;

    while (bit > value)
        bit >>= 2;

    while (bit != 0)
    {
        if (value >= result + bit)
        {
            value -= result + bit;
            result = (result >> 1) + bit;
        }
        else
        {
            result >>= 1;
        }

        bit >>= 2;
    }

    return result;
}

static int32_t ClampToS32(int64_t value)
{
    if (value > static_cast<int64_t>(std::numeric_limits<int32_t>::max()))
        return std::numeric_limits<int32_t>::max();
    if (value < static_cast<int64_t>(std::numeric_limits<int32_t>::min()))
        return std::numeric_limits<int32_t>::min();
    return static_cast<int32_t>(value);
}

static bool GetTargetPositionAddress(melonDS::NDS* nds, uint32_t player, IceWaveTargetKind kind, uint32_t& outPositionAddress)
{
    if (!IsMainRamRange(player, 0xF28u))
        return false;

    if (kind == IceWaveTargetKind::Body)
    {
        outPositionAddress = player + 0x1Cu;
        return IsMainRamRange(outPositionAddress, 12u);
    }

    const uint32_t halfturret = nds->ARM9Read32(player + 0xF24u);
    if (!IsMainRamRange(halfturret, 0x3Cu))
        return false;

    outPositionAddress = halfturret + 0x30u;
    return IsMainRamRange(outPositionAddress, 12u);
}

static bool ComputeFull3DIceWaveDotQ12(
    melonDS::NDS* nds,
    uint32_t beam,
    uint32_t targetPositionAddress,
    int32_t& outDotQ12)
{
    if (!IsMainRamRange(beam, 0xACu))
        return false;

    const uint32_t beamDirectionAddress = beam + 0x70u;
    const uint32_t beamPositionAddress = beam + 0xA0u;

    if (!IsMainRamRange(beamDirectionAddress, 12u) || !IsMainRamRange(beamPositionAddress, 12u))
        return false;

    const int32_t targetX = ReadS32(nds, targetPositionAddress + 0u);
    const int32_t targetY = ReadS32(nds, targetPositionAddress + 4u);
    const int32_t targetZ = ReadS32(nds, targetPositionAddress + 8u);

    const int32_t beamX = ReadS32(nds, beamPositionAddress + 0u);
    const int32_t beamY = ReadS32(nds, beamPositionAddress + 4u);
    const int32_t beamZ = ReadS32(nds, beamPositionAddress + 8u);

    const int32_t dirX = ReadS32(nds, beamDirectionAddress + 0u);
    const int32_t dirY = ReadS32(nds, beamDirectionAddress + 4u);
    const int32_t dirZ = ReadS32(nds, beamDirectionAddress + 8u);

    const int64_t dx = static_cast<int64_t>(targetX) - static_cast<int64_t>(beamX);
    const int64_t dy = static_cast<int64_t>(targetY) - static_cast<int64_t>(beamY);
    const int64_t dz = static_cast<int64_t>(targetZ) - static_cast<int64_t>(beamZ);

    uint64_t lenSq = 0;
    if (!AddSquareChecked(lenSq, dx) ||
        !AddSquareChecked(lenSq, dy) ||
        !AddSquareChecked(lenSq, dz))
        return false;

    if (lenSq == 0)
        return false;

    const uint64_t len = IsqrtU64(lenSq);
    if (len == 0)
        return false;

    // dx/dy/dz and Direction are fx32/Q12. Direction is normalized in Q12.
    // dotNumerator is Q24. Dividing by len(Q12) yields Q12, matching the cos table.
    int64_t dotNumerator = 0;
    if (!AddProductChecked(dotNumerator, dx, static_cast<int64_t>(dirX)) ||
        !AddProductChecked(dotNumerator, dy, static_cast<int64_t>(dirY)) ||
        !AddProductChecked(dotNumerator, dz, static_cast<int64_t>(dirZ)))
        return false;

    outDotQ12 = ClampToS32(dotNumerator / static_cast<int64_t>(len));
    return true;
}

static const IceWaveDecisionHook* FindHook(uint8_t romGroupIndex, uint32_t arm9ExecAddr)
{
    if (romGroupIndex >= 7)
        return nullptr;

    const IceWaveRomHooks& hooks = kRomHooks[romGroupIndex];
    for (std::size_t i = 0; i < hooks.Count; ++i)
    {
        if (hooks.Hooks[i].DecisionAddress == arm9ExecAddr)
            return &hooks.Hooks[i];
    }

    return nullptr;
}

} // anonymous namespace

bool ShadowFreezeRuntimeHook_CheckAndRedirect(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex,
    uint32_t arm9ExecAddr,
    const uint32_t regs[16],
    uint32_t& redirectExecAddr)
{
    redirectExecAddr = 0;

    if (!nds || !regs)
        return false;

    if (!cfg.GetBool("Metroid.BugFix.FixShadowFreeze"))
        return false;

    const IceWaveDecisionHook* hook = FindHook(romGroupIndex, arm9ExecAddr);
    if (!hook)
        return false;

    // Extra guard: only take over the original `ble miss` decision instruction.
    // If another AR patch or ROM variant changed this word, let the CPU execute normally.
    if (nds->ARM9Read32(arm9ExecAddr) != kExpectedDecisionInstruction)
        return false;

    const uint32_t player = regs[6];
    const uint32_t beam = regs[7];
    const int32_t thresholdQ12 = static_cast<int32_t>(regs[1]);

    uint32_t targetPositionAddress = 0;
    if (!GetTargetPositionAddress(nds, player, hook->Kind, targetPositionAddress))
        return false;

    int32_t fullDotQ12 = 0;
    if (!ComputeFull3DIceWaveDotQ12(nds, beam, targetPositionAddress, fullDotQ12))
        return false;

    // Original semantics:
    //   cmp dot, threshold
    //   ble miss
    //   fallthrough hit
    redirectExecAddr = (fullDotQ12 > thresholdQ12) ? hook->HitAddress : hook->MissAddress;
    return true;
}

static bool ShadowFreezeRuntimeHook_NDSCallback(
    melonDS::NDS* nds,
    void* userdata,
    uint32_t arm9ExecAddr,
    const uint32_t regs[16],
    uint32_t& redirectExecAddr)
{
    auto* context = static_cast<ShadowFreezeRuntimeHookContext*>(userdata);
    if (!context || !context->Cfg)
        return false;

    return ShadowFreezeRuntimeHook_CheckAndRedirect(
        nds,
        *context->Cfg,
        context->RomGroupIndex,
        arm9ExecAddr,
        regs,
        redirectExecAddr);
}

void ShadowFreezeRuntimeHook_Install(
    melonDS::NDS* nds,
    ShadowFreezeRuntimeHookContext& context,
    Config::Table& cfg,
    uint8_t romGroupIndex)
{
    if (!nds || romGroupIndex >= 7)
    {
        context.Cfg = nullptr;
        context.RomGroupIndex = 0xFFu;

        if (nds)
            nds->ClearARM9InstructionHook();
        return;
    }

    context.Cfg = &cfg;
    context.RomGroupIndex = romGroupIndex;

    const IceWaveRomHooks& hooks = kRomHooks[romGroupIndex];
    uint32_t addresses[melonDS::NDS::ARM9InstructionHookMaxAddresses] {};
    const uint32_t count = static_cast<uint32_t>(std::min(
        hooks.Count,
        static_cast<std::size_t>(melonDS::NDS::ARM9InstructionHookMaxAddresses)));

    for (uint32_t i = 0; i < count; ++i)
        addresses[i] = hooks.Hooks[i].DecisionAddress;

    nds->SetARM9InstructionHook(
        ShadowFreezeRuntimeHook_NDSCallback,
        &context,
        addresses,
        count);
}

void ShadowFreezeRuntimeHook_Uninstall(
    melonDS::NDS* nds,
    ShadowFreezeRuntimeHookContext& context)
{
    context.Cfg = nullptr;
    context.RomGroupIndex = 0xFFu;

    if (nds)
        nds->ClearARM9InstructionHook();
}

bool ShadowFreezeRuntimeHook_CheckAndRedirectFromPipelinedR15(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex,
    const uint32_t regs[16],
    uint32_t& redirectExecAddr)
{
    if (!regs)
        return false;

    const uint32_t arm9ExecAddr = regs[15] - 8u;
    return ShadowFreezeRuntimeHook_CheckAndRedirect(
        nds,
        cfg,
        romGroupIndex,
        arm9ExecAddr,
        regs,
        redirectExecAddr);
}

void ShadowFreezeRuntimeHook_ResetPatchState()
{
    // No persistent state and no memory writes.
}

} // namespace MelonPrime

#endif // MELONPRIME_DS
