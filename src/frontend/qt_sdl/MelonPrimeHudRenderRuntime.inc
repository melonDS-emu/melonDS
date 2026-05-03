// This file is a unity-build fragment included by MelonPrimeHudRender.cpp.
// Do not add it to CMakeLists.txt.

// =========================================================================
//  Battle HUD — mode-specific score/time display
// =========================================================================

// Battle modes (8-bit read from addrBattleMode)
enum BattleMode : uint8_t {
    MODE_BATTLE        = 2,
    MODE_BOUNTY        = 3,
    MODE_CAPTURE       = 4,
    MODE_DEFENDER      = 5,
    MODE_NODES         = 6,
    MODE_PRIME_HUNTER  = 7,
    MODE_SURVIVAL      = 8,
};

static inline uint8_t NormalizeGoalKey(uint8_t xx)
{
    // Goal value is encoded in bits[5:2]. Other bits are flags and must not
    // affect the score/lives/octolith/node lookup tables.
    return static_cast<uint8_t>(xx & 0x3C);
}

// XX → goal: direct map from document (no index calculation)
static int LookupBattleGoal(uint8_t xx)
{
    switch (xx) {
    case 0x00: return 1;   case 0x04: return 5;   case 0x08: return 7;
    case 0x0C: return 10;  case 0x10: return 15;  case 0x14: return 20;
    case 0x18: return 25;  case 0x1C: return 30;  case 0x20: return 40;
    case 0x24: return 50;  case 0x28: return 60;  case 0x2C: return 70;
    case 0x30: return 80;  case 0x34: return 90;  case 0x38: return 100;
    default: return 0;
    }
}

static int LookupSurvivalGoal(uint8_t xx)
{
    switch (xx) {
    case 0x00: return 0;  case 0x04: return 1;  case 0x08: return 2;
    case 0x0C: return 3;  case 0x10: return 4;  case 0x14: return 5;
    case 0x18: return 6;  case 0x1C: return 7;  case 0x20: return 8;
    case 0x24: return 9;  case 0x28: return 10;
    default: return 0;
    }
}

static int LookupOctoGoal(uint8_t xx)
{
    switch (xx) {
    case 0x00: return 1;  case 0x04: return 2;  case 0x08: return 3;
    case 0x0C: return 4;  case 0x10: return 5;  case 0x14: return 6;
    case 0x18: return 7;  case 0x1C: return 8;  case 0x20: return 9;
    case 0x24: return 10; case 0x28: return 15; case 0x2C: return 20;
    case 0x30: return 25;
    default: return 0;
    }
}

static int LookupNodeGoal(uint8_t xx)
{
    switch (xx) {
    case 0x00: return 40;  case 0x04: return 50;  case 0x08: return 60;
    case 0x0C: return 70;  case 0x10: return 80;  case 0x14: return 90;
    case 0x18: return 100; case 0x1C: return 120; case 0x20: return 140;
    case 0x24: return 160; case 0x28: return 180; case 0x2C: return 190;
    case 0x30: return 200; case 0x34: return 250;
    default: return 0;
    }
}

// Defender/Prime Hunter time goal XY→seconds (from battleSettings+4)
static int LookupTimeGoalSec(uint8_t xy)
{
    switch (xy) {
    case 0x00: return 60;  case 0x02: return 90;  case 0x04: return 120;
    case 0x06: return 150; case 0x08: return 180; case 0x0A: return 210;
    case 0x0C: return 240; case 0x0E: return 270; case 0x10: return 300;
    case 0x12: return 360; case 0x14: return 420; case 0x16: return 480;
    case 0x18: return 540; case 0x1A: return 600;
    default: return 0;
    }
}

// Match time limit: battleSettings+4 byte1 (XX) → minutes
// Bit0 = unused/flag. Bits1-4 = time index (0-14). Bit5+ = flags (WiFi=+0x20, etc. — ignored).
static int LookupTimeLimitMin(uint8_t XX)
{
    static const int kMinutes[] = { 3, 5, 7, 9, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60 };
    int idx = (XX >> 1) & 0xF;
    return idx < 15 ? kMinutes[idx] : 0;
}

static int LookupGoal(uint8_t mode, uint8_t xx)
{
    xx = NormalizeGoalKey(xx);

    switch (mode) {
    case MODE_BATTLE:       return LookupBattleGoal(xx);
    case MODE_SURVIVAL:     return LookupSurvivalGoal(xx);
    case MODE_BOUNTY:
    case MODE_CAPTURE:      return LookupOctoGoal(xx);
    case MODE_NODES:        return LookupNodeGoal(xx);
    default:                return 0;
    }
}

static void FormatTime(char* buf, int bufSize, int seconds)
{
    int m = seconds / 60;
    int s = seconds % 60;
    std::snprintf(buf, bufSize, "%d:%02d", m, s);
}

static void FormatMinuteTime(char* buf, int bufSize, int minutes)
{
    std::snprintf(buf, bufSize, "%d:00", std::max(0, minutes));
}

// =========================================================================
//  Battle match cache — read once at match join, reused every frame
// =========================================================================
struct BattleMatchState {
    uint8_t  mode;
    uint8_t  keyXX;
    uint8_t  teamNibble;  // battleSettings+4 bits[3:0] (Z): bit i = player(i+1) team (0=red, 1=green)
    bool     isTeamGame;  // battleSettings+4 bits[7:4] (Y): any bit set = team play ON
    int      goalValue;
    int      timeLimitMinutes; // match time limit in minutes (from battleSettings+4 XX field)
    bool     isTimeMode;
    bool     valid;
};
static BattleMatchState s_battleState = { .valid = false };

void CustomHud_OnMatchJoin(melonDS::u8* ram, const RomAddresses& rom)
{
    auto& b = s_battleState;
    b.mode = Read8(ram, rom.battleMode);
    b.isTimeMode = false;
    b.goalValue = 0;
    b.timeLimitMinutes = 0;

    if (b.mode < MODE_BATTLE || b.mode > MODE_SURVIVAL) {
        b.valid = false;
        return;
    }

    uint32_t settings = Read32(ram, rom.battleSettings);
    uint8_t xx = NormalizeGoalKey(static_cast<uint8_t>(settings >> 20));
    b.keyXX = xx;

    // Time limit in minutes: battleSettings+4 format 0000XXYZ, XX=time limit index
    // Z nibble (bits[3:0]) = team composition: bit i = player(i+1) team (0=red, 1=green)
    {
        uint32_t ts4 = Read32(ram, rom.battleSettings + 4);
        uint8_t XX = (ts4 >> 8) & 0xFF;
        b.timeLimitMinutes = LookupTimeLimitMin(XX);
        b.teamNibble  = ts4 & 0x0F;          // Z nibble: team assignment per player
        b.isTeamGame  = (ts4 & 0x10) != 0;  // Y nibble bit0 (bit4 of byte): 1=team play ON
    }

    switch (b.mode) {
    case MODE_BATTLE:
    case MODE_NODES:
    case MODE_BOUNTY:
    case MODE_CAPTURE:
        b.goalValue = LookupGoal(b.mode, xx);
        break;
    case MODE_SURVIVAL:
        b.goalValue = LookupGoal(b.mode, xx) + 1; // table = max deaths, lives = deaths + 1
        break;
    case MODE_DEFENDER:
    case MODE_PRIME_HUNTER: {
        uint32_t timeSetting = Read32(ram, rom.battleSettings + 4);
        // Time goal index: Y[3:1] (bits[7:5]) + XX[0] (bit8), skipping Y[0] (bit4 = team-play flag)
        // Shift out bit4 entirely: >> 5 gives [XX0,Y3,Y2,Y1], << 1 produces even values for the lookup table
        uint8_t timeGoalRaw = static_cast<uint8_t>(((timeSetting >> 5) & 0x0F) << 1);
        b.goalValue = LookupTimeGoalSec(timeGoalRaw);
        b.isTimeMode = true;
        break;
    }
    }

    b.valid = true;
}
static const char* ResolveMatchStatusLabel(uint8_t mode, const MatchStatusHudConfig& c)
{
    switch (mode) {
    case MODE_BATTLE:
    case MODE_NODES: return c.matchStatusLabelPoints;
    case MODE_BOUNTY:
    case MODE_CAPTURE: return c.matchStatusLabelOctoliths;
    case MODE_SURVIVAL: return c.matchStatusLabelLives;
    case MODE_DEFENDER: return c.matchStatusLabelRingTime;
    case MODE_PRIME_HUNTER: return c.matchStatusLabelPrimeTime;
    default: return "";
    }
}
struct MatchStatusResolvedState { uint8_t mode, xx; int currentValue, goalValue; bool isTimeMode; };
struct MatchStatusStringCache { uint32_t configEpoch; uint8_t mode, xx; int currentValue, goalValue; bool hasGoal, isTimeMode, valid; char curBuf[24], sepBuf[4], goalBuf[24]; };
struct RankStringCache { uint32_t configEpoch; uint8_t rankByte; bool showOrdinal, valid; char buf[64]; };
struct TimeStringCache { uint32_t configEpoch; int value; bool valid; char buf[16]; };
static inline void UpdateMatchStatusStrings(MatchStatusStringCache& cache, const MatchStatusResolvedState& state)
{
    const bool hasGoal = state.isTimeMode || state.goalValue > 0;
    if (cache.valid && cache.configEpoch == s_cacheEpoch && cache.currentValue == state.currentValue && cache.goalValue == state.goalValue && cache.isTimeMode == state.isTimeMode && cache.mode == state.mode && cache.xx == state.xx && cache.hasGoal == hasGoal) return;
    cache.currentValue = state.currentValue; cache.goalValue = state.goalValue; cache.isTimeMode = state.isTimeMode; cache.mode = state.mode; cache.xx = state.xx; cache.hasGoal = hasGoal; cache.configEpoch = s_cacheEpoch;
    if (state.isTimeMode) { FormatTime(cache.curBuf, sizeof(cache.curBuf), state.currentValue); FormatTime(cache.goalBuf, sizeof(cache.goalBuf), state.goalValue); std::strncpy(cache.sepBuf, "/", sizeof(cache.sepBuf)); cache.sepBuf[sizeof(cache.sepBuf)-1]='\0'; }
    else if (state.goalValue > 0) { std::snprintf(cache.curBuf, sizeof(cache.curBuf), "%d", state.currentValue); std::snprintf(cache.goalBuf, sizeof(cache.goalBuf), "%d", state.goalValue); std::strncpy(cache.sepBuf, " / ", sizeof(cache.sepBuf)); cache.sepBuf[sizeof(cache.sepBuf)-1]='\0'; }
    else { std::snprintf(cache.curBuf, sizeof(cache.curBuf), "%d (XX=0x%02X)", state.currentValue, state.xx); cache.goalBuf[0]='\0'; cache.sepBuf[0]='\0'; }
    cache.valid = true;
}
static inline const char* UpdateRankString(RankStringCache& cache, uint8_t rankByte, bool showOrdinal, const RankTimeHudConfig& c)
{
    if (!cache.valid || cache.configEpoch != s_cacheEpoch || cache.rankByte != rankByte || cache.showOrdinal != showOrdinal) {
        static const char* kOrdinals[4] = { "st", "nd", "rd", "th" };
        if (showOrdinal) std::snprintf(cache.buf, sizeof(cache.buf), "%s%u%s%s", c.rankPrefix, rankByte + 1u, kOrdinals[rankByte], c.rankSuffix);
        else std::snprintf(cache.buf, sizeof(cache.buf), "%s%u%s", c.rankPrefix, rankByte + 1u, c.rankSuffix);
        cache.rankByte = rankByte; cache.showOrdinal = showOrdinal; cache.configEpoch = s_cacheEpoch; cache.valid = true;
    }
    return cache.buf;
}
static inline const char* UpdateTimeString(TimeStringCache& cache, int value, bool minutesOnly)
{
    if (!cache.valid || cache.configEpoch != s_cacheEpoch || cache.value != value) { if (minutesOnly) FormatMinuteTime(cache.buf, sizeof(cache.buf), value); else FormatTime(cache.buf, sizeof(cache.buf), value); cache.value = value; cache.configEpoch = s_cacheEpoch; cache.valid = true; }
    return cache.buf;
}
static bool ComputeMatchStatusState(melonDS::u8* ram, const RomAddresses& rom, uint8_t playerPos, MatchStatusResolvedState& outState)
{
    const BattleMatchState* match = s_battleState.valid ? &s_battleState : nullptr; uint8_t mode = match ? match->mode : Read8(ram, rom.battleMode); if (mode < MODE_BATTLE || mode > MODE_SURVIVAL) return false;
    uint32_t playerOfs = static_cast<uint32_t>(playerPos) * 4; int currentValue = 0; int goalValue = match ? match->goalValue : 0; bool isTimeMode = match ? match->isTimeMode : false; uint8_t xx = match ? match->keyXX : 0;
    uint32_t ts4Local = match ? 0u : Read32(ram, rom.battleSettings + 4); uint8_t teamNibble = match ? match->teamNibble : static_cast<uint8_t>(ts4Local & 0x0F); bool isTeamGame = match ? match->isTeamGame : ((ts4Local & 0x10) != 0);
    auto teamSum = [&](uint32_t baseAddr) -> int { if (isTeamGame && playerPos < 4) { uint8_t myTeam = (teamNibble >> playerPos) & 1; int sum = 0; for (int p = 0; p < 4; ++p) if (((teamNibble >> p) & 1) == myTeam) sum += static_cast<int>(Read32(ram, baseAddr + p * 4)); return sum; } return static_cast<int>(Read32(ram, baseAddr + playerOfs)); };
    switch (mode) { case MODE_BATTLE: case MODE_NODES: case MODE_BOUNTY: case MODE_CAPTURE: currentValue = teamSum(rom.basePoint); break; case MODE_SURVIVAL: currentValue = teamSum(rom.basePoint - 0xB0); break; case MODE_DEFENDER: case MODE_PRIME_HUNTER: currentValue = teamSum(rom.basePoint - 0x180) / 60; break; default: return false; }
    if (!match) { uint32_t settings = Read32(ram, rom.battleSettings); xx = NormalizeGoalKey(static_cast<uint8_t>(settings >> 20)); switch (mode) { case MODE_BATTLE: case MODE_NODES: case MODE_BOUNTY: case MODE_CAPTURE: goalValue = LookupGoal(mode, xx); break; case MODE_SURVIVAL: goalValue = LookupGoal(mode, xx); break; case MODE_DEFENDER: case MODE_PRIME_HUNTER: { uint32_t timeSetting = Read32(ram, rom.battleSettings + 4); uint8_t timeGoalRaw = static_cast<uint8_t>(((timeSetting >> 5) & 0x0F) << 1); goalValue = LookupTimeGoalSec(timeGoalRaw); isTimeMode = true; break; } default: return false; } }
    if (mode == MODE_SURVIVAL && goalValue > 0) { currentValue = goalValue - currentValue; if (currentValue < 0) currentValue = 0; }
    outState = { mode, xx, currentValue, goalValue, isTimeMode }; return true;
}
// Returns the effective outline: global override when enabled, otherwise the per-element one.
static inline const HudOutlineConfig& EffOL(const CachedHudConfig& c, const HudOutlineConfig& el) {
    return c.globalOutline.enable ? c.globalOutline : el;
}

static void RefreshStaticHudDirty(const CachedHudConfig& c,
                                  float hs, float txDs, float tyDs,
                                  int overlayW, int overlayH);

struct HudFrameTransform {
    float topStretchX;
    float hudScale;
    float txDs;
    float tyDs;
    QRect overlayRect;
};

struct HudRuntimeState {
    melonDS::NDS* nds;
    melonDS::u8* ram;
    uint32_t offP;
    uint8_t romGroup;
    uint16_t currentHP;
    uint16_t maxHP;
    uint16_t maxAmmoSpecial;
    uint16_t maxAmmoMissile;
    uint8_t hunterID;
    uint8_t currentWeapon;
    bool isStartPressed;
    bool isGameOver;
    bool isFirstPerson;
    bool isAlt;
    bool isTrans;
    bool isAdventure;
};

static inline QRect OverlayRectFor(const QImage* topBuffer)
{
    const int overlayW = topBuffer ? topBuffer->width()  : 1;
    const int overlayH = topBuffer ? topBuffer->height() : 1;
    return QRect(0, 0, overlayW, overlayH);
}

static inline HudFrameTransform MakeHudFrameTransform(const QImage* topBuffer,
                                                      float topStretchX,
                                                      float hudScale,
                                                      float hudOriginXds,
                                                      float hudOriginYds)
{
    return {
        topStretchX,
        hudScale,
        (topStretchX - 1.0f) * 128.0f + hudOriginXds,
        hudOriginYds,
        OverlayRectFor(topBuffer),
    };
}

static void EnsureCachedConfigForFrame(Config::Table& cfg, const HudFrameTransform& xf)
{
    if (UNLIKELY(!s_cache.valid) || s_cache.lastHudScale != xf.hudScale) {
        RefreshCachedConfig(cfg, xf.topStretchX, xf.hudScale);
        s_cache.valid = true;
    } else if (s_cache.lastStretchX != xf.topStretchX) {
        RecomputeAnchorPositions(xf.topStretchX);
    }
}

static void EnsureStaticDirtyForFrame(const CachedHudConfig& c, const HudFrameTransform& xf)
{
    if (UNLIKELY(s_staticDirtyStale ||
                 s_staticDirtyHudScale != xf.hudScale ||
                 s_staticDirtyTxDs    != xf.txDs     ||
                 s_staticDirtyTyDs    != xf.tyDs)) {
        RefreshStaticHudDirty(c, xf.hudScale, xf.txDs, xf.tyDs,
                              xf.overlayRect.width(), xf.overlayRect.height());
    }
}

static void ApplyHudPainterTransform(QPainter* p, const HudFrameTransform& xf)
{
    p->scale(xf.hudScale, xf.hudScale);
    if (xf.txDs != 0.0f || xf.tyDs != 0.0f)
        p->translate(xf.txDs, xf.tyDs);
}

static void EnsureHudFont(QPainter* p)
{
    if (LIKELY(s_frameFpx == kCustomHudFontSize))
        return;

    // P-9 / OPT-HUD-6 / OPT-HUD-7: Cache font + metrics once for the fixed HUD font size.
    // Screen.cpp now gives each painter a pixel-sized overlayFont, so the normal path avoids
    // p->font() and p->setFont() entirely after the first cache fill.
    QFont f = p->font();
    if (f.pixelSize() != kCustomHudFontSize) {
        f.setPixelSize(kCustomHudFontSize);
        p->setFont(f);
    }
    s_frameFont = f;
    s_frameFm   = QFontMetrics(f);
    s_frameFpx  = kCustomHudFontSize;
}

static inline QRect CurrentHudDirtyRect(const HudFrameTransform& xf, bool includeFirstPersonHud)
{
    QRect dirty = s_staticHudDirtyPx | s_chDirtyThisFrame;
    if (includeFirstPersonHud)
        dirty |= s_staticHudFirstPersonDirtyPx;
    return dirty & xf.overlayRect;
}

static void ClearPreviousCrosshairDirty(const CrosshairHudConfig& ch)
{
    if (s_chPrevPxCx == INT_MIN) return;
    s_chDirtyThisFrame = CrosshairBboxPx(s_chPrevPxCx, s_chPrevPxCy, ch);
    s_chPrevPxCx = INT_MIN;
    s_chPrevPxCy = INT_MIN;
}

static bool ReadHudRuntimeBaseState(EmuInstance* emu,
                                    const RomAddresses& rom,
                                    uint8_t playerPosition,
                                    HudRuntimeState& out)
{
    if (!emu) return false;

    melonDS::NDS* nds = emu->getNDS();
    melonDS::u8* ram = nds ? nds->MainRAM : nullptr;
    if (!ram) return false;

    const uint32_t offP = static_cast<uint32_t>(playerPosition) * Consts::PLAYER_ADDR_INC;
    out.nds = nds;
    out.ram = ram;
    out.offP = offP;
    out.romGroup = rom.romGroupIndex;
    out.currentHP = Read16(ram, rom.playerHP + offP);
    out.isStartPressed = Read8(ram, rom.startPressed) == 0x01;
    out.isGameOver = Read8(ram, rom.gameOver) != 0x00;
    return true;
}

static void ReadHudRuntimeVisibleState(const RomAddresses& rom,
                                       const GameAddressesHot& addrHot,
                                       HudRuntimeState& out)
{
    melonDS::u8* ram = out.ram;
    const uint32_t offP = out.offP;
    const uint8_t viewMode = Read8(ram, rom.baseViewMode + offP);
    const bool isFirstPerson = (viewMode == 0x00);

    out.maxHP = Read16(ram, rom.maxHP + offP);
    out.maxAmmoSpecial = isFirstPerson ? Read16(ram, rom.maxAmmoSpecial + offP) : 0;
    out.maxAmmoMissile = isFirstPerson ? Read16(ram, rom.maxAmmoMissile + offP) : 0;
    out.hunterID = Read8(ram, addrHot.chosenHunter);
    out.currentWeapon = isFirstPerson ? Read8(ram, addrHot.currentWeapon) : 0;
    out.isFirstPerson = isFirstPerson;
    out.isAlt = Read8(ram, addrHot.isAltForm) == 0x02;
    out.isTrans = isFirstPerson && (Read8(ram, addrHot.jumpFlag) & 0x10) != 0;
    out.isAdventure = Read8(ram, rom.isInAdventure) == 0x02;
}

// OPT-DR1: Computes conservative pixel-space dirty rects for non-crosshair elements.
// The always-visible rect excludes first-person-only weapon HUD, so third-person
// and alt-form frames clear/upload a smaller overlay region.
static void RefreshStaticHudDirty(const CachedHudConfig& c,
                                   float hs, float txDs, float tyDs,
                                   int overlayW, int overlayH)
{
    // Expands dirty by a DS-space bbox centred at (cx, cy) with ±padX/±padY DS units.
    QRect dirty;
    QRect firstPersonDirty;
    auto addDS = [&](QRect& dst, int cx, int cy, int padX, int padY) {
        dst |= QRect(
            QPoint(static_cast<int>((cx - padX + txDs) * hs),
                   static_cast<int>((cy - padY + tyDs) * hs)),
            QPoint(static_cast<int>((cx + padX + txDs) * hs) + 1,
                   static_cast<int>((cy + padY + tyDs) * hs) + 1));
    };

    // HP — always drawn in-game (no show flag)
    addDS(dirty, c.hp.hpX, c.hp.hpY, 90, 65);
    if (c.hp.hpGauge)
        addDS(dirty, c.hp.hpGaugePosX, c.hp.hpGaugePosY, c.hp.hpGaugeLen / 2 + 12, c.hp.hpGaugeWid / 2 + 12);

    // Weapon + Ammo — first-person only.
    addDS(firstPersonDirty, c.weapon.wpnX, c.weapon.wpnY, 90, 65);
    if (c.weapon.iconShow) addDS(firstPersonDirty, c.weapon.iconPosX, c.weapon.iconPosY, 35, 35);
    if (c.weapon.ammoGauge) addDS(firstPersonDirty, c.weapon.ammoGaugePosX, c.weapon.ammoGaugePosY, 65, 55);

    // Weapon Inventory — first-person only.
    if (c.weaponInventory.show) addDS(firstPersonDirty, c.weaponInventory.posX, c.weaponInventory.posY, 170, 170);

    // Match Status
    if (c.matchStatus.matchStatusShow) addDS(dirty, c.matchStatus.matchStatusX, c.matchStatus.matchStatusY, 110, 90);

    // Bomb Left
    if (c.bombLeft.bombLeftShow) {
        addDS(dirty, c.bombLeft.bombLeftX, c.bombLeft.bombLeftY, 75, 45);
        if (c.bombLeft.bombIconShow)
            addDS(dirty, c.bombLeft.bombIconPosX, c.bombLeft.bombIconPosY, 35, 35);
    }

    // Rank & Time
    if (c.rankTime.rankShow)      addDS(dirty, c.rankTime.rankX,      c.rankTime.rankY,      80, 28);
    if (c.rankTime.timeLeftShow)  addDS(dirty, c.rankTime.timeLeftX,  c.rankTime.timeLeftY,  80, 28);
    if (c.rankTime.timeLimitShow) addDS(dirty, c.rankTime.timeLimitX, c.rankTime.timeLimitY, 80, 28);

    // Radar — use radarDstSize for radius, plus outline padding
    if (c.radar.radarShow) {
        const int s   = c.radar.radarDstSize;
        const int pad = s / 2 + std::max(8, EffOL(c, c.radar.frameOutline).thickness + 5);
        addDS(dirty, c.radar.radarDstX + s / 2, c.radar.radarDstY + s / 2, pad, pad);
        // Also cover the (potentially larger) SVG frame
        const QRectF& fr = c.radar.frameDstRect;
        if (!fr.isEmpty()) {
            const int fx = static_cast<int>(fr.x()) - 2;
            const int fy = static_cast<int>(fr.y()) - 2;
            const int fW = static_cast<int>(fr.width())  + 4;
            const int fH = static_cast<int>(fr.height()) + 4;
            dirty |= QRect(
                QPoint(static_cast<int>((fx + txDs) * hs),
                       static_cast<int>((fy + tyDs) * hs)),
                QPoint(static_cast<int>((fx + fW + txDs) * hs) + 1,
                       static_cast<int>((fy + fH + tyDs) * hs) + 1));
        }
    }

    s_staticHudDirtyPx  = dirty & QRect(0, 0, overlayW, overlayH);
    s_staticHudFirstPersonDirtyPx = firstPersonDirty & QRect(0, 0, overlayW, overlayH);
    s_staticDirtyHudScale = hs;
    s_staticDirtyTxDs   = txDs;
    s_staticDirtyTyDs   = tyDs;
    s_staticDirtyStale  = false;
}

static void DrawMatchStatusText(QPainter* p, const QFontMetrics& fm, int fontPixelSize, float tds,
                                const MatchStatusResolvedState& state, const MatchStatusHudConfig& c,
                                float overallOpacity = 1.0f, const HudOutlineConfig& ol = {false,QColor(),0.0f,1},
                                float hudScale = 1.0f)
{
    static TextMeasureCache s_curTextCache = { 0, "", 0, 0, false }, s_sepTextCache = { 0, "", 0, 0, false }, s_goalTextCache = { 0, "", 0, 0, false };
    static TextBitmapCache s_curBitmapCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() }, s_sepBitmapCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() }, s_goalBitmapCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() }, s_labelBitmapCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() };
    static TextBitmapCache s_curOlCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() }, s_sepOlCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() }, s_goalOlCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() }, s_labelOlCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() };
    static MatchStatusStringCache s_matchStringCache = { 0, 0, 0, 0, 0, false, false, false, "", "", "" };
    UpdateMatchStatusStrings(s_matchStringCache, state);
    // OPT-HUD-6: use s_frameFont (cached by EnsureHudFont) — no p->font() copy per-frame.
    auto eff = [&](const QColor& sub) -> const QColor& { return sub.isValid() ? sub : c.matchStatusColor; };
    const bool useOutline = ol.enable && ol.opacity > 0.0f;
    const float renderScale = tds * hudScale;
    auto drawText = [&](TextBitmapCache& bmp, TextBitmapCache& olBmp, float x, float y) {
        if (useOutline) {
            PrepareOutlineBitmapCached(olBmp, bmp, ol.color, ol.thickness, renderScale);
            DrawCachedTextOutlined(p, bmp, olBmp, x, y, tds, overallOpacity, ol.opacity);
        } else {
            if (overallOpacity < 1.0f) p->setOpacity(overallOpacity);
            DrawCachedText(p, bmp, x, y, tds);
            p->setOpacity(1.0f);
        }
    };
    float curW = 0.0f, curH = 0.0f;
    MeasureTextCached(fm, fontPixelSize, s_curTextCache, s_matchStringCache.curBuf, curW, curH, tds);
    PrepareTextBitmapCached(fm, s_frameFont, fontPixelSize, s_curBitmapCache, s_matchStringCache.curBuf, eff(c.matchStatusValueColor));
    drawText(s_curBitmapCache, s_curOlCache, static_cast<float>(c.matchStatusX), static_cast<float>(c.matchStatusY));
    if (s_matchStringCache.hasGoal) {
        float sepW = 0.0f, sepH = 0.0f, goalW = 0.0f, goalH = 0.0f;
        MeasureTextCached(fm, fontPixelSize, s_sepTextCache, s_matchStringCache.sepBuf, sepW, sepH, tds);
        MeasureTextCached(fm, fontPixelSize, s_goalTextCache, s_matchStringCache.goalBuf, goalW, goalH, tds);
        float x = c.matchStatusX + curW;
        PrepareTextBitmapCached(fm, s_frameFont, fontPixelSize, s_sepBitmapCache, s_matchStringCache.sepBuf, eff(c.matchStatusSepColor));
        drawText(s_sepBitmapCache, s_sepOlCache, x, static_cast<float>(c.matchStatusY));
        x += sepW;
        PrepareTextBitmapCached(fm, s_frameFont, fontPixelSize, s_goalBitmapCache, s_matchStringCache.goalBuf, eff(c.matchStatusGoalColor));
        drawText(s_goalBitmapCache, s_goalOlCache, x, static_cast<float>(c.matchStatusY));
    }
    const char* label = ResolveMatchStatusLabel(state.mode, c);
    if (label[0] == '\0') return;
    float lx = static_cast<float>(c.matchStatusX), ly = static_cast<float>(c.matchStatusY);
    switch (c.matchStatusLabelPos) { default: case 0: ly -= 10; break; case 1: ly += 10; break; case 2: lx -= 50; break; case 3: lx += 50; break; case 4: break; }
    lx += c.matchStatusLabelOfsX; ly += c.matchStatusLabelOfsY;
    PrepareTextBitmapCached(fm, s_frameFont, fontPixelSize, s_labelBitmapCache, label, eff(c.matchStatusLabelColor));
    drawText(s_labelBitmapCache, s_labelOlCache, lx, ly);
}
static void DrawMatchStatusHud(QPainter* p, melonDS::u8* ram, const RomAddresses& rom, uint8_t playerPos, bool isAdventure, const CachedHudConfig& c)
{
    if (!c.matchStatus.matchStatusShow || isAdventure)
        return;

    MatchStatusResolvedState state = {};
    if (!ComputeMatchStatusState(ram, rom, playerPos, state))
        return;

    DrawMatchStatusText(p, s_frameFm, s_frameFpx, c.textDrawScale,
                        state, c.matchStatus, c.matchStatusOpacity,
                        EffOL(c, c.matchStatus.outline), c.lastHudScale);
}
static float CalcAlignedTextX(int anchorX, int align, float textW);
static void DrawCachedAlignedText(QPainter* p, const QFontMetrics& fm, int fontPixelSize, float tds,
                                   TextMeasureCache& measureCache, TextBitmapCache& bitmapCache,
                                   TextBitmapCache& outlineCache,
                                   const char* text, const QColor& color,
                                   int anchorX, int align, int y,
                                   float opacity, const HudOutlineConfig& ol, float hudScale = 1.0f)
{
    float textW = 0.0f, textH = 0.0f;
    MeasureTextCached(fm, fontPixelSize, measureCache, text, textW, textH, tds);
    const float textX = CalcAlignedTextX(anchorX, align, textW);
    // OPT-HUD-6: use s_frameFont (cached by EnsureHudFont) — no p->font() copy per-frame.
    PrepareTextBitmapCached(fm, s_frameFont, fontPixelSize, bitmapCache, text, color);
    if (ol.enable && ol.opacity > 0.0f) {
        const float renderScale = tds * hudScale;
        PrepareOutlineBitmapCached(outlineCache, bitmapCache, ol.color, ol.thickness, renderScale);
        DrawCachedTextOutlined(p, bitmapCache, outlineCache, textX, y, tds, opacity, ol.opacity);
    } else {
        if (opacity < 1.0f) p->setOpacity(opacity);
        DrawCachedText(p, bitmapCache, textX, y, tds);
        p->setOpacity(1.0f);
    }
}
// =========================================================================
//  Bomb Left HUD
// =========================================================================
static void DrawBombLeft(QPainter* p, melonDS::u8* ram, const RomAddresses& rom, uint32_t offP, const CachedHudConfig& c, float tds, float hudScale)
{
    if (!c.bombLeft.bombLeftShow) return; uint8_t bombs = static_cast<uint8_t>((Read32(ram, rom.baseBomb + offP) >> 8) & 0xF);
    {
        static TextBitmapCache s_bombBitmapCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() };
        static TextBitmapCache s_bombOutlineCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() };
        static TextMeasureCache s_bombMeasureCache = { 0, "", 0, 0, false };
        const QFontMetrics& fm = s_frameFm; const int fontPixelSize = s_frameFpx; // P-9
        // OPT-HUD-6: use s_frameFont (cached by EnsureHudFont) — no p->font() copy per-frame.
        char buf[64];
        if (c.bombLeft.bombLeftTextShow) std::snprintf(buf, sizeof(buf), "%s%u%s", c.bombLeft.bombLeftPrefix, bombs, c.bombLeft.bombLeftSuffix);
        else std::snprintf(buf, sizeof(buf), "%s%s", c.bombLeft.bombLeftPrefix, c.bombLeft.bombLeftSuffix);
        if (buf[0] != '\0') {
            float bombTextW = 0.0f, bombTextH = 0.0f;
            MeasureTextCached(fm, fontPixelSize, s_bombMeasureCache, buf, bombTextW, bombTextH, tds);
            PrepareTextBitmapCached(fm, s_frameFont, fontPixelSize, s_bombBitmapCache, buf, c.bombLeft.bombLeftColor);
            const float bombTextX = CalcAlignedTextX(c.bombLeft.bombLeftX, c.bombLeft.bombLeftAlign, bombTextW);
            { const HudOutlineConfig& _ol = EffOL(c, c.bombLeft.outline);
            if (_ol.enable && _ol.opacity > 0.0f) {
                const float renderScale = tds * hudScale;
                PrepareOutlineBitmapCached(s_bombOutlineCache, s_bombBitmapCache, _ol.color, _ol.thickness, renderScale);
                DrawCachedTextOutlined(p, s_bombBitmapCache, s_bombOutlineCache, bombTextX, c.bombLeft.bombLeftY, tds,
                                       c.bombLeftOpacity, _ol.opacity);
            } else {
                if (c.bombLeftOpacity < 1.0f) p->setOpacity(c.bombLeftOpacity);
                DrawCachedText(p, s_bombBitmapCache, bombTextX, c.bombLeft.bombLeftY, tds);
                p->setOpacity(1.0f);
            } }
        }
    }
    if (c.bombLeft.bombIconShow) {
        EnsureBombIconsLoaded(static_cast<int>(c.bombLeft.bombIconHeight * c.scaleIcons));
        const QImage& icon = GetBombIconForDraw(bombs, c.bombLeft.bombIconColorOverlay, c.bombLeft.bombIconColor);
        if (!icon.isNull()) {
            const float dw = icon.width() / hudScale; const float dh = icon.height() / hudScale;
            float ix = (c.bombLeft.bombIconMode == 0) ? c.bombLeft.bombLeftX + c.bombLeft.bombIconOfsX : c.bombLeft.bombIconPosX;
            float iy = (c.bombLeft.bombIconMode == 0) ? c.bombLeft.bombLeftY + c.bombLeft.bombIconOfsY : c.bombLeft.bombIconPosY;
            const int iconAlignX = c.bombLeft.bombIconAnchorX; const int iconAlignY = c.bombLeft.bombIconAnchorY;
            if (iconAlignX == 1) ix -= dw * 0.5f; else if (iconAlignX == 2) ix -= dw;
            if (iconAlignY == 1) iy -= dh * 0.5f; else if (iconAlignY == 2) iy -= dh;
            { const HudOutlineConfig& _ol = EffOL(c, c.bombLeft.iconOutline);
            if (_ol.enable && _ol.opacity > 0.0f) {
                const int expandR = std::max(1, _ol.thickness);
                EnsureBombOutlineIconsUpdated(_ol.color, expandR);
                const int bombIdx = (bombs >= 0 && bombs <= 3) ? bombs : 0;
                DrawImageOutlined(p, icon, s_bombIconsOutline[bombIdx],
                                  QRectF(ix, iy, dw, dh), (float)expandR / hudScale,
                                  c.bombIconOpacity, _ol.opacity);
            } else {
                if (c.bombIconOpacity < 1.0f) p->setOpacity(c.bombIconOpacity);
                p->drawImage(QRectF(ix, iy, dw, dh), icon);
                p->setOpacity(1.0f);
            } }
        }
    }
}
//  Rank & Time HUD
// =========================================================================
static void DrawRankAndTime(QPainter* p, melonDS::u8* ram, const RomAddresses& rom, uint8_t playerPos, bool isAdventure, const CachedHudConfig& c, float tds)
{
    if (isAdventure)
        return;

    const auto& hud = c.rankTime;

    if (hud.rankShow) {
        static RankStringCache s_rankStringCache = { 0, 0, false, false, "" };
        static TextBitmapCache s_rankCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() };
        static TextBitmapCache s_rankOutlineCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() };
        static TextMeasureCache s_rankMeasure = { 0, "", 0, 0, false };

        const uint32_t rankWord = Read32(ram, rom.matchRank);
        const uint8_t rankByte = (rankWord >> (playerPos * 8)) & 0xFF;
        if (rankByte <= 3) {
            DrawCachedAlignedText(p, s_frameFm, s_frameFpx, tds,
                                  s_rankMeasure, s_rankCache, s_rankOutlineCache,
                                  UpdateRankString(s_rankStringCache, rankByte, hud.rankShowOrdinal, hud),
                                  hud.rankColor, hud.rankX, hud.rankAlign, hud.rankY,
                                  c.rankOpacity, EffOL(c, c.rankTime.rankOutline), c.lastHudScale);
        }
    }

    if (hud.timeLeftShow) {
        static TimeStringCache s_timeLeftStringCache = { 0, 0, false, "" };
        static TextBitmapCache s_timeLeftCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() };
        static TextBitmapCache s_timeLeftOutlineCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() };
        static TextMeasureCache s_timeLeftMeasure = { 0, "", 0, 0, false };

        const int seconds = static_cast<int>(Read32(ram, rom.timeLeft)) / 60;
        DrawCachedAlignedText(p, s_frameFm, s_frameFpx, tds,
                              s_timeLeftMeasure, s_timeLeftCache, s_timeLeftOutlineCache,
                              UpdateTimeString(s_timeLeftStringCache, seconds, false),
                              hud.timeLeftColor, hud.timeLeftX, hud.timeLeftAlign, hud.timeLeftY,
                              c.timeLeftOpacity, EffOL(c, c.rankTime.timeLeftOutline), c.lastHudScale);
    }

    if (hud.timeLimitShow) {
        static TimeStringCache s_timeLimitStringCache = { 0, 0, false, "" };
        static TextBitmapCache s_timeLimitCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() };
        static TextBitmapCache s_timeLimitOutlineCache = { 0, QColor(), "", 0, 0, 0, 1.0f, false, QImage() };
        static TextMeasureCache s_timeLimitMeasure = { 0, "", 0, 0, false };

        const int goalMinutes = s_battleState.valid
            ? s_battleState.timeLimitMinutes
            : LookupTimeLimitMin((Read32(ram, rom.battleSettings + 4) >> 8) & 0xFF);
        DrawCachedAlignedText(p, s_frameFm, s_frameFpx, tds,
                              s_timeLimitMeasure, s_timeLimitCache, s_timeLimitOutlineCache,
                              UpdateTimeString(s_timeLimitStringCache, goalMinutes, true),
                              hud.timeLimitColor, hud.timeLimitX, hud.timeLimitAlign, hud.timeLimitY,
                              c.timeLimitOpacity, EffOL(c, c.rankTime.timeLimitOutline), c.lastHudScale);
    }
}
// =========================================================================
//  Config key (only for IsEnabled — hot path uses s_cache)
// =========================================================================
static constexpr const char* kCfgCustomHud = "Metroid.Visual.CustomHUD";
bool CustomHud_IsEnabled(Config::Table& localCfg)
{
    return localCfg.GetBool(kCfgCustomHud);
}

static inline bool ShouldHideForGameplayState(bool isStartPressed, uint16_t currentHP, bool isGameOver)
{
    return isStartPressed || currentHP == 0 || isGameOver;
}

bool CustomHud_ShouldHideForGameplayState(EmuInstance* emu, const RomAddresses& rom, uint8_t playerPosition)
{
    if (!emu) return true;

    melonDS::NDS* nds = emu->getNDS();
    melonDS::u8* ram = nds ? nds->MainRAM : nullptr;
    if (!ram) return true;

    const uint32_t offP = static_cast<uint32_t>(playerPosition) * Consts::PLAYER_ADDR_INC;
    return ShouldHideForGameplayState(Read8(ram, rom.startPressed) == 0x01,
                                      Read16(ram, rom.playerHP + offP),
                                      Read8(ram, rom.gameOver) != 0x00);
}

bool CustomHud_ShouldDrawRadarOverlay(EmuInstance* emu, const RomAddresses& rom, uint8_t playerPosition)
{
    return !CustomHud_ShouldHideForGameplayState(emu, rom, playerPosition);
}
// One-shot guard: when CustomHUD transitions to disabled, perform a full
// ForceRestore on the next in-game frame regardless of s_hudPatchApplied,
// so any out-of-sync RAM state (savestate / persistent RAM / JIT corner
// cases) is normalized exactly once. Reset to true when CustomHUD becomes
// enabled or on emu start/stop, so a future disable triggers it again.
static bool s_hudPatchForceRestorePending = true;

static void CustomHud_MarkNoHudPatchDesired()
{
    s_hudPatchForceRestorePending = true;
}

static void CustomHud_RestoreNativeHudState(const RomAddresses& /*rom*/,
                                            const HudRuntimeState& st)
{
    const bool needForceRestore = s_hudPatchForceRestorePending;
    if (NoHudPatch_GetAppliedMask() == 0 && !needForceRestore)
        return;

    if (needForceRestore) {
        NoHudPatch_RestoreAll(st.nds, st.romGroup);
        s_hudPatchForceRestorePending = false;
    } else {
        NoHudPatch_Sync(st.nds, st.romGroup, 0);
    }
}

void CustomHud_EnsurePatchRestored(
    EmuInstance* emu,
    Config::Table& localCfg,
    const RomAddresses& rom,
    uint8_t playerPosition,
    bool isInGame)
{
    if (isInGame && CustomHud_IsEnabled(localCfg)) {
        // HUD is enabled — Render() handles patches. Re-arm the force-restore
        // for the next disable event.
        CustomHud_MarkNoHudPatchDesired();
        return;
    }

    HudRuntimeState st = {};
    if (!ReadHudRuntimeBaseState(emu, rom, playerPosition, st))
        return;

    CustomHud_RestoreNativeHudState(rom, st);
}

void CustomHud_ResetPatchState()
{
    NoHudPatch_ResetState();
    s_cache.valid = false;
    s_battleState.valid = false;
    s_staticDirtyStale = true;
    s_chPrevPxCx = INT_MIN;
    s_chPrevPxCy = INT_MIN;
    s_hudPatchForceRestorePending = true;
}

// P-3: Called from settings dialog save to trigger config re-read next frame
void CustomHud_InvalidateConfigCache()
{
    s_cache.valid = false;
    s_bombTintCacheValid = false;
    for (int i = 0; i < 9; i++) s_weaponTintValid[i] = false;
    s_effectiveRadarHunterID = 0xFF; // P-13: force re-evaluation of effective radar color
    s_staticDirtyStale = true;       // OPT-DR1: force static dirty rect recomputation
    // Bump epoch immediately so Screen.cpp's m_hudEnabled cache refreshes on the
    // next frame. Without this, when CustomHUD is toggled off, the stale cache
    // keeps Render() running for one extra frame and re-applies NoHudPatch.
    if (++s_cacheEpoch == 0) s_cacheEpoch = 1;
}

uint32_t CustomHud_GetCacheEpoch()
{
    return s_cacheEpoch;
}

