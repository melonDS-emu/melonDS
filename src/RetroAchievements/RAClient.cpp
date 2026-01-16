#include "RAClient.h"
#include "../NDS.h"
#include <cstring>
#include <string>
#include <curl/curl.h>
#include <cstdio>
#include <rcheevos/include/rc_consoles.h>
#include <rcheevos/include/rc_client.h>
#include <rcheevos/src/rc_version.h>
#include <rcheevos/include/rc_runtime.h>
#include "RetroAchievements/cacert.c"
#include "version.h"
#include "Savestate.h"

extern const unsigned char _accacert[];
extern const size_t _accacert_len;
void RAContext::SetDisplayName(const char* name)
{
    m_displayName = name ? name : "";
}

const std::string& RAContext::GetDisplayName() const
{
    return m_displayName;
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
    rc_runtime_init(&m_runtime);
    rc_client_set_event_handler(client,
    [](const rc_client_event_t* event, rc_client_t* client)
    {
    auto* ctx = static_cast<RAContext*>(rc_client_get_userdata(client));
    if (!ctx) return;
    switch (event->type)
    {
        case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
        {
            const rc_client_achievement_t* ach = (const rc_client_achievement_t*)event->achievement;
            
            for (auto& a : ctx->allAchievements)
            {
                if (a.id == ach->id)
                {
                    a.unlocked = true;
                    a.measured = false; 
                    a.state = ach->state;
                    break;
                }
            }

            if (ctx->m_onAchievementUnlocked)
                ctx->m_onAchievementUnlocked(
                    ach->title,
                    ach->description,
                    ach->badge_url
                );
            break;
        }
        case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
        {
            auto* ctx = static_cast<RAContext*>(rc_client_get_userdata(client));
            if (ctx && ctx->onLeaderboardStarted)
                ctx->onLeaderboardStarted(event->leaderboard);
            break;
        }

        case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
        {
            auto* ctx = static_cast<RAContext*>(rc_client_get_userdata(client));
            if (ctx && ctx->onLeaderboardFailed)
                ctx->onLeaderboardFailed(event->leaderboard);
            break;
        }

        case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
        {
            auto* ctx = static_cast<RAContext*>(rc_client_get_userdata(client));
            if (ctx && ctx->onLeaderboardSubmitted)
                ctx->onLeaderboardSubmitted(event->leaderboard);
            break;
        }
        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW:
        {
            if (event->leaderboard_tracker) {
                ctx->activeTrackers[event->leaderboard_tracker->id] = { 
                    event->leaderboard_tracker->id, 
                    event->leaderboard_tracker->display 
                };
            }
            break;
        }
        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE:
        {
            if (event->leaderboard_tracker) {
                auto it = ctx->activeTrackers.find(event->leaderboard_tracker->id);
                if (it != ctx->activeTrackers.end()) {
                    it->second.display = event->leaderboard_tracker->display;
                }
            }
            break;
        }
        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE:
        {
            if (event->leaderboard_tracker) {
                ctx->activeTrackers.erase(event->leaderboard_tracker->id);
            }
            break;
        }
        case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
        {
            const rc_client_achievement_t* ach = (const rc_client_achievement_t*)event->achievement;
            if (ach && ctx->m_onChallenge) {
                ctx->m_onChallenge(ach->badge_name);
            }
            break;
        }
        case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE:
        {
            if (ctx->m_onChallengeHide)
                ctx->m_onChallengeHide();
            break;
        }
        case RC_CLIENT_EVENT_GAME_COMPLETED:
        {
        RAContext* ctx = static_cast<RAContext*>(rc_client_get_userdata(client));
            if (ctx && ctx->m_OnGameMastered)
            {
                const rc_client_game_t* game = rc_client_get_game_info(client);
                std::string gameTitle = (game && game->title) ? game->title : "Unknown Game";

                char rp_buffer[256];
                rc_client_get_rich_presence_message(client, rp_buffer, sizeof(rp_buffer));

                ctx->m_OnGameMastered(
                    gameTitle,
                    rp_buffer
                );
            }
            break;
        }

        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
        {
            if (ctx->m_suppressProgressFrames > 0) {
            break; 
            }
            const rc_client_achievement_t* ach = (const rc_client_achievement_t*)event->achievement;
            
            if (ctx->m_onAchievementProgress) {
                ctx->m_onAchievementProgress(
                    ach->title,
                    ach->measured_progress, 
                    ach->badge_locked_url ? ach->badge_locked_url : ach->badge_url
                );
            }
            break;
        }

        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:
        {
            if (ctx->m_suppressProgressFrames > 0) {
            break; 
            }
            const rc_client_achievement_t* ach = (const rc_client_achievement_t*)event->achievement;
            
            if (ctx->m_onAchievementProgress) {
                ctx->m_onAchievementProgress(
                    ach->title,
                    ach->measured_progress,
                    ach->badge_locked_url ? ach->badge_locked_url : ach->badge_url
                );
            }
            break;
        }

        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE:
        {
            if (ctx->m_onChallengeHide) {
            }
            break;
        }
        default:
        break;
    }
    });
}

void RAContext::Shutdown()
{
    SavePlaytime();
    if (client) {
        rc_client_destroy(client);
        client = nullptr;
    }
    rc_runtime_destroy(&m_runtime);
    ResetGameState();
}

uint32_t RC_CCONV RuntimePeek(uint32_t address, uint32_t num_bytes, void* ud)
{
    RAContext* ctx = static_cast<RAContext*>(ud);
    if (!ctx || !ctx->client)
        return 0;

    uint8_t buffer[4] = {0};

    ctx->ReadMemory(address, buffer, num_bytes, ctx->client);

    uint32_t value = 0;
    memcpy(&value, buffer, num_bytes);
    return value;
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
    if (m_suppressProgressFrames > 0) 
        m_suppressProgressFrames--;
    rc_runtime_do_frame(
        &m_runtime,
        nullptr,
        &RuntimePeek,
        this,
        nullptr
    );

    rc_client_do_frame(client);

    UpdateMeasuredAchievements();

    
    static int playtimeSaveCounter = 0;
    if (gameLoaded && ++playtimeSaveCounter >= 3600) {
        SavePlaytime();
        playtimeSaveCounter = 0;
    }
}

void RAContext::UpdateMeasuredAchievements()
{
    if (!client || trackedAchievements.empty())
        return;

    char formatted[128];

    for (auto& ach : trackedAchievements)
    {
        unsigned value = 0;
        unsigned target = 0;

        bool isMeasured = rc_runtime_get_achievement_measured(&m_runtime, ach.id, &value, &target) != 0;

        if (!isMeasured)
            continue;

        rc_runtime_format_achievement_measured(&m_runtime, ach.id, formatted, sizeof(formatted));
        
        bool valueChanged = (value != ach.prev_value);
        ach.prev_value = value;

        if (valueChanged && m_onMeasuredProgress) {
             m_onMeasuredProgress(ach.id, value, target, formatted);
        }
        for (auto& full : allAchievements)
        {
            if (full.id == ach.id)
            {
                if (!full.measured || full.value != value || valueChanged) 
                {
                    full.measured = true;
                    full.value = value;
                    full.target = target;
                    full.progressText = std::string(formatted);
                    full.measured_percent = (target > 0) 
                        ? (static_cast<float>(value) / static_cast<float>(target) * 100.0f) 
                        : 0.0f;
                }
                break;
            }
        }
    }
}

void RAContext::SetLoggedIn(bool v)
{
    if (m_logged_in == v)
        return;
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
        [](int result, const char* err, rc_client_t*, void* userdata)
        {
            auto* context = static_cast<RAContext*>(userdata);

            if (result != RC_OK)
            {
                context->gameLoaded = false;
                context->pendingLoadError = err ? err : "Unknown error";
                context->pendingLoadFailed = true;
                context->isLoading = false;

                if (context->onGameLoaded)
                    context->onGameLoaded();

                return;
            }
            
            rc_runtime_reset(&context->m_runtime);
            context->ResetGameState();
            context->trackedAchievements.clear();
            context->allAchievements.clear();
            context->pendingLoadFailed = false;
            context->isLoading = false;
            context->gameLoaded = true;
            context->currentGameInfo = rc_client_get_game_info(context->client);

            if (context->currentGameInfo) {
                context->m_currentGameId = context->currentGameInfo->id;
                context->m_sessionStart = time(nullptr);
                context->m_accumulatedTime = 0;

                if (context->m_playtimeLoader) {
                    context->m_accumulatedTime = context->m_playtimeLoader(context->m_currentGameId);
                }
            }

            rc_client_achievement_list_t* list = rc_client_create_achievement_list(
                context->client,
                RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
                RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS
            );

            for (uint32_t i = 0; i < list->num_buckets; ++i) {
                const rc_client_achievement_bucket_t& bucket = list->buckets[i];

                for (uint32_t j = 0; j < bucket.num_achievements; ++j) {
                    const rc_client_achievement_t* ach = bucket.achievements[j];

                    FullAchievement fa;
                    fa.id = ach->id;
                    fa.title = ach->title ? ach->title : "";
                    fa.description = ach->description ? ach->description : "";
                    fa.points = ach->points;
                    fa.unlocked = (ach->state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);

                    fa.measured = false;
                    fa.progressText.clear();
                    fa.measured_percent = 0.0f;
                    fa.value = 0;
                    fa.target = 0;

                    if (ach->type == RC_CLIENT_ACHIEVEMENT_TYPE_PROGRESSION ||
                        ach->type == RC_CLIENT_ACHIEVEMENT_TYPE_STANDARD)
                    {
                        context->trackedAchievements.push_back({ ach->id, 0 });
                    }

                    fa.value = 0; 
                    fa.target = 0;

                    std::strncpy(fa.badge_name, ach->badge_name, sizeof(fa.badge_name));
                    fa.unlock_time = ach->unlock_time;
                    fa.state = ach->state;
                    fa.category = ach->category;
                    fa.bucket = ach->bucket;
                    fa.rarity = ach->rarity;
                    fa.rarity_hardcore = ach->rarity_hardcore;
                    fa.type = ach->type;
                    fa.badge_url = ach->badge_url;
                    fa.badge_locked_url = ach->badge_locked_url;

                    fa.bucket_type = bucket.bucket_type;
                    fa.label = bucket.label ? bucket.label : "";
                    fa.num_achievements = bucket.num_achievements;
                    fa.achievements = bucket.achievements;

                    context->allAchievements.push_back(fa);
                }
            }

            rc_client_destroy_achievement_list(list);

            context->m_suppressProgressFrames = 50;

            if (context->onGameLoaded)
                context->onGameLoaded();
        },
        this
    );
}

uint32_t RAContext::ReadMemory(uint32_t address, uint8_t* buffer, uint32_t size, rc_client_t* client)
{
    g_ra_mem_reads++;
    RAContext* ctx = static_cast<RAContext*>(rc_client_get_userdata(client));

    if (!ctx || !ctx->nds || !buffer || size == 0)
        return 0;

    memset(buffer, 0, size);

    if (address < 0x00400000)
    {
        uint32_t count = std::min(size, 0x00400000 - address);
        memcpy(buffer, ctx->nds->MainRAM + address, count);
        return count;
    }

    if (address >= 0x00400000 && address < 0x00408000)
    {
        uint32_t offset = address - 0x00400000;
        uint32_t count = std::min(size, 0x00408000 - address);
        memcpy(buffer, ctx->nds->SharedWRAM + offset, count);
        return count;
    }

    if (address >= 0x00408000 && address < 0x00418000)
    {
        uint32_t offset = address - 0x00408000;
        uint32_t count = std::min(size, 0x00418000 - address);
        memcpy(buffer, ctx->nds->ARM7WRAM + offset, count);
        return count;
    }

    if (address >= 0x00480000 && address < 0x00580000)
    {
        uint32_t offset = address - 0x00480000;
        uint32_t remaining = std::min(size, 0x00580000 - address);
        uint32_t copied = 0;

        struct { uint8_t* data; uint32_t size; } banks[] = {
            {ctx->nds->GPU.VRAM_A, 128*1024},
            {ctx->nds->GPU.VRAM_B, 128*1024},
            {ctx->nds->GPU.VRAM_C, 128*1024},
            {ctx->nds->GPU.VRAM_D, 128*1024},
            {ctx->nds->GPU.VRAM_E,  64*1024},
            {ctx->nds->GPU.VRAM_F,  16*1024},
            {ctx->nds->GPU.VRAM_G,  16*1024},
            {ctx->nds->GPU.VRAM_H,  32*1024},
            {ctx->nds->GPU.VRAM_I,  16*1024}
        };
        uint32_t bank_offset = 0;

        for (int i = 0; i < 9; ++i) {
            if (offset >= bank_offset + banks[i].size) {
                bank_offset += banks[i].size;
                continue;
            }

            uint32_t local_offset = offset - bank_offset;
            uint32_t to_copy = std::min(remaining - copied, banks[i].size - local_offset);
            memcpy(buffer + copied, banks[i].data + local_offset, to_copy);
            copied += to_copy;

            if (copied >= remaining) break;

            bank_offset += banks[i].size;
        }

        return copied;
    }

    if (address >= 0x00600000 && address < 0x00602000)
    {
        uint32_t offset = address - 0x00600000;
        uint32_t count = std::min(size, 0x00602000 - address);
        uint32_t io_base = 0x04000000 + offset;

        for (uint32_t i = 0; i < count; ++i) {
            buffer[i] = ctx->nds->ARM9Read8(io_base + i);
        }
        return count;
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
    std::string rcheevosVer =
    std::to_string(RCHEEVOS_VERSION_MAJOR) + "." +
    std::to_string(RCHEEVOS_VERSION_MINOR) + "." +
    std::to_string(RCHEEVOS_VERSION_PATCH);

    std::string ua =
        std::string("melonDS-Menel-RA/") + MELONDS_VERSION +
    #if defined(_WIN32)
        " (Windows)"
    #elif defined(__APPLE__)
        " (macOS)"
    #elif defined(__linux__)
        " (Linux)"
    #else
        " (UnknownOS)"
    #endif
        + " rcheevos/" + rcheevosVer;

    curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());
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
        ResetGameState();
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
    ResetGameState();
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
    m_onAchievementUnlocked = std::move(cb);
}
void RAContext::SetOnAchievementProgress(AchievementProgressCallback cb)
{
    m_onAchievementProgress = std::move(cb);
}

void RAContext::SetOnGameLoadedCallback(std::function<void()> cb)
{
    onGameLoaded = std::move(cb);
}
void RAContext::SetOnMeasuredProgress(MeasuredProgressCallback cb)
{
    m_onMeasuredProgress = std::move(cb);
}
void RAContext::ResetGameState()
{
    trackedAchievements.clear();
    allAchievements.clear();
    activeTrackers.clear();
    m_sessionStart = 0;
    m_accumulatedTime = 0;
    m_currentGameId = 0;
}
void RAContext::showProgressIndicator(const rc_client_achievement_t* ach)
{
    if (ach) {
        printf("[RA] PROGRESS SHOW: %s (%s)\n", ach->title, ach->measured_progress);
    }
}

void RAContext::updateProgressIndicator(const rc_client_achievement_t* ach)
{
    if (ach) {
        printf("[RA] PROGRESS UPDATE: %s (%s)\n", ach->title, ach->measured_progress);
    }
}

void RAContext::hideProgressIndicator()
{
    printf("[RA] PROGRESS HIDE\n");
}

void RAContext::SaveSavestate(melonDS::Savestate* file)
{
    printf("wywołanie savesavestate");
    if (!client)
        return;

    uint32_t size = rc_client_progress_size(client);
    std::vector<uint8_t> buffer(size);

    if (rc_client_serialize_progress(client, buffer.data()) != RC_OK)
        return;

    file->Section("RACH");
    file->Var32(&size);
    file->VarArray(buffer.data(), size);
}

void RAContext::LoadSavestate(melonDS::Savestate* file)
{
    printf("wywołanie loadsavestate");
    if (!client)
        return;

    file->Section("RACH");

    if (file->Error)
    {
        rc_client_deserialize_progress(client, nullptr);
        return;
    }

    uint32_t size = 0;
    file->Var32(&size);

    std::vector<uint8_t> buffer(size);
    file->VarArray(buffer.data(), size);

    rc_client_deserialize_progress(client, buffer.data());
}
void RAContext::SetOnChallenge(ChallengeShowCallback cb)
{
    m_onChallenge = cb;
}

void RAContext::SetOnChallengeHide(ChallengeHideCallback cb)
{
    m_onChallengeHide = cb;
}

void RAContext::SetOnGameMastered(GameMasteredCallback cb)
{
    m_OnGameMastered = cb;
}
const std::vector<RAContext::FullAchievement>& RAContext::GetAllAchievements()
{
    if (!client) return allAchievements;

    for (auto& ach : allAchievements)
    {
        const rc_client_achievement_t* rc_ach = rc_client_get_achievement_info(client, ach.id);
        
        if (rc_ach)
        {
            ach.unlocked = (rc_ach->unlocked != RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
            
            if (rc_ach->measured_progress[0] != '\0')
            {
                ach.measured = true;
                ach.progressText = std::string(rc_ach->measured_progress);
                ach.measured_percent = rc_ach->measured_percent;
            }
            else
            {
                ach.measured = false;
                ach.measured_percent = 0.0f;
                ach.progressText = "";
            }
        }
    }

    return allAchievements;
}
void RAContext::SavePlaytime() {
    if (m_currentGameId == 0 || !gameLoaded) return;

    uint32_t totalMinutes = GetGamePlaytime();

    if (m_playtimeSaver) {
        m_playtimeSaver(m_currentGameId, totalMinutes);
    }
}