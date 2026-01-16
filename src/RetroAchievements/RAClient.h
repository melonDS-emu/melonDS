#pragma once

#include <cstdint>
#include <string>
#include <rcheevos/include/rc_client.h>
#include <optional>
#include <functional>
#include <vector>
#include <rcheevos/include/rc_runtime.h>
#include <ctime>


using AchievementUnlockedCallback =
    std::function<void(const char* title,
                       const char* desc,
                       const char* badge_url)>;
using MeasuredProgressCallback =
    std::function<void(unsigned id,
                       unsigned value,
                       unsigned target,
                       const char* text)>;
using AchievementProgressCallback =
        std::function<void(const char* title,
                       const char* progress,
                       const char* badgeUrl)>;
using ChallengeShowCallback = 
    std::function<void(const char* badgeUrl)>;
using ChallengeHideCallback = 
    std::function<void()>;
using GameMasteredCallback =
    std::function<void(const std::string& title,
                       const char* gameBadge)>;

struct TrackedAchievement
{
    uint32_t id = 0;
    unsigned prev_value = 0;
};
namespace melonDS { 
    class NDS; 
    class Savestate;
}
class RAContext {
public:
    using PlaytimeLoadHandler = std::function<uint32_t(uint32_t gameId)>;
    using PlaytimeSaveHandler = std::function<void(uint32_t gameId, uint32_t totalMinutes)>;

    void SetPlaytimeHandlers(PlaytimeLoadHandler loader, PlaytimeSaveHandler saver) {
        m_playtimeLoader = std::move(loader);
        m_playtimeSaver = std::move(saver);
    }
    int m_suppressProgressFrames = 0;
    void SaveSavestate(melonDS::Savestate* file);
    void LoadSavestate(melonDS::Savestate* file);

    void showProgressIndicator(const rc_client_achievement_t* ach);
    void updateProgressIndicator(const rc_client_achievement_t* ach);
    void hideProgressIndicator();
        struct LeaderboardTracker {
        uint32_t id;
        std::string display;
    };
    std::unordered_map<uint32_t, LeaderboardTracker> activeTrackers;
    using LeaderboardCallback = std::function<void(const rc_client_leaderboard_t*)>;
    LeaderboardCallback onLeaderboardStarted;
    LeaderboardCallback onLeaderboardFailed;
    LeaderboardCallback onLeaderboardSubmitted;
    friend uint32_t RC_CCONV RuntimePeek(uint32_t, uint32_t, void*);
    struct FullAchievement
    {
        uint32_t id = 0;
        std::string title;
        std::string description;
        uint32_t points = 0;
        bool unlocked = false;

        bool measured = false;
        int value = 0;
        int target = 0;
        std::string progressText;

        char badge_name[8] = {};
        float measured_percent = 0.0f;
        time_t unlock_time = 0;
        uint8_t state = 0;
        uint8_t category = 0;
        uint8_t bucket = 0;
        float rarity = 0.0f;
        float rarity_hardcore = 0.0f;
        uint8_t type = 0;
        const char* badge_url = nullptr;
        const char* badge_locked_url = nullptr;
        const rc_client_achievement_t** achievements = nullptr;
        uint32_t num_achievements;

        const char* label;
        uint32_t subset_id;
        uint8_t bucket_type;
    };


    const std::vector<FullAchievement>& GetAllAchievements();
    void ResetGameState();
    RAContext();
    ~RAContext();
    bool m_shuttingDown = false;
    bool m_IsPaused = false;
    bool CanPause(uint32_t* frames_remaining);
    void SetPaused(bool paused);
    std::function<void(bool success, const std::string& message)> onLoginResponse;
    bool pendingLoginResult = false;
    bool loginSuccess = false;
    std::string loginErrorMessage;
    bool isLoading = false;
    std::string m_lastHash;
    std::string pendingLoadError;
    bool pendingLoadFailed = false;
    void SetOnGameLoadedCallback(std::function<void()> cb);
    std::function<void()> onGameLoaded;
    const rc_client_game_t* GetCurrentGameInfo() const { return currentGameInfo; }
    bool IsGameLoaded() const { return gameLoaded; }
    void SetOnAchievementUnlocked(AchievementUnlockedCallback cb);
    void SetOnAchievementProgress(AchievementProgressCallback cb);
    void SetOnChallenge(ChallengeShowCallback cb);
    void SetOnChallengeHide(ChallengeHideCallback cb);
    void SetOnGameMastered(GameMasteredCallback cb);
    void SetOnMeasuredProgress(MeasuredProgressCallback cb);
    void UpdateMeasuredAchievements();
    void LoginNow();
    void Enable();
    void Disable();
    bool IsEnabled() const { return m_enabled; }
    bool IsHardcoreEnabled() const { return m_hardcore; }
    using LoginCallback = std::function<void()>;
    void SetOnLogin(LoginCallback cb)
    {
        m_onLogin = std::move(cb);
    }
    void AttachNDS(melonDS::NDS* nds_);
    void SetPendingGameHash(const char* hash);
    void SetToken(const std::string& token) { m_token = token; }
    void SetLoggedIn(bool v);
    void Init(melonDS::NDS* nds);
    void Shutdown();
    void DoFrame();
    void LoginWithPassword(const char* username, const char* password, bool hardcore);
    void SetCredentials(const char* user, const char* password, bool hardcore);
    void LoadGame(const char* hash);
    void Reset();
    bool IsLoggedIn() const { return m_logged_in; }
    const std::string& GetUser() const { return m_user; }
    const std::string& GetToken() const { return m_token; }
    const std::string& GetDisplayName() const;
    melonDS::NDS* nds = nullptr;
    rc_client_t* client = nullptr;
    bool m_logged_in = false;
    const char* GetUserPicURL() const {
        const rc_client_user_t* user = rc_client_get_user_info(client);
        return (user) ? user->avatar_url : nullptr;
    }

    const char* GetGameTitle() const {
        const rc_client_game_t* game = rc_client_get_game_info(client);
        return (game) ? game->title : "Unknown Game";
    }

    const char* GetGameIconURL() const {
        const rc_client_game_t* game = rc_client_get_game_info(client);
        return (game) ? game->badge_url : nullptr;
    }
    
    uint32_t GetGameID() const {
        const rc_client_game_t* game = rc_client_get_game_info(client);
        return (game) ? game->id : 0;
    }
    uint32_t GetGamePlaytime() const {
        if (!gameLoaded || m_sessionStart == 0) return m_accumulatedTime;
        uint32_t sessionMinutes = (uint32_t)((time(nullptr) - m_sessionStart) / 60);
        return m_accumulatedTime + sessionMinutes;
    }
private:
    PlaytimeLoadHandler m_playtimeLoader;
    PlaytimeSaveHandler m_playtimeSaver;
    void SavePlaytime();
    time_t m_sessionStart = 0;
    uint32_t m_accumulatedTime = 0;
    uint32_t m_currentGameId = 0;
    rc_runtime_t m_runtime;
    std::vector<TrackedAchievement> trackedAchievements;
    std::vector<FullAchievement> allAchievements;
    void SetDisplayName(const char* name);
    AchievementUnlockedCallback m_onAchievementUnlocked;
    MeasuredProgressCallback m_onMeasuredProgress;
    AchievementProgressCallback m_onAchievementProgress;
    ChallengeShowCallback m_onChallenge;
    ChallengeHideCallback m_onChallengeHide;
    GameMasteredCallback m_OnGameMastered;
    std::string m_displayName;
    const rc_client_game_t* currentGameInfo = nullptr;
    bool m_enabled = false;
    LoginCallback m_onLogin;
    std::optional<std::string> pendingGameHash;
    bool gameLoaded = false;
    std::string m_user;
    std::string m_token;
    std::string m_password;
    bool m_hardcore = false;
    static uint32_t ReadMemory(
        uint32_t address,
        uint8_t* buffer,
        uint32_t size,
        rc_client_t* client
    );
    static void ServerCall(
        const rc_api_request_t* request,
        rc_client_server_callback_t callback,
        void* userdata,
        rc_client_t* client
    );
};