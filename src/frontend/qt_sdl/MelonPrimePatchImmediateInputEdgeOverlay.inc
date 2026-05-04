#ifdef MELONPRIME_DS

    namespace {

        // ROM group order:
        // JP1_0=0, JP1_1=1, US1_0=2, US1_1=3, EU1_0=4, EU1_1=5, KR1_0=6
        //
        // Hook at the action consumer entry.  Fires AFTER the input poll has written
        // fresh values into player+0x464, and BEFORE any action handler (morph check,
        // fire gate, jump gate, etc.) reads from it.
        static constexpr uint32_t kImmediateOverlayActionConsumerPc[] = {
            0x02024174u,  // JP1_0
            0x02024174u,  // JP1_1
            0x02024198u,  // US1_0
            0x02024198u,  // US1_1
            0x02024190u,  // EU1_0
            0x02024198u,  // EU1_1
            0x0200F6DCu,  // KR1_0
        };


        [[nodiscard]] static FORCE_INLINE bool ImmediateOverlay_IsMainRamRange(
            uint32_t address, uint32_t size) noexcept
        {
            return size != 0
                && address >= 0x02000000u
                && size - 1u <= 0x023FFFFFu - address;
        }

        [[nodiscard]] static FORCE_INLINE uint16_t ImmediateOverlay_ReadBindingLow(
            const uint8_t* mainRAM, uint32_t addr) noexcept
        {
            uint32_t v = 0;
            std::memcpy(&v, mainRAM + (addr & 0x3FFFFFu), sizeof(v));
            return static_cast<uint16_t>(v & 0xFFFFu);
        }

    } // anonymous namespace

    uint32_t MelonPrimeCore::ImmediateInputEdgeOverlay_GetAddresses(
        uint8_t romGroupIndex,
        uint32_t* out,
        uint32_t maxCount)
    {
        if (!out || maxCount == 0)
            return 0;
        if (romGroupIndex >= sizeof(kImmediateOverlayActionConsumerPc) / sizeof(kImmediateOverlayActionConsumerPc[0]))
            return 0;

        out[0] = kImmediateOverlayActionConsumerPc[romGroupIndex];
        return 1;
    }

    void MelonPrimeCore::ImmediateInputEdgeOverlay_DispatchCheck(
        melonDS::NDS* nds,
        uint32_t arm9ExecAddr,
        uint32_t /*regs*/[16])
    {
        if (!nds)
            return;

        // This hook handles Fire / Jump / Zoom / Movement injection only.
        // AltForm / Transform is handled by TransformGateHook separately.
        if (!m_enableImmediateInputEdgeOverlay)
            return;
        const bool doOverlay = true;

        if (!m_flags.test(StateFlags::BIT_IN_GAME_INIT))
            return;
        if (!m_flags.test(StateFlags::BIT_LAST_FOCUSED))
            return;
        if (isStylusMode)
            return;
        if (m_aimBlockBits & AIMBLK_NOT_IN_GAME)
            return;

        const uint8_t romIdx = m_currentRom.romGroupIndex;
        if (romIdx >= sizeof(kImmediateOverlayActionConsumerPc) / sizeof(kImmediateOverlayActionConsumerPc[0]))
            return;
        if (kImmediateOverlayActionConsumerPc[romIdx] != arm9ExecAddr)
            return;

        const uint32_t offP = static_cast<uint32_t>(m_playerPosition)
                            * static_cast<uint32_t>(Consts::PLAYER_ADDR_INC);
        const uint32_t playerBase = m_currentRom.playerStructStart + offP;

        if (!ImmediateOverlay_IsMainRamRange(playerBase, 0x470u))
            return;

        uint8_t* const mainRAM = nds->MainRAM;

        // Morph / AltForm transform is handled by TransformGateHook, not here.
        // TransformGateHook hooks at Gate A (02023B30 etc.) and redirects
        // execution to the transform call site when IB_MORPH is pressed.
        // No morph injection is needed in this overlay.

        // ── General action bindings (ImmediateInputEdgeOverlay) ───────────────
        //   +0x368  Move left         +0x36C  Move right
        //   +0x370  Move forward      +0x374  Move backward
        //   +0x398  Fire              +0x39C  Jump (normal binding path)
        //   +0x3E0  Zoom
        uint16_t movLMask = 0, movRMask = 0, movFMask = 0, movBMask = 0;
        uint16_t fireMask = 0, jumpMask = 0, zoomMask = 0;
        if (doOverlay) {
            movLMask = ImmediateOverlay_ReadBindingLow(mainRAM, playerBase + 0x368u);
            movRMask = ImmediateOverlay_ReadBindingLow(mainRAM, playerBase + 0x36Cu);
            movFMask = ImmediateOverlay_ReadBindingLow(mainRAM, playerBase + 0x370u);
            movBMask = ImmediateOverlay_ReadBindingLow(mainRAM, playerBase + 0x374u);
            fireMask = ImmediateOverlay_ReadBindingLow(mainRAM, playerBase + 0x398u);
            jumpMask = ImmediateOverlay_ReadBindingLow(mainRAM, playerBase + 0x39Cu);
            zoomMask = ImmediateOverlay_ReadBindingLow(mainRAM, playerBase + 0x3E0u);
        }

        const uint16_t managedMask = static_cast<uint16_t>(
            movLMask | movRMask | movFMask | movBMask |
            fireMask  | jumpMask | zoomMask);
        if (managedMask == 0)
            return;

        // Build emulator-side held state for all managed actions.
        const uint64_t down = m_input.down;
        uint16_t injectedHeld = 0;
        if (down & IB_MOVE_L) injectedHeld = static_cast<uint16_t>(injectedHeld | movLMask);
        if (down & IB_MOVE_R) injectedHeld = static_cast<uint16_t>(injectedHeld | movRMask);
        if (down & IB_MOVE_F) injectedHeld = static_cast<uint16_t>(injectedHeld | movFMask);
        if (down & IB_MOVE_B) injectedHeld = static_cast<uint16_t>(injectedHeld | movBMask);
        if (down & IB_SHOOT)  injectedHeld = static_cast<uint16_t>(injectedHeld | fireMask);
        if (down & IB_JUMP)   injectedHeld = static_cast<uint16_t>(injectedHeld | jumpMask);
        if (down & IB_ZOOM)   injectedHeld = static_cast<uint16_t>(injectedHeld | zoomMask);

        const uint16_t prevInjectedHeld = static_cast<uint16_t>(m_immediateOverlayPrevHeld & managedMask);
        const uint16_t injectedPressed  = static_cast<uint16_t>( injectedHeld & ~prevInjectedHeld);
        const uint16_t injectedReleased = static_cast<uint16_t>(~injectedHeld &  prevInjectedHeld);

        // Read game-polled input struct at player+0x464.
        const uint32_t inputBase = playerBase + 0x464u;
        uint16_t current  = 0;
        uint16_t pressed  = 0;
        uint16_t released = 0;
        std::memcpy(&current,  mainRAM + ((inputBase + 0x00u) & 0x3FFFFFu), sizeof(current));
        std::memcpy(&pressed,  mainRAM + ((inputBase + 0x04u) & 0x3FFFFFu), sizeof(pressed));
        std::memcpy(&released, mainRAM + ((inputBase + 0x08u) & 0x3FFFFFu), sizeof(released));

        // Replace only managed bits; leave all others from the game's poll.
        const uint16_t newCurrent  = static_cast<uint16_t>((current  & ~managedMask) | injectedHeld);
        const uint16_t newPressed  = static_cast<uint16_t>((pressed  & ~managedMask) | injectedPressed);
        const uint16_t newReleased = static_cast<uint16_t>((released & ~managedMask) | injectedReleased);

        std::memcpy(mainRAM + ((inputBase + 0x00u) & 0x3FFFFFu), &newCurrent,  sizeof(newCurrent));
        std::memcpy(mainRAM + ((inputBase + 0x04u) & 0x3FFFFFu), &newPressed,  sizeof(newPressed));
        std::memcpy(mainRAM + ((inputBase + 0x08u) & 0x3FFFFFu), &newReleased, sizeof(newReleased));

        m_immediateOverlayPrevHeld = injectedHeld;
    }

#endif // MELONPRIME_DS
