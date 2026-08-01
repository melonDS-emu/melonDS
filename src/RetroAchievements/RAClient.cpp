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
#include <chrono>
#include <algorithm>
#include <functional>
#include <atomic>
#include <set>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <thread>

struct RAHttpJob {
    std::string url;
    std::string post_data;
    rc_client_server_callback_t callback;
    void* userdata;
};

struct RAPendingCallback {
    rc_client_server_callback_t callback;
    rc_api_server_response_t resp;
    std::string body;
    void* userdata;
};

static void PerformServerCall(const RAHttpJob& job);
static void EnsureHTTPThread();

static std::mutex s_httpMutex;
static std::condition_variable s_httpCV;
static std::queue<RAHttpJob> s_httpJobs;

static std::mutex s_cbMutex;
static std::queue<RAPendingCallback> s_pendingCallbacks;

static std::thread s_httpThread;
static std::atomic<bool> s_httpRunning{false};  

static std::once_flag s_curlInitOnce;
static thread_local CURL* s_curl = nullptr;
static void EnsureCurlInit()
{
    std::call_once(s_curlInitOnce, [] {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });
}
static bool s_raOffline = false;
static int s_pendingCount = 0;
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
    EnsureCurlInit();
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
            ctx->m_progressThrottle.erase(ach->id);
            break;
        }
        case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
        {
            printf("[RA][LB] START id=%u title=%s\n",
                event->leaderboard->id,
                event->leaderboard->title);
            auto* ctx = static_cast<RAContext*>(rc_client_get_userdata(client));
            if (ctx && ctx->onLeaderboardStarted)
                ctx->onLeaderboardStarted(event->leaderboard);
            break;
        }

        case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
        {
            printf("[RA][LB] FAILED id=%u\n",
                event->leaderboard->id);
            auto* ctx = static_cast<RAContext*>(rc_client_get_userdata(client));
            if (ctx && ctx->onLeaderboardFailed)
                ctx->onLeaderboardFailed(event->leaderboard);
            break;
        }

        case RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD:
        {
            auto* ctx = static_cast<RAContext*>(rc_client_get_userdata(client));
            if (ctx && ctx->m_onLeaderboardSubmitted && event->leaderboard_scoreboard) {
                bool isNewRecord = (strcmp(event->leaderboard_scoreboard->submitted_score, 
                                        event->leaderboard_scoreboard->best_score) == 0);

                const char* title = isNewRecord ? "New Personal Best!" : "Score Submitted";
                
                ctx->m_onLeaderboardSubmitted(
                    title, 
                    event->leaderboard_scoreboard->submitted_score, 
                    event->leaderboard_scoreboard->new_rank
                );
            }
            break;
        }

        case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
            break;
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
            if (ctx && ctx->m_onGameMastered)
            {
                const rc_client_game_t* game = rc_client_get_game_info(client);
                std::string gameTitle = (game && game->title) ? game->title : "Unknown Game";

                char rp_buffer[256];
                rc_client_get_rich_presence_message(client, rp_buffer, sizeof(rp_buffer));

                ctx->m_onGameMastered(
                    gameTitle,
                    rp_buffer
                );
            }
            break;
        }

        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
        {
            if (ctx->m_suppressProgressFrames > 0) break;

            const rc_client_achievement_t* ach = (const rc_client_achievement_t*)event->achievement;
            
            if (ctx->m_onAchievementProgress && ctx->ShouldShowProgress(ach->id, (unsigned)ach->measured_percent, 100)) {
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
            if (ctx->m_suppressProgressFrames > 0) break;

            const rc_client_achievement_t* ach = (const rc_client_achievement_t*)event->achievement;
            
            if (ctx->m_onAchievementProgress && ctx->ShouldShowProgress(ach->id, (unsigned)ach->measured_percent, 100)) {
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
    s_httpRunning = false;
    s_httpCV.notify_all();
    if (s_httpThread.joinable())
    s_httpThread.join();
    m_onRADisconnected = nullptr;
    m_onRAReconnected  = nullptr;
    m_onRAPendingSent  = nullptr;
    m_onLeaderboardSubmitted = nullptr;
    m_onLeaderboardTrackerUpdate = nullptr;
    m_onAchievementUnlocked = nullptr;
    m_onAchievementProgress = nullptr;
    m_onMeasuredProgress = nullptr;
    m_onChallenge = nullptr;
    m_onChallengeHide = nullptr;
    m_onGameMastered = nullptr;
    m_onLogin = nullptr;
    onGameLoaded = nullptr;
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
    {
        std::queue<RAPendingCallback> local;

        {
            std::lock_guard<std::mutex> lock(s_cbMutex);
            std::swap(local, s_pendingCallbacks);
        }

        while (!local.empty()) {
            auto& cb = local.front();
            if (cb.callback) {
                cb.callback(
                    cb.resp.body ? &cb.resp : nullptr,
                    cb.userdata
                );
            }
            local.pop();
        }
    }
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

    auto f0 = std::chrono::steady_clock::now();

    rc_client_do_frame(client);

    auto f1 = std::chrono::steady_clock::now();
    auto fms = std::chrono::duration_cast<std::chrono::milliseconds>(f1 - f0).count();

    if (fms > 20) {
        printf("[RA][FRAME] rc_client_do_frame blocked %lld ms\n", fms);
    }

    if (m_onLeaderboardTrackerUpdate) {
        if (activeTrackers.empty()) {
            m_onLeaderboardTrackerUpdate("");
        } else {
            std::string combinedDisplay = "";
            for (auto const& [id, tracker] : activeTrackers) {
                if (!combinedDisplay.empty()) combinedDisplay += "\n";
                combinedDisplay += tracker.display;
            }
            m_onLeaderboardTrackerUpdate(combinedDisplay.c_str());
        }
    }

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

        if (valueChanged) {
            ach.prev_value = value;
            if (m_onMeasuredProgress && ShouldShowProgress(ach.id, value, target)) {
                m_onMeasuredProgress(ach.id, value, target, formatted);
            }
        }
        for (auto& full : allAchievements)
        {
            if (full.id == ach.id)
            {
                if (!full.measured || full.value != value || full.target != target) 
                {
                    full.measured = true;
                    full.value = value;
                    full.target = target;
                    full.progressText = formatted;
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

void RAContext::SetEncoreMode(bool enabled)
{
    if (client) {
        rc_client_set_encore_mode_enabled(client, enabled ? 1 : 0);
    }
}

void RAContext::SetUnofficialEnabled(bool enabled)
{
    if (client) {
        rc_client_set_unofficial_enabled(client, enabled ? 1 : 0);
    }
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

static curl_blob s_cacertBlob = {
    (void*)_accacert,
    _accacert_len,
    CURL_BLOB_COPY
};

static std::set<uint32_t> s_pendingAchiIDs;

void RAContext::ServerCall(const rc_api_request_t* request,
                           rc_client_server_callback_t callback,
                           void* userdata,
                           rc_client_t*)
{
    EnsureHTTPThread();

    RAHttpJob job;
    job.url = request->url;
    job.post_data = request->post_data ? request->post_data : "";
    job.callback = callback;
    job.userdata = userdata;

    {
        std::lock_guard<std::mutex> lock(s_httpMutex);
        s_httpJobs.push(std::move(job));
    }

    s_httpCV.notify_one();
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
    m_onGameMastered = cb;
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
bool RAContext::ShouldShowProgress(uint32_t id, unsigned value, unsigned target) {
    if (target == 0) return false;

    auto nowObj = std::chrono::steady_clock::now();
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(nowObj.time_since_epoch()).count();
    
    auto& s = m_progressThrottle[id];
    float percent = (static_cast<float>(value) / static_cast<float>(target));

    if (now - s.rapidWindowStart > 3000) {
        s.rapidWindowStart = now;
        s.rapidChanges = 0;
    }
    if (value != s.lastValue) s.rapidChanges++;

    if (s.rapidChanges > 5) {
        s.lastValue = value;
        return false;
    }

    float milestone = 0.0f;
    if (percent >= 0.95f)      milestone = 0.95f;
    else if (percent >= 0.90f) milestone = 0.90f;
    else if (percent >= 0.75f) milestone = 0.75f;
    else if (percent >= 0.50f) milestone = 0.50f;
    else if (percent >= 0.25f) milestone = 0.25f;

    if (milestone > s.lastPercent) {
        s.lastPercent = milestone;
        s.lastValue = value;
        s.lastShownMs = now;
        return true; 
    }

    return false;
}
std::vector<std::string> RAContext::GetActiveTrackerTexts() {
    std::vector<std::string> texts;
    for (auto const& [id, tracker] : activeTrackers) {
        texts.push_back(tracker.display);
    }
    return texts;
}
void RAContext::SetOnLeaderboardTrackerUpdate(LeaderboardTrackerUpdateCallback cb) {
    m_onLeaderboardTrackerUpdate = std::move(cb);
}

void RAContext::SetOnLeaderboardSubmitted(LeaderboardSubmittedCallback cb) {
    m_onLeaderboardSubmitted = std::move(cb);
}
void RAContext::SetOnRADisconnected(std::function<void()> cb)
{
    m_onRADisconnected = std::move(cb);
}

void RAContext::SetOnRAReconnected(std::function<void()> cb)
{
    m_onRAReconnected = std::move(cb);
}

void RAContext::SetOnRAPendingSent(RAPendingSentCallback cb)
{
    m_onRAPendingSent = std::move(cb);
}

static void PerformServerCall(const RAHttpJob& job)
{
    EnsureCurlInit();

    CURL* curl = curl_easy_init();
    if (!curl) {
        return;
    }

    curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &s_cacertBlob);
    curl_easy_setopt(curl, CURLOPT_URL, job.url.c_str());

    if (!job.post_data.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, job.post_data.c_str());
    }

    std::string response_body;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        RAPendingCallback cb;
        cb.callback = job.callback;
        cb.userdata = job.userdata;
        cb.body = std::move(response_body);
        cb.resp.body = cb.body.c_str();
        cb.resp.body_length = (uint32_t)cb.body.size();
        cb.resp.http_status_code = (int)http_code;

        {
            std::lock_guard<std::mutex> lock(s_cbMutex);
            s_pendingCallbacks.push(std::move(cb));
        }
    }
    else {
        RAPendingCallback cb;
        cb.callback = job.callback;
        cb.userdata = job.userdata;
        cb.resp = {};
        {
            std::lock_guard<std::mutex> lock(s_cbMutex);
            s_pendingCallbacks.push(std::move(cb));
        }
    }

    curl_easy_cleanup(curl);
}

static void EnsureHTTPThread()
{
    if (s_httpRunning)
        return;

    s_httpRunning = true;
    s_httpThread = std::thread([] {
        while (s_httpRunning) {
            RAHttpJob job;

            {
                std::unique_lock<std::mutex> lock(s_httpMutex);
                s_httpCV.wait(lock, [] {
                    return !s_httpJobs.empty() || !s_httpRunning;
                });

                if (!s_httpRunning)
                    return;

                job = std::move(s_httpJobs.front());
                s_httpJobs.pop();
            }

            PerformServerCall(job);
        }
    });
}

void RAContext::ProcessAsyncCallbacks()
{
    std::lock_guard<std::mutex> lock(s_cbMutex);
    while (!s_pendingCallbacks.empty())
    {
        RAPendingCallback cb = std::move(s_pendingCallbacks.front());
        s_pendingCallbacks.pop();

        if (!cb.body.empty()) {
            cb.resp.body = cb.body.c_str();
            cb.resp.body_length = (uint32_t)cb.body.size();
        }

        if (cb.callback) {
            cb.callback(&cb.resp, cb.userdata);
        }
    }
}