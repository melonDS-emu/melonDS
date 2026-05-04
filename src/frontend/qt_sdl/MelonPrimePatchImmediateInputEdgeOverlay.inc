#ifdef MELONPRIME_DS

    namespace {

        // ROM group order:
        // JP1_0=0, JP1_1=1, US1_0=2, US1_1=3, EU1_0=4, EU1_1=5, KR1_0=6
        //
        // Hook fires after the game's input poll has written fresh pressed/released
        // edges into the player input struct, but before the fire gate / action
        // consumer reads from it.
        static constexpr uint32_t kImmediateOverlayFireGatePc[] = {
            0x020271E0u,  // JP1_0
            0x020271E0u,  // JP1_1
            0x02027204u,  // US1_0
            0x02027204u,  // US1_1
            0x020271FCu,  // EU1_0
            0x02027204u,  // EU1_1
            0x0200BD28u,  // KR1_0
        };

        [[nodiscard]] static FORCE_INLINE bool ImmediateOverlay_IsMainRamRange(
            uint32_t address, uint32_t size) noexcept
        {
            return size != 0
                && address >= 0x02000000u
                && size - 1u <= 0x023FFFFFu - address;
        }

        // Read a 32-bit action binding field and return the physical NDS button mask
        // (low 16 bits only).  The high 16 bits are input-helper selector flags
        // (e.g. 0x40000 = pressed-edge, 0x80000 = quick-repress) — they must NOT
        // be written into the input struct.
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
        if (romGroupIndex >= sizeof(kImmediateOverlayFireGatePc) / sizeof(kImmediateOverlayFireGatePc[0]))
            return 0;

        out[0] = kImmediateOverlayFireGatePc[romGroupIndex];
        return 1;
    }

    void MelonPrimeCore::ImmediateInputEdgeOverlay_DispatchCheck(
        melonDS::NDS* nds,
        uint32_t arm9ExecAddr,
        uint32_t /*regs*/[16])
    {
        if (!nds)
            return;
        if (!m_enableImmediateInputEdgeOverlay)
            return;
        if (!m_flags.test(StateFlags::BIT_IN_GAME_INIT))
            return;
        if (!m_flags.test(StateFlags::BIT_LAST_FOCUSED))
            return;
        if (isStylusMode)
            return;
        if (m_aimBlockBits & AIMBLK_NOT_IN_GAME)
            return;

        const uint8_t romIdx = m_currentRom.romGroupIndex;
        if (romIdx >= sizeof(kImmediateOverlayFireGatePc) / sizeof(kImmediateOverlayFireGatePc[0]))
            return;
        if (kImmediateOverlayFireGatePc[romIdx] != arm9ExecAddr)
            return;

        const uint32_t offP = static_cast<uint32_t>(m_playerPosition)
                            * static_cast<uint32_t>(Consts::PLAYER_ADDR_INC);
        const uint32_t playerBase = m_currentRom.playerStructStart + offP;

        // Must reach at least 0x3E0+4 (zoom binding) and 0x464+12 (input struct).
        if (!ImmediateOverlay_IsMainRamRange(playerBase, 0x470u))
            return;

        uint8_t* const mainRAM = nds->MainRAM;

        // Read the physical NDS button mask (low 16 bits) for each managed action.
        // These are read from the player's current control-preset binding table so
        // the overlay is automatically correct for Touch R / Touch L / Dual R / Dual L.
        //   +0x368  Move left       +0x36C  Move right
        //   +0x370  Move up/forward +0x374  Move down/backward
        //   +0x398  Fire            +0x39C  Jump (normal binding path)
        //   +0x3E0  Zoom
        const uint16_t movLMask  = ImmediateOverlay_ReadBindingLow(mainRAM, playerBase + 0x368u);
        const uint16_t movRMask  = ImmediateOverlay_ReadBindingLow(mainRAM, playerBase + 0x36Cu);
        const uint16_t movFMask  = ImmediateOverlay_ReadBindingLow(mainRAM, playerBase + 0x370u);
        const uint16_t movBMask  = ImmediateOverlay_ReadBindingLow(mainRAM, playerBase + 0x374u);
        const uint16_t fireMask  = ImmediateOverlay_ReadBindingLow(mainRAM, playerBase + 0x398u);
        const uint16_t jumpMask  = ImmediateOverlay_ReadBindingLow(mainRAM, playerBase + 0x39Cu);
        const uint16_t zoomMask  = ImmediateOverlay_ReadBindingLow(mainRAM, playerBase + 0x3E0u);

        // Union of all managed NDS bits.  Bits outside this set are left untouched
        // in the game's input struct (the game's own poll remains authoritative for
        // any action not listed above).
        const uint16_t managedMask = static_cast<uint16_t>(
            movLMask | movRMask | movFMask | movBMask |
            fireMask | jumpMask | zoomMask);
        if (managedMask == 0)
            return;

        // Build the emulator-side held-button bitmask for all managed actions.
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

        // Read the game-polled input struct at player+0x464.
        //   +0x00  u16  buttons_current  (held)
        //   +0x04  u16  buttons_pressed  (just-pressed edge)
        //   +0x08  u16  buttons_released (just-released edge)
        const uint32_t inputBase = playerBase + 0x464u;
        uint16_t current  = 0;
        uint16_t pressed  = 0;
        uint16_t released = 0;
        std::memcpy(&current,  mainRAM + ((inputBase + 0x00u) & 0x3FFFFFu), sizeof(current));
        std::memcpy(&pressed,  mainRAM + ((inputBase + 0x04u) & 0x3FFFFFu), sizeof(pressed));
        std::memcpy(&released, mainRAM + ((inputBase + 0x08u) & 0x3FFFFFu), sizeof(released));

        // Replace only the managed bits; leave all other bits from the game's poll.
        const uint16_t newCurrent  = static_cast<uint16_t>((current  & ~managedMask) | injectedHeld);
        const uint16_t newPressed  = static_cast<uint16_t>((pressed  & ~managedMask) | injectedPressed);
        const uint16_t newReleased = static_cast<uint16_t>((released & ~managedMask) | injectedReleased);

        std::memcpy(mainRAM + ((inputBase + 0x00u) & 0x3FFFFFu), &newCurrent,  sizeof(newCurrent));
        std::memcpy(mainRAM + ((inputBase + 0x04u) & 0x3FFFFFu), &newPressed,  sizeof(newPressed));
        std::memcpy(mainRAM + ((inputBase + 0x08u) & 0x3FFFFFu), &newReleased, sizeof(newReleased));

        m_immediateOverlayPrevHeld = injectedHeld;
    }

#endif // MELONPRIME_DS
