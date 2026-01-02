#include "RAClient.h"
#include "../NDS.h"
#include <cstring>
#include <string>
#include <curl/curl.h>
#include <cstdio>
#include "../rcheevos/include/rc_consoles.h"
#include "../rcheevos/include/rc_client.h"
#include "RetroAchievements/cacert.c"

extern const unsigned char _accacert[];
extern const size_t _accacert_len;
static AchievementUnlockedCallback g_onAchievementUnlocked;
std::function<void()> m_onLogin;
std::string m_displayName;
void SetDisplayName(const char* name)
{
    m_displayName = name ? name : "";
}

const std::string& RAContext::GetDisplayName() const
{
    return m_displayName;
}
static uint64_t g_ra_mem_reads = 0;
static uint64_t g_ra_mem_logged = 0;

// ================= cURL helper =================

static size_t CurlWrite(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::string* s = static_cast<std::string*>(userdata);
    s->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

// ================= RAContext =================

bool RAContext::CanPause(uint32_t* frames_remaining) {
    if (!client) return true; // Zmienione z m_Client na client
    return rc_client_can_pause(client, frames_remaining) != 0;
}

void RAContext::SetPaused(bool paused) {
    m_IsPaused = paused;
}

RAContext::RAContext() : nds(nullptr), client(nullptr)
{
    curl_global_init(CURL_GLOBAL_ALL);
}

RAContext::~RAContext()
{
    Shutdown();
    curl_global_cleanup();
}

void RAContext::Init(melonDS::NDS* nds_)
{
    nds = nds_;
    client = rc_client_create(
        &RAContext::ReadMemory,
        &RAContext::ServerCall
        );
    // NDS = Nintendo DS
    rc_client_set_userdata(client, this); // ← TO JEST KLUCZ
    rc_client_set_allow_background_memory_reads(client, 1);

    // 3. 🔥 TUTAJ WKLEJAMY OBSŁUGĘ ZDARZEŃ (EVENT HANDLER) 🔥
    rc_client_set_event_handler(client,
    [](const rc_client_event_t* event, rc_client_t* client)
    {
    switch (event->type)
    {
        case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
        {
            const rc_client_achievement_t* ach =
            (const rc_client_achievement_t*)event->achievement;
            printf("[RA] ACHIEVEMENT TRIGGERED LOCAL: %s\n", ach->title);    

            if (g_onAchievementUnlocked)
        {
                g_onAchievementUnlocked(
                    ach->title,
                    ach->description,
                    ach->badge_url
            );
        }
            break;
        }

        default:
        break;
    }
});

    //printf("[RA] Client initialized (rcheevos 12+)\n");
}

void RAContext::Shutdown()
{
    if (client) {
        rc_client_destroy(client);
        client = nullptr;
    }
}

void RAContext::DoFrame()
{
    // 1. Zostawiamy Twoje podstawowe warunki
    if (!nds || !client || m_IsPaused)
        return;

    // 2. Zostawiamy Twoją logikę sprawdzania hashu
    std::string currentHash = pendingGameHash.value_or("");
    if (currentHash != m_lastHash) {
        gameLoaded = false;
        m_lastHash = currentHash;
        pendingLoadFailed = false;
        isLoading = false;
    }

    // 3. Zostawiamy Twoje blokady ładowania
    if (pendingLoadFailed || isLoading) return;

    // 4. Zostawiamy Twoje wywołanie LoadGame
    if (!gameLoaded &&
        pendingGameHash.has_value() &&
        nds->IsGameRunning())
    {
        isLoading = true;
        LoadGame(pendingGameHash->c_str());
        return; // <--- Dodałem to, żeby nie leciał dalej w tej samej klatce
    }

    // --- MOJA GŁÓWNA POPRAWKA PONIŻEJ ---

    // 5. Blokada: dopóki gra nie jest w pełni rozpoznana, nie mielimy klatek RA
    if (!gameLoaded) return;

    // 6. Twoje sprawdzenie, czy logika jest wymagana (teraz w dobrym miejscu)
    if (rc_client_is_processing_required(client) == 0) {
        return;
    } 

    // 7. Dopiero na końcu faktyczne przetwarzanie klatki
    rc_client_do_frame(client);
}
// ================= Login / Load =================

    void RAContext::SetLoggedIn(bool v)
    {
    if (m_logged_in == v)
        return;
    //printf("zalogowano i wykonano setloggedin");
    m_logged_in = v;
    
    if (m_onLogin)
        m_onLogin();
    }

static void LoginPasswordCallback(int result, const char* err, rc_client_t* client, void* userdata)
{
    RAContext* ctx = static_cast<RAContext*>(userdata);
    if (result == RC_OK)
    {
        const rc_client_user_t* user_info = rc_client_get_user_info(client);
        printf("[RA] Logged in as %s\n", user_info->display_name);
        if (ctx)
        {
            //printf("[RA] Login OK, waiting for game hash\n");
        }
        //const rc_client_game_t* game = rc_client_get_game(client);
        // Zapisz otrzymany token z RA
    ctx->SetToken(user_info->token ? user_info->token : "");
    ctx->SetLoggedIn(true);
    
    // Możemy zapisać token w configu
    //printf("[RA] New token saved: %s\n", ctx->GetToken().c_str());
        if (ctx->onLoginResponse) {
            ctx->onLoginResponse(true, "Zalogowano pomyślnie!");
        }
    }
    else
    {
        ctx->loginSuccess = false;
        ctx->loginErrorMessage = err ? err : "Unknown error";
        ctx->SetLoggedIn(false);
        printf("[RA] Login failed: %s\n", err ? err : "unknown error");
            if (ctx->onLoginResponse) {
            ctx->onLoginResponse(false, ctx->loginErrorMessage);
            }
    }
}



void RAContext::LoadGame(const char* hash)
{
    if (!client) return;
    //printf("[RA] LoadGame called with hash: %s\n", hash);

    // Dodajemy [this], aby lambda widziała zmienną gameLoaded
    rc_client_begin_load_game(
        client,
        hash,
        [](int result, const char* err, rc_client_t*, void* userdata) {
            // Rzutujemy userdata z powrotem na RAContext*, aby mieć dostęp do klasy
            auto* context = static_cast<RAContext*>(userdata);

            if (result == 0) {
                context->pendingLoadFailed = false;
                context->isLoading = false;
                printf("[RA] Game loaded\n");
                context->gameLoaded = true; // Używamy wskaźnika przekazanego przez userdata
                context->currentGameInfo = rc_client_get_game_info(context->client);
                //printf("[RA] Przzed Ongameloaded");
                if (context->onGameLoaded){
                    //printf("[RA] W Ongameloaded");
                    context->onGameLoaded();}
            }
            else {
                printf("[RA] Load failed: %s\n", err);
                context->gameLoaded = false;
                context->pendingLoadError = err;
                context->pendingLoadFailed = true;
                context->isLoading = false;
                if (context->onGameLoaded) {
                    context->onGameLoaded(); 
                }
                
            }
        },
        this // Przekazujemy 'this' jako userdata
    );
}

// ================= CALLBACKS rcheevos =================

uint32_t RAContext::ReadMemory(uint32_t address,
                               uint8_t* buffer,
                               uint32_t size,
                               rc_client_t* client)
{
    //printf("[RA][ReadMemory] called addr=%08X size=%u\n", address, size);
    if (address >= 0x04000000 && address < 0x04000400)
    {
    //printf("[RA][IO READ] %08X\n", address);
}
    g_ra_mem_reads++;
    RAContext* ctx =
        static_cast<RAContext*>(rc_client_get_userdata(client));

    if (buffer && size > 0)
        memset(buffer, 0, size);

    if (!ctx || !ctx->nds || !buffer || size == 0)
        return 0;

    // ===== ARM9 MAIN RAM =====
    // RA: 0x00000000 -> ARM9 0x02000000 (4MB)
    constexpr uint32_t ARM9_RA_BASE   = 0x00000000;
    constexpr uint32_t ARM9_RA_SIZE   = 0x00400000;
    constexpr uint32_t ARM9_PHYS_BASE = 0x02000000;

    if (address >= ARM9_RA_BASE && address + size <= ARM9_RA_SIZE)
    {
        memcpy(buffer,
               ctx->nds->MainRAM + address,
               size);
    /*
    if (++g_ra_mem_logged % 10 == 0)
    {
        printf("[RA][ReadMemory][ARM9] reads=%llu addr=%08X size=%u val=%02X\n",
               (unsigned long long)g_ra_mem_reads,
               address,
               size,
               buffer[0]);
    }
            */

        return size;
    }

    // ===== WRAM =====
    // RA: 0x03000000 -> WRAM
    constexpr uint32_t WRAM_RA_BASE   = 0x03000000;
    constexpr uint32_t WRAM_SIZE      = 0x00008000; // 32KB

    if (address >= WRAM_RA_BASE &&
        address + size <= WRAM_RA_BASE + WRAM_SIZE)
    {
        uint32_t off = address - WRAM_RA_BASE;
        memcpy(buffer,
               ctx->nds->SharedWRAM + off,
               size);
        return size;
    }

    // ===== ARM7 RAM (opcjonalne, ale bardzo zalecane) =====
    // RA: 0x01000000 -> ARM7 0x03800000
    constexpr uint32_t ARM7_RA_BASE   = 0x01000000;
    constexpr uint32_t ARM7_SIZE      = 0x00010000; // 64KB
    constexpr uint32_t ARM7_PHYS_BASE = 0x03800000;

    if (address >= ARM7_RA_BASE &&
        address + size <= ARM7_RA_BASE + ARM7_SIZE)
    {
        uint32_t off = address - ARM7_RA_BASE;
        memcpy(buffer,
               ctx->nds->ARM7WRAM + off,
               size);
        return size;
    }

    /*
    // ===== VRAM =====
    constexpr uint32_t VRAM_BASE = 0x06000000;
    constexpr uint32_t VRAM_SIZE = 0x00180000; // 1.5 MB
    if (address >= VRAM_BASE && address + size <= VRAM_BASE + VRAM_SIZE)
    {
    // w melonDS VRAM jest podzielone na banki, ale dla RA można narazie:
    memset(buffer, 0, size);
    return size;
    }

    // ===== ARM9 IO =====
    constexpr uint32_t ARM9_IO_BASE = 0x04000000;
    constexpr uint32_t ARM9_IO_SIZE = 0x00000400; // 1 KB
    if (address >= ARM9_IO_BASE && address + size <= ARM9_IO_BASE + ARM9_IO_SIZE)
    {
    // tu można ewentualnie zwracać prawdziwe rejestry IO
    // na razie tylko zero:
    memset(buffer, 0, size);
    return size;
    }
    */
    // ===== NIEOBSŁUGIWANE =====
    return 0;
}

// funkcja do zapisu cacert do pliku tymczasowego
static std::string WriteCACertToTempFile() {
#ifdef _WIN32
    char tempPath[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, tempPath)) {
        return "";
    }
    char tempFile[MAX_PATH];
    if (!GetTempFileNameA(tempPath, "ra", 0, tempFile)) {
        return "";
    }

    FILE* f = fopen(tempFile, "wb");
    if (!f) return "";
    fwrite(_accacert, 1, _accacert_len, f);
    fclose(f);

    return std::string(tempFile);
#else
    char tmpName[] = "/tmp/ra_cacertXXXXXX";
    int fd = mkstemp(tmpName);
    if (fd < 0) return "";

    FILE* f = fdopen(fd, "wb");
    if (!f) return "";

    fwrite(_accacert, 1, _accacert_len, f);
    fclose(f);

    return std::string(tmpName);
#endif
}

// funkcja do usuwania tymczasowego pliku
static void RemoveTempFile(const std::string& path) {
    if (!path.empty()) {
#ifdef _WIN32
        DeleteFileA(path.c_str());
#else
        unlink(path.c_str());
#endif
    }
}

// główna funkcja
void RAContext::ServerCall(const rc_api_request_t* request,
                           rc_client_server_callback_t callback,
                           void* userdata,
                           rc_client_t*) 
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        callback(nullptr, userdata);
        return;
    }

    // zapisz cacert do pliku tymczasowego
    std::string cacertPath = WriteCACertToTempFile();
    if (cacertPath.empty()) {
        printf("[RA] Failed to create temp file for CA cert!\n");
        callback(nullptr, userdata);
        curl_easy_cleanup(curl);
        return;
    }

    curl_easy_setopt(curl, CURLOPT_CAINFO, cacertPath.c_str());

    std::string response_body;
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "melonDS/1.1 rcheevos/1.0");
    curl_easy_setopt(curl, CURLOPT_URL, request->url);

    if (request->post_data) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->post_data);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (res == CURLE_OK) {
        rc_api_server_response_t api_resp;
        api_resp.body = response_body.c_str();
        api_resp.body_length = static_cast<uint32_t>(response_body.length());
        api_resp.http_status_code = static_cast<int>(http_code);
        callback(&api_resp, userdata);
    } else {
        printf("[RA] cURL Error %d: %s\n", res, curl_easy_strerror(res));
        callback(nullptr, userdata);
    }

    curl_easy_cleanup(curl);
    RemoveTempFile(cacertPath);
}
///////RESET
void RAContext::Reset() {
    if (client)
        rc_client_reset(client);
}

void RAContext::DisableHardcore(const char* reason)
{ 
    if (client && rc_client_get_hardcore_enabled(client))
    {
        rc_client_set_hardcore_enabled(client, 0);
        printf("[RA] Hardcore disabled: %s\n", reason);
    }
}
void RAContext::LoginWithPassword(const char* username, const char* password, bool hardcore)
{
    if (!client) return;

    // hardcore ustawiamy wcześniej
    rc_client_set_hardcore_enabled(client, hardcore ? 1 : 0);

    // Wywołujemy logowanie hasłem
    rc_client_begin_login_with_password(
        client,
        username,
        password,
        &LoginPasswordCallback,
        this
    );
}
void RAContext::SetCredentials(const char* user, const char* password, bool hardcore) {
    m_user = user;
    m_password = password;
    m_hardcore = hardcore;
    //printf("[RA] Credentials set. user='%s'\n", m_user.c_str());
}

void RAContext::SetPendingGameHash(const char* hash)
{
    if (!hash || !*hash)
        return;

    pendingGameHash = std::string(hash);
    gameLoaded = false;

    //printf("[RA] Pending game hash set: %s\n", hash);
}
/*
void RAContext::SetPendingGame(const char* hash)
{
    pendingGameHash = hash;

    if (IsLoggedIn())
    {
        LoadGame(pendingGameHash.c_str());
    }
}
*/
void RAContext::AttachNDS(melonDS::NDS* nds_)
{
    if (nds == nds_)
        return;

    nds = nds_;
    //printf("[RA] NDS attached: %p\n", (void*)nds);
}
RAContext& RAContext::Get()
{
    static RAContext instance;
    return instance;
}

void RAContext::Enable()
{
    if (m_enabled)
        return;

    m_enabled = true;

    Init(nds);

    if (!IsLoggedIn())
    {
        LoginNow();
    }
}

void RAContext::Disable()
{
    if (!m_enabled)
        return;

    m_enabled = false;

    Shutdown();
    m_logged_in = false;
    gameLoaded = false;
    pendingGameHash.reset();

    printf("[RA] Disabled\n");
}

void RAContext::LoginNow()
{
    /*
    if (!client)
    {
        // inicjalizacja klienta, podłączenie readmemory
        client = rc_client_create(&RAContext::ReadMemory, &RAContext::ServerCall);
        rc_client_set_userdata(client, this);
        rc_client_set_allow_background_memory_reads(client, 1);
        printf("[RA] Client initialized (rcheevos 12+)\n");
    }
    */
    if (m_logged_in)
        return;

    if (!m_user.empty() && !m_password.empty())
    {
        //printf("[RA] LoginNow: password\n");
        LoginWithPassword(m_user.c_str(), m_password.c_str(), m_hardcore);
    }
    else
    {
        printf("[RA] LoginNow: no credentials\n");
    }
}

void RAContext::SetOnAchievementUnlocked(AchievementUnlockedCallback cb)
{
    g_onAchievementUnlocked = std::move(cb);
}

void RAContext::SetOnGameLoadedCallback(std::function<void()> cb)
{
    //printf("[RA] SetOnGameLoadedCallback called\n");
    onGameLoaded = std::move(cb);
}

/*
void RAContext::SetOnLogin(LoginCallback cb)
{
    m_onLogin = std::move(cb);

    // jeśli już zalogowany, odpal callback od razu
    if (m_logged_in && m_onLogin)
        m_onLogin();
}
*/