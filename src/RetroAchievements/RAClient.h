#pragma once

#include <cstdint>
#include <string>
#include <rcheevos/include/rc_client.h>
#include <optional>
#include <functional>


using AchievementUnlockedCallback =
    std::function<void(const char* title,
                       const char* desc,
                       const char* badge_url)>;
namespace melonDS { class NDS; }
class RAContext {
public:
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
private:
    void SetDisplayName(const char* name);
    AchievementUnlockedCallback m_onAchievementUnlocked;
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