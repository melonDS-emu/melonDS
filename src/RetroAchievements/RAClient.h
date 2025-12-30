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

// forward declaration – ZERO include hell
namespace melonDS { class NDS; }

class RAContext {
public:
    bool m_IsPaused = false;
    bool CanPause(uint32_t* frames_remaining);
    void SetPaused(bool paused);
    std::function<void(bool success, const std::string& message)> onLoginResponse;
    bool pendingLoginResult = false; // Czy mamy nowy wynik logowania do pokazania?
    bool loginSuccess = false;       // Czy się udało?
    std::string loginErrorMessage;   // Treść błędu jeśli się nie udało
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
    using LoginCallback = std::function<void()>;

    void SetOnLogin(LoginCallback cb)
    {
        m_onLogin = std::move(cb);
    }
    //bool IsLoggedIn() const;
    static RAContext& Get();

    void AttachNDS(melonDS::NDS* nds_);
    void SetPendingGameHash(const char* hash);
    //void SetPendingGame(const char* hash);
    void LoginOnce();
    void SetToken(const std::string& token) { m_token = token; }
    void SetLoggedIn(bool v);
    // Singleton
    /*
    static RAContext& Instance() {
    return Get(); // Przekierowanie do jedynej słusznej instancji
    }
    
    static RAContext& Instance() {
        static RAContext instance;
        return instance;
    }
    */


    
    // Inicjalizacja i zamknięcie
    void Init(melonDS::NDS* nds);
    void Shutdown();

    // Wywoływane co klatkę
    void DoFrame();

    // Logowanie hasłem
    void LoginWithPassword(const char* username, const char* password, bool hardcore);

    // Ustawienia konta
    void SetCredentials(const char* user, const char* password, bool hardcore);

    // Ładowanie gry po hash
    void LoadGame(const char* hash);

    // Hardcore / reset
    void Reset();
    void DisableHardcore(const char* reason);

    // Gettery
    bool IsLoggedIn() const { return m_logged_in; }
    const std::string& GetUser() const { return m_user; }
    const std::string& GetToken() const { return m_token; }
    const std::string& GetDisplayName() const;

    // Wskaźniki
    melonDS::NDS* nds = nullptr;
    rc_client_t* client = nullptr;

    //new
    bool m_logged_in = false;
private:
    // Konstruktor / Destruktor
    RAContext();
    ~RAContext();
    const rc_client_game_t* currentGameInfo = nullptr;
    bool m_enabled = false;
    LoginCallback m_onLogin;
    std::optional<std::string> pendingGameHash;
    // Dane konta i stan
    bool gameLoaded = false;
    bool Loaowaniegry = false;
    std::string m_user;
    std::string m_token;
    std::string m_password;

    bool m_hardcore = false;


    // Callbacki rcheevos
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