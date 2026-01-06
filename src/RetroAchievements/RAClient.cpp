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
void RAContext::SetDisplayName(const char* name)
{
    m_displayName = name ? name : "";
}

const std::string& RAContext::GetDisplayName() const
{
    return RAContext::m_displayName;
}
static uint64_t g_ra_mem_reads = 0;
static uint64_t g_ra_mem_logged = 0;
static size_t CurlWrite(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::string* s = static_cast<std::string*>(userdata);
    s->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}
bool RAContext::CanPause(uint32_t* frames_remaining) {
    if (!client) return true;
    return rc_client_can_pause(client, frames_remaining) != 0;
}

void RAContext::SetPaused(bool paused) {
    m_IsPaused = paused;
}

RAContext::RAContext() : nds(nullptr), client(nullptr)
{
}

RAContext::~RAContext()
{
    Shutdown();
}

void RAContext::Init(melonDS::NDS* nds_)
{
    nds = nds_;
    client = rc_client_create(
        &RAContext::ReadMemory,
        &RAContext::ServerCall
        );
    rc_client_set_userdata(client, this);
    rc_client_set_allow_background_memory_reads(client, 1);
    rc_client_set_event_handler(client,
    [](const rc_client_event_t* event, rc_client_t* client)
    {
    switch (event->type)
    {
        case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
        {
            const rc_client_achievement_t* ach =
            (const rc_client_achievement_t*)event->achievement;
            RAContext* ctx = static_cast<RAContext*>(rc_client_get_userdata(client));
            if (!ctx) return;

            if (ctx->m_onAchievementUnlocked)
                ctx->m_onAchievementUnlocked(
                    ach->title,
                    ach->description,
                    ach->badge_url
                );
            break;
        }
        default:
        break;
    }
    });
};



void RAContext::Shutdown()
{
    if (client) {
        rc_client_destroy(client);
        client = nullptr;
    }
}

void RAContext::DoFrame()
{
    if (!nds || !client || m_IsPaused)
        return;
    std::string currentHash = pendingGameHash.value_or("");
    if (currentHash != m_lastHash) {
        gameLoaded = false;
        m_lastHash = currentHash;
        pendingLoadFailed = false;
        isLoading = false;
    }
    if (pendingLoadFailed || isLoading) return;
    if (!gameLoaded &&
        pendingGameHash.has_value() &&
        nds->IsGameRunning())
    {
        isLoading = true;
        LoadGame(pendingGameHash->c_str());
        return;
    }
    if (!gameLoaded) return;
    if (rc_client_is_processing_required(client) == 0) {
        return;
    } 
    rc_client_do_frame(client);
}
// ================= Login / Load =================

    void RAContext::SetLoggedIn(bool v)
    {
    if (m_logged_in == v)
        return;
    m_logged_in = v;
    
    if (RAContext::m_onLogin)
        RAContext::m_onLogin();
    }

static void LoginPasswordCallback(int result, const char* err, rc_client_t* client, void* userdata)
{
    RAContext* ctx = static_cast<RAContext*>(userdata);
    if (result == RC_OK)
    {
    const rc_client_user_t* user_info = rc_client_get_user_info(client);
    ctx->SetToken(user_info->token ? user_info->token : "");
    ctx->SetLoggedIn(true);
        if (ctx->onLoginResponse) {
            ctx->onLoginResponse(true, "Logged in Successfully!");
        }
    }
    else
    {
        ctx->loginSuccess = false;
        ctx->loginErrorMessage = err ? err : "Unknown error";
        ctx->SetLoggedIn(false);
            if (ctx->onLoginResponse) {
            ctx->onLoginResponse(false, ctx->loginErrorMessage);
            }
    }
}



void RAContext::LoadGame(const char* hash)
{
    if (!client) return;
    rc_client_begin_load_game(
        client,
        hash,
        [](int result, const char* err, rc_client_t*, void* userdata) {
            auto* context = static_cast<RAContext*>(userdata);

            if (result == 0) {
                context->pendingLoadFailed = false;
                context->isLoading = false;
                context->gameLoaded = true;
                context->currentGameInfo = rc_client_get_game_info(context->client);
                if (context->onGameLoaded){
                    context->onGameLoaded();}
            }
            else {
                context->gameLoaded = false;
                context->pendingLoadError = err;
                context->pendingLoadFailed = true;
                context->isLoading = false;
                if (context->onGameLoaded) {
                    context->onGameLoaded(); 
                }
                
            }
        },
        this
    );
}

// ================= CALLBACKS rcheevos =================

uint32_t RAContext::ReadMemory(uint32_t address,
                               uint8_t* buffer,
                               uint32_t size,
                               rc_client_t* client)
{
    if (address >= 0x04000000 && address < 0x04000400)
    {
}
    g_ra_mem_reads++;
    RAContext* ctx =
        static_cast<RAContext*>(rc_client_get_userdata(client));

    if (buffer && size > 0)
        memset(buffer, 0, size);

    if (!ctx || !ctx->nds || !buffer || size == 0)
        return 0;

    // ===== ARM9 MAIN RAM =====
    constexpr uint32_t ARM9_RA_BASE   = 0x00000000;
    constexpr uint32_t ARM9_RA_SIZE   = 0x00400000;
    constexpr uint32_t ARM9_PHYS_BASE = 0x02000000;

    if (address >= ARM9_RA_BASE && address + size <= ARM9_RA_SIZE)
    {
        memcpy(buffer,
               ctx->nds->MainRAM + address,
               size);
        return size;
    }

    // ===== WRAM =====
    constexpr uint32_t WRAM_RA_BASE   = 0x03000000;
    constexpr uint32_t WRAM_SIZE      = 0x00008000;

    if (address >= WRAM_RA_BASE &&
        address + size <= WRAM_RA_BASE + WRAM_SIZE)
    {
        uint32_t off = address - WRAM_RA_BASE;
        memcpy(buffer,
               ctx->nds->SharedWRAM + off,
               size);
        return size;
    }

    // ===== ARM7 RAM =====
    constexpr uint32_t ARM7_RA_BASE   = 0x01000000;
    constexpr uint32_t ARM7_SIZE      = 0x00010000;
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
    return 0;
}

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

static void RemoveTempFile(const std::string& path) {
    if (!path.empty()) {
#ifdef _WIN32
        DeleteFileA(path.c_str());
#else
        unlink(path.c_str());
#endif
    }
}

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

    std::string cacertPath = WriteCACertToTempFile();
    if (cacertPath.empty()) {
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
        callback(nullptr, userdata);
    }

    curl_easy_cleanup(curl);
    RemoveTempFile(cacertPath);
}
void RAContext::Reset() {
    if (client)
        rc_client_reset(client);
}

void RAContext::LoginWithPassword(const char* username, const char* password, bool hardcore)
{
    if (!client) return;

    rc_client_set_hardcore_enabled(client, hardcore ? 1 : 0);

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
}

void RAContext::SetPendingGameHash(const char* hash)
{
    if (!hash || !*hash)
        return;

    pendingGameHash = std::string(hash);
    gameLoaded = false;
}
void RAContext::AttachNDS(melonDS::NDS* nds_)
{
    if (nds == nds_)
        return;

    nds = nds_;
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
}

void RAContext::LoginNow()
{
    if (m_logged_in)
        return;

    if (!m_user.empty() && !m_password.empty())
    {
        LoginWithPassword(m_user.c_str(), m_password.c_str(), m_hardcore);
    }
}

void RAContext::SetOnAchievementUnlocked(AchievementUnlockedCallback cb)
{
    RAContext::m_onAchievementUnlocked = std::move(cb);
}

void RAContext::SetOnGameLoadedCallback(std::function<void()> cb)
{
    onGameLoaded = std::move(cb);
}