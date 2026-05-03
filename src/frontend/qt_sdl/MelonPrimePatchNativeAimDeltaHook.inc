#ifdef MELONPRIME_DS

    namespace {

        struct NativeAimDeltaHookInfo {
            uint32_t hookPc;
            uint8_t horizontalReg;
            uint8_t verticalReg;
            uint32_t localPlayerPtrAddr;
        };

        // ROM group order:
        // JP1_0=0, JP1_1=1, US1_0=2, US1_1=3, EU1_0=4, EU1_1=5, KR1_0=6
        static constexpr NativeAimDeltaHookInfo kNativeAimDeltaHooks[] = {
            { 0x02024318u, 4, 6, 0x020BE790u },
            { 0x02024318u, 4, 6, 0x020BE750u },
            { 0x0202433Cu, 4, 6, 0x020BCA70u },
            { 0x0202433Cu, 4, 6, 0x020BD2D0u },
            { 0x02024334u, 4, 6, 0x020BD2F0u },
            { 0x0202433Cu, 4, 6, 0x020BD370u },
            { 0x0200D208u, 5, 6, 0x020B6240u },
        };

        [[nodiscard]] static FORCE_INLINE bool NativeAimDeltaHook_IsMainRamRange(
            uint32_t address,
            uint32_t size) noexcept
        {
            return size != 0
                && address >= 0x02000000u
                && size - 1u <= 0x023FFFFFu - address;
        }

        [[nodiscard]] static FORCE_INLINE bool NativeAimDeltaHook_ReadMainRam32(
            melonDS::NDS* nds,
            uint32_t address,
            uint32_t& out) noexcept
        {
            if (!nds || !NativeAimDeltaHook_IsMainRamRange(address, sizeof(out)))
                return false;
            std::memcpy(&out, nds->MainRAM + (address & 0x3FFFFFu), sizeof(out));
            return true;
        }

        [[nodiscard]] static FORCE_INLINE int16_t NativeAimDeltaHook_ClampToS16(
            int32_t value) noexcept
        {
            if (value > 32767)
                return 32767;
            if (value < -32768)
                return -32768;
            return static_cast<int16_t>(value);
        }

        [[nodiscard]] static FORCE_INLINE const NativeAimDeltaHookInfo* NativeAimDeltaHook_Find(
            uint8_t romGroupIndex,
            uint32_t arm9ExecAddr) noexcept
        {
            if (romGroupIndex >= sizeof(kNativeAimDeltaHooks) / sizeof(kNativeAimDeltaHooks[0]))
                return nullptr;

            const auto& info = kNativeAimDeltaHooks[romGroupIndex];
            return (info.hookPc == arm9ExecAddr) ? &info : nullptr;
        }

    } // anonymous namespace

    uint32_t MelonPrimeCore::NativeAimDeltaHook_GetAddresses(
        uint8_t romGroupIndex,
        uint32_t* out,
        uint32_t maxCount)
    {
        if (!out || maxCount == 0)
            return 0;
        if (romGroupIndex >= sizeof(kNativeAimDeltaHooks) / sizeof(kNativeAimDeltaHooks[0]))
            return 0;

        out[0] = kNativeAimDeltaHooks[romGroupIndex].hookPc;
        return 1;
    }

    void MelonPrimeCore::NativeAimDeltaHook_DispatchCheck(
        melonDS::NDS* nds,
        uint32_t arm9ExecAddr,
        uint32_t regs[16])
    {
        if (!nds || !regs)
            return;
        if (!m_enableNativeAimDeltaHook || !m_disableMphAimSmoothing || isStylusMode)
            return;
        if (IsSamusMorphBallAltForm())
            return;
        if ((m_aimBlockBits | static_cast<uint32_t>(m_isLayoutChangePending)) != 0)
            return;
        if (!m_flags.test(StateFlags::BIT_LAST_FOCUSED))
            return;

        const NativeAimDeltaHookInfo* const info =
            NativeAimDeltaHook_Find(m_currentRom.romGroupIndex, arm9ExecAddr);
        if (!info)
            return;

        const uint32_t player = regs[10];
        uint32_t localPlayer = 0;
        if (!NativeAimDeltaHook_ReadMainRam32(nds, info->localPlayerPtrAddr, localPlayer))
            return;
        if (player != localPlayer)
            return;
        if (!NativeAimDeltaHook_IsMainRamRange(player, 0x400u))
            return;

        int32_t outX = m_nativeAimDeltaX;
        int32_t outY = m_nativeAimDeltaY;
        m_nativeAimDeltaX = 0;
        m_nativeAimDeltaY = 0;

#ifdef _WIN32
        int lateX = 0;
        int lateY = 0;
        if (m_rawFilter)
            m_rawFilter->LateLatchMouseDelta(lateX, lateY);

        if ((lateX | lateY) != 0)
        {
            int64_t resX = m_aimResidualX + static_cast<int64_t>(lateX) * m_aimFixedScaleX;
            int64_t resY = m_aimResidualY + static_cast<int64_t>(lateY) * m_aimFixedScaleY;
            resX = ClampAimResidual(resX, AIM_MAX_RESIDUAL);
            resY = ClampAimResidual(resY, AIM_MAX_RESIDUAL);

            const int16_t lateOutX = static_cast<int16_t>(resX >> AIM_DIRECT_BITS);
            const int16_t lateOutY = static_cast<int16_t>(resY >> AIM_DIRECT_BITS);

            if ((lateOutX | lateOutY) != 0)
            {
                resX -= static_cast<int64_t>(lateOutX) << AIM_DIRECT_BITS;
                resY -= static_cast<int64_t>(lateOutY) << AIM_DIRECT_BITS;

                outX += lateOutX;
                outY += lateOutY;

                if (!m_enableAimAccumulator)
                {
                    resX = 0;
                    resY = 0;
                }
            }

            m_aimResidualX = resX;
            m_aimResidualY = resY;
        }
#endif

        if ((outX | outY) == 0)
            return;

        const int16_t finalX = NativeAimDeltaHook_ClampToS16(outX);
        const int16_t finalY = NativeAimDeltaHook_ClampToS16(outY);

        uint32_t sensXRaw = 0;
        uint32_t sensYRaw = 0;
        if (!NativeAimDeltaHook_ReadMainRam32(nds, player + 0x3F8u, sensXRaw)
            || !NativeAimDeltaHook_ReadMainRam32(nds, player + 0x3FCu, sensYRaw))
        {
            return;
        }

        const int32_t sensX = static_cast<int32_t>(sensXRaw);
        const int32_t sensY = static_cast<int32_t>(sensYRaw);
        const int64_t horizontalScaled = static_cast<int64_t>(finalX) * static_cast<int64_t>(sensX);
        const int64_t verticalScaled   = static_cast<int64_t>(finalY) * static_cast<int64_t>(sensY);

        regs[info->horizontalReg] = static_cast<uint32_t>(horizontalScaled);
        regs[info->verticalReg]   = static_cast<uint32_t>(verticalScaled);
    }

#endif // MELONPRIME_DS
