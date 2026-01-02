#include "rc_client.h"

#include "rc_consoles.h"
#include "rc_internal.h"
#include "rc_api_runtime.h"

#include "../src/rc_client_internal.h"
#include "../src/rc_client_external.h"
#include "../src/rc_version.h"

#include "test_framework.h"

#ifdef RC_CLIENT_SUPPORTS_HASH
#include "rc_hash.h"
#include "rhash/data.h"
#include "rhash/mock_filereader.h"
#endif

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__unix__) && __STDC_VERSION__ >= 199309L
#include <unistd.h>
#else
#define RC_NO_SLEEP
#endif

static rc_client_t* g_client;
static void* g_callback_userdata = &g_client; /* dummy object to use for callback userdata validation */

#define GENERIC_ACHIEVEMENT_JSON(id, memaddr) "{\"ID\":" id ",\"Title\":\"Achievement " id "\"," \
      "\"Description\":\"Desc " id "\",\"Flags\":3,\"Points\":5,\"MemAddr\":\"" memaddr "\"," \
      "\"Author\":\"User1\",\"BadgeName\":\"00" id "\",\"Created\":1367266583,\"Modified\":1376929305}"

#define UNOFFICIAL_ACHIEVEMENT_JSON(id, memaddr) "{\"ID\":" id ",\"Title\":\"Achievement " id "\"," \
      "\"Description\":\"Desc " id "\",\"Flags\":5,\"Points\":5,\"MemAddr\":\"" memaddr "\"," \
      "\"Author\":\"User1\",\"BadgeName\":\"00" id "\",\"Created\":1367266583,\"Modified\":1376929305}"

#define TYPED_ACHIEVEMENT_JSON(id, memaddr, type, rarity, rarity_hardcore) "{\"ID\":" id ",\"Title\":\"Achievement " id "\"," \
      "\"Description\":\"Desc " id "\",\"Flags\":3,\"Points\":5,\"MemAddr\":\"" memaddr "\"," \
      "\"Type\":\"" type "\",\"Rarity\":" rarity ",\"RarityHardcore\":" rarity_hardcore "," \
      "\"Author\":\"User1\",\"BadgeName\":\"00" id "\",\"Created\":1367266583,\"Modified\":1376929305}"

#define GENERIC_LEADERBOARD_JSON(id, memaddr, format) "{\"ID\":" id ",\"Title\":\"Leaderboard " id "\"," \
      "\"Description\":\"Desc " id "\",\"Mem\":\"" memaddr "\",\"Format\":\"" format "\"}"

static const char* patchdata_empty = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":[],"
      "\"Leaderboards\":[]"
    "}]}";

static const char* patchdata_2ach_0lbd = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
       "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
        "\"MemAddr\":\"0xH0001=3_0xH0002=7\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
        "\"Created\":1367266583,\"Modified\":1376929305},"
       "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
        "\"MemAddr\":\"0xH0001=2_0x0002=9\",\"Author\":\"User1\",\"BadgeName\":\"00235\","
        "\"Created\":1376970283,\"Modified\":1376970283}"
      "],"
      "\"Leaderboards\":[]"
    "}]}";

static const char* patchdata_2ach_1lbd = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
       "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
        "\"MemAddr\":\"0xH0001=3_0xH0002=7\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
        "\"Created\":1367266583,\"Modified\":1376929305},"
       "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
        "\"MemAddr\":\"0xH0001=2_0x0002=9\",\"Author\":\"User1\",\"BadgeName\":\"00235\","
        "\"Created\":1376970283,\"Modified\":1376970283}"
      "],"
      "\"Leaderboards\":["
       "{\"ID\":4401,\"Title\":\"Leaderboard1\",\"Description\":\"Desc1\","
        "\"Mem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xH000D=2::VAL:0x 000E\",\"Format\":\"SCORE\"}"
      "]"
    "}]}";

static const char* patchdata_rich_presence_only = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\n@Number(0xH0001)\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":[],"
      "\"Leaderboards\":[]"
    "}]}";

static const char* patchdata_leaderboard_only = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\n@Number(0xH0001)\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":[],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE")
      "]"
    "}]}";

static const char* patchdata_leaderboard_immediate_submit = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\n@Number(0xH0001)\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":[],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0=1::SUB:1=1::VAL:0x 000E", "SCORE")
      "]"
    "}]}";

static const char* patchdata_bounds_check_system = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":7,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("1", "0xH0000=5") ","
        GENERIC_ACHIEVEMENT_JSON("2", "0xHFFFF=5") ","
        GENERIC_ACHIEVEMENT_JSON("3", "0xH10000=5") ","
        GENERIC_ACHIEVEMENT_JSON("4", "0x FFFE=5") ","
        GENERIC_ACHIEVEMENT_JSON("5", "0x FFFF=5") ","
        GENERIC_ACHIEVEMENT_JSON("6", "0x 10000=5") ","
        GENERIC_ACHIEVEMENT_JSON("7", "I:0xH0000_0xHFFFF=5")
      "],"
      "\"Leaderboards\":[]"
    "}]}";

static const char* patchdata_bounds_check_8 = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":7,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("408", "0xH0004=5") ","
        GENERIC_ACHIEVEMENT_JSON("508", "0xH0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("608", "0xH0006=5") ","
        GENERIC_ACHIEVEMENT_JSON("708", "0xH0007=5") ","
        GENERIC_ACHIEVEMENT_JSON("808", "0xH0008=5") ","
        GENERIC_ACHIEVEMENT_JSON("416", "0x 0004=5") ","
        GENERIC_ACHIEVEMENT_JSON("516", "0x 0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("616", "0x 0006=5") ","
        GENERIC_ACHIEVEMENT_JSON("716", "0x 0007=5") ","
        GENERIC_ACHIEVEMENT_JSON("816", "0x 0008=5") ","
        GENERIC_ACHIEVEMENT_JSON("424", "0xW0004=5") ","
        GENERIC_ACHIEVEMENT_JSON("524", "0xW0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("624", "0xW0006=5") ","
        GENERIC_ACHIEVEMENT_JSON("724", "0xW0007=5") ","
        GENERIC_ACHIEVEMENT_JSON("824", "0xW0008=5") ","
        GENERIC_ACHIEVEMENT_JSON("432", "0xX0004=5") ","
        GENERIC_ACHIEVEMENT_JSON("532", "0xX0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("632", "0xX0006=5") ","
        GENERIC_ACHIEVEMENT_JSON("732", "0xX0007=5") ","
        GENERIC_ACHIEVEMENT_JSON("832", "0xX0008=5")
      "],"
      "\"Leaderboards\":[]"
    "}]}";

static const char* patchdata_exhaustive = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("5", "0xH0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("6", "M:0xH0006=6") ","
        GENERIC_ACHIEVEMENT_JSON("7", "T:0xH0007=7_0xH0001=1") ","
        GENERIC_ACHIEVEMENT_JSON("8", "0xH0008=8") ","
        GENERIC_ACHIEVEMENT_JSON("9", "0xH0009=9") ","
        GENERIC_ACHIEVEMENT_JSON("70", "M:0xX0010=100000") ","
        GENERIC_ACHIEVEMENT_JSON("71", "G:0xX0010=100000")
      "],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("45", "STA:0xH000A=1::CAN:0xH000C=2::SUB:0xH000D=1::VAL:0xH000E", "SCORE") ","   /* different size */
        GENERIC_LEADERBOARD_JSON("46", "STA:0xH000A=1::CAN:0xH000C=3::SUB:0xH000D=1::VAL:0x 000E", "VALUE") ","   /* different format */
        GENERIC_LEADERBOARD_JSON("47", "STA:0xH000A=1::CAN:0xH000C=4::SUB:0xH000D=2::VAL:0x 000E", "SCORE") ","   /* different submit */
        GENERIC_LEADERBOARD_JSON("48", "STA:0xH000A=2::CAN:0xH000C=5::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","   /* different start */
        GENERIC_LEADERBOARD_JSON("51", "STA:0xH000A=3::CAN:0xH000C=6::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE") "," /* hit count */
        GENERIC_LEADERBOARD_JSON("52", "STA:0xH000B=3::CAN:0xH000C=7::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE")     /* hit count */
      "]"
    "}]}";


static const char* patchdata_exhaustive_typed = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
        TYPED_ACHIEVEMENT_JSON("5", "0xH0005=5", "", "100.0", "99.5") ","
        TYPED_ACHIEVEMENT_JSON("6", "M:0xH0006=6", "progression", "95.3", "84.7") ","
        TYPED_ACHIEVEMENT_JSON("7", "T:0xH0007=7_0xH0001=1", "missable", "47.6", "38.2") ","
        TYPED_ACHIEVEMENT_JSON("8", "0xH0008=8", "progression", "86.0", "73.1") ","
        TYPED_ACHIEVEMENT_JSON("9", "0xH0009=9", "win_condition", "81.4", "66.4") ","
        TYPED_ACHIEVEMENT_JSON("70", "M:0xX0010=100000", "missable", "11.4", "6.3") ","
        TYPED_ACHIEVEMENT_JSON("71", "G:0xX0010=100000", "", "8.7", "3.8")
      "],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("45", "STA:0xH000A=1::CAN:0xH000C=2::SUB:0xH000D=1::VAL:0xH000E", "SCORE") ","   /* different size */
        GENERIC_LEADERBOARD_JSON("46", "STA:0xH000A=1::CAN:0xH000C=3::SUB:0xH000D=1::VAL:0x 000E", "VALUE") ","   /* different format */
        GENERIC_LEADERBOARD_JSON("47", "STA:0xH000A=1::CAN:0xH000C=4::SUB:0xH000D=2::VAL:0x 000E", "SCORE") ","   /* different submit */
        GENERIC_LEADERBOARD_JSON("48", "STA:0xH000A=2::CAN:0xH000C=5::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","   /* different start */
        GENERIC_LEADERBOARD_JSON("51", "STA:0xH000A=3::CAN:0xH000C=6::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE") "," /* hit count */
        GENERIC_LEADERBOARD_JSON("52", "STA:0xH000B=3::CAN:0xH000C=7::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE")     /* hit count */
      "]"
    "}]}";

static const char* patchdata_big_ids = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("5", "0xH0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("6", "M:0xH0006=6") ","
        GENERIC_ACHIEVEMENT_JSON("4294967295", "0xH0009=9") "," /* UINT_MAX */
        GENERIC_ACHIEVEMENT_JSON("71", "G:0xX0010=100000")
      "],"
      "\"Leaderboards\":[]"
    "}]}";

#define HIDDEN_LEADERBOARD_JSON(id, memaddr, format) "{\"ID\":" id ",\"Title\":\"Leaderboard " id "\"," \
      "\"Description\":\"Desc " id "\",\"Mem\":\"" memaddr "\",\"Format\":\"" format "\",\"Hidden\":true}"

static const char* patchdata_leaderboards_hidden = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":[],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        HIDDEN_LEADERBOARD_JSON("45", "STA:0xH000A=1::CAN:0xH000C=2::SUB:0xH000D=1::VAL:0xH000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("46", "STA:0xH000A=1::CAN:0xH000C=3::SUB:0xH000D=1::VAL:0x 000E", "VALUE") ","
        GENERIC_LEADERBOARD_JSON("47", "STA:0xH000A=1::CAN:0xH000C=4::SUB:0xH000D=2::VAL:0x 000E", "SCORE") ","
        HIDDEN_LEADERBOARD_JSON("48", "STA:0xH000A=2::CAN:0xH000C=5::SUB:0xH000D=1::VAL:0x 000E", "SCORE")
      "]"
    "}]}";

static const char* patchdata_unofficial_unsupported = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
       "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
        "\"MemAddr\":\"0xH0001=1_0xH0002=7\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
        "\"Created\":1367266583,\"Modified\":1376929305},"
       "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":5,\"Points\":2,"
        "\"MemAddr\":\"0xH0001=2_0x0002=9\",\"Author\":\"User1\",\"BadgeName\":\"00235\","
        "\"Created\":1376970283,\"Modified\":1376970283},"
       "{\"ID\":5503,\"Title\":\"Ach3\",\"Description\":\"Desc3\",\"Flags\":3,\"Points\":2,"
        "\"MemAddr\":\"0xHFEFEFEFE=2_0x0002=9\",\"Author\":\"User1\",\"BadgeName\":\"00236\","
        "\"Created\":1376971283,\"Modified\":1376971283}"
      "],"
      "\"Leaderboards\":["
       "{\"ID\":4401,\"Title\":\"Leaderboard1\",\"Description\":\"Desc1\","
        "\"Mem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xHFEFEFEFE=2::VAL:0x 000E\",\"Format\":\"SCORE\"}"
      "]"
    "}]}";

static const char* patchdata_subset = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("5", "0xH0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("6", "M:0xH0006=6") ","
        GENERIC_ACHIEVEMENT_JSON("7", "T:0xH0007=7_0xH0001=1") ","
        GENERIC_ACHIEVEMENT_JSON("8", "0xH0008=8") ","
        GENERIC_ACHIEVEMENT_JSON("9", "0xH0009=9") ","
        GENERIC_ACHIEVEMENT_JSON("70", "M:0xX0010=100000") ","
        GENERIC_ACHIEVEMENT_JSON("71", "G:0xX0010=100000")
      "],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("45", "STA:0xH000A=1::CAN:0xH000C=2::SUB:0xH000D=1::VAL:0xH000E", "SCORE") ","   /* different size */
        GENERIC_LEADERBOARD_JSON("46", "STA:0xH000A=1::CAN:0xH000C=3::SUB:0xH000D=1::VAL:0x 000E", "VALUE") ","   /* different format */
        GENERIC_LEADERBOARD_JSON("47", "STA:0xH000A=1::CAN:0xH000C=4::SUB:0xH000D=2::VAL:0x 000E", "SCORE") ","   /* different submit */
        GENERIC_LEADERBOARD_JSON("48", "STA:0xH000A=2::CAN:0xH000C=5::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","   /* different start */
        GENERIC_LEADERBOARD_JSON("51", "STA:0xH000A=3::CAN:0xH000C=6::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE") "," /* hit count */
        GENERIC_LEADERBOARD_JSON("52", "STA:0xH000B=3::CAN:0xH000C=7::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE")     /* hit count */
      "]"
    "},{"
      "\"AchievementSetId\":2345,\"GameId\":1235,\"Title\":\"Bonus\",\"Type\":\"bonus\","
      "\"ImageIconUrl\":\"http://server/Images/112234.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("5501", "0xH0017=7") ","
        GENERIC_ACHIEVEMENT_JSON("5502", "0xH0018=8") ","
        GENERIC_ACHIEVEMENT_JSON("5503", "0xH0019=9")
      "],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("81", "STA:0xH0008=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("82", "STA:0xH0008=2::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE")
      "]"
    "}]}";

static const char* patchdata_subset_unofficial_unsupported = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("5", "0xH0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("6", "M:0xH0006=6") ","
        GENERIC_ACHIEVEMENT_JSON("7", "T:0xH0007=7_0xH0001=1") ","
        UNOFFICIAL_ACHIEVEMENT_JSON("8", "0xH0008=8") ","
        GENERIC_ACHIEVEMENT_JSON("9", "0xH0009=9") ","
        GENERIC_ACHIEVEMENT_JSON("70", "M:0xX0010=100000") ","
        GENERIC_ACHIEVEMENT_JSON("71", "G:0xX0010=100000")
      "],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("45", "STA:0xH000A=1::CAN:0xH000C=2::SUB:0xH000D=1::VAL:0xH000E", "SCORE") ","   /* different size */
        GENERIC_LEADERBOARD_JSON("46", "STA:0xH000A=1::CAN:0xH000C=3::SUB:0xH000D=1::VAL:0x 000E", "VALUE") ","   /* different format */
        GENERIC_LEADERBOARD_JSON("47", "STA:0xH000A=1::CAN:0xH000C=4::SUB:0xH000D=2::VAL:0x 000E", "SCORE") ","   /* different submit */
        GENERIC_LEADERBOARD_JSON("48", "STA:0xH000A=2::CAN:0xH000C=5::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","   /* different start */
        GENERIC_LEADERBOARD_JSON("51", "STA:0xH000A=3::CAN:0xH000C=6::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE") "," /* hit count */
        GENERIC_LEADERBOARD_JSON("52", "STA:0xH000B=3::CAN:0xH000C=7::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE")     /* hit count */
      "]"
    "},{"
      "\"AchievementSetId\":2345,\"GameId\":1235,\"Title\":\"Bonus\",\"Type\":\"bonus\","
      "\"ImageIconUrl\":\"http://server/Images/112234.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("5501", "0xH0017=7") ","
        UNOFFICIAL_ACHIEVEMENT_JSON("5502", "0xH0018=8") ","
        GENERIC_ACHIEVEMENT_JSON("5503", "0xHFEFEFEFE=9")
      "],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("81", "STA:0xH0008=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("82", "STA:0xH0008=2::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE")
      "]"
    "}]}";

static const char* patchdata_not_found = "{\"Success\":false,\"Error\":\"Unknown game\",\"Code\":\"not_found\",\"Status\":404}";

static const char* no_unlocks = "{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[]}";

/* startsession API only returns HardcoreUnlocks if an achievement has been earned in hardcore,
 * even if the softcore unlock has a different timestamp */
static const char* unlock_5501h_and_5502 = "{\"Success\":true,\"Unlocks\":["
      "{\"ID\":5502,\"When\":1234567899}"
    "],\"HardcoreUnlocks\":["
      "{\"ID\":5501,\"When\":1234567890}"
    "]}";
static const char* unlock_5501_and_5502 = "{\"Success\":true,\"HardcoreUnlocks\":["
      "{\"ID\":5501,\"When\":1234567890},"
      "{\"ID\":5502,\"When\":1234567899}"
    "]}";
static const char* unlock_5501_5502_and_5503 = "{\"Success\":true,\"HardcoreUnlocks\":["
      "{\"ID\":5501,\"When\":1234567890},"
      "{\"ID\":5502,\"When\":1234567899},"
      "{\"ID\":5503,\"When\":1234567999}"
    "]}";
static const char* unlock_8 = "{\"Success\":true,\"HardcoreUnlocks\":[{\"ID\":8,\"When\":1234567890}]}";
static const char* unlock_8_and_5502 = "{\"Success\":true,\"HardcoreUnlocks\":["
      "{\"ID\":8,\"When\":1234567890},"
      "{\"ID\":5502,\"When\":1234567890}"
    "]}";
static const char* unlock_6_8h_and_9 = "{\"Success\":true,\"Unlocks\":["
      "{\"ID\":6,\"When\":1234567890},"
      "{\"ID\":9,\"When\":1234567899}"
    "],\"HardcoreUnlocks\":["
      "{\"ID\":8,\"When\":1234567895}"
    "]}";

static const char* response_429 =
    "<html>\n"
    "<head><title>429 Too Many Requests</title></head>\n"
    "<body>\n"
    "<center><h1>429 Too Many Requests</h1></center>\n"
    "<hr><center>nginx</center>\n"
    "</body>\n"
    "</html>";

static const char* response_502 =
    "<html>\n"
    "<head><title>502 Bad Gateway</title></head>\n"
    "<body>\n"
    "<center><h1>502 Bad Gateway</h1></center>\n"
    "<hr><center>nginx</center>\n"
    "</body>\n"
    "</html>";

static const char* response_503 =
    "<html>\n"
    "<head><title>503 Service Temporarily Unavailable</title></head>\n"
    "<body>\n"
    "<center><h1>503 Service Temporarily Unavailable</h1></center>\n"
    "<hr><center>nginx</center>\n"
    "</body>\n"
    "</html>";

static const char* default_game_badge = "https://media.retroachievements.org/Images/000001.png";

/* ----- helpers ----- */

static void _assert_achievement_state(rc_client_t* client, uint32_t id, int expected_state)
{
  const rc_client_achievement_t* achievement = rc_client_get_achievement_info(client, id);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, expected_state);
}
#define assert_achievement_state(client, id, expected_state) ASSERT_HELPER(_assert_achievement_state(client, id, expected_state), "assert_achievement_state")

typedef struct rc_client_captured_event_t
{
  rc_client_event_t event;
  rc_client_server_error_t server_error; /* server_error goes out of scope, it needs to be copied too */
  uint32_t id;
} rc_client_captured_event_t;

static rc_client_captured_event_t events[16];
static int event_count = 0;

static void rc_client_event_handler(const rc_client_event_t* e, rc_client_t* client)
{
  memcpy(&events[event_count], e, sizeof(rc_client_event_t));
  memset(&events[event_count].server_error, 0, sizeof(events[event_count].server_error));

  switch (e->type) {
    case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
    case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
    case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE:
    case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
    case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:
      events[event_count].id = e->achievement->id;
      break;

    case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
    case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
    case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
      events[event_count].id = e->leaderboard->id;
      break;

    case RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD: {
      /* scoreboard is not maintained out of scope, copy it */
      static rc_client_leaderboard_scoreboard_t scoreboard;
      static rc_client_leaderboard_scoreboard_entry_t scoreboard_entries[2];
      static char scoreboard_top_usernames[2][128];
      uint32_t i;

      /* cap the entries at 2, none of the mocked responses have anything larger */
      memcpy(&scoreboard, e->leaderboard_scoreboard, sizeof(scoreboard));
      scoreboard.num_top_entries = (scoreboard.num_top_entries > 2) ? 2 : scoreboard.num_top_entries;
      memcpy(scoreboard_entries, e->leaderboard_scoreboard->top_entries, scoreboard.num_top_entries * sizeof(scoreboard_entries[0]));
      for (i = 0; i < scoreboard.num_top_entries; i++) {
        strcpy_s(scoreboard_top_usernames[i], sizeof(scoreboard_top_usernames[i]), scoreboard_entries[i].username);
        scoreboard_entries[i].username = scoreboard_top_usernames[i];
      }
      scoreboard.top_entries = scoreboard_entries;

      events[event_count].id = e->leaderboard_scoreboard->leaderboard_id;
      events[event_count].event.leaderboard_scoreboard = &scoreboard;
      break;
    }

    case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW:
    case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE:
    case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE:
      events[event_count].id = e->leaderboard_tracker->id;
      break;

    case RC_CLIENT_EVENT_GAME_COMPLETED:
      events[event_count].id = rc_client_get_game_info(client)->id;
      break;

    case RC_CLIENT_EVENT_SERVER_ERROR: {
      static char event_server_error_message[128];

      /* server error data is not maintained out of scope, copy it */
      memcpy(&events[event_count].server_error, e->server_error, sizeof(events[event_count].server_error));
      strcpy_s(event_server_error_message, sizeof(event_server_error_message), e->server_error->error_message);
      events[event_count].server_error.error_message = event_server_error_message;
      events[event_count].event.server_error = &events[event_count].server_error;
      events[event_count].id = 0;
      break;
    }

    case RC_CLIENT_EVENT_SUBSET_COMPLETED:
      events[event_count].id = e->subset->id;
      break;

    default:
      events[event_count].id = 0;
      break;
  }

  ++event_count;
}

static rc_client_event_t* find_event(uint8_t type, uint32_t id)
{
  int i;

  for (i = 0; i < event_count; ++i) {
    if (events[i].id == id && events[i].event.type == type)
      return &events[i].event;
  }

  return NULL;
}

static uint8_t* g_memory = NULL;
static uint32_t g_memory_size = 0;

static void mock_memory(uint8_t* memory, uint32_t size)
{
  g_memory = memory;
  g_memory_size = size;
}

static uint32_t rc_client_read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client)
{
  if (g_memory_size > 0) {
    if (address >= g_memory_size)
      return 0;

    uint32_t num_avail = g_memory_size - address;
    if (num_avail < num_bytes)
      num_bytes = num_avail;

    memcpy(buffer, &g_memory[address], num_bytes);
    return num_bytes;
  }

  memset(&buffer, 0, num_bytes);
  return num_bytes;
}

static rc_clock_t g_now;

static rc_clock_t rc_client_get_now_millisecs(const rc_client_t* client)
{
  return g_now;
}

/* ----- API mocking ----- */

typedef struct rc_mock_api_response
{
  const char* request_params;
  rc_api_server_response_t server_response;
  int seen;
  rc_client_server_callback_t async_callback;
  void* async_callback_data;
} rc_mock_api_response;

static rc_mock_api_response g_mock_api_responses[12];
static int g_num_mock_api_responses = 0;

void rc_client_server_call(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client)
{
  rc_api_server_response_t server_response;

  int i;
  for (i = 0; i < g_num_mock_api_responses; i++) {
    if (strcmp(g_mock_api_responses[i].request_params, request->post_data) == 0) {
      g_mock_api_responses[i].seen++;
      callback(&g_mock_api_responses[i].server_response, callback_data);
      return;
    }
  }

  ASSERT_FAIL("No API response for: %s", request->post_data);

  /* still call the callback to prevent memory leak */
  memset(&server_response, 0, sizeof(server_response));
  server_response.body = "";
  server_response.http_status_code = 500;
  callback(&server_response, callback_data);
}

void rc_client_server_call_async(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client)
{
  g_mock_api_responses[g_num_mock_api_responses].request_params = strdup(request->post_data);
  g_mock_api_responses[g_num_mock_api_responses].async_callback = callback;
  g_mock_api_responses[g_num_mock_api_responses].async_callback_data = callback_data;
  g_mock_api_responses[g_num_mock_api_responses].seen = -1;
  g_num_mock_api_responses++;
}

static void _async_api_response(const char* request_params, const char* response_body, int http_status_code)
{
  int i;
  for (i = 0; i < g_num_mock_api_responses; i++)
  {
    if (g_mock_api_responses[i].request_params && strcmp(g_mock_api_responses[i].request_params, request_params) == 0)
    {
      /* request_params are set to NULL, so we can't find the requests to get a count anyway */
      g_mock_api_responses[i].seen++;
      g_mock_api_responses[i].server_response.body = response_body;
      g_mock_api_responses[i].server_response.body_length = strlen(response_body);
      g_mock_api_responses[i].server_response.http_status_code = http_status_code;
      g_mock_api_responses[i].async_callback(&g_mock_api_responses[i].server_response, g_mock_api_responses[i].async_callback_data);
      free((void*)g_mock_api_responses[i].request_params);
      g_mock_api_responses[i].request_params = NULL;

      while (g_num_mock_api_responses > 0 && g_mock_api_responses[g_num_mock_api_responses - 1].request_params == NULL)
        --g_num_mock_api_responses;
	    return;
    }
  }

  ASSERT_FAIL("No pending API request for: %s", request_params);
}

void async_api_response(const char* request_params, const char* response_body)
{
  _async_api_response(request_params, response_body, 200);
}

void async_api_error(const char* request_params, const char* response_body, int http_status_code)
{
  _async_api_response(request_params, response_body, http_status_code);
}

static void _assert_api_called(const char* request_params, int count)
{
  int i;
  for (i = 0; i < g_num_mock_api_responses; i++) {
    if (g_mock_api_responses[i].request_params &&
        strcmp(g_mock_api_responses[i].request_params, request_params) == 0) {
      ASSERT_NUM_EQUALS(g_mock_api_responses[i].seen, count);
      return;
    }
  }

  ASSERT_NUM_EQUALS(0, count);
}
#define assert_api_called(request_params) ASSERT_HELPER(_assert_api_called(request_params, 1), "assert_api_called")
#define assert_api_not_called(request_params) ASSERT_HELPER(_assert_api_called(request_params, 0), "assert_api_not_called")
#define assert_api_call_count(request_params, num) ASSERT_HELPER(_assert_api_called(request_params, num), "assert_api_call_count")
#define assert_api_pending(request_params) ASSERT_HELPER(_assert_api_called(request_params, -1), "assert_api_pending")
#define assert_api_not_pending(request_params) ASSERT_HELPER(_assert_api_called(request_params, 0), "assert_api_not_pending")

void reset_mock_api_handlers(void)
{
  g_num_mock_api_responses = 0;
  memset(g_mock_api_responses, 0, sizeof(g_mock_api_responses));
}

void mock_api_response(const char* request_params, const char* response_body)
{
  g_mock_api_responses[g_num_mock_api_responses].request_params = request_params;
  g_mock_api_responses[g_num_mock_api_responses].server_response.body = response_body;
  g_mock_api_responses[g_num_mock_api_responses].server_response.body_length = strlen(response_body);
  g_mock_api_responses[g_num_mock_api_responses].server_response.http_status_code = 200;
  g_num_mock_api_responses++;
}

void mock_api_error(const char* request_params, const char* response_body, int http_status_code)
{
  g_mock_api_responses[g_num_mock_api_responses].request_params = request_params;
  g_mock_api_responses[g_num_mock_api_responses].server_response.body = response_body;
  g_mock_api_responses[g_num_mock_api_responses].server_response.body_length = strlen(response_body);
  g_mock_api_responses[g_num_mock_api_responses].server_response.http_status_code = http_status_code;
  g_num_mock_api_responses++;
}

static void rc_client_callback_expect_success(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_OK);
  ASSERT_PTR_NULL(error_message);
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void rc_client_callback_expect_timeout(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_NO_RESPONSE);
  ASSERT_STR_EQUALS(error_message, "Request has timed out.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void rc_client_callback_expect_no_internet(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_NO_RESPONSE);
  ASSERT_STR_EQUALS(error_message, "Internet not available.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void rc_client_callback_expect_no_game_loaded(int result, const char* error_message, rc_client_t* client, void* callback_data)
{
  ASSERT_NUM_EQUALS(result, RC_NO_GAME_LOADED);
  ASSERT_STR_EQUALS(error_message, "No game loaded");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_data, g_callback_userdata);
}

static void rc_client_callback_expect_uncalled(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_FAIL("Callback should not have been called.");
}

static rc_client_t* mock_client_not_logged_in(void)
{
  rc_client_t* client = rc_client_create(rc_client_read_memory, rc_client_server_call);
  rc_client_set_event_handler(client, rc_client_event_handler);
  rc_client_set_get_time_millisecs_function(client, rc_client_get_now_millisecs);

  mock_memory(NULL, 0);
  rc_api_set_host(NULL);
  reset_mock_api_handlers();
  g_now = 100000;

  return client;
}

static rc_client_t* mock_client_not_logged_in_async(void)
{
  rc_client_t* client = mock_client_not_logged_in();
  client->callbacks.server_call = rc_client_server_call_async;
  return client;
}

static rc_client_t* mock_client_logged_in(void)
{
  rc_client_t* client = mock_client_not_logged_in();
  client->user.username = "Username";
  client->user.display_name = "DisplayName";
  client->user.token = "ApiToken";
  client->user.score = 12345;
  client->user.avatar_url = "http://server/UserPic/Username.png";
  client->state.user = RC_CLIENT_USER_STATE_LOGGED_IN;

  return client;
}

static rc_client_t* mock_client_logged_in_async(void)
{
  rc_client_t* client = mock_client_logged_in();
  client->callbacks.server_call = rc_client_server_call_async;
  return client;
}

static void mock_client_load_game(const char* patchdata, const char* unlocks)
{
  reset_mock_api_handlers();
  event_count = 0;
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, unlocks);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  if (!g_client->game)
    ASSERT_MESSAGE("client->game is NULL");
}

static void mock_client_load_game_softcore(const char* patchdata, const char* unlocks)
{
  reset_mock_api_handlers();
  event_count = 0;
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=0&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, unlocks);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  if (!g_client->game)
    ASSERT_MESSAGE("client->game is NULL");
}

static rc_client_t* mock_client_game_loaded(const char* patchdata, const char* unlocks)
{
  g_client = mock_client_logged_in();

  mock_client_load_game(patchdata, unlocks);

  return g_client;
}

/* ----- login ----- */

static void test_login_with_password(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_response("r=login2&u=User&p=Pa%24%24word",
	  "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_success, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "User");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->score_softcore, 123);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);

  rc_client_destroy(g_client);
}

static void test_login_with_token(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_response("r=login2&u=User&t=ApiToken",
	  "{\"Success\":true,\"User\":\"User\",\"AvatarUrl\":\"http://server/UserPic/USER.png\",\"Token\":\"ApiToken\",\"Score\":12345,\"Messages\":2}");

  rc_client_begin_login_with_token(g_client, "User", "ApiToken", rc_client_callback_expect_success, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "User");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_STR_EQUALS(user->avatar_url, "http://server/UserPic/USER.png");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_username_required(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "username is required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void rc_client_callback_expect_password_required(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "password is required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void rc_client_callback_expect_token_required(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "token is required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_login_required_fields(void)
{
  g_client = mock_client_not_logged_in();

  rc_client_begin_login_with_password(g_client, "User", "", rc_client_callback_expect_password_required, g_callback_userdata);
  rc_client_begin_login_with_password(g_client, "", "Pa$$word", rc_client_callback_expect_username_required, g_callback_userdata);
  rc_client_begin_login_with_password(g_client, "", "", rc_client_callback_expect_username_required, g_callback_userdata);

  rc_client_begin_login_with_token(g_client, "User", "", rc_client_callback_expect_token_required, g_callback_userdata);
  rc_client_begin_login_with_token(g_client, "", "ApiToken", rc_client_callback_expect_username_required, g_callback_userdata);
  rc_client_begin_login_with_token(g_client, "", "", rc_client_callback_expect_username_required, g_callback_userdata);

  ASSERT_NUM_EQUALS(g_client->state.user, RC_CLIENT_USER_STATE_NONE);

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_credentials_error(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_CREDENTIALS);
  ASSERT_STR_EQUALS(error_message, "Invalid User/Password combination. Please try again");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_login_with_incorrect_password(void)
{
  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_error("r=login2&u=User&p=Pa%24%24word",
      "{\"Success\":false,\"Error\":\"Invalid User/Password combination. Please try again\","
       "\"Status\":401,\"Code\":\"invalid_credentials\"}", 401);

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_credentials_error, g_callback_userdata);

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_token_error(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_CREDENTIALS);
  ASSERT_STR_EQUALS(error_message, "Invalid User/Token combination.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_login_with_incorrect_token(void)
{
  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_error("r=login2&u=User&t=TOKEN",
      "{\"Success\":false,\"Error\":\"Invalid User/Token combination.\","
      "\"Status\":401,\"Code\":\"invalid_credentials\"}", 401);

  rc_client_begin_login_with_token(g_client, "User", "TOKEN", rc_client_callback_expect_token_error, g_callback_userdata);

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_expired_token(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_EXPIRED_TOKEN);
  ASSERT_STR_EQUALS(error_message, "The access token has expired. Please log in again.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_login_with_expired_token(void)
{
  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_error("r=login2&u=User&t=EXPIRED",
      "{\"Success\":false,\"Error\":\"The access token has expired. Please log in again.\","
      "\"Status\":401,\"Code\":\"expired_token\"}", 403);

  rc_client_begin_login_with_token(g_client, "User", "EXPIRED", rc_client_callback_expect_expired_token, g_callback_userdata);

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_access_denied(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_ACCESS_DENIED);
  ASSERT_STR_EQUALS(error_message, "Access denied.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_login_with_banned_account(void)
{
  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_error("r=login2&u=User&p=Pa%24%24word",
      "{\"Success\":false,\"Error\":\"Access denied.\","
      "\"Status\":403,\"Code\":\"access_denied\"}", 403);

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_access_denied, g_callback_userdata);

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_missing_token(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_MISSING_VALUE);
  ASSERT_STR_EQUALS(error_message, "Token not found in response");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_login_incomplete_response(void)
{
  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_response("r=login2&u=User&p=Pa%24%24word", "{\"Success\":true,\"User\":\"Username\"}");

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_missing_token, g_callback_userdata);

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void test_login_with_password_async(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_success, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NULL(user);

  async_api_response("r=login2&u=User&p=Pa%24%24word",
	    "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "User");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);

  rc_client_destroy(g_client);
}

static void test_login_with_password_async_aborted(void)
{
  const rc_client_user_t* user;
  rc_client_async_handle_t* handle;

  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  handle = rc_client_begin_login_with_password(g_client, "User", "Pa$$word",
      rc_client_callback_expect_uncalled, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NULL(user);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=login2&u=User&p=Pa%24%24word",
    "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NULL(user);

  rc_client_destroy(g_client);
}

static void test_login_with_password_async_destroyed(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word",
    rc_client_callback_expect_uncalled, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NULL(user);

  rc_client_destroy(g_client);

  async_api_response("r=login2&u=User&p=Pa%24%24word",
    "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");
}

static void test_login_with_password_client_error(void)
{
  const rc_client_user_t* user;
  rc_client_async_handle_t* handle;

  g_client = mock_client_not_logged_in();

  mock_api_error("r=login2&u=User&p=Pa%24%24word", "Internet not available.", RC_API_SERVER_RESPONSE_CLIENT_ERROR);

  handle = rc_client_begin_login_with_password(g_client, "User", "Pa$$word",
      rc_client_callback_expect_no_internet, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NULL(user);

  ASSERT_PTR_NULL(handle);

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_login_required(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_LOGIN_REQUIRED);
  ASSERT_STR_EQUALS(error_message, "Login required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_logout(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_logged_in();

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);

  rc_client_logout(g_client);
  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  /* reference pointer should be NULLed out */
  ASSERT_PTR_NULL(user->display_name);
  ASSERT_PTR_NULL(user->username);
  ASSERT_PTR_NULL(user->token);

  /* attempt to load game should fail */
  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_login_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  
  rc_client_destroy(g_client);
}

static void test_logout_with_game_loaded(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  ASSERT_PTR_NOT_NULL(rc_client_get_user_info(g_client));
  ASSERT_PTR_NOT_NULL(rc_client_get_game_info(g_client));

  rc_client_logout(g_client);

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));
  ASSERT_PTR_NULL(rc_client_get_game_info(g_client));

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_login_aborted(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_ABORTED);
  ASSERT_STR_EQUALS(error_message, "Login aborted");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_logout_during_login(void)
{
  g_client = mock_client_not_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_login_aborted, g_callback_userdata);
  rc_client_logout(g_client);

  async_api_response("r=login2&u=User&p=Pa%24%24word",
    "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_no_longer_active(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_ABORTED);
  ASSERT_STR_EQUALS(error_message, "The requested game is no longer active");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_logout_during_fetch_game(void)
{
  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "0123456789ABCDEF",
    rc_client_callback_expect_no_longer_active, g_callback_userdata);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);

  rc_client_logout(g_client);

  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void test_user_get_image_url(void)
{
  char buffer[256];
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  ASSERT_STR_EQUALS(g_client->user.avatar_url, "http://server/UserPic/Username.png");

  ASSERT_NUM_EQUALS(rc_client_user_get_image_url(rc_client_get_user_info(g_client), buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "http://server/UserPic/Username.png");

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive, unlock_6_8h_and_9);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 1);

  ASSERT_NUM_EQUALS(summary.points_core, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 5);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_softcore(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive, unlock_6_8h_and_9);
  rc_client_set_hardcore_enabled(g_client, 0);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 3);

  ASSERT_NUM_EQUALS(summary.points_core, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 15);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_encore_mode(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_exhaustive);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, unlock_6_8h_and_9);

  rc_client_set_encore_mode_enabled(g_client, 1);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 1);

  ASSERT_NUM_EQUALS(summary.points_core, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 5);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_with_unsupported_and_unofficial(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_unofficial_unsupported, no_unlocks);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 2);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 1);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 1);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 0);

  ASSERT_NUM_EQUALS(summary.points_core, 7);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 0);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_with_unsupported_unlocks(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_unofficial_unsupported, unlock_5501_5502_and_5503);

  /* unlocked unsupported achievement should be counted in both unlocked and unsuppored buckets */
  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 2);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 1);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 1);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 2);

  ASSERT_NUM_EQUALS(summary.points_core, 7);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 7);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 1234567999);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_with_unofficial_off(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 0);
  mock_client_load_game(patchdata_unofficial_unsupported, no_unlocks);

  /* unofficial achievements are not copied from the patch data to the runtime if unofficial is off */
  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 2);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 1);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 0);

  ASSERT_NUM_EQUALS(summary.points_core, 7);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 0);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_no_achievements(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_empty, no_unlocks);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 0);

  ASSERT_NUM_EQUALS(summary.points_core, 0);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 0);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_unknown_game(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_NO_GAME_LOADED);
  ASSERT_STR_EQUALS(error_message, "Unknown game");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_get_user_game_summary_unknown_game(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_not_found);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_unknown_game, g_callback_userdata);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 0);

  ASSERT_NUM_EQUALS(summary.points_core, 0);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_progress_incomplete(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive_typed, unlock_8);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 1);

  ASSERT_NUM_EQUALS(summary.points_core, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 5);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_progress_progression_no_win(void)
{
  rc_client_user_game_summary_t summary;
  /* 6 and 8 are progression, 9 is win condition */
  const char* unlock_5_6_and_8 = "{\"Success\":true,\"HardcoreUnlocks\":["
      "{\"ID\":5,\"When\":1234568890},"
      "{\"ID\":6,\"When\":1234567999},"
      "{\"ID\":8,\"When\":1234567895}"
    "]}";

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive_typed, unlock_5_6_and_8);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 3);

  ASSERT_NUM_EQUALS(summary.points_core, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 15);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_progress_win_only(void)
{
  rc_client_user_game_summary_t summary;
  /* 6 and 8 are progression, 9 is win condition */
  const char* unlock_5_and_9 = "{\"Success\":true,\"HardcoreUnlocks\":["
      "{\"ID\":5,\"When\":1234568890},"
      "{\"ID\":9,\"When\":1234567999}"
    "]}";

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive_typed, unlock_5_and_9);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 2);

  ASSERT_NUM_EQUALS(summary.points_core, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 10);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_beat(void)
{
  rc_client_user_game_summary_t summary;
  /* 6 and 8 are progression, 9 is win condition */
  const char* unlocks = "{\"Success\":true,\"HardcoreUnlocks\":["
      "{\"ID\":5,\"When\":1234568890},"
      "{\"ID\":6,\"When\":1234567999},"
      "{\"ID\":8,\"When\":1234567890},"
      "{\"ID\":9,\"When\":1234568765}"
    "]}";

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive_typed, unlocks);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 4);

  ASSERT_NUM_EQUALS(summary.points_core, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 20);

  ASSERT_NUM_EQUALS(summary.beaten_time, 1234568765);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_mastery(void)
{
  rc_client_user_game_summary_t summary;
  const char* unlocks = "{\"Success\":true,\"HardcoreUnlocks\":["
    "{\"ID\":5,\"When\":1234568890},"
    "{\"ID\":6,\"When\":1234567999},"
    "{\"ID\":7,\"When\":1234569123},"
    "{\"ID\":8,\"When\":1234567890},"
    "{\"ID\":9,\"When\":1234568765},"
    "{\"ID\":70,\"When\":1234568901},"
    "{\"ID\":71,\"When\":1234566789}"
    "]}";

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive_typed, unlocks);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 7);

  ASSERT_NUM_EQUALS(summary.points_core, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 35);

  ASSERT_NUM_EQUALS(summary.beaten_time, 1234568765);
  ASSERT_NUM_EQUALS(summary.completed_time, 1234569123);

  rc_client_destroy(g_client);
}

/* ----- load game ----- */

static void rc_client_callback_expect_hash_required(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "hash is required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_load_game_required_fields(void)
{
  g_client = mock_client_logged_in();

  rc_client_begin_load_game(g_client, NULL, rc_client_callback_expect_hash_required, g_callback_userdata);
  rc_client_begin_load_game(g_client, "", rc_client_callback_expect_hash_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_load_game_unknown_hash(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_not_found);

  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);
  ASSERT_NUM_EQUALS(rc_client_is_game_loaded(g_client), 0);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_unknown_game, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_DONE);
  ASSERT_NUM_EQUALS(rc_client_is_game_loaded(g_client), 0);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_url, default_game_badge);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 0);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 0);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.id, 0);
    ASSERT_STR_EQUALS(g_client->game->subsets->public_.title, "");
    ASSERT_NUM_EQUALS(g_client->game->subsets->active, 0);
  }
  rc_client_destroy(g_client);
}

static void test_load_game_unknown_hash_repeated(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  /* first request should resolve the hash asynchronously */
  handle = rc_client_begin_load_game(g_client,
      "0123456789ABCDEF", rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(handle);
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_url, default_game_badge);

  /* second request should use the hash cache and not need an asynchronous call */
  handle = rc_client_begin_load_game(g_client,
      "0123456789ABCDEF", rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NULL(handle);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_url, default_game_badge);

  rc_client_destroy(g_client);
}

static void test_load_game_unknown_hash_multiple(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  /* first request */
  handle = rc_client_begin_load_game(g_client, "0123456789ABCDEF",
                                     rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(handle);
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_url, default_game_badge);

  /* second request */
  handle = rc_client_begin_load_game(g_client, "FEDCBA9876543210",
                                     rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(handle);
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=FEDCBA9876543210", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "FEDCBA9876543210");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_url, default_game_badge);

  rc_client_destroy(g_client);
}

static void test_load_game_not_logged_in(void)
{
  g_client = mock_client_not_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_login_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game(void)
{
  rc_client_achievement_info_t* achievement;
  rc_client_leaderboard_info_t* leaderboard;
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);
  ASSERT_NUM_EQUALS(rc_client_is_game_loaded(g_client), 0);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_DONE);
  ASSERT_NUM_EQUALS(rc_client_is_game_loaded(g_client), 1);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_url, "http://server/Images/112233.png");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);

    achievement = &g_client->game->subsets->achievements[0];
    ASSERT_NUM_EQUALS(achievement->public_.id, 5501);
    ASSERT_STR_EQUALS(achievement->public_.title, "Ach1");
    ASSERT_STR_EQUALS(achievement->public_.description, "Desc1");
    ASSERT_STR_EQUALS(achievement->public_.badge_name, "00234");
    ASSERT_STR_EQUALS(achievement->public_.badge_url, "https://media.retroachievements.org/Badge/00234.png");
    ASSERT_STR_EQUALS(achievement->public_.badge_locked_url, "https://media.retroachievements.org/Badge/00234_lock.png");
    ASSERT_NUM_EQUALS(achievement->public_.points, 5);
    ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    achievement = &g_client->game->subsets->achievements[1];
    ASSERT_NUM_EQUALS(achievement->public_.id, 5502);
    ASSERT_STR_EQUALS(achievement->public_.title, "Ach2");
    ASSERT_STR_EQUALS(achievement->public_.description, "Desc2");
    ASSERT_STR_EQUALS(achievement->public_.badge_name, "00235");
    ASSERT_STR_EQUALS(achievement->public_.badge_url, "https://media.retroachievements.org/Badge/00235.png");
    ASSERT_STR_EQUALS(achievement->public_.badge_locked_url, "https://media.retroachievements.org/Badge/00235_lock.png");
    ASSERT_NUM_EQUALS(achievement->public_.points, 2);
    ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    leaderboard = &g_client->game->subsets->leaderboards[0];
    ASSERT_NUM_EQUALS(leaderboard->public_.id, 4401);
    ASSERT_STR_EQUALS(leaderboard->public_.title, "Leaderboard1");
    ASSERT_STR_EQUALS(leaderboard->public_.description, "Desc1");
    ASSERT_NUM_EQUALS(leaderboard->public_.state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
    ASSERT_PTR_NOT_NULL(leaderboard->lboard);
    ASSERT_NUM_NOT_EQUALS(leaderboard->value_djb2, 0);
    ASSERT_PTR_NULL(leaderboard->tracker);
  }

  rc_client_destroy(g_client);
}

static void test_load_game_async_load_different_game(void)
{
  static const char* patchdata_alternate = "{\"Success\":true,"
    "\"GameId\":2345,\"Title\":\"Other Game\",\"ConsoleId\":7,"
    "\"ImageIconUrl\":\"http://server/Images/555555.png\","
    "\"RichPresenceGameId\":2345,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":2222,\"GameId\":2345,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/555555.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("1", "0xH0000=5") ","
        GENERIC_ACHIEVEMENT_JSON("2", "0xHFFFF=5") ","
        GENERIC_ACHIEVEMENT_JSON("3", "0xH10000=5")
      "],"
      "\"Leaderboards\":[]"
    "}]}";

  g_client = mock_client_logged_in_async();
  reset_mock_api_handlers();

  /* start loading first game */
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_no_longer_active, g_callback_userdata);
  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF");
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_IDENTIFYING_GAME);

  /* receive data for first game, start session for first game */
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_STARTING_SESSION);

  /* start loading second game*/
  rc_client_begin_load_game(g_client, "ABCDEF0123456789", rc_client_callback_expect_success, g_callback_userdata);
  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=ABCDEF0123456789");
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_IDENTIFYING_GAME);

  /* session started for first game, should abort */
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_IDENTIFYING_GAME);

  /* receive data for second game, start session for second game */
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=ABCDEF0123456789", patchdata_alternate);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_STARTING_SESSION);

  /* session started for second game, should succeed */
  async_api_response("r=startsession&u=Username&t=ApiToken&g=2345&h=1&m=ABCDEF0123456789&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_DONE);

  /* verify second game was loaded */
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 2345);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 7);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Other Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "ABCDEF0123456789");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "555555");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 3);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 0);
  }

  rc_client_destroy(g_client);
}

static void test_load_game_async_login(void)
{
  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  rc_client_begin_login_with_password(g_client, "Username", "Pa$$word", rc_client_callback_expect_success, g_callback_userdata);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_AWAIT_LOGIN);

  /* game load process will stop here waiting for the login to complete */
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF");

  /* login completion will trigger process to continue */
  async_api_response("r=login2&u=Username&p=Pa%24%24word",
	    "{\"Success\":true,\"User\":\"Username\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");
  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF");
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_IDENTIFYING_GAME);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_STARTING_SESSION);

  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_DONE);

  ASSERT_STR_EQUALS(g_client->user.username, "Username");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
}

static void test_load_game_async_login_with_incorrect_password(void)
{
  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  rc_client_begin_login_with_password(g_client, "Username", "Pa$$word", rc_client_callback_expect_credentials_error, g_callback_userdata);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_login_required, g_callback_userdata);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_AWAIT_LOGIN);

  /* game load process will stop here waiting for the login to complete */
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF");

  /* login failure will trigger process to continue */
  async_api_error("r=login2&u=Username&p=Pa%24%24word",
      "{\"Success\":false,\"Error\":\"Invalid User/Password combination. Please try again\","
      "\"Status\":401,\"Code\":\"invalid_credentials\"}", 401);
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF");

  ASSERT_PTR_NULL(g_client->user.username);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_async_login_logout(void)
{
  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  rc_client_begin_login_with_password(g_client, "Username", "Pa$$word", rc_client_callback_expect_login_aborted, g_callback_userdata);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_login_aborted, g_callback_userdata);

  /* game load process will stop here waiting for the login to complete */
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF");

  /* logout will cancel login and allow game load to proceed with failure */
  rc_client_logout(g_client);
  async_api_response("r=login2&u=Username&p=Pa%24%24word",
    "{\"Success\":true,\"User\":\"Username\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF");

  ASSERT_PTR_NULL(g_client->user.username);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_async_login_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  handle = rc_client_begin_login_with_password(g_client, "Username", "Pa$$word", rc_client_callback_expect_uncalled, g_callback_userdata);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_login_aborted, g_callback_userdata);

  /* game load process will stop here waiting for the login to complete */
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF");

  /* login abort will trigger game load process to continue */
  rc_client_abort_async(g_client, handle);
  async_api_response("r=login2&u=Username&p=Pa%24%24word",
    "{\"Success\":true,\"User\":\"Username\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF");

  ASSERT_PTR_NULL(g_client->user.username);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_too_many_requests(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_JSON);
  ASSERT_STR_EQUALS(error_message, "429 Too Many Requests");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_load_game_patch_failure(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_error("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", response_429, 429);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_too_many_requests, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_startsession_failure(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);
  mock_api_error("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, response_429, 429);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_too_many_requests, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_startsession_timeout(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);
  mock_api_error("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "", 504);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_timeout, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_startsession_custom_timeout(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);
  mock_api_error("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING,
    "Request has timed out.", RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_timeout, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_patch_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  handle = rc_client_begin_load_game(g_client, "0123456789ABCDEF",
    rc_client_callback_expect_uncalled, g_callback_userdata);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);
  assert_api_not_called("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_startsession_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  handle = rc_client_begin_load_game(g_client, "0123456789ABCDEF",
    rc_client_callback_expect_uncalled, g_callback_userdata);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_while_spectating(void)
{
  rc_client_achievement_info_t* achievement;
  rc_client_leaderboard_info_t* leaderboard;
  g_client = mock_client_logged_in();
  rc_client_set_spectator_mode_enabled(g_client, 1);

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);
  /* spectator mode should not start a session or fetch unlocks */

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_DONE);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);

    achievement = &g_client->game->subsets->achievements[0];
    ASSERT_NUM_EQUALS(achievement->public_.id, 5501);
    ASSERT_STR_EQUALS(achievement->public_.title, "Ach1");
    ASSERT_STR_EQUALS(achievement->public_.description, "Desc1");
    ASSERT_STR_EQUALS(achievement->public_.badge_name, "00234");
    ASSERT_NUM_EQUALS(achievement->public_.points, 5);
    ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    achievement = &g_client->game->subsets->achievements[1];
    ASSERT_NUM_EQUALS(achievement->public_.id, 5502);
    ASSERT_STR_EQUALS(achievement->public_.title, "Ach2");
    ASSERT_STR_EQUALS(achievement->public_.description, "Desc2");
    ASSERT_STR_EQUALS(achievement->public_.badge_name, "00235");
    ASSERT_NUM_EQUALS(achievement->public_.points, 2);
    ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    leaderboard = &g_client->game->subsets->leaderboards[0];
    ASSERT_NUM_EQUALS(leaderboard->public_.id, 4401);
    ASSERT_STR_EQUALS(leaderboard->public_.title, "Leaderboard1");
    ASSERT_STR_EQUALS(leaderboard->public_.description, "Desc1");
    ASSERT_NUM_EQUALS(leaderboard->public_.state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
    ASSERT_PTR_NOT_NULL(leaderboard->lboard);
    ASSERT_NUM_NOT_EQUALS(leaderboard->value_djb2, 0);
    ASSERT_PTR_NULL(leaderboard->tracker);
  }

  /* spectator mode cannot be disabled if it was enabled before loading the game */
  rc_client_set_spectator_mode_enabled(g_client, 0);
  ASSERT_TRUE(rc_client_get_spectator_mode_enabled(g_client));

  rc_client_unload_game(g_client);

  /* spectator mode can be disabled after unloading game */
  rc_client_set_spectator_mode_enabled(g_client, 0);
  ASSERT_FALSE(rc_client_get_spectator_mode_enabled(g_client));

  rc_client_destroy(g_client);
}

static int rc_client_callback_process_game_sets_called = 0;
static void rc_client_callback_process_game_sets(const rc_api_server_response_t* server_response,
    struct rc_api_fetch_game_sets_response_t* game_sets_response, rc_client_t* client, void* userdata)
{
  ASSERT_STR_EQUALS(server_response->body, patchdata_2ach_1lbd);
  ASSERT_NUM_EQUALS(game_sets_response->id, 1234);
  ASSERT_NUM_EQUALS(game_sets_response->num_sets, 1);
  ASSERT_NUM_EQUALS(game_sets_response->sets[0].num_achievements, 2);
  ASSERT_NUM_EQUALS(game_sets_response->sets[0].num_leaderboards, 1);
  rc_client_callback_process_game_sets_called = 1;
}

static void test_load_game_process_game_sets(void)
{
  rc_client_achievement_info_t* achievement;
  rc_client_leaderboard_info_t* leaderboard;
  g_client = mock_client_logged_in();
  g_client->callbacks.post_process_game_sets_response = rc_client_callback_process_game_sets;
  rc_client_callback_process_game_sets_called = 0;

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);

  achievement = &g_client->game->subsets->achievements[0];
  ASSERT_NUM_EQUALS(achievement->public_.id, 5501);
  ASSERT_STR_EQUALS(achievement->public_.title, "Ach1");
  ASSERT_STR_EQUALS(achievement->public_.description, "Desc1");
  ASSERT_STR_EQUALS(achievement->public_.badge_name, "00234");
  ASSERT_NUM_EQUALS(achievement->public_.points, 5);
  ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
  ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_PTR_NOT_NULL(achievement->trigger);

  achievement = &g_client->game->subsets->achievements[1];
  ASSERT_NUM_EQUALS(achievement->public_.id, 5502);
  ASSERT_STR_EQUALS(achievement->public_.title, "Ach2");
  ASSERT_STR_EQUALS(achievement->public_.description, "Desc2");
  ASSERT_STR_EQUALS(achievement->public_.badge_name, "00235");
  ASSERT_NUM_EQUALS(achievement->public_.points, 2);
  ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
  ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_PTR_NOT_NULL(achievement->trigger);

  leaderboard = &g_client->game->subsets->leaderboards[0];
  ASSERT_NUM_EQUALS(leaderboard->public_.id, 4401);
  ASSERT_STR_EQUALS(leaderboard->public_.title, "Leaderboard1");
  ASSERT_STR_EQUALS(leaderboard->public_.description, "Desc1");
  ASSERT_NUM_EQUALS(leaderboard->public_.state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
  ASSERT_PTR_NOT_NULL(leaderboard->lboard);
  ASSERT_NUM_NOT_EQUALS(leaderboard->value_djb2, 0);
  ASSERT_PTR_NULL(leaderboard->tracker);

  ASSERT_NUM_NOT_EQUALS(rc_client_callback_process_game_sets_called, 0);

  rc_client_destroy(g_client);
}

static void test_load_game_destroy_while_fetching_game_data(void)
{
  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;
  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_uncalled, g_callback_userdata);

  rc_client_destroy(g_client);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);
}

static void test_load_unknown_game(void)
{
  const char* hash = "0123456789ABCDEFFEDCBA9876543210";
  g_client = mock_client_logged_in();

  rc_client_load_unknown_game(g_client, hash);

  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_DONE);
  ASSERT_NUM_EQUALS(rc_client_is_game_loaded(g_client), 0);

  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, hash);
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 0);
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 0);
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.id, 0);
  ASSERT_STR_EQUALS(g_client->game->subsets->public_.title, "");
  ASSERT_NUM_EQUALS(g_client->game->subsets->active, 0);

  rc_client_destroy(g_client);
}

static void test_load_unknown_game_multihash(void)
{
  const char* hash = "0123456789ABCDEFFEDCBA9876543210,FEDCBA98765432100123456789ABCDEF";
  g_client = mock_client_logged_in();

  rc_client_load_unknown_game(g_client, hash);

  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_DONE);
  ASSERT_NUM_EQUALS(rc_client_is_game_loaded(g_client), 0);

  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, hash);
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 0);
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 0);
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.id, 0);
  ASSERT_STR_EQUALS(g_client->game->subsets->public_.title, "");
  ASSERT_NUM_EQUALS(g_client->game->subsets->active, 0);

  rc_client_destroy(g_client);
}

static void test_load_game_dispatched_read_memory(void)
{
  g_client = mock_client_logged_in();
  rc_client_set_allow_background_memory_reads(g_client, 0);

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_STARTING_SESSION);
  ASSERT_PTR_NOT_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_idle(g_client);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
}

/* ----- unload game ----- */

static void test_unload_game(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_NOT_NULL(rc_client_get_game_info(g_client));
  ASSERT_PTR_NOT_NULL(rc_client_get_user_info(g_client));
  ASSERT_PTR_NOT_NULL(rc_client_get_achievement_info(g_client, 5501));
  ASSERT_PTR_NOT_NULL(rc_client_get_leaderboard_info(g_client, 4401));

  event_count = 0;
  rc_client_unload_game(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  ASSERT_PTR_NULL(g_client->game);
  ASSERT_PTR_NULL(rc_client_get_game_info(g_client));
  ASSERT_PTR_NOT_NULL(rc_client_get_user_info(g_client));
  ASSERT_PTR_NULL(rc_client_get_achievement_info(g_client, 5501));
  ASSERT_PTR_NULL(rc_client_get_leaderboard_info(g_client, 4401));

  rc_client_destroy(g_client);
}

static void test_unload_game_hides_ui(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  mock_memory(memory, sizeof(memory));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  memory[0x01] = 1;   /* show indicator */
  memory[0x06] = 3;   /* progress tracker */
  memory[0x0B] = 1;   /* start leaderboard */
  memory[0x0E] = 17;  /* leaderboard value */
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 4);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));
  event_count = 0;

  rc_client_unload_game(g_client);

  ASSERT_NUM_EQUALS(event_count, 3);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));
  event_count = 0;

  rc_client_destroy(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);
}

static void test_unload_game_while_fetching_game_data(void)
{
  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;
  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_uncalled, g_callback_userdata);

  rc_client_unload_game(g_client);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_unload_game_while_starting_session(void)
{
  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;
  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_uncalled, g_callback_userdata);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_2ach_1lbd);

  rc_client_unload_game(g_client);

  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

#ifdef RC_CLIENT_SUPPORTS_HASH

/* ----- identify and load game ----- */

static void rc_client_callback_expect_data_or_file_path_required(int result, const char* error_message, rc_client_t* client, void* callback_data)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "either data or file_path is required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_data, g_callback_userdata);
}

static void test_identify_and_load_game_required_fields(void)
{
  g_client = mock_client_logged_in();

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, NULL, NULL, 0,
      rc_client_callback_expect_data_or_file_path_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_identify_and_load_game_console_specified(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=6a2305a2b6675a97ff792709be1ca857&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_GAMEBOY, "foo.zip#foo.gb",
      image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_console_not_specified(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=6a2305a2b6675a97ff792709be1ca857&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "foo.zip#foo.gb",
      image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

#ifndef RC_HASH_NO_ROM
uint8_t* generate_nes_file(size_t kb, int with_header, size_t* image_size);

static void test_identify_and_load_game_multiconsole_first(void)
{
  rc_hash_iterator_t* iterator;
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "foo.zip#foo.nes",
    image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  /* first hash lookup should be pending. inject a secondary console into the iterator */
  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857");
  iterator = rc_client_get_load_state_hash_iterator(g_client);
  ASSERT_NUM_EQUALS(iterator->index, 1);
  ASSERT_NUM_EQUALS(iterator->consoles[iterator->index], 0);
  iterator->consoles[iterator->index] = RC_CONSOLE_MEGA_DRIVE; /* full buffer hash */
  iterator->consoles[iterator->index + 1] = 0;

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857", patchdata_2ach_1lbd);
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=6a2305a2b6675a97ff792709be1ca857&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  assert_api_not_pending("r=achievementsets&u=Username&t=ApiToken&m=64b131c5c7fec32985d9c99700babb7e");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17); /* actual console ID returned from server */
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_multiconsole_second(void)
{
  rc_hash_iterator_t* iterator;
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "foo.zip#foo.nes",
    image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  /* first hash lookup should be pending. inject a secondary console into the iterator */
  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857");
  iterator = rc_client_get_load_state_hash_iterator(g_client);
  ASSERT_NUM_EQUALS(iterator->index, 1);
  ASSERT_NUM_EQUALS(iterator->consoles[iterator->index], 0);
  iterator->consoles[iterator->index] = RC_CONSOLE_MEGA_DRIVE; /* full buffer hash */
  iterator->consoles[iterator->index + 1] = 0;

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857", patchdata_not_found);

  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=64b131c5c7fec32985d9c99700babb7e");
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=64b131c5c7fec32985d9c99700babb7e" ,patchdata_2ach_1lbd);
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=64b131c5c7fec32985d9c99700babb7e&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17); /* actual console ID returned from server */
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "64b131c5c7fec32985d9c99700babb7e");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

#endif /* RC_HASH_NO_ROM */

#ifndef RC_HASH_NO_DISC

static void test_identify_and_load_game_from_disc(void)
{
  rc_hash_callbacks_t hash_callbacks;

  size_t image_size;
  uint8_t* image = generate_psx_bin("SLUS_007.45", 0x07D800, &image_size);
  const char cue_single_track[] =
    "FILE \"game.bin\" BINARY\n"
    "  TRACK 01 MODE2/2352\n"
    "    INDEX 01 00:00:00\n";

  g_client = mock_client_logged_in();

  memset(&hash_callbacks, 0, sizeof(hash_callbacks));
  get_mock_filereader(&hash_callbacks.filereader);
  rc_client_set_hash_callbacks(g_client, &hash_callbacks);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)cue_single_track, sizeof(cue_single_track));

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=db433fb038cde4fb15c144e8c7dea6e3", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=db433fb038cde4fb15c144e8c7dea6e3&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_PLAYSTATION, "game.cue",
    NULL, 0, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "db433fb038cde4fb15c144e8c7dea6e3");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

#endif

static void test_identify_and_load_game_unknown_hash(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857", patchdata_not_found);

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "foo.zip#foo.gb",
      image, image_size, rc_client_callback_expect_unknown_game, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_GAMEBOY);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_unknown_hash_repeated(void)
{
  rc_client_async_handle_t* handle;
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  /* first request should resolve the hash asynchronously */
  handle = rc_client_begin_identify_and_load_game(g_client,
      RC_CONSOLE_UNKNOWN, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(handle);
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_GAMEBOY);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");

  /* second request should use the hash cache and not need an asynchronous call */
  handle = rc_client_begin_identify_and_load_game(g_client,
      RC_CONSOLE_UNKNOWN, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NULL(handle);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_GAMEBOY);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_unknown_hash_multiple(void)
{
  rc_client_async_handle_t* handle;

  const size_t disk_size = 256 * 16 * 35; /* 140KB - Apple II disk size */
  uint8_t* disk = generate_generic_file(disk_size);

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  /* first request */
  handle = rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_APPLE_II,
    "disk1.dsk", disk, disk_size, rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(handle);
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=88be638f4d78b4072109e55f13e8a0ac", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_APPLE_II);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "88be638f4d78b4072109e55f13e8a0ac");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_url, default_game_badge);

  /* second request - modify file so new hash is generated */
  disk[16]++;
  handle = rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_APPLE_II,
    "disk1.dsk", disk, disk_size, rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(handle);
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=8e39c6077108cafd6193d1c649b5d695", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_APPLE_II);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "8e39c6077108cafd6193d1c649b5d695");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_url, default_game_badge);

  free(disk);
  rc_client_destroy(g_client);
}

#ifndef RC_HASH_NO_ROM

static void test_identify_and_load_game_unknown_hash_multiconsole(void)
{
  rc_hash_iterator_t* iterator;
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "foo.zip#foo.nes",
    image, image_size, rc_client_callback_expect_unknown_game, g_callback_userdata);

  /* first hash lookup should be pending. inject a secondary console into the iterator */
  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857");
  iterator = rc_client_get_load_state_hash_iterator(g_client);
  ASSERT_NUM_EQUALS(iterator->index, 1);
  ASSERT_NUM_EQUALS(iterator->consoles[iterator->index], 0);
  iterator->consoles[iterator->index] = RC_CONSOLE_MEGA_DRIVE; /* full buffer hash */
  iterator->consoles[iterator->index + 1] = 0;

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857", patchdata_not_found);

  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=64b131c5c7fec32985d9c99700babb7e");
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=64b131c5c7fec32985d9c99700babb7e", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    /* when multiple hashes are tried, console will be unknown and hash will be a CSV */
    ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857,64b131c5c7fec32985d9c99700babb7e");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  }

  rc_client_destroy(g_client);
  free(image);
}

#endif

static void test_identify_and_load_game_unknown_hash_console_specified(void)
{
  rc_hash_iterator_t* iterator;
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  /* explicitly specify we only want the NES hash processed */
  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_GAMEBOY, "foo.zip#foo.gb",
    image, image_size, rc_client_callback_expect_unknown_game, g_callback_userdata);

  /* first hash lookup should be pending. iterator should not have been initialized */
  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857");
  iterator = rc_client_get_load_state_hash_iterator(g_client);
  ASSERT_NUM_EQUALS(iterator->index, 0);
  ASSERT_NUM_EQUALS(iterator->consoles[iterator->index], 0);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_GAMEBOY);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  }

  rc_client_destroy(g_client);
  free(image);
}

static void assert_unknown_hash_parameters(uint32_t console_id, const char* hash, rc_client_t* client, void* callback_data)
{
  ASSERT_NUM_EQUALS(console_id, RC_CONSOLE_GAMEBOY);
  ASSERT_STR_EQUALS(hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_data, g_callback_userdata);
}

static uint32_t rc_client_identify_unknown_hash(uint32_t console_id, const char* hash, rc_client_t* client, void* callback_data)
{
  assert_unknown_hash_parameters(console_id, hash, client, callback_data);
  return 1234;
}

static void test_identify_and_load_game_unknown_hash_client_provided(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();
  g_client->callbacks.identify_unknown_hash = rc_client_identify_unknown_hash;

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857", patchdata_not_found);
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=6a2305a2b6675a97ff792709be1ca857&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_GAMEBOY, "foo.zip#foo.gb",
    image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17); /* patchdata returns 17 */
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_multihash(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=6a2305a2b6675a97ff792709be1ca857&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "abc.dsk",
      image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_multihash_unknown_game(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857", patchdata_not_found);

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "abc.dsk",
      image, image_size, rc_client_callback_expect_unknown_game, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_APPLE_II);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  }

  /* same hash generated for all dsk consoles - only one server call should be made */
  assert_api_call_count("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857", 1);

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_multihash_differ(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "abc.dsk",
      image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  /* modify the checksum so callback for first lookup will generate a new lookup */
  memset(&image[256], 0, 32);

  /* first lookup fails */
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857", patchdata_not_found);
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  /* second lookup should succeed */
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=4989b063a40dcfa28291ff8d675050e3", patchdata_2ach_1lbd);
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=4989b063a40dcfa28291ff8d675050e3&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
 
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "4989b063a40dcfa28291ff8d675050e3");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

#endif /* RC_CLIENT_SUPPORTS_HASH */

/* ----- change media ----- */

static void rc_client_callback_expect_hardcore_disabled_undentified_media(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_HARDCORE_DISABLED);
  ASSERT_STR_EQUALS(error_message, "Hardcore disabled. Unidentified media inserted.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

#ifdef RC_CLIENT_SUPPORTS_HASH

static void test_change_media_required_fields(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  rc_client_begin_identify_and_change_media(g_client, NULL, NULL, 0,
      rc_client_callback_expect_data_or_file_path_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_no_game_loaded(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();

  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_no_game_loaded, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_same_game(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  /* changing known discs within a game set is expected to succeed */
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  /* resetting with a disc from the current game is allowed */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_known_game(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":5555}");

  /* changing to a known disc from another game is allowed */
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  /* resetting with a disc from another game will disable the client */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_unknown_game(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  ASSERT_TRUE(rc_client_get_hardcore_enabled(g_client));

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":0}");

  /* changing to an unknown disc is not allowed - could be a hacked version of one of the game's discs */
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_hardcore_disabled_undentified_media, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  ASSERT_FALSE(rc_client_get_hardcore_enabled(g_client));

  /* resetting with a disc not from the current game will disable the client */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_unhashable(void)
{
  const char* original_hash = NULL;
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  original_hash = g_client->game->public_.hash;

  /* N64 hash will fail with Not a Nintendo 64 ROM */
  g_client->game->public_.console_id = RC_CONSOLE_NINTENDO_64;

  /* changing to a disc not supported by the system is allowed */
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
    rc_client_callback_expect_success, g_callback_userdata);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "[NO HASH]");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  /* switch back to the original media so we can detect a switch back to the unhashable media */
  rc_client_begin_change_media(g_client, original_hash, rc_client_callback_expect_success, g_callback_userdata);

  /* re-entrant call won't try to regenerate the hash */
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "[NO HASH]");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }


  /* resetting with a disc not from the current game will disable the client */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_unhashable_without_generation(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  /* changing to a disc not supported by the system is allowed */
  rc_client_begin_change_media(g_client, "[NO HASH]",
    rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "[NO HASH]");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  /* resetting with a disc not from the current game will disable the client */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_back_and_forth(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);
  uint8_t* image2 = generate_generic_file(image_size);
  memset(&image2[256], 0, 32); /* force image2 to be different */

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=gameid&m=4989b063a40dcfa28291ff8d675050e3", "{\"Success\":true,\"GameID\":1234}");

  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo2.nes", image2, image_size,
      rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo2.nes", image2, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  assert_api_call_count("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", 1);
  assert_api_call_count("r=gameid&m=4989b063a40dcfa28291ff8d675050e3", 1);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "4989b063a40dcfa28291ff8d675050e3");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image2);
  free(image);
}

static void test_change_media_while_loading(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "4989b063a40dcfa28291ff8d675050e3",
      rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  /* media request won't occur until patch data is received */
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=4989b063a40dcfa28291ff8d675050e3", patchdata_2ach_1lbd);
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");

  /* finish loading game */
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=4989b063a40dcfa28291ff8d675050e3&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  /* secondary hash resolution does not occur until game is fully loaded or hash can't be compared to loaded game */
  assert_api_pending("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");
  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_while_loading_later(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "4989b063a40dcfa28291ff8d675050e3",
      rc_client_callback_expect_success, g_callback_userdata);

  /* get past fetching the patch data so there's a valid console for the change media call */
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=4989b063a40dcfa28291ff8d675050e3", patchdata_2ach_1lbd);

  /* change_media should immediately attempt to resolve the new hash */
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);
  assert_api_pending("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");

  /* finish loading game - session will be started with the old hash because the new hash hasn't resolved yet */
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=4989b063a40dcfa28291ff8d675050e3&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_async_aborted(void)
{
  rc_client_async_handle_t* handle;
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  /* changing known discs within a game set is expected to succeed */
  handle = rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
    rc_client_callback_expect_uncalled, g_callback_userdata);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF"); /* old hash retained */
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  /* hash should still have been captured and lookup should succeed without having to call server again */
  reset_mock_api_handlers();

  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
    rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857");

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_client_error(void)
{
  rc_client_async_handle_t* handle;
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  mock_api_error("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "Internet not available.", RC_API_SERVER_RESPONSE_CLIENT_ERROR);

  handle = rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_no_internet, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF"); /* old hash retained */
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  ASSERT_PTR_NULL(handle);

  rc_client_destroy(g_client);
  free(image);
}

#endif

static void test_change_media_from_hash_required_fields(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  rc_client_begin_change_media(g_client, NULL,
    rc_client_callback_expect_hash_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");

  rc_client_destroy(g_client);
}


static void test_change_media_from_hash_no_game_loaded(void)
{
  g_client = mock_client_logged_in();

  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_no_game_loaded, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_same_game(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  /* changing known discs within a game set is expected to succeed */
  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");

  /* resetting with a disc from the current game is allowed */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_known_game(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":5555}");

  /* changing to a known disc from another game is allowed */
  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");

  /* resetting with a disc from another game will disable the client */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_unknown_game(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  ASSERT_TRUE(rc_client_get_hardcore_enabled(g_client));

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":0}");

  /* changing to an unknown disc is not allowed - could be a hacked version of one of the game's discs */
  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_hardcore_disabled_undentified_media, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");

  ASSERT_FALSE(rc_client_get_hardcore_enabled(g_client));

  /* resetting with a disc not from the current game will disable the client */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_back_and_forth(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=gameid&m=4989b063a40dcfa28291ff8d675050e3", "{\"Success\":true,\"GameID\":1234}");

  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_change_media(g_client, "4989b063a40dcfa28291ff8d675050e3",
    rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_change_media(g_client, "4989b063a40dcfa28291ff8d675050e3",
    rc_client_callback_expect_success, g_callback_userdata);

  assert_api_call_count("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", 1);
  assert_api_call_count("r=gameid&m=4989b063a40dcfa28291ff8d675050e3", 1);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "4989b063a40dcfa28291ff8d675050e3");

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_while_loading(void)
{
  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "4989b063a40dcfa28291ff8d675050e3",
    rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_success, g_callback_userdata);

  /* media request won't occur until patch data is received */
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=4989b063a40dcfa28291ff8d675050e3", patchdata_2ach_1lbd);
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");

  /* finish loading game */
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=4989b063a40dcfa28291ff8d675050e3&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  /* secondary hash resolution does not occur until game is fully loaded or hash can't be compared to loaded game */
  assert_api_pending("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");
  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_while_loading_later(void)
{
  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "4989b063a40dcfa28291ff8d675050e3",
    rc_client_callback_expect_success, g_callback_userdata);

  /* get past fetching the patch data so there's a valid console for the change media call */
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=4989b063a40dcfa28291ff8d675050e3", patchdata_2ach_1lbd);

  /* change_media should immediately attempt to resolve the new hash */
  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_success, g_callback_userdata);
  assert_api_pending("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");

  /* finish loading game - session will be started with the old hash because the new hash hasn't resolved yet */
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=4989b063a40dcfa28291ff8d675050e3&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_async_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  /* changing known discs within a game set is expected to succeed */
  handle = rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_uncalled, g_callback_userdata);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF"); /* old hash retained */

  /* hash should still have been captured and lookup should succeed without having to call server again */
  reset_mock_api_handlers();

  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_client_error(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  mock_api_error("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "Internet not available.", RC_API_SERVER_RESPONSE_CLIENT_ERROR);

  handle = rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_no_internet, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF"); /* old hash retained */

  ASSERT_PTR_NULL(handle);

  rc_client_destroy(g_client);
}

/* ----- get game image ----- */

static void test_game_get_image_url(void)
{
  char buffer[256];
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  ASSERT_NUM_EQUALS(rc_client_game_get_image_url(rc_client_get_game_info(g_client), buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "http://server/Images/112233.png");

  rc_client_destroy(g_client);
}

/* ----- fetch hash library ----- */

static void test_fetch_hash_library_response(int result, const char* error_message,
  rc_client_hash_library_t* list, rc_client_t* client, void* callback_userdata)
{
  rc_client_callback_expect_success(result, error_message, client, callback_userdata);
  if (result != RC_OK)
    return;

  ASSERT_NUM_EQUALS(list->num_entries, 3);
  ASSERT_NUM_EQUALS(list->entries[0].game_id, 1);
  ASSERT_STR_EQUALS(list->entries[0].hash, "aabbccddeeff00112233445566778899");
  ASSERT_NUM_EQUALS(list->entries[1].game_id, 1);
  ASSERT_STR_EQUALS(list->entries[1].hash, "99aabbccddeeff001122334455667788");
  ASSERT_NUM_EQUALS(list->entries[2].game_id, 2);
  ASSERT_STR_EQUALS(list->entries[2].hash, "8899aabbccddeeff0011223344556677");

  rc_client_destroy_hash_library(list);
}

static void test_fetch_hash_library(void)
{
  g_client = mock_client_not_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=hashlibrary&c=1", "{\"Success\":true,\"MD5List\":{"
    "\"aabbccddeeff00112233445566778899\":1,"
    "\"99aabbccddeeff001122334455667788\":1,"
    "\"8899aabbccddeeff0011223344556677\":2"
    "}}");

  rc_client_begin_fetch_hash_library(g_client, 1, test_fetch_hash_library_response, g_callback_userdata);
  rc_client_destroy(g_client);
}

/* ----- fetch game titles ----- */

static void test_fetch_game_titles_response(int result, const char* error_message,
  rc_client_game_title_list_t* list, rc_client_t* client, void* callback_userdata)
{
  rc_client_callback_expect_success(result, error_message, client, callback_userdata);
  if (result != RC_OK)
    return;

  ASSERT_NUM_EQUALS(list->num_entries, 3);
  ASSERT_NUM_EQUALS(list->entries[0].game_id, 3);
  ASSERT_STR_EQUALS(list->entries[0].title, "Game Name 3");
  ASSERT_STR_EQUALS(list->entries[0].badge_name, "010003");
  ASSERT_NUM_EQUALS(list->entries[1].game_id, 4);
  ASSERT_STR_EQUALS(list->entries[1].title, "Game Name 4");
  ASSERT_STR_EQUALS(list->entries[1].badge_name, "010004");
  ASSERT_NUM_EQUALS(list->entries[2].game_id, 7);
  ASSERT_STR_EQUALS(list->entries[2].title, "Game Name 7");
  ASSERT_STR_EQUALS(list->entries[2].badge_name, "010007");

  rc_client_destroy_game_title_list(list);
}

static void test_fetch_game_titles(void)
{
  const uint32_t game_ids[] = { 3, 4, 7 };
  g_client = mock_client_not_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameinfolist&g=3,4,7", "{\"Success\":true,\"Response\":["
    "{\"ID\": 3, \"Title\":\"Game Name 3\", \"ImageIcon\": \"/Images/010003.png\"},"
    "{\"ID\": 4, \"Title\":\"Game Name 4\", \"ImageIcon\": \"/Images/010004.png\"},"
    "{\"ID\": 7, \"Title\":\"Game Name 7\", \"ImageIcon\": \"/Images/010007.png\"}"
    "]}");

  rc_client_begin_fetch_game_titles(g_client, game_ids, 3, test_fetch_game_titles_response, g_callback_userdata);
  rc_client_destroy(g_client);
}

/* ----- all user progress ----- */

static void test_fetch_all_user_progress_response(int result, const char* error_message,
  rc_client_all_user_progress_t* list, rc_client_t* client, void* callback_userdata)
{
  rc_client_callback_expect_success(result, error_message, client, callback_userdata);
  if (result != RC_OK)
    return;

  ASSERT_NUM_EQUALS(list->num_entries, 4);
  ASSERT_NUM_EQUALS(list->entries[0].game_id, 10);
  ASSERT_NUM_EQUALS(list->entries[0].num_achievements, 11);
  ASSERT_NUM_EQUALS(list->entries[0].num_unlocked_achievements, 0);
  ASSERT_NUM_EQUALS(list->entries[0].num_unlocked_achievements_hardcore, 0);
  ASSERT_NUM_EQUALS(list->entries[1].game_id, 20);
  ASSERT_NUM_EQUALS(list->entries[1].num_achievements, 21);
  ASSERT_NUM_EQUALS(list->entries[1].num_unlocked_achievements, 22);
  ASSERT_NUM_EQUALS(list->entries[1].num_unlocked_achievements_hardcore, 0);
  ASSERT_NUM_EQUALS(list->entries[2].game_id, 30);
  ASSERT_NUM_EQUALS(list->entries[2].num_achievements, 31);
  ASSERT_NUM_EQUALS(list->entries[2].num_unlocked_achievements, 32);
  ASSERT_NUM_EQUALS(list->entries[2].num_unlocked_achievements_hardcore, 33);
  ASSERT_NUM_EQUALS(list->entries[3].game_id, 40);
  ASSERT_NUM_EQUALS(list->entries[3].num_achievements, 41);
  ASSERT_NUM_EQUALS(list->entries[3].num_unlocked_achievements, 0);
  ASSERT_NUM_EQUALS(list->entries[3].num_unlocked_achievements_hardcore, 43);

  rc_client_destroy_all_user_progress(list);
}

static void test_fetch_all_user_progress(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=allprogress&u=Username&t=ApiToken&c=1", "{\"Success\":true,\"Response\":{\"10\":{\"Achievements\":11},"
      "\"20\":{\"Achievements\":21,\"Unlocked\":22},"
      "\"30\":{\"Achievements\":31,\"Unlocked\":32,\"UnlockedHardcore\":33},"
      "\"40\":{\"Achievements\":41,\"UnlockedHardcore\":43}}}");

  rc_client_begin_fetch_all_user_progress(g_client, 1, test_fetch_all_user_progress_response, g_callback_userdata);
  rc_client_destroy(g_client);
}

/* ----- subset ----- */

static void test_load_subset(void)
{
  rc_client_achievement_info_t* achievement;
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_subset_info_t* subset_info;
  const rc_client_subset_t* subset;
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_subset);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 7);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 7);
  }

  subset = rc_client_get_subset_info(g_client, 2345);
  ASSERT_PTR_NOT_NULL(subset);
  if (subset) {
    subset_info = g_client->game->subsets->next;
    ASSERT_PTR_EQUALS(subset, &subset_info->public_);

    ASSERT_NUM_EQUALS(subset->id, 2345);
    ASSERT_STR_EQUALS(subset->title, "Bonus");
    ASSERT_STR_EQUALS(subset->badge_name, "112234");
    ASSERT_STR_EQUALS(subset->badge_url, "http://server/Images/112234.png");
    ASSERT_NUM_EQUALS(subset->num_achievements, 3);
    ASSERT_NUM_EQUALS(subset->num_leaderboards, 2);

    achievement = &subset_info->achievements[0];
    ASSERT_NUM_EQUALS(achievement->public_.id, 5501);
    ASSERT_STR_EQUALS(achievement->public_.title, "Achievement 5501");
    ASSERT_STR_EQUALS(achievement->public_.description, "Desc 5501");
    ASSERT_STR_EQUALS(achievement->public_.badge_name, "005501");
    ASSERT_STR_EQUALS(achievement->public_.badge_url, "https://media.retroachievements.org/Badge/005501.png");
    ASSERT_STR_EQUALS(achievement->public_.badge_locked_url, "https://media.retroachievements.org/Badge/005501_lock.png");
    ASSERT_NUM_EQUALS(achievement->public_.points, 5);
    ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_STR_EQUALS(achievement->author, "User1");
    ASSERT_NUM_EQUALS(achievement->created_time, 1367266583);
    ASSERT_NUM_EQUALS(achievement->updated_time, 1376929305);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    achievement = &subset_info->achievements[1];
    ASSERT_NUM_EQUALS(achievement->public_.id, 5502);
    ASSERT_STR_EQUALS(achievement->public_.title, "Achievement 5502");
    ASSERT_STR_EQUALS(achievement->public_.description, "Desc 5502");
    ASSERT_STR_EQUALS(achievement->public_.badge_name, "005502");
    ASSERT_NUM_EQUALS(achievement->public_.points, 5);
    ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    achievement = &subset_info->achievements[2];
    ASSERT_NUM_EQUALS(achievement->public_.id, 5503);
    ASSERT_STR_EQUALS(achievement->public_.title, "Achievement 5503");
    ASSERT_STR_EQUALS(achievement->public_.description, "Desc 5503");
    ASSERT_STR_EQUALS(achievement->public_.badge_name, "005503");
    ASSERT_NUM_EQUALS(achievement->public_.points, 5);
    ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    leaderboard = &subset_info->leaderboards[0];
    ASSERT_NUM_EQUALS(leaderboard->public_.id, 81);
    ASSERT_STR_EQUALS(leaderboard->public_.title, "Leaderboard 81");
    ASSERT_STR_EQUALS(leaderboard->public_.description, "Desc 81");
    ASSERT_NUM_EQUALS(leaderboard->public_.state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
    ASSERT_PTR_NOT_NULL(leaderboard->lboard);
    ASSERT_NUM_NOT_EQUALS(leaderboard->value_djb2, 0);
    ASSERT_PTR_NULL(leaderboard->tracker);

    leaderboard = &subset_info->leaderboards[1];
    ASSERT_NUM_EQUALS(leaderboard->public_.id, 82);
    ASSERT_STR_EQUALS(leaderboard->public_.title, "Leaderboard 82");
    ASSERT_STR_EQUALS(leaderboard->public_.description, "Desc 82");
    ASSERT_NUM_EQUALS(leaderboard->public_.state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
    ASSERT_PTR_NOT_NULL(leaderboard->lboard);
    ASSERT_NUM_NOT_EQUALS(leaderboard->value_djb2, 0);
    ASSERT_PTR_NULL(leaderboard->tracker);
  }

  rc_client_destroy(g_client);
}

static void test_subset_list(void)
{
  rc_client_subset_list_t* list;
  const rc_client_subset_t* subset;

  g_client = mock_client_game_loaded(patchdata_subset, no_unlocks);

  list = rc_client_create_subset_list(g_client);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_subsets, 2);

    subset = list->subsets[0];
    ASSERT_NUM_EQUALS(subset->id, 1111);
    ASSERT_STR_EQUALS(subset->title, "Sample Game");
    ASSERT_STR_EQUALS(subset->badge_name, "112233");
    ASSERT_NUM_EQUALS(subset->num_achievements, 7);
    ASSERT_NUM_EQUALS(subset->num_leaderboards, 7);
    ASSERT_STR_EQUALS(subset->badge_url, "http://server/Images/112233.png");

    subset = list->subsets[1];
    ASSERT_NUM_EQUALS(subset->id, 2345);
    ASSERT_STR_EQUALS(subset->title, "Bonus");
    ASSERT_STR_EQUALS(subset->badge_name, "112234");
    ASSERT_NUM_EQUALS(subset->num_achievements, 3);
    ASSERT_NUM_EQUALS(subset->num_leaderboards, 2);
    ASSERT_STR_EQUALS(subset->badge_url, "http://server/Images/112234.png");

    rc_client_destroy_subset_list(list);
  }

  rc_client_unload_game(g_client);
  ASSERT_PTR_NULL(g_client->game);

  list = rc_client_create_subset_list(g_client);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_subsets, 0);
    rc_client_destroy_subset_list(list);
  }

  rc_client_destroy(g_client);
}

/* ----- achievement list ----- */

static void test_achievement_list_simple(void)
{
  rc_client_achievement_list_t* list;
  const rc_client_achievement_t** iter;
  const rc_client_achievement_t* achievement;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 0);
    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_simple_with_unlocks(void)
{
  rc_client_achievement_list_t* list;
  const rc_client_achievement_t** iter;
  const rc_client_achievement_t* achievement;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    /* in hardcore mode, 5501 should be unlocked, but 5502 will be locked */
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    iter = list->buckets[1].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_set_hardcore_enabled(g_client, 0);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    /* in softcore mode, both should be unlocked */
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_simple_with_unlocks_encore_mode(void)
{
  rc_client_achievement_list_t* list;
  const rc_client_achievement_t** iter;
  const rc_client_achievement_t* achievement;

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_logged_in();
  rc_client_set_encore_mode_enabled(g_client, 1);
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    /* in hardcore mode, 5501 should be unlocked, but both will appear locked due to encore mode */
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_set_hardcore_enabled(g_client, 0);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=0&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, unlock_5501h_and_5502);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    /* in softcore mode, both should be unlocked, but will appear locked due to encore mode */
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);

    rc_client_destroy_achievement_list(list);
  }

  /* unlock 5501, should appear unlocked again */
  mock_memory(memory, sizeof(memory));
  rc_client_do_frame(g_client);
  memory[1] = 3;
  memory[2] = 7;
  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5501&h=0&m=0123456789ABCDEF&v=7f066800cd3962efeb1f479e5671b59c",
    "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":5,\"AchievementsRemaining\":6}");

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5501));

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);

    achievement = list->buckets[0].achievements[0];
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);

    achievement = list->buckets[1].achievements[0];
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Recently Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);

    achievement = list->buckets[0].achievements[0];
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);

    achievement = list->buckets[1].achievements[0];
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_simple_with_unofficial_and_unsupported(void)
{
  rc_client_achievement_list_t* list;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_unofficial_unsupported, no_unlocks);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Unofficial");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unofficial");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5502);
    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_simple_with_unofficial_off(void)
{
  rc_client_achievement_list_t* list;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 0);
  mock_client_load_game(patchdata_unofficial_unsupported, no_unlocks);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 0);
    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_buckets(void)
{
  rc_client_achievement_list_t* list;
  const rc_client_achievement_t** iter;
  const rc_client_achievement_t* achievement;

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, unlock_8);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client); /* advance achievements out of waiting state */
  event_count = 0;

  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5&h=1&m=0123456789ABCDEF&v=732f8e30e9c1eb08948dda098c305d8b",
      "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":5,\"AchievementsRemaining\":6}");

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 6);
    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 7);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  memory[5] = 5; /* trigger achievement 5 */
  memory[6] = 2; /* start measuring achievement 6 */
  memory[1] = 1; /* begin challenge achievement 7 */
  memory[0x11] = 100; /* start measuring achievements 70 and 71 */
  rc_client_do_frame(g_client);
  event_count = 0;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 4);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active Challenges");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 7);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Recently Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 4);
    iter = list->buckets[2].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "2/6");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 33.333333);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25,600/100,000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25%");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  /* also check mapping to lock state */
  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 5);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 6);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[1]->id, 7);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[2]->id, 9);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[3]->id, 70);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[4]->id, 71);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[1]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  /* recently unlocked achievement no longer recent */
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5))->unlock_time -= 15 * 60;
  memory[6] = 5; /* almost there achievement 6 */
  memory[1] = 0; /* stop challenge achievement 7 */
  rc_client_do_frame(g_client);
  event_count = 0;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Almost There");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 6);
    ASSERT_STR_EQUALS(list->buckets[0].achievements[0]->measured_progress, "5/6");
    ASSERT_FLOAT_EQUALS(list->buckets[0].achievements[0] ->measured_percent, 83.333333);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 4);
    iter = list->buckets[1].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 7);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25,600/100,000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25%");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[1]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_buckets_progress_sort(void)
{
  rc_client_achievement_list_t* list;
  const rc_client_achievement_t** iter;
  const rc_client_achievement_t* achievement;

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client); /* advance achievements out of waiting state */
  event_count = 0;

  g_client->game->subsets->achievements[0].trigger->measured_target = 100;
  g_client->game->subsets->achievements[0].trigger->measured_value = 86;
  g_client->game->subsets->achievements[1].trigger->measured_target = 100;
  g_client->game->subsets->achievements[1].trigger->measured_value = 85;
  g_client->game->subsets->achievements[2].trigger->measured_target = 1000;
  g_client->game->subsets->achievements[2].trigger->measured_value = 855;
  g_client->game->subsets->achievements[3].trigger->measured_target = 100;
  g_client->game->subsets->achievements[3].trigger->measured_value = 87;
  g_client->game->subsets->achievements[4].trigger->measured_target = 100;
  g_client->game->subsets->achievements[4].trigger->measured_value = 85;
  g_client->game->subsets->achievements[5].trigger->measured_target = 100;
  g_client->game->subsets->achievements[5].trigger->measured_value = 75;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Almost There");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 5);
    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 87.0f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "87/100");
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 86.0f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "86/100");
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 85.5f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "855/1,000");
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 85.0f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "85/100");
    ASSERT_NUM_EQUALS(achievement->id, 6);
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 85.0f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "85/100");
    ASSERT_NUM_EQUALS(achievement->id, 9);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 2);
    ASSERT_FLOAT_EQUALS(list->buckets[1].achievements[0]->measured_percent, 75.0f);
    ASSERT_STR_EQUALS(list->buckets[1].achievements[0]->measured_progress, "75/100");
    ASSERT_FLOAT_EQUALS(list->buckets[1].achievements[1]->measured_percent, 0.0f);
    ASSERT_STR_EQUALS(list->buckets[1].achievements[1]->measured_progress, "");

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_buckets_progress_sort_big_ids(void)
{
  rc_client_achievement_list_t* list;
  const rc_client_achievement_t** iter;
  const rc_client_achievement_t* achievement;

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_big_ids, no_unlocks);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client); /* advance achievements out of waiting state */
  event_count = 0;

  g_client->game->subsets->achievements[0].trigger->measured_target = 100;
  g_client->game->subsets->achievements[0].trigger->measured_value = 86;
  g_client->game->subsets->achievements[1].trigger->measured_target = 100;
  g_client->game->subsets->achievements[1].trigger->measured_value = 85;
  g_client->game->subsets->achievements[2].trigger->measured_target = 100;
  g_client->game->subsets->achievements[2].trigger->measured_value = 85;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Almost There");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 3);
    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 86.0f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "86/100");
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 85.0f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "85/100");
    ASSERT_NUM_EQUALS(achievement->id, 6);
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 85.0f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "85/100");
    ASSERT_NUM_EQUALS(achievement->id, 4294967295UL);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_FLOAT_EQUALS(list->buckets[1].achievements[0]->measured_percent, 0.0f);
    ASSERT_STR_EQUALS(list->buckets[1].achievements[0]->measured_progress, "");

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_buckets_with_unsynced(void)
{
  rc_client_achievement_list_t* list;
  rc_client_achievement_t* achievement;
  const char* unlock_request_params = "r=awardachievement&u=Username&t=ApiToken&a=5&h=1&m=0123456789ABCDEF&v=732f8e30e9c1eb08948dda098c305d8b";
  const char* unlock_request_params2 = "r=awardachievement&u=Username&t=ApiToken&a=5&h=1&m=0123456789ABCDEF&o=2&v=3cca3f9a9cfd6c0d5b6a17b9bb539070";

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, unlock_8);
  g_client->callbacks.server_call = rc_client_server_call_async;
  mock_memory(memory, sizeof(memory));

  /* discard the queued ping to make finding the retry easier */
  g_client->state.scheduled_callbacks = NULL;

  rc_client_do_frame(g_client); /* advance achievements out of waiting state */
  event_count = 0;

  memory[5] = 5; /* trigger achievement 5 */
  memory[1] = 1; /* begin challenge achievement 7 */
  rc_client_do_frame(g_client);
  event_count = 0;

  /* first failure will immediately requeue the request */
  async_api_error(unlock_request_params, response_503, 503);
  assert_api_pending(unlock_request_params);
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);
  rc_client_idle(g_client);

  /* second failure will queue it */
  async_api_error(unlock_request_params, response_503, 503);
  assert_api_call_count(unlock_request_params, 0);
  ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
  rc_client_idle(g_client);
  event_count = 0;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list)
  {
    ASSERT_NUM_EQUALS(list->num_buckets, 4);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active Challenges");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 7);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Recently Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 4);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  /* adjust unlock time on achievement 5 so it's no longer recent */
  achievement = (rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5);
  achievement->unlock_time -= 15 * 60;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list)
  {
    ASSERT_NUM_EQUALS(list->num_buckets, 4);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active Challenges");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 7);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocks Not Synced to Server");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 4);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  /* allow unlock request to succeed */
  g_now += 2 * 1000;
  rc_client_idle(g_client);
  assert_api_pending(unlock_request_params2);
  async_api_response(unlock_request_params2, "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list)
  {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active Challenges");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 7);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 4);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[1]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_subset_with_unofficial_and_unsupported(void)
{
  rc_client_achievement_list_t* list;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_subset_unofficial_unsupported, no_unlocks);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 6);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[1]->id, 6);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[2]->id, 7);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[3]->id, 9);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[4]->id, 70);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[5]->id, 71);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Bonus - Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5501);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Bonus - Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Unofficial");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 8);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Bonus - Unofficial");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 5);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 6);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[1]->id, 6);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[2]->id, 7);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[3]->id, 9);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[4]->id, 70);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[5]->id, 71);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Sample Game - Unofficial");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 8);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Bonus - Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5501);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Bonus - Unofficial");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 5502);

    ASSERT_NUM_EQUALS(list->buckets[4].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[4].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[4].label, "Bonus - Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[4].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[4].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_subset_buckets(void)
{
  rc_client_achievement_list_t* list;
  const rc_client_achievement_t** iter;
  const rc_client_achievement_t* achievement;

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_subset, unlock_8_and_5502);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client); /* advance achievements out of waiting state */
  event_count = 0;

  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5&h=1&m=0123456789ABCDEF&v=732f8e30e9c1eb08948dda098c305d8b",
      "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":5,\"AchievementsRemaining\":6}");
  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&v=9b9bdf5501eb6289a6655affbcc695e6",
      "{\"Success\":true,\"Score\":5437,\"SoftcoreScore\":777,\"AchievementID\":5,\"AchievementsRemaining\":6}");

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 4);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 6);
    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 7);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Sample Game - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 8);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Bonus - Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[1]->id, 5503);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Bonus - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  memory[5] = 5; /* trigger achievement 5 */
  memory[6] = 2; /* start measuring achievement 6 */
  memory[1] = 1; /* begin challenge achievement 7 */
  memory[0x11] = 100; /* start measuring achievements 70 and 71 */
  memory[0x17] = 7; /* trigger achievement 5501 */
  rc_client_do_frame(g_client);
  event_count = 0;

  /* set the unlock time for achievement 5 back one second to ensure consistent sorting */
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5))->unlock_time--;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 6);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active Challenges");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 7);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Recently Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[1]->id, 5);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 4);
    iter = list->buckets[2].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "2/6");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 33.333333);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25,600/100,000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25%");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Sample Game - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 8);

    ASSERT_NUM_EQUALS(list->buckets[4].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[4].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[4].label, "Bonus - Locked");
    ASSERT_NUM_EQUALS(list->buckets[4].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[4].achievements[0]->id, 5503);

    ASSERT_NUM_EQUALS(list->buckets[5].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[5].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[5].label, "Bonus - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[5].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[5].achievements[0]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  /* recently unlocked achievements no longer recent */
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5))->unlock_time -= 15 * 60;
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5501))->unlock_time -= 15 * 60;
  memory[6] = 5; /* almost there achievement 6 */
  memory[1] = 0; /* stop challenge achievement 7 */
  rc_client_do_frame(g_client);
  event_count = 0;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 5);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Almost There");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 6);
    ASSERT_STR_EQUALS(list->buckets[0].achievements[0]->measured_progress, "5/6");
    ASSERT_FLOAT_EQUALS(list->buckets[0].achievements[0] ->measured_percent, 83.333333);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 4);
    iter = list->buckets[1].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 7);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25,600/100,000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25%");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Sample Game - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[1]->id, 8);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Bonus - Locked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 5503);

    ASSERT_NUM_EQUALS(list->buckets[4].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[4].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[4].label, "Bonus - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[4].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[4].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[4].achievements[1]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_get_image_url(void)
{
  char buffer[256];
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  ASSERT_NUM_EQUALS(rc_client_achievement_get_image_url(rc_client_get_achievement_info(g_client, 5501),
      RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED, buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "https://media.retroachievements.org/Badge/00234.png");

  ASSERT_NUM_EQUALS(rc_client_achievement_get_image_url(rc_client_get_achievement_info(g_client, 5501),
      RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE, buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "https://media.retroachievements.org/Badge/00234_lock.png");

  ASSERT_NUM_EQUALS(rc_client_achievement_get_image_url(rc_client_get_achievement_info(g_client, 5501),
      RC_CLIENT_ACHIEVEMENT_STATE_DISABLED, buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "https://media.retroachievements.org/Badge/00234_lock.png");

  ASSERT_NUM_EQUALS(rc_client_achievement_get_image_url(rc_client_get_achievement_info(g_client, 5501),
      RC_CLIENT_ACHIEVEMENT_STATE_INACTIVE, buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "https://media.retroachievements.org/Badge/00234_lock.png");

  rc_client_destroy(g_client);
}

/* ----- leaderboards ----- */

static void test_leaderboard_list_simple(void)
{
  rc_client_leaderboard_list_t* list;
  const rc_client_leaderboard_t** iter;
  const rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, sizeof(memory));
  mock_client_load_game(patchdata_exhaustive, no_unlocks);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ALL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "All");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 7);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0A] = 1; /* start 45,46,47 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ALL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "All");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 7);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_simple_with_unsupported(void)
{
  rc_client_leaderboard_list_t* list;
  const rc_client_leaderboard_t** iter;
  const rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, 0x0E); /* 0x0E address is now invalid (44,45,46,47,48)*/
  mock_client_load_game(patchdata_exhaustive, no_unlocks);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ALL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "All");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 2);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 5);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_buckets(void)
{
  rc_client_leaderboard_list_t* list;
  const rc_client_leaderboard_t** iter;
  const rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, sizeof(memory));
  mock_client_load_game(patchdata_exhaustive, no_unlocks);

  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 7);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0A] = 1; /* start 45,46,47 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 3);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 4);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_buckets_with_unsupported(void)
{
  rc_client_leaderboard_list_t* list;
  const rc_client_leaderboard_t** iter;
  const rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, 0x0E); /* 0x0E address is now invalid (44,45,46,47,48)*/
  mock_client_load_game(patchdata_exhaustive, no_unlocks);

  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 2);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 5);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0B] = 3; /* start 52 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 1);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 1);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[2].num_leaderboards, 5);

    iter = list->buckets[2].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_subset(void)
{
  rc_client_leaderboard_list_t* list;
  const rc_client_leaderboard_t** iter;
  const rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, sizeof(memory));
  mock_client_load_game(patchdata_subset, no_unlocks);

  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Inactive");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 7);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Bonus - Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 2);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 81);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 82);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0A] = 1; /* start 45,46,47 */
  memory[0x08] = 2; /* start 82 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 4);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 82);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Sample Game - Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 4);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Bonus - Inactive");
    ASSERT_NUM_EQUALS(list->buckets[2].num_leaderboards, 1);

    iter = list->buckets[2].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 81);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_hidden(void)
{
  rc_client_leaderboard_list_t* list;
  const rc_client_leaderboard_t** iter;
  const rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, sizeof(memory));
  mock_client_load_game(patchdata_leaderboards_hidden, no_unlocks);

  rc_client_do_frame(g_client);

  /* hidden leaderboards (45+48) should not appear in list */

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 3);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0A] = 1; /* start 45,46,47 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 2);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 1);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static const char* lbinfo_4401_top_10 = "{\"Success\":true,\"LeaderboardData\":{\"LBID\":4401,\"GameID\":1234,"
    "\"LowerIsBetter\":1,\"LBTitle\":\"Leaderboard1\",\"LBDesc\":\"Desc1\",\"LBFormat\":\"SCORE\","
    "\"LBMem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xH000D=2::VAL:0x 000E\",\"LBAuthor\":null,"
    "\"LBCreated\":\"2013-10-20 22:12:21\",\"LBUpdated\":\"2021-06-14 08:18:19\",\"TotalEntries\":78,"
    "\"Entries\":["
      "{\"User\":\"PlayerG\",\"Score\":3524,\"Rank\":1,\"Index\":1,\"DateSubmitted\":1615654895},"
      "{\"User\":\"PlayerB\",\"Score\":3645,\"Rank\":2,\"Index\":2,\"DateSubmitted\":1615634566},"
      "{\"User\":\"DisplayName\",\"Score\":3754,\"Rank\":3,\"Index\":3,\"DateSubmitted\":1615234553},"
      "{\"User\":\"PlayerC\",\"Score\":3811,\"Rank\":4,\"Index\":4,\"DateSubmitted\":1615653844},"
      "{\"User\":\"PlayerF\",\"Score\":3811,\"Rank\":4,\"Index\":5,\"DateSubmitted\":1615623878},"
      "{\"User\":\"PlayerA\",\"Score\":3811,\"Rank\":4,\"Index\":6,\"DateSubmitted\":1615653284},"
      "{\"User\":\"PlayerI\",\"Score\":3902,\"Rank\":7,\"Index\":7,\"DateSubmitted\":1615632174},"
      "{\"User\":\"PlayerE\",\"Score\":3956,\"Rank\":8,\"Index\":8,\"DateSubmitted\":1616384834},"
      "{\"User\":\"PlayerD\",\"Score\":3985,\"Rank\":9,\"Index\":9,\"DateSubmitted\":1615238383},"
      "{\"User\":\"PlayerH\",\"Score\":4012,\"Rank\":10,\"Index\":10,\"DateSubmitted\":1615638984}"
    "]"
  "}}";

static const char* lbinfo_4401_top_10_no_user = "{\"Success\":true,\"LeaderboardData\":{\"LBID\":4401,\"GameID\":1234,"
    "\"LowerIsBetter\":1,\"LBTitle\":\"Leaderboard1\",\"LBDesc\":\"Desc1\",\"LBFormat\":\"SCORE\","
    "\"LBMem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xH000D=2::VAL:0x 000E\",\"LBAuthor\":null,"
    "\"LBCreated\":\"2013-10-20 22:12:21\",\"LBUpdated\":\"2021-06-14 08:18:19\",\"TotalEntries\":78,"
    "\"Entries\":["
      "{\"User\":\"PlayerG\",\"Score\":3524,\"Rank\":1,\"Index\":1,\"DateSubmitted\":1615654895},"
      "{\"User\":\"PlayerB\",\"Score\":3645,\"Rank\":2,\"Index\":2,\"DateSubmitted\":1615634566},"
      "{\"User\":\"PlayerJ\",\"Score\":3754,\"Rank\":3,\"Index\":3,\"DateSubmitted\":1615234553},"
      "{\"User\":\"PlayerC\",\"Score\":3811,\"Rank\":4,\"Index\":4,\"DateSubmitted\":1615653844},"
      "{\"User\":\"PlayerF\",\"Score\":3811,\"Rank\":4,\"Index\":5,\"DateSubmitted\":1615623878},"
      "{\"User\":\"PlayerA\",\"Score\":3811,\"Rank\":4,\"Index\":6,\"DateSubmitted\":1615653284},"
      "{\"User\":\"PlayerI\",\"Score\":3902,\"Rank\":7,\"Index\":7,\"DateSubmitted\":1615632174},"
      "{\"User\":\"PlayerE\",\"Score\":3956,\"Rank\":8,\"Index\":8,\"DateSubmitted\":1616384834},"
      "{\"User\":\"PlayerD\",\"Score\":3985,\"Rank\":9,\"Index\":9,\"DateSubmitted\":1615238383},"
      "{\"User\":\"PlayerH\",\"Score\":4012,\"Rank\":10,\"Index\":10,\"DateSubmitted\":1615638984}"
    "]"
  "}}";

static const char* lbinfo_4401_near_user = "{\"Success\":true,\"LeaderboardData\":{\"LBID\":4401,\"GameID\":1234,"
    "\"LowerIsBetter\":1,\"LBTitle\":\"Leaderboard1\",\"LBDesc\":\"Desc1\",\"LBFormat\":\"SCORE\","
    "\"LBMem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xH000D=2::VAL:0x 000E\",\"LBAuthor\":null,"
    "\"LBCreated\":\"2013-10-20 22:12:21\",\"LBUpdated\":\"2021-06-14 08:18:19\",\"TotalEntries\":78,"
    "\"Entries\":["
      "{\"User\":\"PlayerG\",\"Score\":3524,\"Rank\":17,\"Index\":17,\"DateSubmitted\":1615654895},"
      "{\"User\":\"PlayerB\",\"Score\":3645,\"Rank\":18,\"Index\":18,\"DateSubmitted\":1615634566},"
      "{\"User\":\"PlayerC\",\"Score\":3811,\"Rank\":19,\"Index\":19,\"DateSubmitted\":1615653844},"
      "{\"User\":\"PlayerF\",\"Score\":3811,\"Rank\":19,\"Index\":20,\"DateSubmitted\":1615623878},"
      "{\"User\":\"DisplayName\",\"Score\":3811,\"Rank\":19,\"Index\":21,\"DateSubmitted\":1615234553},"
      "{\"User\":\"PlayerA\",\"Score\":3811,\"Rank\":19,\"Index\":22,\"DateSubmitted\":1615653284},"
      "{\"User\":\"PlayerI\",\"Score\":3902,\"Rank\":23,\"Index\":23,\"DateSubmitted\":1615632174},"
      "{\"User\":\"PlayerE\",\"Score\":3956,\"Rank\":24,\"Index\":24,\"DateSubmitted\":1616384834},"
      "{\"User\":\"PlayerD\",\"Score\":3985,\"Rank\":25,\"Index\":25,\"DateSubmitted\":1615238383},"
      "{\"User\":\"PlayerH\",\"Score\":4012,\"Rank\":26,\"Index\":26,\"DateSubmitted\":1615638984}"
    "]"
  "}}";

static rc_client_leaderboard_entry_list_t* g_leaderboard_entries = NULL;
static void rc_client_callback_expect_leaderboard_entry_list(int result, const char* error_message, rc_client_leaderboard_entry_list_t* list, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_OK);
  ASSERT_PTR_NULL(error_message);
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);

  ASSERT_PTR_NOT_NULL(list);
  g_leaderboard_entries = list;
}

static void test_fetch_leaderboard_entries(void)
{
  rc_client_leaderboard_entry_t* entry;
  char url[256];

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  g_leaderboard_entries = NULL;

  mock_api_response("r=lbinfo&i=4401&c=10", lbinfo_4401_top_10);

  rc_client_begin_fetch_leaderboard_entries(g_client, 4401, 1, 10,
      rc_client_callback_expect_leaderboard_entry_list, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(g_leaderboard_entries);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->num_entries, 10);
  ASSERT_NUM_EQUALS(g_leaderboard_entries->total_entries, 78);

  entry = g_leaderboard_entries->entries;
  ASSERT_STR_EQUALS(entry->user, "PlayerG");
  ASSERT_STR_EQUALS(entry->display, "003524");
  ASSERT_NUM_EQUALS(entry->index, 1);
  ASSERT_NUM_EQUALS(entry->rank, 1);
  ASSERT_NUM_EQUALS(entry->submitted, 1615654895);

  ASSERT_NUM_EQUALS(rc_client_leaderboard_entry_get_user_image_url(entry, url, sizeof(url)), RC_OK);
  ASSERT_STR_EQUALS(url, "https://media.retroachievements.org/UserPic/PlayerG.png");

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerB");
  ASSERT_STR_EQUALS(entry->display, "003645");
  ASSERT_NUM_EQUALS(entry->index, 2);
  ASSERT_NUM_EQUALS(entry->rank, 2);
  ASSERT_NUM_EQUALS(entry->submitted, 1615634566);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "DisplayName");
  ASSERT_STR_EQUALS(entry->display, "003754");
  ASSERT_NUM_EQUALS(entry->index, 3);
  ASSERT_NUM_EQUALS(entry->rank, 3);
  ASSERT_NUM_EQUALS(entry->submitted, 1615234553);

  ASSERT_NUM_EQUALS(rc_client_leaderboard_entry_get_user_image_url(entry, url, sizeof(url)), RC_OK);
  ASSERT_STR_EQUALS(url, "https://media.retroachievements.org/UserPic/DisplayName.png");

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerC");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 4);
  ASSERT_NUM_EQUALS(entry->rank, 4);
  ASSERT_NUM_EQUALS(entry->submitted, 1615653844);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerF");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 5);
  ASSERT_NUM_EQUALS(entry->rank, 4);
  ASSERT_NUM_EQUALS(entry->submitted, 1615623878);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerA");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 6);
  ASSERT_NUM_EQUALS(entry->rank, 4);
  ASSERT_NUM_EQUALS(entry->submitted, 1615653284);

  ASSERT_NUM_EQUALS(rc_client_leaderboard_entry_get_user_image_url(entry, url, sizeof(url)), RC_OK);
  ASSERT_STR_EQUALS(url, "https://media.retroachievements.org/UserPic/PlayerA.png");

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerI");
  ASSERT_STR_EQUALS(entry->display, "003902");
  ASSERT_NUM_EQUALS(entry->index, 7);
  ASSERT_NUM_EQUALS(entry->rank, 7);
  ASSERT_NUM_EQUALS(entry->submitted, 1615632174);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerE");
  ASSERT_STR_EQUALS(entry->display, "003956");
  ASSERT_NUM_EQUALS(entry->index, 8);
  ASSERT_NUM_EQUALS(entry->rank, 8);
  ASSERT_NUM_EQUALS(entry->submitted, 1616384834);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerD");
  ASSERT_STR_EQUALS(entry->display, "003985");
  ASSERT_NUM_EQUALS(entry->index, 9);
  ASSERT_NUM_EQUALS(entry->rank, 9);
  ASSERT_NUM_EQUALS(entry->submitted, 1615238383);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerH");
  ASSERT_STR_EQUALS(entry->display, "004012");
  ASSERT_NUM_EQUALS(entry->index, 10);
  ASSERT_NUM_EQUALS(entry->rank, 10);
  ASSERT_NUM_EQUALS(entry->submitted, 1615638984);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->user_index, 2);

  rc_client_destroy_leaderboard_entry_list(g_leaderboard_entries);
  rc_client_destroy(g_client);
}

static void test_fetch_leaderboard_entries_no_user(void)
{
  rc_client_leaderboard_entry_t* entry;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  g_leaderboard_entries = NULL;

  mock_api_response("r=lbinfo&i=4401&c=10", lbinfo_4401_top_10_no_user);

  rc_client_begin_fetch_leaderboard_entries(g_client, 4401, 1, 10,
      rc_client_callback_expect_leaderboard_entry_list, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(g_leaderboard_entries);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->num_entries, 10);

  entry = g_leaderboard_entries->entries;
  ASSERT_STR_EQUALS(entry->user, "PlayerG");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerB");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerJ");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerC");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerF");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerA");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerI");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerE");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerD");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerH");

  ASSERT_NUM_EQUALS(g_leaderboard_entries->user_index, -1);

  rc_client_destroy_leaderboard_entry_list(g_leaderboard_entries);
  rc_client_destroy(g_client);
}

static void test_fetch_leaderboard_entries_around_user(void)
{
  rc_client_leaderboard_entry_t* entry;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  g_leaderboard_entries = NULL;

  mock_api_response("r=lbinfo&i=4401&u=Username&c=10", lbinfo_4401_near_user);

  rc_client_begin_fetch_leaderboard_entries_around_user(g_client, 4401, 10,
      rc_client_callback_expect_leaderboard_entry_list, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(g_leaderboard_entries);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->num_entries, 10);
  ASSERT_NUM_EQUALS(g_leaderboard_entries->total_entries, 78);

  entry = g_leaderboard_entries->entries;
  ASSERT_STR_EQUALS(entry->user, "PlayerG");
  ASSERT_STR_EQUALS(entry->display, "003524");
  ASSERT_NUM_EQUALS(entry->index, 17);
  ASSERT_NUM_EQUALS(entry->rank, 17);
  ASSERT_NUM_EQUALS(entry->submitted, 1615654895);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerB");
  ASSERT_STR_EQUALS(entry->display, "003645");
  ASSERT_NUM_EQUALS(entry->index, 18);
  ASSERT_NUM_EQUALS(entry->rank, 18);
  ASSERT_NUM_EQUALS(entry->submitted, 1615634566);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerC");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 19);
  ASSERT_NUM_EQUALS(entry->rank, 19);
  ASSERT_NUM_EQUALS(entry->submitted, 1615653844);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerF");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 20);
  ASSERT_NUM_EQUALS(entry->rank, 19);
  ASSERT_NUM_EQUALS(entry->submitted, 1615623878);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "DisplayName");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 21);
  ASSERT_NUM_EQUALS(entry->rank, 19);
  ASSERT_NUM_EQUALS(entry->submitted, 1615234553);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerA");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 22);
  ASSERT_NUM_EQUALS(entry->rank, 19);
  ASSERT_NUM_EQUALS(entry->submitted, 1615653284);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerI");
  ASSERT_STR_EQUALS(entry->display, "003902");
  ASSERT_NUM_EQUALS(entry->index, 23);
  ASSERT_NUM_EQUALS(entry->rank, 23);
  ASSERT_NUM_EQUALS(entry->submitted, 1615632174);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerE");
  ASSERT_STR_EQUALS(entry->display, "003956");
  ASSERT_NUM_EQUALS(entry->index, 24);
  ASSERT_NUM_EQUALS(entry->rank, 24);
  ASSERT_NUM_EQUALS(entry->submitted, 1616384834);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerD");
  ASSERT_STR_EQUALS(entry->display, "003985");
  ASSERT_NUM_EQUALS(entry->index, 25);
  ASSERT_NUM_EQUALS(entry->rank, 25);
  ASSERT_NUM_EQUALS(entry->submitted, 1615238383);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerH");
  ASSERT_STR_EQUALS(entry->display, "004012");
  ASSERT_NUM_EQUALS(entry->index, 26);
  ASSERT_NUM_EQUALS(entry->rank, 26);
  ASSERT_NUM_EQUALS(entry->submitted, 1615638984);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->user_index, 4);

  rc_client_destroy_leaderboard_entry_list(g_leaderboard_entries);
  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_leaderboard_entry_list_login_required(int result, const char* error_message,
    rc_client_leaderboard_entry_list_t* list, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_LOGIN_REQUIRED);
  ASSERT_STR_EQUALS(error_message, "Login required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
  ASSERT_PTR_NULL(list);
}

static void test_fetch_leaderboard_entries_around_user_not_logged_in(void)
{
  g_client = mock_client_not_logged_in();
  g_leaderboard_entries = NULL;

  mock_api_response("r=lbinfo&i=4401&u=Username&c=10", lbinfo_4401_near_user);

  rc_client_begin_fetch_leaderboard_entries_around_user(g_client, 4401, 10,
      rc_client_callback_expect_leaderboard_entry_list_login_required, g_callback_userdata);
  ASSERT_PTR_NULL(g_leaderboard_entries);

  assert_api_not_called("r=lbinfo&i=4401&u=Username&c=10");

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_leaderboard_uncalled(int result, const char* error_message,
  rc_client_leaderboard_entry_list_t* list, rc_client_t* client, void* callback_userdata)
{
  ASSERT_FAIL("Callback should not have been called.")
}

static void test_fetch_leaderboard_entries_async_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  g_client->callbacks.server_call = rc_client_server_call_async;

  g_leaderboard_entries = NULL;

  handle = rc_client_begin_fetch_leaderboard_entries(g_client, 4401, 1, 10,
    rc_client_callback_expect_leaderboard_uncalled, g_callback_userdata);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=lbinfo&i=4401&c=10", lbinfo_4401_top_10);
  ASSERT_PTR_NULL(g_leaderboard_entries);

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_leaderboard_internet_not_available(int result, const char* error_message,
    rc_client_leaderboard_entry_list_t* list, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_NO_RESPONSE);
  ASSERT_STR_EQUALS(error_message, "Internet not available.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
  ASSERT_PTR_NULL(list);
}

static void test_fetch_leaderboard_entries_client_error(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  g_leaderboard_entries = NULL;

  mock_api_error("r=lbinfo&i=4401&c=10", "Internet not available.", RC_API_SERVER_RESPONSE_CLIENT_ERROR);

  handle = rc_client_begin_fetch_leaderboard_entries(g_client, 4401, 1, 10,
      rc_client_callback_expect_leaderboard_internet_not_available, g_callback_userdata);

  ASSERT_PTR_NULL(handle);
  ASSERT_PTR_NULL(g_leaderboard_entries);

  rc_client_destroy(g_client);
}

static void test_map_leaderboard_format(void)
{
  int i;

  for (i = 0; i < 30; ++i) {
    switch (i) {
      case RC_FORMAT_SECONDS:
      case RC_FORMAT_CENTISECS:
      case RC_FORMAT_MINUTES:
      case RC_FORMAT_SECONDS_AS_MINUTES:
      case RC_FORMAT_FRAMES:
        ASSERT_NUM_EQUALS(rc_client_map_leaderboard_format(i), RC_CLIENT_LEADERBOARD_FORMAT_TIME);
        break;

      case RC_FORMAT_SCORE:
        ASSERT_NUM_EQUALS(rc_client_map_leaderboard_format(i), RC_CLIENT_LEADERBOARD_FORMAT_SCORE);
        break;

      default:
        ASSERT_NUM_EQUALS(rc_client_map_leaderboard_format(i), RC_CLIENT_LEADERBOARD_FORMAT_VALUE);
        break;
    }
  }
}

/* ----- do frame ----- */

static void test_do_frame_bounds_check_system(void)
{
  const uint32_t memory_size = 0x10010; /* provide more memory than system expects */
  uint8_t* memory = (uint8_t*)calloc(1, memory_size);
  ASSERT_PTR_NOT_NULL(memory);

  g_client = mock_client_game_loaded(patchdata_bounds_check_system, no_unlocks);

  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=7&h=1&m=0123456789ABCDEF&v=c39308ba325ba4a72919b081fb18fdd4",
    "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":7,\"AchievementsRemaining\":4}");

  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->max_valid_address, 0xFFFF);

  assert_achievement_state(g_client, 1, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  assert_achievement_state(g_client, 2, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  assert_achievement_state(g_client, 3, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* 0x10000 out of range for system */
  assert_achievement_state(g_client, 4, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  assert_achievement_state(g_client, 5, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE); /* cannot read two bytes from 0xFFFF, but size isn't enforced until do_frame */
  assert_achievement_state(g_client, 6, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* 0x10000 out of range for system */
  assert_achievement_state(g_client, 7, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);

  /* verify that reading at the edge of the memory bounds fails */
  mock_memory(memory, 0x10000);
  rc_client_do_frame(g_client);
  assert_achievement_state(g_client, 5, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* cannot read two bytes from 0xFFFF */

  /* set up memory so achievement 7 would trigger if the pointed at address were valid */
  /* achievement should not trigger - invalid address should be ignored */
  memory[0x10000] = 5;
  memory[0x00000] = 1; /* byte(0xFFFF + byte(0x0000)) == 5 */
  rc_client_do_frame(g_client);
  assert_achievement_state(g_client, 7, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);

  /* even if the extra memory is available, it shouldn't try to read beyond the system defined max address */
  mock_memory(memory, memory_size);
  rc_client_do_frame(g_client);
  assert_achievement_state(g_client, 7, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);

  /* change max valid address so memory will be evaluated. achievement should trigger */
  g_client->game->max_valid_address = memory_size - 1;
  rc_client_do_frame(g_client);
  assert_achievement_state(g_client, 7, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);

  rc_client_destroy(g_client);
  free(memory);
}

static void test_do_frame_bounds_check_available(void)
{
  const rc_trigger_t* trigger;
  uint8_t memory[8] = { 0,0,0,0,0,0,0,0 };
  g_client = mock_client_game_loaded(patchdata_bounds_check_8, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    /* all addresses are valid according to the system, so no achievements should be disabled yet. */
    assert_achievement_state(g_client, 808, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    trigger = ((rc_client_achievement_info_t*)rc_client_get_achievement_info(g_client, 808))->trigger;
    ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_WAITING);

    /* limit the memory that's actually exposed and try to process a frame */
    mock_memory(memory, sizeof(memory));
    rc_client_do_frame(g_client);

    assert_achievement_state(g_client, 408, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 508, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 608, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 708, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 808, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* out of bounds*/
    ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_DISABLED);

    assert_achievement_state(g_client, 416, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 516, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 616, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 716, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* only one byte available */
    assert_achievement_state(g_client, 816, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* out of bounds*/

    assert_achievement_state(g_client, 424, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 524, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* 24-bit read actually fetches 32-bits */
    assert_achievement_state(g_client, 624, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* only two bytes available */
    assert_achievement_state(g_client, 724, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* only one byte available */
    assert_achievement_state(g_client, 824, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* out of bounds*/

    assert_achievement_state(g_client, 432, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 532, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* only three bytes available */
    assert_achievement_state(g_client, 632, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* only two bytes available */
    assert_achievement_state(g_client, 732, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* only one byte available */
    assert_achievement_state(g_client, 832, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* out of bounds*/
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_already_awarded(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_server_error(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":false,\"Error\":\"Achievement not found\"}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    /* achievement still counts as triggered */
    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 12345 + 5); /* score will have been adjusted locally, but not from server */

    /* but an error should have been reported */
    event = find_event(RC_CLIENT_EVENT_SERVER_ERROR, 0);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_STR_EQUALS(event->server_error->api, "award_achievement");
    ASSERT_STR_EQUALS(event->server_error->error_message, "Achievement not found");
    ASSERT_NUM_EQUALS(event->server_error->result, RC_API_FAILURE);
    ASSERT_NUM_EQUALS(event->server_error->related_id, 8);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_while_spectating(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    ASSERT_FALSE(rc_client_get_spectator_mode_enabled(g_client));
    rc_client_set_spectator_mode_enabled(g_client, 1);
    ASSERT_TRUE(rc_client_get_spectator_mode_enabled(g_client));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":false,\"Error\":\"Achievement should not have been unlocked in spectating mode\"}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    /* achievement still counts as triggered */
    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 12345 + 5); /* score will have been adjusted locally, but not from server */

    /* expect API not called */
    assert_api_not_called("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    rc_client_set_spectator_mode_enabled(g_client, 0);
    ASSERT_FALSE(rc_client_get_spectator_mode_enabled(g_client));
  }

  rc_client_destroy(g_client);
}

static int rc_client_callback_deny_unlock(uint32_t achievement_id, rc_client_t* client)
{
  return 0;
}

static int rc_client_callback_allow_unlock(uint32_t achievement_id, rc_client_t* client)
{
  return 1;
}

static void test_do_frame_achievement_trigger_blocked(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  uint32_t num_active;
  const char* api_call8 = "r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217";
  const char* api_call9 = "r=awardachievement&u=Username&t=ApiToken&a=9&h=1&m=0123456789ABCDEF&v=6d989ee0f408660a87d6440a13563bf6";
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  g_client->callbacks.can_submit_achievement_unlock = rc_client_callback_deny_unlock;

  ASSERT_PTR_NOT_NULL(g_client->game);
  mock_memory(memory, sizeof(memory));
  num_active = g_client->game->runtime.trigger_count;

  mock_api_response(api_call8,
    "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");
  mock_api_response(api_call9,
    "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  memory[8] = 8;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 1);

  event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
  ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
  ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
  ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
  ASSERT_NUM_EQUALS(g_client->user.score, 12350); /* 12345+5 - not updated by server response that didn't happen */
  ASSERT_NUM_EQUALS(g_client->user.score_softcore, 0);

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  assert_api_not_called(api_call8);

  g_client->callbacks.can_submit_achievement_unlock = rc_client_callback_allow_unlock;

  memory[9] = 9;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 1);

  event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 9);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
  ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
  ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
  ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 9));

  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 2);
  ASSERT_NUM_EQUALS(g_client->user.score, 5432);
  ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

  assert_api_called(api_call9);

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_automatic_retry(const char* response, int status_code)
{
  const char* unlock_request_params = "r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&v=9b9bdf5501eb6289a6655affbcc695e6";
  const char* unlock_request_params1 = "r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&o=1&v=4b4af09722aeb0e8a16c152f9a646281";
  const char* unlock_request_params3 = "r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&o=3&v=d5bc287a030084aa77a911b6c2c5fc96";
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  g_client->callbacks.server_call = rc_client_server_call_async;

  /* discard the queued ping to make finding the retry easier */
  g_client->state.scheduled_callbacks = NULL;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[1] = 3;
    memory[2] = 7;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5501);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 5501));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* first failure will immediately requeue the request */
    async_api_error(unlock_request_params, response, status_code);
    assert_api_pending(unlock_request_params);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);
    rc_client_idle(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* second failure will queue it */
    async_api_error(unlock_request_params, response, status_code);
    assert_api_call_count(unlock_request_params, 0);
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);

    rc_client_idle(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_DISCONNECTED, 0));
    event_count = 0;

    /* advance time so queued request gets processed */
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 1 * 1000);
    g_now += 1 * 1000;
    rc_client_idle(g_client);
    assert_api_pending(unlock_request_params1);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* third failure will requeue it */
    async_api_error(unlock_request_params1, response, status_code);
    assert_api_call_count(unlock_request_params, 0);
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 2 * 1000);
    g_now += 2 * 1000;

    rc_client_idle(g_client);
    assert_api_pending(unlock_request_params3);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* success should not requeue it and update player score */
    async_api_response(unlock_request_params3, "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    /* reconnected event should be pending, watch for it */
    ASSERT_NUM_EQUALS(event_count, 0);
    rc_client_idle(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_RECONNECTED, 0));
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_automatic_retry_empty(void)
{
  test_do_frame_achievement_trigger_automatic_retry("", 200);
}

static void test_do_frame_achievement_trigger_automatic_retry_429(void)
{
  test_do_frame_achievement_trigger_automatic_retry(response_429, 429);
}

static void test_do_frame_achievement_trigger_automatic_retry_502(void)
{
  test_do_frame_achievement_trigger_automatic_retry(response_502, 502);
}

static void test_do_frame_achievement_trigger_automatic_retry_503(void)
{
  test_do_frame_achievement_trigger_automatic_retry(response_503, 503);
}

static void test_do_frame_achievement_trigger_automatic_retry_custom_timeout(void)
{
  test_do_frame_achievement_trigger_automatic_retry("Request has timed out.", RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR);
}

static void test_do_frame_achievement_trigger_automatic_retry_generic_empty_response(void)
{
  test_do_frame_achievement_trigger_automatic_retry("", 0);
}

static void test_do_frame_achievement_trigger_subset(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_subset, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&v=9b9bdf5501eb6289a6655affbcc695e6",
        "{\"Success\":true,\"Score\":5437,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    memory[0x17] = 7;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5501);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 5501));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 2);
    ASSERT_NUM_EQUALS(g_client->user.score, 5437);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_rarity(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive_typed, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
      "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->type, RC_CLIENT_ACHIEVEMENT_TYPE_PROGRESSION);
    ASSERT_FLOAT_EQUALS(event->achievement->rarity, 86.0f);
    ASSERT_FLOAT_EQUALS(event->achievement->rarity_hardcore, 73.1f);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_measured(void)
{
  const rc_client_achievement_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=70&h=1&m=0123456789ABCDEF&v=61e40027573e2cde88b49d27f6804879",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":70,\"AchievementsRemaining\":11}");
    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=71&h=1&m=0123456789ABCDEF&v=3a8d55b81d391557d5111306599a2b0d",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":71,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[0x10] = 0x39; memory[0x11] = 0x30; /* 12345 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1); /* one PROGRESS_INDICATOR_SHOW event */

    achievement = rc_client_get_achievement_info(g_client, 70);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "12,345/100,000");

    achievement = rc_client_get_achievement_info(g_client, 71);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "12%");

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* increment measured value - raw counter will report progress change, percentage will not */
    memory[0x10] = 0x3A; /* 12346 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1); /* one PROGRESS_INDICATOR_SHOW event */

    achievement = rc_client_get_achievement_info(g_client, 70);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "12,346/100,000");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* increment measured value - raw counter will report progress change, percentage will not */
    memory[0x11] = 0x33; /* 13114 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1); /* one PROGRESS_INDICATOR_SHOW event */

    achievement = rc_client_get_achievement_info(g_client, 70);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "13,114/100,000");

    achievement = rc_client_get_achievement_info(g_client, 71);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "13%");

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* trigger measured achievements - progress becomes blank */
    memory[0x10] = 0xA0; memory[0x11] = 0x86; memory[0x12] = 0x01; /* 100000 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2); /* two TRIGGERED events, and no PROGRESS_INDICATOR_SHOW events */

    achievement = rc_client_get_achievement_info(g_client, 70);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");

    achievement = rc_client_get_achievement_info(g_client, 71);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 2);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_measured_progress_event(void)
{
  rc_client_event_t* event;
  const rc_client_achievement_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=6&h=1&m=0123456789ABCDEF&v=65206f4290098ecd30c7845e895057d0",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":6,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[0x06] = 3;                         /* 3/6 */
    memory[0x11] = 0xC3; memory[0x10] = 0x4F; /* 49999/100000 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    /* 3/6 = 50%, 49999/100000 = 49.999% */
    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 6));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "3/6");

    /* both achievements should have been updated, */
    achievement = rc_client_get_achievement_info(g_client, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "3/6");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 50.0);

    achievement = rc_client_get_achievement_info(g_client, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "49,999/100,000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 49.999);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* any change will trigger the popup - even dropping */
    memory[0x10] = 0x4E; /* 49998 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE, 70);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 70));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "49,998/100,000");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* don't trigger popup when value changes to 0 as the measured_progress string will be blank */
    memory[0x06] = 0; /* 0 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    achievement = rc_client_get_achievement_info(g_client, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);

    /* both at 50%, only report first */
    memory[0x06] = 3;                         /* 3/6 */
    memory[0x11] = 0xC3; memory[0x10] = 0x50; /* 50000/100000 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE, 6);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 6));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "3/6");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* second slightly ahead */
    memory[0x6] = 4;                                             /* 4/6 */
    memory[0x12] = 1;  memory[0x11] = 0x04; memory[0x10] = 0x6B; /* 66667/100000 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE, 70);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 70));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "66,667/100,000");
    
    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* don't show popup on trigger */
    memory[0x06] = 6;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 6);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 6));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    ASSERT_PTR_NOT_NULL(g_client->game->progress_tracker.hide_callback);
    g_now += 2 * 1000;

    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_measured_progress_reshown(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_PTR_NOT_NULL(g_client->game);
  mock_memory(memory, sizeof(memory));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  memory[0x06] = 3;                         /* 3/6 */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));
  event_count = 0;

  /* should be two callbacks queued - hiding the progress indicator, and the rich presence update */
  ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks, g_client->game->progress_tracker.hide_callback);
  ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks->next);
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks->next->next);

  /* advance time to hide the progress indicator */
  g_now = g_client->game->progress_tracker.hide_callback->when;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));
  event_count = 0;

  /* only the rich presence update should be scheduled */
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks->next);

  /* advance time to just before the rich presence update */
  g_now = g_client->state.scheduled_callbacks->when - 10;

  /* reschedule the progress indicator */
  memory[0x06] = 4;                         /* 4/6 */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));
  event_count = 0;

  /* should be two callbacks queued - rich presence update, then hiding the progress indicator */
  ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
  ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->next, g_client->game->progress_tracker.hide_callback);
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks->next->next);

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_challenge_indicator(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=7&h=1&m=0123456789ABCDEF&v=c39308ba325ba4a72919b081fb18fdd4",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":7,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[1] = 1; /* show indicator */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[1] = 0; /* hide indicator */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[1] = 1; /* show indicator */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* trigger achievement - expect both hide and trigger events. both should have triggered achievement data */
    memory[7] = 7;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_challenge_indicator_primed_while_reset(void)
{
  static const char* patchdata = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
       "{\"ID\":7,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
        "\"MemAddr\":\"0xH0001=3_T:0xH0002=4_R:0xH0003=3\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
        "\"Created\":1367266583,\"Modified\":1376929305}"
      "],"
      "\"Leaderboards\":[]"
    "}]}";

  rc_client_event_t* event;
  rc_client_achievement_info_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata, no_unlocks);
  mock_memory(memory, sizeof(memory));

  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=7&h=1&m=0123456789ABCDEF&v=c39308ba325ba4a72919b081fb18fdd4",
    "{\"Success\":true,\"Score\":5432,\"AchievementID\":7,\"AchievementsRemaining\":11}");

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  achievement = (rc_client_achievement_info_t*)rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->trigger->state, RC_TRIGGER_STATE_ACTIVE);

  memory[1] = 3; /* primed */
  memory[3] = 3; /* but reset */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);
  ASSERT_NUM_EQUALS(achievement->trigger->state, RC_TRIGGER_STATE_ACTIVE);

  memory[3] = 0; /* disable reset */

  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 1); /* show indicator event */
  event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
  ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
  ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
  ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

  ASSERT_NUM_EQUALS(achievement->trigger->state, RC_TRIGGER_STATE_PRIMED);

  memory[3] = 3; /* reset active */

  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 1); /* hide indicator event */
  event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
  ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
  ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_challenge_indicator_primed_while_reset_next(void)
{
  static const char* patchdata = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
       "{\"ID\":7,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
        "\"MemAddr\":\"0xH0001=3_T:0xH0002=4_Z:0xH0003=3_P:0xH0006=1.10.\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
        "\"Created\":1367266583,\"Modified\":1376929305}"
      "],"
      "\"Leaderboards\":[]"
    "}]}";

  rc_client_event_t* event;
  rc_client_achievement_info_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata, no_unlocks);
  mock_memory(memory, sizeof(memory));

  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=7&h=1&m=0123456789ABCDEF&v=c39308ba325ba4a72919b081fb18fdd4",
    "{\"Success\":true,\"Score\":5432,\"AchievementID\":7,\"AchievementsRemaining\":11}");

  memory[6] = 1; /* pause condition will gain a hit, but not enough to pause it */
  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  achievement = (rc_client_achievement_info_t*)rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->trigger->state, RC_TRIGGER_STATE_ACTIVE);

  /* ResetNextIf will clear hits on condition 4, _and_ the trigger will be primed at the same time.
   * The ResetNextIf will cause rc_evaluate_trigger to return RESET, but we still want to raise a
   * PRIMED event out of rc_client. */
  memory[1] = 3; /* primed */
  memory[3] = 3; /* reset pause condition */
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 1); /* show indicator event */
  event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
  ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
  ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
  ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

  ASSERT_NUM_EQUALS(achievement->trigger->state, RC_TRIGGER_STATE_PRIMED);

  memory[3] = 0; /* disable reset, pause counter increases */
  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  memory[3] = 0; /* re-enable reset. don't expect any events */

  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 0);

  rc_client_destroy(g_client);
}

static void test_do_frame_mastery(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  g_client->callbacks.server_call = rc_client_server_call_async;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 12345+5);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 0);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    async_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":0}");

    ASSERT_NUM_EQUALS(event_count, 0);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_GAME_COMPLETED, 1234);
    ASSERT_PTR_NOT_NULL(event);

    memory[9] = 9;
    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 9);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 9));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 2);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432+5);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    async_api_response("r=awardachievement&u=Username&t=ApiToken&a=9&h=1&m=0123456789ABCDEF&v=6d989ee0f408660a87d6440a13563bf6",
        "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":9,\"AchievementsRemaining\":0}");

    ASSERT_NUM_EQUALS(event_count, 0);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_mastery_encore(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  g_client->callbacks.server_call = rc_client_server_call_async;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 12345+5);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 0);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    async_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":0}");

    ASSERT_NUM_EQUALS(event_count, 0);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_GAME_COMPLETED, 1234);
    ASSERT_PTR_NOT_NULL(event);

    memory[9] = 9;
    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 9);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 9));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 2);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432+5);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    async_api_response("r=awardachievement&u=Username&t=ApiToken&a=9&h=1&m=0123456789ABCDEF&v=6d989ee0f408660a87d6440a13563bf6",
        "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":9,\"AchievementsRemaining\":0}");

    ASSERT_NUM_EQUALS(event_count, 0);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_mastery_subset(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_subset, unlock_5501_and_5502);
  g_client->callbacks.server_call = rc_client_server_call_async;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[0x19] = 9;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5503);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 5503));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 12345 + 5);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 0);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    async_api_response("r=awardachievement&u=Username&t=ApiToken&a=5503&h=1&m=0123456789ABCDEF&v=50486c3ea9e2e32bb3683017b1488f88",
      "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":5503,\"AchievementsRemaining\":0}");

    ASSERT_NUM_EQUALS(event_count, 0);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_SUBSET_COMPLETED, 2345);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_PTR_EQUALS(event->subset, rc_client_get_subset_info(g_client, 2345));
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_started(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_update(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* update the leaderboard */
    memory[0x0E] = 18;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000018");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_failed(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* cancel the leaderboard */
    memory[0x0C] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6",
        "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
        "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
        "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* submit the leaderboard */
    memory[0x0D] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 3);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->leaderboard_id, 44);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->submitted_score, "000017");
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->best_score, "000023");
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->new_rank, 2);
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->num_entries, 2);
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->num_top_entries, 2);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[0].username, "Player1");
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->top_entries[0].rank, 1);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[0].score, "000044");
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[1].username, "Username");
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->top_entries[1].rank, 2);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[1].score, "000023");
    ASSERT_PTR_NOT_NULL(event->leaderboard_scoreboard->top_entries);
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit_server_error(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6",
        "{\"Success\":false,\"Error\":\"Leaderboard not found\"}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* submit the leaderboard */
    memory[0x0D] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 3);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    /* an error should have also been reported */
    event = find_event(RC_CLIENT_EVENT_SERVER_ERROR, 0);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_STR_EQUALS(event->server_error->api, "submit_lboard_entry");
    ASSERT_STR_EQUALS(event->server_error->error_message, "Leaderboard not found");
    ASSERT_NUM_EQUALS(event->server_error->result, RC_API_FAILURE);
    ASSERT_NUM_EQUALS(event->server_error->related_id, 44);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit_while_spectating(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    ASSERT_FALSE(rc_client_get_spectator_mode_enabled(g_client));
    rc_client_set_spectator_mode_enabled(g_client, 1);
    ASSERT_TRUE(rc_client_get_spectator_mode_enabled(g_client));

    mock_api_response("r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6",
        "{\"Success\":false,\"Error\":\"Leaderboard entry should not have been submitted in spectating mode\"}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* submit the leaderboard */
    memory[0x0D] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* expect API not called */
    assert_api_not_called("r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6");
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit_immediate(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_leaderboard_immediate_submit, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6",
      "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
      "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
      "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start the leaderboard (it will immediately submit) */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2); /* don't expect start or tracker events - only submit and scoreboard */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->leaderboard_id, 44);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->submitted_score, "000017");
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->best_score, "000023");
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->new_rank, 2);
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->num_entries, 2);
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->num_top_entries, 2);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[0].username, "Player1");
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->top_entries[0].rank, 1);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[0].score, "000044");
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[1].username, "Username");
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->top_entries[1].rank, 2);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[1].score, "000023");
    ASSERT_PTR_NOT_NULL(event->leaderboard_scoreboard->top_entries);
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit_hidden(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_leaderboards_hidden, no_unlocks);

  /* hidden leaderboards should still start/track/submit normally. they just don't appear in list */

  ASSERT_PTR_NOT_NULL(g_client->game);
  mock_memory(memory, sizeof(memory));

  mock_api_response("r=submitlbentry&u=Username&t=ApiToken&i=48&s=17&m=0123456789ABCDEF&v=468a1f9e9475d8c4d862f48cc8806018",
    "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
    "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
    "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* start the leaderboard */
  memory[0x0A] = 2;
  memory[0x0E] = 17;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 48));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* submit the leaderboard */
  memory[0x0D] = 1;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 3);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 48);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
  ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 48));

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD, 48);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->leaderboard_id, 48);
  ASSERT_STR_EQUALS(event->leaderboard_scoreboard->submitted_score, "000017");
  ASSERT_STR_EQUALS(event->leaderboard_scoreboard->best_score, "000023");
  ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->new_rank, 2);
  ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->num_entries, 2);
  ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->num_top_entries, 2);
  ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[0].username, "Player1");
  ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->top_entries[0].rank, 1);
  ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[0].score, "000044");
  ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[1].username, "Username");
  ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->top_entries[1].rank, 2);
  ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[1].score, "000023");
  ASSERT_PTR_NOT_NULL(event->leaderboard_scoreboard->top_entries);
  ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 48));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  rc_client_destroy(g_client);
}

static int rc_client_callback_deny_leaderboard(uint32_t leaderboard_id, rc_client_t* client)
{
  return 0;
}

static int rc_client_callback_allow_leaderboard(uint32_t leaderboard_id, rc_client_t* client)
{
  return 1;
}

static void test_do_frame_leaderboard_submit_blocked(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  const char* api_call = "r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6";
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  g_client->callbacks.can_submit_leaderboard_entry = rc_client_callback_deny_leaderboard;

  ASSERT_PTR_NOT_NULL(g_client->game);
  mock_memory(memory, sizeof(memory));

  mock_api_response(api_call,
    "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
    "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
    "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* start the leaderboard */
  memory[0x0B] = 1;
  memory[0x0E] = 17;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* submit the leaderboard */
  memory[0x0B] = 0;
  memory[0x0D] = 1;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 2);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
  ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

  /* but make sure the server wasn't called */
  assert_api_not_called(api_call);

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  g_client->callbacks.can_submit_leaderboard_entry = rc_client_callback_allow_leaderboard;

  /* restart the leaderboard - will immediately submit */
  memory[0x0B] = 1;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD, 44));

  /* make sure the server was called */
  assert_api_called(api_call);

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit_softcore(void)
{
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_event_t* event;
  uint8_t memory[64];
  const char* api_call = "r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6";
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  rc_client_set_hardcore_enabled(g_client, 0);

  ASSERT_PTR_NOT_NULL(g_client->game);
  mock_memory(memory, sizeof(memory));

  mock_api_response(api_call,
    "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
    "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
    "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* start the leaderboard - will be ignored in softcore */
  memory[0x0B] = 1;
  memory[0x0E] = 17;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* allow leaderboards to be processed in softcore. have to manually activate as it wasn't activated on game load */
  g_client->state.allow_leaderboards_in_softcore = 1;
  leaderboard = (rc_client_leaderboard_info_t*)rc_client_get_leaderboard_info(g_client, 44);
  leaderboard->public_.state = RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE;
  leaderboard->lboard->state = RC_LBOARD_STATE_ACTIVE;

  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* submit the leaderboard */
  memory[0x0B] = 0;
  memory[0x0D] = 1;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 2);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
  ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

  /* but make sure the server wasn't called */
  assert_api_not_called(api_call);

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  g_client->state.hardcore = 1;

  /* restart the leaderboard - will immediately submit */
  memory[0x0B] = 1;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD, 44));

  /* make sure the server was called */
  assert_api_called(api_call);

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_tracker_sharing(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start one leaderboard (one tracker) */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    memory[0x0F] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000273");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start additional leaderboards (45,46,47) - 45 and 46 should generate new trackers */
    memory[0x0A] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 5);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 45);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 45));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->next->reference_count, 1); /* 45 */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 46);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 46));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->next->reference_count, 1); /* 46 */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 47);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 47));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->reference_count, 2); /* 44,47 */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 2);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 2);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017"); /* 45 has different size */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 3);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 3);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "273"); /* 46 has different format */

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start additional leaderboard (48) - should share tracker with 44 */
    memory[0x0A] = 2;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 48);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 48));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->reference_count, 3); /* 44,47,48 */

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* cancel leaderboard 44 */
    memory[0x0C] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->reference_count, 2); /* 47,48 */

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* cancel leaderboard 45 */
    memory[0x0C] = 2;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 45);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 45));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->next->reference_count, 0); /* */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 2);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 2);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* cancel leaderboard 46 */
    memory[0x0C] = 3;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 46);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 46));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->next->reference_count, 0); /* */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 3);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 3);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "273");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
    
    /* cancel 47, start 51 */
    memory[0x0A] = 3;
    memory[0x0B] = 0;
    memory[0x0C] = 4;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 3);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 47);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 47));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->reference_count, 1); /* 48 */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 51);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "0");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 51));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->next->reference_count, 1); /* 51 */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 2);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 2);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "0");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* cancel 48 */
    memory[0x0C] = 5;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 48);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 48));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->reference_count, 0); /*  */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000273");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_tracker_sharing_hits(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start leaderboards 51,52 (share tracker) */
    memory[0x0A] = 3;
    memory[0x0B] = 3;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 3);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 51);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "0");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 51));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 52);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "0");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 52));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "0");

    /* hit count ticks */
    memory[0x09] = 1;
    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "1");

    /* cancel leaderboard 51 */
    memory[0x0C] = 6;
    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 51);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "2");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 51));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "2");

    /* hit count ticks */
    memory[0x0A] = 0;
    memory[0x0C] = 0;
    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "3");

    /* restart leaderboard 51 - hit count differs, can't share */
    memory[0x0A] = 3;
    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 3);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 51);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "1");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 51));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "4"); /* 52 */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 2);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 2);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "1"); /* 51 */
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit_automatic_retry(void)
{
  const char* submit_entry_params = "r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6";
  const char* submit_entry_params1 = "r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&o=1&v=659685352e6d8e14923ea32da6f8c3e4";
  const char* submit_entry_params3 = "r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&o=3&v=91debb749e90c2beff4f21430716dac9";
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  g_client->callbacks.server_call = rc_client_server_call_async;

  /* discard the queued ping to make finding the retry easier */
  g_client->state.scheduled_callbacks = NULL;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* submit the leaderboard */
    memory[0x0D] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* first failure will immediately requeue the request */
    async_api_response(submit_entry_params, "");
    assert_api_pending(submit_entry_params);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    rc_client_idle(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* second failure will queue it for one second later */
    async_api_response(submit_entry_params, "");
    assert_api_not_pending(submit_entry_params);
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);

    /* disconnected event should be raised after retry is queued */
    rc_client_idle(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_DISCONNECTED, 0));
    event_count = 0;

    /* advance time and process scheduled callbacks */
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 1 * 1000);
    g_now += 1 * 1000;

    rc_client_idle(g_client);
    assert_api_pending(submit_entry_params1);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* third failure will requeue it for two seconds later */
    async_api_response(submit_entry_params1, "");
    assert_api_not_pending(submit_entry_params1);
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 2 * 1000);
    g_now += 2 * 1000;

    rc_client_idle(g_client);
    assert_api_pending(submit_entry_params3);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* success should not requeue it and update player score */
    async_api_response(submit_entry_params3,
        "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
        "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
        "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* reconnected event should be pending, watch for it */
    ASSERT_NUM_EQUALS(event_count, 0);
    rc_client_idle(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_RECONNECTED, 0));
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_multiple_automatic_retry(void)
{
  const char* unlock_5501_request_params = "r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&v=9b9bdf5501eb6289a6655affbcc695e6";
  const char* unlock_5501_request_params1 = "r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&o=1&v=4b4af09722aeb0e8a16c152f9a646281";
  const char* unlock_5501_request_params4 = "r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&o=4&v=63646fa1b30797144cc87881f7d95932";
  const char* unlock_5502_request_params = "r=awardachievement&u=Username&t=ApiToken&a=5502&h=1&m=0123456789ABCDEF&v=8d7405f5aacc9b4b334619a0dae62a56";
  const char* unlock_5502_request_params1 = "r=awardachievement&u=Username&t=ApiToken&a=5502&h=1&m=0123456789ABCDEF&o=1&v=46d77e89548126a88c0cdda1980c4dd4";
  const char* submit_entry_params = "r=submitlbentry&u=Username&t=ApiToken&i=4401&s=17&m=0123456789ABCDEF&v=13b4cdfe295a97783e78bb48c8910867";
  const char* submit_entry_params1 = "r=submitlbentry&u=Username&t=ApiToken&i=4401&s=17&m=0123456789ABCDEF&o=1&v=16fe62616f0e39d0a2620b50cc004928";
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  g_client->callbacks.server_call = rc_client_server_call_async;

  /* discard the queued ping to make finding the retry easier */
  g_client->state.scheduled_callbacks = NULL;

  ASSERT_PTR_NOT_NULL(g_client->game);

  mock_memory(memory, sizeof(memory));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  g_now = 100000; /* time 0 */
  memory[1] = 3; /* trigger 5501 */
  memory[2] = 7; /* trigger 5501 */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5501));
  event_count = 0;

  /* first failure will immediately requeue the request */
  async_api_response(unlock_5501_request_params, "");
  assert_api_pending(unlock_5501_request_params);

  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* second failure will queue it for one second later (101000) */
  async_api_response(unlock_5501_request_params, "");
  assert_api_not_pending(unlock_5501_request_params);

  /* disconnected event should be raised after retry is queued */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_DISCONNECTED, 0));
  event_count = 0;

  /* advance time 500ms. trigger second achievement and start leaderboard */
  g_now += 500; /* 100500 */
  memory[1] = 2; /* trigger 5502 */
  memory[2] = 9; /* trigger 5502 */
  memory[0x0C] = 1; /* start leaderboard 1 */
  memory[0x0E] = 17; /* leaderboard 1 value */

  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 3);
  /* disconnected event should not be raised again */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5502));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 4401));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));
  event_count = 0;

  /* first failure will immediately requeue the request */
  async_api_response(unlock_5502_request_params, "");
  assert_api_pending(unlock_5502_request_params);

  /* second failure will queue it for one second later (101500) */
  async_api_response(unlock_5502_request_params, "");
  assert_api_not_pending(unlock_5502_request_params);

  /* advance time 250ms. submit leaderboard */
  g_now += 250; /* 100750 */
  memory[0x0D] = 2; /* submit leaderboard 1 */

  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 2);
  /* disconnected event should not be raised again */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 4401));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));
  event_count = 0;

  /* first failure will immediately requeue the request */
  async_api_response(submit_entry_params, "");
  assert_api_pending(submit_entry_params);

  /* second failure will queue it for one second later (101750) */
  async_api_response(submit_entry_params, "");
  assert_api_not_pending(submit_entry_params);

  /* advance time. first callback will fail again and be queued for two seconds later (103000) */
  g_now += 350; /* 101100 */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  assert_api_pending(unlock_5501_request_params1);
  async_api_response(unlock_5501_request_params1, "");
  assert_api_not_pending(unlock_5501_request_params1);

  /* advance time. second callback will succeed */
  g_now += 500; /* 101600 */
  rc_client_do_frame(g_client);
  assert_api_pending(unlock_5502_request_params1);
  async_api_response(unlock_5502_request_params1, "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");
  assert_api_not_pending(unlock_5502_request_params1);

  /* reconnected event should not be raised until all pending requests are handled */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* advance time. third callback will succeed */
  g_now += 500; /* 102100 */
  rc_client_do_frame(g_client);
  assert_api_pending(submit_entry_params1);
  async_api_response(submit_entry_params1,
    "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
    "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
    "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");
  assert_api_not_pending(submit_entry_params);

  /* reconnected event should not be raised until all pending requests are handled */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* advance time. first callback will finally succeed */
  g_now += 2000; /* 104100 */
  rc_client_do_frame(g_client);
  assert_api_pending(unlock_5501_request_params4);
  async_api_response(unlock_5501_request_params4, "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");
  assert_api_not_pending(unlock_5501_request_params4);

  /* reconnected event should be pending, watch for it */
  ASSERT_NUM_EQUALS(event_count, 0);
  rc_client_idle(g_client);
  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_RECONNECTED, 0));

  rc_client_destroy(g_client);
}

static void test_do_frame_rich_presence_hitcount(void)
{
  char buffer[8];
  const char* patchdata_rich_presence_hit_count = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\n@Number(M:1=1)\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":[],"
      "\"Leaderboards\":[]"
    "}]}";
  g_client = mock_client_game_loaded(patchdata_rich_presence_hit_count, no_unlocks);
  ASSERT_PTR_NOT_NULL(g_client->game);

  ASSERT_NUM_EQUALS(rc_client_get_rich_presence_message(g_client, buffer, sizeof(buffer)), 1);
  ASSERT_STR_EQUALS(buffer, "0");

  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(rc_client_get_rich_presence_message(g_client, buffer, sizeof(buffer)), 1);
  ASSERT_STR_EQUALS(buffer, "1");

  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(rc_client_get_rich_presence_message(g_client, buffer, sizeof(buffer)), 1);
  ASSERT_STR_EQUALS(buffer, "2");

  rc_client_destroy(g_client);
}

static void test_clock_get_now_millisecs(void)
{
  rc_client_t* client = rc_client_create(rc_client_read_memory, rc_client_server_call);
  rc_get_time_millisecs_func_t get_millisecs = client->callbacks.get_time_millisecs;

#ifdef RC_NO_SLEEP
  rc_clock_t time1;
  ASSERT_PTR_NOT_NULL(get_millisecs);
  time1 = get_millisecs(client);
  ASSERT_NUM_NOT_EQUALS(time1, 0);
#else
  rc_clock_t time1, time2, diff;
  time_t first = time(NULL), now;

  do {
    ASSERT_PTR_NOT_NULL(get_millisecs);
    time1 = get_millisecs(client);
    ASSERT_NUM_NOT_EQUALS(time1, 0);

#if defined(_WIN32)
    Sleep(50);
#else
    usleep(50000);
#endif

    time2 = get_millisecs(client);
    ASSERT_NUM_NOT_EQUALS(time2, 0);
    diff = time2 - time1;

    ASSERT_NUM_GREATER(diff, 49);
    if (diff < 100)
      break;

    now = time(NULL);
    if (now - first >= 3) {
      ASSERT_FAIL("could not get a 50ms sleep interval within 3 seconds");
      break;
    }

  } while (1);
#endif

  rc_client_destroy(client);
}

/* ----- ping ----- */

static void test_idle_ping(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    rc_client_scheduled_callback_t ping_callback;

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ping_callback = g_client->state.scheduled_callbacks->callback;

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 30 * 1000);
    g_now += 30 * 1000;

    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&h=1&x=0123456789ABCDEF", "{\"Success\":true}");

    g_client->state.frames_processed++; /* ping won't get called if no frames have been processed */
    rc_client_idle(g_client);

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&h=1&x=0123456789ABCDEF");

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
  }

  /* unloading game should unschedule ping */
  rc_client_unload_game(g_client);
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

  rc_client_destroy(g_client);
}

static void test_do_frame_ping_rich_presence(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    rc_client_scheduled_callback_t ping_callback;

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ping_callback = g_client->state.scheduled_callbacks->callback;

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 30 * 1000);
    g_now += 30 * 1000;

    mock_memory(memory, sizeof(memory));
    memory[0x03] = 25;

    /* before rc_client_do_frame, memory will not have been read. all values will be 0 */
    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a0&h=1&x=0123456789ABCDEF", "{\"Success\":true}");

    g_client->state.frames_processed++; /* ping won't get called if no frames have been processed */
    rc_client_idle(g_client);

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a0&h=1&x=0123456789ABCDEF");

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    g_now += 120 * 1000;

    /* rc_client_do_frame will update the memory, so the message will contain appropriate data */
    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF", "{\"Success\":true}");

    rc_client_do_frame(g_client);

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    g_now += 120 * 1000;

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF");

    /* change the memory to make sure the rich presence gets updated */
    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a75&h=1&x=0123456789ABCDEF", "{\"Success\":true}");
    memory[0x03] = 75;

    rc_client_do_frame(g_client);

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    g_now += 120 * 1000;

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a75&h=1&x=0123456789ABCDEF");

    /* no change to rich presence strings. make sure the callback still gets called again */
    rc_client_do_frame(g_client);

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    g_now += 120 * 1000;

    assert_api_call_count("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a75&h=1&x=0123456789ABCDEF", 2);
  }

  rc_client_destroy(g_client);
}

static int rc_client_callback_rich_presence_override_allow(rc_client_t* client, char buffer[], size_t buffersize)
{
  memcpy(buffer, "Custom", 7);
  return 0;
}

static int rc_client_callback_rich_presence_override_replace(rc_client_t* client, char buffer[], size_t buffersize)
{
  memcpy(buffer, "Custom", 7);
  return 6;
}

static void test_do_frame_ping_rich_presence_override_allowed(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  g_client->callbacks.rich_presence_override = rc_client_callback_rich_presence_override_allow;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    rc_client_scheduled_callback_t ping_callback;

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ping_callback = g_client->state.scheduled_callbacks->callback;

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 30 * 1000);
    g_now += 30 * 1000;

    mock_memory(memory, sizeof(memory));
    memory[0x03] = 25;
    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF", "{\"Success\":true}");

    /* ping won't get called if no frames have been processed. do_frame will increment frames_processed */
    rc_client_do_frame(g_client);

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF");

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_ping_rich_presence_override_replaced(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  g_client->callbacks.rich_presence_override = rc_client_callback_rich_presence_override_replace;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    rc_client_scheduled_callback_t ping_callback;

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ping_callback = g_client->state.scheduled_callbacks->callback;

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 30 * 1000);
    g_now += 30 * 1000;

    mock_memory(memory, sizeof(memory));
    memory[0x03] = 25;

    /* before rc_client_do_frame, can_submit only ignores the m parameter. ping still occurs */
    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Custom&h=1&x=0123456789ABCDEF", "{\"Success\":true}");

    /* ping won't get called if no frames have been processed. do_frame will increment frames_processed */
    rc_client_do_frame(g_client);

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&m=Custom&h=1&x=0123456789ABCDEF");

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    g_now += 120 * 1000;

    /* ping still happens every two minutes, even if message not provided */
    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Custom&h=1&x=0123456789ABCDEF", "{\"Success\":true}");
  }

  rc_client_destroy(g_client);
}

static void test_idle_ping_while_not_running(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    rc_client_scheduled_callback_t ping_callback;
    memory[0x03] = 25;
    mock_memory(memory, sizeof(memory));

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ping_callback = g_client->state.scheduled_callbacks->callback;

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 30 * 1000);
    g_now += 30 * 1000;

    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF", "{\"Success\":true}");

    /* if no frames have been processed since the last ping, a ping shouldn't be sent */
    g_client->state.frames_processed = g_client->state.frames_at_last_ping;
    rc_client_idle(g_client);

    assert_api_not_called("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF");

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    g_now += 120 * 1000;

    /* do_frame will increment frames_processed, and ping will get called */

    rc_client_do_frame(g_client);

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF");

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    g_now += 120 * 1000;

    /* no frames processed. ping shouldn't be sent, but still rescheduled */

    rc_client_idle(g_client);

    assert_api_call_count("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF", 1);

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
  }

  /* unloading game should unschedule ping */
  rc_client_unload_game(g_client);
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

  rc_client_destroy(g_client);
}

/* ----- reset ----- */

static void test_reset_hides_widgets(void)
{
  const rc_client_leaderboard_t* leaderboard;
  const rc_client_achievement_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  memory[0x01] = 1; /* challenge indicator for achievement 7 */
  memory[0x06] = 3; /* progress indicator for achievement 6 */
  memory[0x0A] = 2; /* tracker for leaderboard 48 */
  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 4);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 48));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_PRIMED);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  rc_client_reset(g_client);

  ASSERT_NUM_EQUALS(event_count, 3);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_WAITING);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_WAITING);

  /* non tracked achievements/leaderboards should also be reset to waiting */
  achievement = rc_client_get_achievement_info(g_client, 5);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_WAITING);

  leaderboard = rc_client_get_leaderboard_info(g_client, 46);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_WAITING);

  rc_client_destroy(g_client);
}

static void test_reset_detaches_hide_progress_indicator_event(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  memory[0x06] = 3; /* progress indicator for achievement 6 */
  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));
  event_count = 0;

  /* should be two callbacks queued - hiding the progress indicator, and the rich presence update */
  ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks, g_client->game->progress_tracker.hide_callback);
  ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks->next);
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks->next->next);

  /* advance time to hide the progress indicator */
  g_now = g_client->game->progress_tracker.hide_callback->when;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));
  event_count = 0;

  /* only the rich presence update should be scheduled */
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks->next);

  /* advance time to just before the rich presence update */
  g_now = g_client->state.scheduled_callbacks->when - 10;

  /* reschedule the progress indicator */
  memory[0x06] = 4;                         /* 4/6 */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));
  event_count = 0;

  /* should be two callbacks queued - rich presence update, then hiding the progress indicator */
  ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
  ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->next, g_client->game->progress_tracker.hide_callback);
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks->next->next);

  rc_client_reset(g_client);

  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));

  /* only the rich presence update should be scheduled */
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks->next);

  rc_client_destroy(g_client);
}

/* ----- pause ----- */

static void test_can_pause(void)
{
  uint16_t frames_needed, frames_needed2, frames_needed3, frames_needed4;
  uint32_t frames_remaining;
  int i;

  /* pause without client should be allowed */
  ASSERT_NUM_EQUALS(rc_client_can_pause(NULL, NULL), 1);

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);

  rc_client_do_frame(g_client);

  /* first pause should always be allowed */
  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);
  frames_needed = g_client->state.unpaused_frame_decay;

  /* if no frames have been processed, the client is still paused, so pause is allowed */
  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);
  ASSERT_NUM_EQUALS(g_client->state.unpaused_frame_decay, frames_needed);

  /* do a few frames (not enough to allow pause) - pause should still not be allowed */
  for (i = 0; i < 10; i++)
    rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 0);
  ASSERT_NUM_EQUALS(frames_remaining, 10);
  ASSERT_NUM_EQUALS(g_client->state.unpaused_frame_decay, frames_needed - 10);

  /* do enough frames to allow pause, but not clear out the decay value.
   * pause should be allowed, and the decay value should be reset to a higher value. */
  for (i = 0; i < 20; i++)
    rc_client_do_frame(g_client);

  ASSERT_NUM_GREATER(g_client->state.unpaused_frame_decay, 0);
  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);
  frames_needed2 = g_client->state.unpaused_frame_decay;
  ASSERT_NUM_GREATER(frames_needed2, frames_needed);

  /* do enough frames to allow pause before - should not allow pause now */
  for (i = 0; i < 25; i++)
    rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 0);
  ASSERT_NUM_EQUALS(frames_remaining, 15);
  ASSERT_NUM_EQUALS(g_client->state.unpaused_frame_decay, frames_needed2 - 25);

  /* do enough frames to allow pause, but not clear out the decay value.
   * pause should be allowed, and the decay value should be reset to an even higher value. */
  for (i = 0; i < 35; i++)
    rc_client_do_frame(g_client);

  ASSERT_NUM_GREATER(g_client->state.unpaused_frame_decay, 0);
  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);
  frames_needed3 = g_client->state.unpaused_frame_decay;
  ASSERT_NUM_GREATER(frames_needed3, frames_needed2);

  /* completely clear out the decay. decay value should drop, but not all the way. */
  for (i = 0; i < frames_needed3; i++)
    rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);
  frames_needed4 = g_client->state.unpaused_frame_decay;
  ASSERT_NUM_LESS(frames_needed4, frames_needed3);
  ASSERT_NUM_GREATER(frames_needed4, frames_needed);

  /* completely clear out the decay. decay value should drop back to default
   * have to do this twice to get through the decayed cycles */
  for (i = 0; i < frames_needed4 * 2; i++)
    rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);
  ASSERT_NUM_EQUALS(g_client->state.unpaused_frame_decay, frames_needed);

  /* disable hardcore. pause should be allowed immediately */
  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);

  rc_client_destroy(g_client);
}

/* ----- progress ----- */

static void test_deserialize_progress_updates_widgets(void)
{
  const rc_client_leaderboard_t* leaderboard;
  const rc_client_achievement_t* achievement;
  const rc_client_event_t* event;
  uint8_t* serialized1;
  uint8_t* serialized2;
  size_t serialize_size;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  /* create an initial checkpoint */
  serialize_size = rc_client_progress_size(g_client);
  serialized1 = (uint8_t*)malloc(serialize_size);
  serialized2 = (uint8_t*)malloc(serialize_size);
  ASSERT_NUM_EQUALS(rc_client_serialize_progress(g_client, serialized1), RC_OK);

  /* activate some widgets */
  memory[0x01] = 1; /* challenge indicator for achievement 7 */
  memory[0x06] = 4; /* progress indicator for achievement 6*/
  memory[0x0A] = 2; /* tracker for leaderboard 48 */
  memory[0x0E] = 25; /* leaderboard 48 value */
  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 4);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 48));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 2);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  /* capture the state with the widgets visible */
  ASSERT_NUM_EQUALS(rc_client_serialize_progress(g_client, serialized2), RC_OK);

  /* deserialize current state. expect progress tracker hide */
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, serialized2), RC_OK);
  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));
  event_count = 0;

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 2);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  /* deserialize original state. expect challenge indicator hide, tracker hide */
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, serialized1), RC_OK);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 0);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_ACTIVE);

  /* deserialize second state. expect challenge indicator show, tracker show */
  event_count = 0;
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, serialized2), RC_OK);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 2);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  /* update tracker value */
  memory[0x0E] = 30;
  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 1);
  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000030");

  /* deserialize second state. expect challenge tracker update to old value */
  event_count = 0;
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, serialized2), RC_OK);
  ASSERT_NUM_EQUALS(event_count, 1);
  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000025");

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  free(serialized2);
  free(serialized1);
  rc_client_destroy(g_client);
}

static void test_deserialize_progress_null(void)
{
  const rc_client_leaderboard_t* leaderboard;
  const rc_client_achievement_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  /* activate some widgets */
  memory[0x01] = 1; /* challenge indicator for achievement 7 */
  memory[0x0A] = 2; /* tracker for leaderboard 48 */
  memory[0x0E] = 25; /* leaderboard 48 value */
  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 3); /* challenge indicator show, leaderboard start, tracker show */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 2);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  /* deserialize null state. expect all widgets to be hidden and achievements reset to waiting */
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, NULL), RC_OK);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 0);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_WAITING);

  /* must be false before it can be true to change from WAITING to ACTIVE. do so manually */
  ((rc_client_leaderboard_info_t*)leaderboard)->lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* advance frame, challenge indicator and leaderboard tracker should reappear */
  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 3); /* challenge indicator show, leaderboard start, tracker show */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  rc_client_destroy(g_client);
}

static void test_deserialize_progress_invalid(void)
{
  const rc_client_leaderboard_t* leaderboard;
  const rc_client_achievement_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  /* activate some widgets */
  memory[0x01] = 1; /* challenge indicator for achievement 7 */
  memory[0x0A] = 2; /* tracker for leaderboard 48 */
  memory[0x0E] = 25; /* leaderboard 48 value */
  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 3); /* challenge indicator show, leaderboard start, tracker show */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 2);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  /* deserialize null state. expect all widgets to be hidden and achievements reset to waiting */
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, memory), RC_INVALID_STATE);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 0);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_WAITING);

  /* must be false before it can be true to change from WAITING to ACTIVE. do so manually */
  ((rc_client_leaderboard_info_t*)leaderboard)->lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* advance frame, challenge indicator and leaderboard tracker should reappear */
  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 3); /* challenge indicator show, leaderboard start, tracker show */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  rc_client_destroy(g_client);
}

static void test_deserialize_progress_sized(void)
{
  uint8_t* serialized1;
  uint8_t* serialized2;
  size_t serialize_size;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  /* create an initial checkpoint */
  serialize_size = rc_client_progress_size(g_client);
  serialized1 = (uint8_t*)malloc(serialize_size);
  serialized2 = (uint8_t*)malloc(serialize_size);
  ASSERT_NUM_EQUALS(rc_client_serialize_progress_sized(g_client, serialized1, serialize_size - 1), RC_INSUFFICIENT_BUFFER);
  ASSERT_NUM_EQUALS(rc_client_serialize_progress_sized(g_client, serialized1, serialize_size), RC_OK);

  /* activate some widgets */
  memory[0x01] = 1; /* challenge indicator for achievement 7 */
  memory[0x06] = 4; /* progress indicator for achievement 6*/
  memory[0x0A] = 2; /* tracker for leaderboard 48 */
  memory[0x0E] = 25; /* leaderboard 48 value */
  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 4);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 48));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));
  event_count = 0;

  /* capture the state with the widgets visible */
  ASSERT_NUM_EQUALS(rc_client_serialize_progress_sized(g_client, serialized2, serialize_size), RC_OK);

  /* deserialize current state. expect progress tracker hide */
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress_sized(g_client, serialized2, serialize_size), RC_OK);
  ASSERT_NUM_EQUALS(event_count, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));
  event_count = 0;

  /* deserialize original state with incorrect size. expect challenge indicator hide, tracker hide */
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress_sized(g_client, serialized1, serialize_size - 1), RC_INSUFFICIENT_BUFFER);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));

  /* deserialize second state. expect challenge indicator show, tracker show */
  event_count = 0;
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress_sized(g_client, serialized2, serialize_size), RC_OK);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  free(serialized2);
  free(serialized1);
  rc_client_destroy(g_client);
}

static void test_deserialize_progress_unknown_game(void)
{
  uint8_t buffer[128];

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF", patchdata_not_found);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_unknown_game, g_callback_userdata);

  ASSERT_NUM_EQUALS(rc_client_progress_size(g_client), 0);
  ASSERT_NUM_EQUALS(rc_client_serialize_progress_sized(g_client, buffer, sizeof(buffer)), RC_NO_GAME_LOADED);
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress_sized(g_client, buffer, sizeof(buffer)), RC_NO_GAME_LOADED);

  rc_client_destroy(g_client);
}

/* ----- processing required ----- */

static void test_processing_required(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  ASSERT_TRUE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_empty_game(void)
{
  g_client = mock_client_game_loaded(patchdata_empty, no_unlocks);

  ASSERT_FALSE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_rich_presence_only(void)
{
  g_client = mock_client_game_loaded(patchdata_rich_presence_only, no_unlocks);

  ASSERT_TRUE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_leaderboard_only(void)
{
  g_client = mock_client_game_loaded(patchdata_leaderboard_only, no_unlocks);

  ASSERT_TRUE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_after_mastery(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501_and_5502);

  ASSERT_TRUE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_after_mastery_no_leaderboards(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_0lbd, unlock_5501_and_5502);

  ASSERT_FALSE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

/* ----- settings ----- */

static void test_set_hardcore_disable(void)
{
  const rc_client_achievement_t* achievement;
  const rc_client_leaderboard_t* leaderboard;
  const rc_trigger_t* trigger;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501h_and_5502);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  trigger = ((rc_client_achievement_info_t*)achievement)->trigger;
  ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_TRIGGERED);

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  trigger = ((rc_client_achievement_info_t*)achievement)->trigger;
  ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_WAITING);

  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 1); /* only 5502 should be active*/

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(g_client->game->runtime.lboard_count, 1);

  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 0);

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  trigger = ((rc_client_achievement_info_t*)achievement)->trigger;

  ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 0); /* 5502 should not be active*/

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_INACTIVE);
  ASSERT_NUM_EQUALS(g_client->game->runtime.lboard_count, 0);

  rc_client_destroy(g_client);
}

static void test_set_hardcore_disable_active_tracker(void)
{
  const rc_client_leaderboard_t* leaderboard;
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501h_and_5502);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  memory[0x0C] = 1;
  memory[0x0E] = 25;
  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 2);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 4401);
  ASSERT_PTR_NOT_NULL(event);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000025");

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);

  event_count = 0;
  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 0);
  ASSERT_NUM_EQUALS(event_count, 1);

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_INACTIVE);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);

  rc_client_destroy(g_client);
}

static void test_set_hardcore_enable(void)
{
  const rc_client_achievement_t* achievement;
  const rc_client_leaderboard_t* leaderboard;

  g_client = mock_client_logged_in();
  rc_client_set_hardcore_enabled(g_client, 0);
  mock_client_load_game_softcore(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 0); /* 5502 should not be active*/
  }

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  if (leaderboard) {
    ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_INACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.lboard_count, 0);
  }

  /* when enabling hardcore, flag waiting_for_reset. this will prevent processing until rc_client_reset is called */
  event_count = 0;
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_RESET, 0));

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 1); /* 5502 should be active*/
  }

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  if (leaderboard) {
    ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.lboard_count, 1);
  }

  /* resetting clears waiting_for_reset */
  rc_client_reset(g_client);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 0);

  /* hardcore already enabled, attempting to set it again shouldn't flag waiting_for_reset */
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 0);

  rc_client_destroy(g_client);
}

static void test_set_hardcore_enable_no_game_loaded(void)
{
  g_client = mock_client_logged_in();
  rc_client_set_hardcore_enabled(g_client, 0);

  /* enabling hardcore before a game is loaded just toggles the flag  */
  event_count = 0;
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(event_count, 0);

  rc_client_destroy(g_client);
}

static void test_set_hardcore_enable_encore_mode(void)
{
  const rc_client_achievement_t* achievement;
  rc_client_achievement_info_t* achievement_info;

  g_client = mock_client_logged_in();
  rc_client_set_encore_mode_enabled(g_client, 1);
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 2);

  g_client->game->runtime.triggers[0].trigger->state = RC_TRIGGER_STATE_ACTIVE;
  g_client->game->runtime.triggers[1].trigger->state = RC_TRIGGER_STATE_ACTIVE;

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH); /* unlock information still tracked */
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);     /* but achievement remains active */
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  }

  /* toggle hardcore mode should retain active achievements */
  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 2);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  }

  /* toggle hardcore mode should retain active achievements */
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 2);

  /* trigger an achievement */
  achievement_info = (rc_client_achievement_info_t*)rc_client_get_achievement_info(g_client, 5501);
  achievement_info->public_.state = RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED;
  g_client->game->runtime.triggers[0].trigger->state = RC_TRIGGER_STATE_TRIGGERED;

  /* toggle hardcore mode should retain active achievements */
  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 1); /* only one active now */

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
    ASSERT_PTR_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger, g_client->game->runtime.triggers[0].trigger);
  }

  /* toggle hardcore mode should retain active achievements */
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 1);

  rc_client_destroy(g_client);
}

static void test_set_encore_mode_enable(void)
{
  const rc_client_achievement_t* achievement;

  g_client = mock_client_logged_in();
  rc_client_set_encore_mode_enabled(g_client, 1);
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH); /* unlock information still tracked */
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);     /* but achievement remains active */
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }

  /* toggle encore mode with a game loaded has no effect */
  rc_client_set_encore_mode_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 0);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }

  rc_client_destroy(g_client);
}

static void test_set_encore_mode_disable(void)
{
  const rc_client_achievement_t* achievement;

  g_client = mock_client_logged_in();
  rc_client_set_encore_mode_enabled(g_client, 1);
  rc_client_set_encore_mode_enabled(g_client, 0);
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 0);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }

  /* toggle encore mode with a game loaded has no effect */
  rc_client_set_encore_mode_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }

  rc_client_destroy(g_client);
}

static void test_get_user_agent_clause(void)
{
  char expected_clause[] = "rcheevos/" RCHEEVOS_VERSION_STRING;
  char buffer[64];

  g_client = mock_client_not_logged_in();
  ASSERT_NUM_EQUALS(rc_client_get_user_agent_clause(g_client, buffer, sizeof(buffer)), sizeof(expected_clause) - 1);
  ASSERT_STR_EQUALS(buffer, expected_clause);

  /* snprintf will return the number of characters it wants, even if the buffer is too small,
   * but will only fill as much of the buffer is available */
  ASSERT_NUM_EQUALS(rc_client_get_user_agent_clause(g_client, buffer, 8), sizeof(expected_clause) - 1);
  ASSERT_STR_EQUALS(buffer, "rcheevo");

  rc_client_destroy(g_client);
}

static void test_version(void)
{
  ASSERT_STR_EQUALS(rc_version_string(), RCHEEVOS_VERSION_STRING);
  ASSERT_NUM_EQUALS(rc_version(), RCHEEVOS_VERSION);
}

/* ----- harness ----- */

void test_client(void) {
  TEST_SUITE_BEGIN();

  /* login */
  TEST(test_login_with_password);
  TEST(test_login_with_token);
  TEST(test_login_required_fields);
  TEST(test_login_with_incorrect_password);
  TEST(test_login_with_incorrect_token);
  TEST(test_login_with_expired_token);
  TEST(test_login_with_banned_account);
  TEST(test_login_incomplete_response);
  TEST(test_login_with_password_async);
  TEST(test_login_with_password_async_aborted);
  TEST(test_login_with_password_async_destroyed);
  TEST(test_login_with_password_client_error);

  /* logout */
  TEST(test_logout);
  TEST(test_logout_with_game_loaded);
  TEST(test_logout_during_login);
  TEST(test_logout_during_fetch_game);

  /* user */
  TEST(test_user_get_image_url);

  TEST(test_get_user_game_summary);
  TEST(test_get_user_game_summary_softcore);
  TEST(test_get_user_game_summary_encore_mode);
  TEST(test_get_user_game_summary_with_unsupported_and_unofficial);
  TEST(test_get_user_game_summary_with_unsupported_unlocks);
  TEST(test_get_user_game_summary_with_unofficial_off);
  TEST(test_get_user_game_summary_no_achievements);
  TEST(test_get_user_game_summary_unknown_game);
  TEST(test_get_user_game_summary_progress_incomplete);
  TEST(test_get_user_game_summary_progress_progression_no_win);
  TEST(test_get_user_game_summary_progress_win_only);
  TEST(test_get_user_game_summary_beat);
  TEST(test_get_user_game_summary_mastery);

  /* load game */
  TEST(test_load_game_required_fields);
  TEST(test_load_game_unknown_hash);
  TEST(test_load_game_unknown_hash_repeated);
  TEST(test_load_game_unknown_hash_multiple);
  TEST(test_load_game_not_logged_in);
  TEST(test_load_game);
  TEST(test_load_game_async_load_different_game);
  TEST(test_load_game_async_login);
  TEST(test_load_game_async_login_with_incorrect_password);
  TEST(test_load_game_async_login_logout);
  TEST(test_load_game_async_login_aborted);
  TEST(test_load_game_patch_failure);
  TEST(test_load_game_startsession_failure);
  TEST(test_load_game_startsession_timeout);
  TEST(test_load_game_startsession_custom_timeout);
  TEST(test_load_game_patch_aborted);
  TEST(test_load_game_startsession_aborted);
  TEST(test_load_game_while_spectating);
  TEST(test_load_game_process_game_sets);
  TEST(test_load_game_destroy_while_fetching_game_data);
  TEST(test_load_unknown_game);
  TEST(test_load_unknown_game_multihash);
  TEST(test_load_game_dispatched_read_memory);

  /* unload game */
  TEST(test_unload_game);
  TEST(test_unload_game_hides_ui);
  TEST(test_unload_game_while_fetching_game_data);
  TEST(test_unload_game_while_starting_session);

#ifdef RC_CLIENT_SUPPORTS_HASH
  /* identify and load game */
  TEST(test_identify_and_load_game_required_fields);
  TEST(test_identify_and_load_game_console_specified);
  TEST(test_identify_and_load_game_console_not_specified);
 #ifndef RC_HASH_NO_ROM
  TEST(test_identify_and_load_game_multiconsole_first);
  TEST(test_identify_and_load_game_multiconsole_second);
 #endif
 #ifndef RC_HASH_NO_DISC
  TEST(test_identify_and_load_game_from_disc);
 #endif
  TEST(test_identify_and_load_game_unknown_hash);
  TEST(test_identify_and_load_game_unknown_hash_repeated);
  TEST(test_identify_and_load_game_unknown_hash_multiple);
 #ifndef RC_HASH_NO_ROM
  TEST(test_identify_and_load_game_unknown_hash_multiconsole);
 #endif
  TEST(test_identify_and_load_game_unknown_hash_console_specified);
  TEST(test_identify_and_load_game_unknown_hash_client_provided);
  TEST(test_identify_and_load_game_multihash);
  TEST(test_identify_and_load_game_multihash_unknown_game);
  TEST(test_identify_and_load_game_multihash_differ);

  /* change media */
  TEST(test_change_media_required_fields);
  TEST(test_change_media_no_game_loaded);
  TEST(test_change_media_same_game);
  TEST(test_change_media_known_game);
  TEST(test_change_media_unknown_game);
  TEST(test_change_media_unhashable);
  TEST(test_change_media_unhashable_without_generation);
  TEST(test_change_media_back_and_forth);
  TEST(test_change_media_while_loading);
  TEST(test_change_media_while_loading_later);
  TEST(test_change_media_async_aborted);
  TEST(test_change_media_client_error);
#endif

  TEST(test_change_media_from_hash_required_fields);
  TEST(test_change_media_from_hash_no_game_loaded);
  TEST(test_change_media_from_hash_same_game);
  TEST(test_change_media_from_hash_known_game);
  TEST(test_change_media_from_hash_unknown_game);
  TEST(test_change_media_from_hash_back_and_forth);
  TEST(test_change_media_from_hash_while_loading);
  TEST(test_change_media_from_hash_while_loading_later);
  TEST(test_change_media_from_hash_async_aborted);
  TEST(test_change_media_from_hash_client_error);

  /* game */
  TEST(test_game_get_image_url);

  /* hash library */
  TEST(test_fetch_hash_library);

  /* game titles */
  TEST(test_fetch_game_titles);

  /* all user progress */
  TEST(test_fetch_all_user_progress);

  /* subset */
  TEST(test_load_subset);
  TEST(test_subset_list);

  /* achievements */
  TEST(test_achievement_list_simple);
  TEST(test_achievement_list_simple_with_unlocks);
  TEST(test_achievement_list_simple_with_unlocks_encore_mode);
  TEST(test_achievement_list_simple_with_unofficial_and_unsupported);
  TEST(test_achievement_list_simple_with_unofficial_off);
  TEST(test_achievement_list_buckets);
  TEST(test_achievement_list_buckets_progress_sort);
  TEST(test_achievement_list_buckets_progress_sort_big_ids);
  TEST(test_achievement_list_buckets_with_unsynced);
  TEST(test_achievement_list_subset_with_unofficial_and_unsupported);
  TEST(test_achievement_list_subset_buckets);

  TEST(test_achievement_get_image_url);

  /* leaderboards */
  TEST(test_leaderboard_list_simple);
  TEST(test_leaderboard_list_simple_with_unsupported);
  TEST(test_leaderboard_list_buckets);
  TEST(test_leaderboard_list_buckets_with_unsupported);
  TEST(test_leaderboard_list_subset);
  TEST(test_leaderboard_list_hidden);

  TEST(test_fetch_leaderboard_entries);
  TEST(test_fetch_leaderboard_entries_no_user);
  TEST(test_fetch_leaderboard_entries_around_user);
  TEST(test_fetch_leaderboard_entries_around_user_not_logged_in);
  TEST(test_fetch_leaderboard_entries_async_aborted);
  TEST(test_fetch_leaderboard_entries_client_error);

  TEST(test_map_leaderboard_format);

  /* do frame */
  TEST(test_do_frame_bounds_check_system);
  TEST(test_do_frame_bounds_check_available);
  TEST(test_do_frame_achievement_trigger);
  TEST(test_do_frame_achievement_trigger_already_awarded);
  TEST(test_do_frame_achievement_trigger_server_error);
  TEST(test_do_frame_achievement_trigger_while_spectating);
  TEST(test_do_frame_achievement_trigger_blocked);
  TEST(test_do_frame_achievement_trigger_automatic_retry_empty);
  TEST(test_do_frame_achievement_trigger_automatic_retry_429);
  TEST(test_do_frame_achievement_trigger_automatic_retry_502);
  TEST(test_do_frame_achievement_trigger_automatic_retry_503);
  TEST(test_do_frame_achievement_trigger_automatic_retry_custom_timeout);
  TEST(test_do_frame_achievement_trigger_automatic_retry_generic_empty_response);
  TEST(test_do_frame_achievement_trigger_subset);
  TEST(test_do_frame_achievement_trigger_rarity);
  TEST(test_do_frame_achievement_measured);
  TEST(test_do_frame_achievement_measured_progress_event);
  TEST(test_do_frame_achievement_measured_progress_reshown);
  TEST(test_do_frame_achievement_challenge_indicator);
  TEST(test_do_frame_achievement_challenge_indicator_primed_while_reset);
  TEST(test_do_frame_achievement_challenge_indicator_primed_while_reset_next);
  TEST(test_do_frame_mastery);
  TEST(test_do_frame_mastery_encore);
  TEST(test_do_frame_mastery_subset);
  TEST(test_do_frame_leaderboard_started);
  TEST(test_do_frame_leaderboard_update);
  TEST(test_do_frame_leaderboard_failed);
  TEST(test_do_frame_leaderboard_submit);
  TEST(test_do_frame_leaderboard_submit_server_error);
  TEST(test_do_frame_leaderboard_submit_while_spectating);
  TEST(test_do_frame_leaderboard_submit_immediate);
  TEST(test_do_frame_leaderboard_submit_hidden);
  TEST(test_do_frame_leaderboard_submit_blocked);
  TEST(test_do_frame_leaderboard_submit_softcore);
  TEST(test_do_frame_leaderboard_tracker_sharing);
  TEST(test_do_frame_leaderboard_tracker_sharing_hits);
  TEST(test_do_frame_leaderboard_submit_automatic_retry);
  TEST(test_do_frame_multiple_automatic_retry);
  TEST(test_do_frame_rich_presence_hitcount);

  TEST(test_clock_get_now_millisecs);

  /* ping */
  TEST(test_idle_ping);
  TEST(test_do_frame_ping_rich_presence);
  TEST(test_do_frame_ping_rich_presence_override_allowed);
  TEST(test_do_frame_ping_rich_presence_override_replaced);
  TEST(test_idle_ping_while_not_running);

  /* reset */
  TEST(test_reset_hides_widgets);
  TEST(test_reset_detaches_hide_progress_indicator_event);

  /* pause */
  TEST(test_can_pause);

  /* deserialize_progress */
  TEST(test_deserialize_progress_updates_widgets);
  TEST(test_deserialize_progress_null);
  TEST(test_deserialize_progress_invalid);
  TEST(test_deserialize_progress_sized);
  TEST(test_deserialize_progress_unknown_game);

  /* processing required */
  TEST(test_processing_required);
  TEST(test_processing_required_empty_game);
  TEST(test_processing_required_rich_presence_only);
  TEST(test_processing_required_leaderboard_only);
  TEST(test_processing_required_after_mastery);
  TEST(test_processing_required_after_mastery_no_leaderboards);

  /* settings */
  TEST(test_set_hardcore_disable);
  TEST(test_set_hardcore_disable_active_tracker);
  TEST(test_set_hardcore_enable);
  TEST(test_set_hardcore_enable_no_game_loaded);
  TEST(test_set_hardcore_enable_encore_mode);
  TEST(test_set_encore_mode_enable);
  TEST(test_set_encore_mode_disable);

  TEST(test_get_user_agent_clause);
  TEST(test_version);

  TEST_SUITE_END();
}
