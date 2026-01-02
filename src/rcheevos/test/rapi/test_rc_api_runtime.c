#include "rc_api_runtime.h"

#include "rc_error.h"
#include "rc_runtime_types.h"

#include "../src/rapi/rc_api_common.h"
#include "../test_framework.h"

#define DOREQUEST_URL "https://retroachievements.org/dorequest.php"

static void init_server_response(rc_api_server_response_t* server_response,
                                 int status_code, const char* body, size_t body_length) {
  memset(server_response, 0, sizeof(*server_response));
  server_response->body = body;
  server_response->body_length = body_length;
  server_response->http_status_code = status_code;
}

/* ----- resolve hash ----- */

static void test_init_resolve_hash_request() {
  rc_api_resolve_hash_request_t resolve_hash_request;
  rc_api_request_t request;

  memset(&resolve_hash_request, 0, sizeof(resolve_hash_request));
  resolve_hash_request.username = "Username"; /* credentials are ignored - turns out server doesn't validate this API */
  resolve_hash_request.api_token = "API_TOKEN";
  resolve_hash_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_resolve_hash_request(&request, &resolve_hash_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=gameid&m=ABCDEF0123456789");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_resolve_hash_request_no_credentials() {
  rc_api_resolve_hash_request_t resolve_hash_request;
  rc_api_request_t request;

  memset(&resolve_hash_request, 0, sizeof(resolve_hash_request));
  resolve_hash_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_resolve_hash_request(&request, &resolve_hash_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=gameid&m=ABCDEF0123456789");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_resolve_hash_request_no_hash() {
  rc_api_resolve_hash_request_t resolve_hash_request;
  rc_api_request_t request;

  memset(&resolve_hash_request, 0, sizeof(resolve_hash_request));

  ASSERT_NUM_EQUALS(rc_api_init_resolve_hash_request(&request, &resolve_hash_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_resolve_hash_request_empty_hash() {
  rc_api_resolve_hash_request_t resolve_hash_request;
  rc_api_request_t request;

  memset(&resolve_hash_request, 0, sizeof(resolve_hash_request));
  resolve_hash_request.game_hash = "";

  ASSERT_NUM_EQUALS(rc_api_init_resolve_hash_request(&request, &resolve_hash_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_process_resolve_hash_response_match() {
  rc_api_resolve_hash_response_t resolve_hash_response;
  const char* server_response = "{\"Success\":true,\"GameID\":1446}";

  memset(&resolve_hash_response, 0, sizeof(resolve_hash_response));

  ASSERT_NUM_EQUALS(rc_api_process_resolve_hash_response(&resolve_hash_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(resolve_hash_response.response.succeeded, 1);
  ASSERT_PTR_NULL(resolve_hash_response.response.error_message);
  ASSERT_NUM_EQUALS(resolve_hash_response.game_id, 1446);

  rc_api_destroy_resolve_hash_response(&resolve_hash_response);
}

static void test_process_resolve_hash_response_no_match() {
  rc_api_resolve_hash_response_t resolve_hash_response;
  const char* server_response = "{\"Success\":true,\"GameID\":0}";

  memset(&resolve_hash_response, 0, sizeof(resolve_hash_response));

  ASSERT_NUM_EQUALS(rc_api_process_resolve_hash_response(&resolve_hash_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(resolve_hash_response.response.succeeded, 1);
  ASSERT_PTR_NULL(resolve_hash_response.response.error_message);
  ASSERT_NUM_EQUALS(resolve_hash_response.game_id, 0);

  rc_api_destroy_resolve_hash_response(&resolve_hash_response);
}

/* ----- fetch game data ----- */

static void test_init_fetch_game_data_request() {
  rc_api_fetch_game_data_request_t fetch_game_data_request;
  rc_api_request_t request;

  memset(&fetch_game_data_request, 0, sizeof(fetch_game_data_request));
  fetch_game_data_request.username = "Username";
  fetch_game_data_request.api_token = "API_TOKEN";
  fetch_game_data_request.game_id = 1234;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_game_data_request(&request, &fetch_game_data_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=patch&u=Username&t=API_TOKEN&g=1234");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_game_data_request_by_hash() {
  rc_api_fetch_game_data_request_t fetch_game_data_request;
  rc_api_request_t request;

  memset(&fetch_game_data_request, 0, sizeof(fetch_game_data_request));
  fetch_game_data_request.username = "Username";
  fetch_game_data_request.api_token = "API_TOKEN";
  fetch_game_data_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_fetch_game_data_request(&request, &fetch_game_data_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=patch&u=Username&t=API_TOKEN&m=ABCDEF0123456789");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_game_data_request_by_id_and_hash() {
  rc_api_fetch_game_data_request_t fetch_game_data_request;
  rc_api_request_t request;

  memset(&fetch_game_data_request, 0, sizeof(fetch_game_data_request));
  fetch_game_data_request.username = "Username";
  fetch_game_data_request.api_token = "API_TOKEN";
  fetch_game_data_request.game_id = 1234;
  fetch_game_data_request.game_hash = "ABCDEF0123456789"; /* ignored when game_id provided */

  ASSERT_NUM_EQUALS(rc_api_init_fetch_game_data_request(&request, &fetch_game_data_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=patch&u=Username&t=API_TOKEN&g=1234");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_game_data_request_no_id() {
  rc_api_fetch_game_data_request_t fetch_game_data_request;
  rc_api_request_t request;

  memset(&fetch_game_data_request, 0, sizeof(fetch_game_data_request));
  fetch_game_data_request.username = "Username";
  fetch_game_data_request.api_token = "API_TOKEN";

  ASSERT_NUM_EQUALS(rc_api_init_fetch_game_data_request(&request, &fetch_game_data_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_process_fetch_game_data_response_empty() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
      "\"ID\":177,\"Title\":\"My Game\",\"ConsoleID\":23,\"ImageIcon\":\"/Images/012345.png\","
      "\"Achievements\":[],\"Leaderboards\":[]"
      "}}";

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_game_data_response.id, 177);
  ASSERT_STR_EQUALS(fetch_game_data_response.title, "My Game");
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 23);
  ASSERT_STR_EQUALS(fetch_game_data_response.image_name, "012345");
  ASSERT_STR_EQUALS(fetch_game_data_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 0);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_invalid_credentials() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(fetch_game_data_response.response.error_message, "Credentials invalid (0)");
  ASSERT_NUM_EQUALS(fetch_game_data_response.id, 0);
  ASSERT_PTR_NULL(fetch_game_data_response.title);
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 0);
  ASSERT_PTR_NULL(fetch_game_data_response.image_name);
  ASSERT_PTR_NULL(fetch_game_data_response.rich_presence_script);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 0);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_not_found() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Unknown game\",\"Code\":\"not_found\",\"Status\":404}";

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_NOT_FOUND);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(fetch_game_data_response.response.error_message, "Unknown game");
  ASSERT_NUM_EQUALS(fetch_game_data_response.id, 0);
  ASSERT_PTR_NULL(fetch_game_data_response.title);
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 0);
  ASSERT_PTR_NULL(fetch_game_data_response.image_name);
  ASSERT_PTR_NULL(fetch_game_data_response.rich_presence_script);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 0);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_achievements() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
      "\"ID\":20,\"Title\":\"Another Amazing Game\",\"ConsoleID\":19,\"ImageIcon\":\"/Images/112233.png\","
      "\"Achievements\":["
       "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
        "\"MemAddr\":\"0=1\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
        "\"Created\":1367266583,\"Modified\":1376929305},"
       "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
        "\"MemAddr\":\"0=2\",\"Author\":\"User1\",\"BadgeName\":\"00235\","
        "\"Created\":1376970283,\"Modified\":1376970283},"
       "{\"ID\":5503,\"Title\":\"Ach3\",\"Description\":\"Desc3\",\"Flags\":5,\"Points\":0,"
        "\"MemAddr\":\"0=3\",\"Author\":\"User2\",\"BadgeName\":\"00236\","
        "\"Created\":1376969412,\"Modified\":1376969412},"
       "{\"ID\":5504,\"Title\":\"Ach4\",\"Description\":\"Desc4\",\"Flags\":3,\"Points\":10,"
        "\"MemAddr\":\"0=4\",\"Author\":\"User1\",\"BadgeName\":\"00236\","
        "\"Created\":1504474554,\"Modified\":1504474554}"
      "],\"Leaderboards\":[]"
      "}}";
  rc_api_achievement_definition_t* achievement;

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_game_data_response.id, 20);
  ASSERT_STR_EQUALS(fetch_game_data_response.title, "Another Amazing Game");
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 19);
  ASSERT_STR_EQUALS(fetch_game_data_response.image_name, "112233");
  ASSERT_STR_EQUALS(fetch_game_data_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 4);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 0);

  ASSERT_PTR_NOT_NULL(fetch_game_data_response.achievements);
  achievement = fetch_game_data_response.achievements;

  ASSERT_NUM_EQUALS(achievement->id, 5501);
  ASSERT_STR_EQUALS(achievement->title, "Ach1");
  ASSERT_STR_EQUALS(achievement->description, "Desc1");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 5);
  ASSERT_STR_EQUALS(achievement->definition, "0=1");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00234");
  ASSERT_NUM_EQUALS(achievement->created, 1367266583);
  ASSERT_NUM_EQUALS(achievement->updated, 1376929305);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5502);
  ASSERT_STR_EQUALS(achievement->title, "Ach2");
  ASSERT_STR_EQUALS(achievement->description, "Desc2");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 2);
  ASSERT_STR_EQUALS(achievement->definition, "0=2");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00235");
  ASSERT_NUM_EQUALS(achievement->created, 1376970283);
  ASSERT_NUM_EQUALS(achievement->updated, 1376970283);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5503);
  ASSERT_STR_EQUALS(achievement->title, "Ach3");
  ASSERT_STR_EQUALS(achievement->description, "Desc3");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_UNOFFICIAL);
  ASSERT_NUM_EQUALS(achievement->points, 0);
  ASSERT_STR_EQUALS(achievement->definition, "0=3");
  ASSERT_STR_EQUALS(achievement->author, "User2");
  ASSERT_STR_EQUALS(achievement->badge_name, "00236");
  ASSERT_NUM_EQUALS(achievement->created, 1376969412);
  ASSERT_NUM_EQUALS(achievement->updated, 1376969412);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5504);
  ASSERT_STR_EQUALS(achievement->title, "Ach4");
  ASSERT_STR_EQUALS(achievement->description, "Desc4");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 10);
  ASSERT_STR_EQUALS(achievement->definition, "0=4");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00236");
  ASSERT_NUM_EQUALS(achievement->created, 1504474554);
  ASSERT_NUM_EQUALS(achievement->updated, 1504474554);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_achievement_types() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":20,\"Title\":\"Another Amazing Game\",\"ConsoleID\":19,\"ImageIcon\":\"/Images/112233.png\","
    "\"Achievements\":["
    "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
     "\"MemAddr\":\"0=1\",\"Author\":\"User1\",\"BadgeName\":\"00234\",\"Type\":\"\","
     "\"Created\":1367266583,\"Modified\":1376929305},"
    "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
     "\"MemAddr\":\"0=2\",\"Author\":\"User1\",\"BadgeName\":\"00235\",\"Type\":\"missable\","
     "\"Created\":1376970283,\"Modified\":1376970283},"
    "{\"ID\":5503,\"Title\":\"Ach3\",\"Description\":\"Desc3\",\"Flags\":5,\"Points\":0,"
     "\"MemAddr\":\"0=3\",\"Author\":\"User2\",\"BadgeName\":\"00236\",\"Type\":\"progression\","
     "\"Created\":1376969412,\"Modified\":1376969412},"
    "{\"ID\":5504,\"Title\":\"Ach4\",\"Description\":\"Desc4\",\"Flags\":3,\"Points\":10,"
     "\"MemAddr\":\"0=4\",\"Author\":\"User1\",\"BadgeName\":\"00236\",\"Type\":\"win_condition\","
     "\"Created\":1504474554,\"Modified\":1504474554},"
    "{\"ID\":5505,\"Title\":\"Ach5 [m]\",\"Description\":\"Desc5\",\"Flags\":3,\"Points\":10,"
     "\"MemAddr\":\"0=4\",\"Author\":\"User1\",\"BadgeName\":\"00236\",\"Type\":\"\","
     "\"Created\":1504474554,\"Modified\":1504474554},"
    "{\"ID\":5506,\"Title\":\"[m] Ach6\",\"Description\":\"Desc6\",\"Flags\":3,\"Points\":10,"
     "\"MemAddr\":\"0=4\",\"Author\":\"User1\",\"BadgeName\":\"00236\",\"Type\":\"\","
     "\"Created\":1504474554,\"Modified\":1504474554},"
    "{\"ID\":5507,\"Title\":\"Ach7\",\"Description\":\"Desc7\",\"Flags\":3,\"Points\":5,"
     "\"MemAddr\":\"0=1\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
     "\"Created\":1367266583,\"Modified\":1376929305}"
    "],\"Leaderboards\":[]"
    "}}";
  rc_api_achievement_definition_t* achievement;

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 7);

  ASSERT_PTR_NOT_NULL(fetch_game_data_response.achievements);
  achievement = fetch_game_data_response.achievements;

  ASSERT_NUM_EQUALS(achievement->id, 5501);
  ASSERT_STR_EQUALS(achievement->title, "Ach1");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_STANDARD);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5502);
  ASSERT_STR_EQUALS(achievement->title, "Ach2");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_MISSABLE);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5503);
  ASSERT_STR_EQUALS(achievement->title, "Ach3");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_PROGRESSION);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5504);
  ASSERT_STR_EQUALS(achievement->title, "Ach4");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_WIN);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5505);
  ASSERT_STR_EQUALS(achievement->title, "Ach5");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_MISSABLE);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5506);
  ASSERT_STR_EQUALS(achievement->title, "Ach6");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_MISSABLE);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5507);
  ASSERT_STR_EQUALS(achievement->title, "Ach7");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_STANDARD);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_achievement_rarity() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":20,\"Title\":\"Another Amazing Game\",\"ConsoleID\":19,\"ImageIcon\":\"/Images/112233.png\","
    "\"Achievements\":["
    "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
     "\"MemAddr\":\"0=1\",\"Author\":\"User1\",\"BadgeName\":\"00234\",\"Rarity\":100.0,\"RarityHardcore\":66.67,"
     "\"Created\":1367266583,\"Modified\":1376929305},"
    "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
     "\"MemAddr\":\"0=2\",\"Author\":\"User1\",\"BadgeName\":\"00235\",\"Rarity\":57.43,\"RarityHardcore\":57.43,"
     "\"Created\":1376970283,\"Modified\":1376970283},"
    "{\"ID\":5503,\"Title\":\"Ach3\",\"Description\":\"Desc3\",\"Flags\":5,\"Points\":0,"
     "\"MemAddr\":\"0=3\",\"Author\":\"User2\",\"BadgeName\":\"00236\",\"Rarity\":6.8,\"RarityHardcore\":0,"
     "\"Created\":1376969412,\"Modified\":1376969412},"
    "{\"ID\":5504,\"Title\":\"Ach4\",\"Description\":\"Desc4\",\"Flags\":5,\"Points\":0,"
     "\"MemAddr\":\"0=3\",\"Author\":\"User2\",\"BadgeName\":\"00236\","
     "\"Created\":1376969412,\"Modified\":1376969412}"
    "],\"Leaderboards\":[]"
    "}}";
  rc_api_achievement_definition_t* achievement;

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 4);

  ASSERT_PTR_NOT_NULL(fetch_game_data_response.achievements);
  achievement = fetch_game_data_response.achievements;

  ASSERT_NUM_EQUALS(achievement->id, 5501);
  ASSERT_STR_EQUALS(achievement->title, "Ach1");
  ASSERT_FLOAT_EQUALS(achievement->rarity, 100.0f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 66.67f);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5502);
  ASSERT_STR_EQUALS(achievement->title, "Ach2");
  ASSERT_FLOAT_EQUALS(achievement->rarity, 57.43f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 57.43f);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5503);
  ASSERT_STR_EQUALS(achievement->title, "Ach3");
  ASSERT_FLOAT_EQUALS(achievement->rarity, 6.8f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 0.0f);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5504);
  ASSERT_STR_EQUALS(achievement->title, "Ach4");
  ASSERT_FLOAT_EQUALS(achievement->rarity, 100.0f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 100.0f);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_achievement_null_author()
{
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":20,\"Title\":\"Another Amazing Game\",\"ConsoleID\":19,\"ImageIcon\":\"/Images/112233.png\","
    "\"Achievements\":["
    "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
    "\"MemAddr\":\"0=1\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
    "\"BadgeURL\":\"http://host/Badge/00234.png\",\"BadgeLockedURL\":\"http://host/Badge/00234_lock.png\","
    "\"Created\":1367266583,\"Modified\":1376929305},"
    "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
    "\"MemAddr\":\"0=2\",\"Author\":null,\"BadgeName\":\"00235\","
    "\"Created\":1376970283,\"Modified\":1376970283},"
    "{\"ID\":5503,\"Title\":\"Ach3\",\"Description\":\"Desc3\",\"Flags\":5,\"Points\":0,"
    "\"MemAddr\":\"0=3\",\"Author\":null,\"BadgeName\":\"00236\","
    "\"Created\":1376969412,\"Modified\":1376969412},"
    "{\"ID\":5504,\"Title\":\"Ach4\",\"Description\":\"Desc4\",\"Flags\":3,\"Points\":10,"
    "\"MemAddr\":\"0=4\",\"Author\":\"User1\",\"BadgeName\":\"00236\","
    "\"Created\":1504474554,\"Modified\":1504474554}"
    "],\"Leaderboards\":[]"
    "}}";
  rc_api_achievement_definition_t* achievement;

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_game_data_response.id, 20);
  ASSERT_STR_EQUALS(fetch_game_data_response.title, "Another Amazing Game");
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 19);
  ASSERT_STR_EQUALS(fetch_game_data_response.image_name, "112233");
  ASSERT_STR_EQUALS(fetch_game_data_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 4);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 0);

  ASSERT_PTR_NOT_NULL(fetch_game_data_response.achievements);
  achievement = fetch_game_data_response.achievements;

  ASSERT_NUM_EQUALS(achievement->id, 5501);
  ASSERT_STR_EQUALS(achievement->title, "Ach1");
  ASSERT_STR_EQUALS(achievement->description, "Desc1");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 5);
  ASSERT_STR_EQUALS(achievement->definition, "0=1");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00234");
  ASSERT_STR_EQUALS(achievement->badge_url, "http://host/Badge/00234.png");
  ASSERT_STR_EQUALS(achievement->badge_locked_url, "http://host/Badge/00234_lock.png");
  ASSERT_NUM_EQUALS(achievement->created, 1367266583);
  ASSERT_NUM_EQUALS(achievement->updated, 1376929305);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5502);
  ASSERT_STR_EQUALS(achievement->title, "Ach2");
  ASSERT_STR_EQUALS(achievement->description, "Desc2");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 2);
  ASSERT_STR_EQUALS(achievement->definition, "0=2");
  ASSERT_STR_EQUALS(achievement->author, "");
  ASSERT_STR_EQUALS(achievement->badge_name, "00235");
  ASSERT_STR_EQUALS(achievement->badge_url, "https://media.retroachievements.org/Badge/00235.png");
  ASSERT_STR_EQUALS(achievement->badge_locked_url, "https://media.retroachievements.org/Badge/00235_lock.png");
  ASSERT_NUM_EQUALS(achievement->created, 1376970283);
  ASSERT_NUM_EQUALS(achievement->updated, 1376970283);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5503);
  ASSERT_STR_EQUALS(achievement->title, "Ach3");
  ASSERT_STR_EQUALS(achievement->description, "Desc3");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_UNOFFICIAL);
  ASSERT_NUM_EQUALS(achievement->points, 0);
  ASSERT_STR_EQUALS(achievement->definition, "0=3");
  ASSERT_STR_EQUALS(achievement->author, "");
  ASSERT_STR_EQUALS(achievement->badge_name, "00236");
  ASSERT_STR_EQUALS(achievement->badge_url, "https://media.retroachievements.org/Badge/00236.png");
  ASSERT_STR_EQUALS(achievement->badge_locked_url, "https://media.retroachievements.org/Badge/00236_lock.png");
  ASSERT_NUM_EQUALS(achievement->created, 1376969412);
  ASSERT_NUM_EQUALS(achievement->updated, 1376969412);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5504);
  ASSERT_STR_EQUALS(achievement->title, "Ach4");
  ASSERT_STR_EQUALS(achievement->description, "Desc4");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 10);
  ASSERT_STR_EQUALS(achievement->definition, "0=4");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00236");
  ASSERT_STR_EQUALS(achievement->badge_url, "https://media.retroachievements.org/Badge/00236.png");
  ASSERT_STR_EQUALS(achievement->badge_locked_url, "https://media.retroachievements.org/Badge/00236_lock.png");
  ASSERT_NUM_EQUALS(achievement->created, 1504474554);
  ASSERT_NUM_EQUALS(achievement->updated, 1504474554);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_leaderboards() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
      "\"ID\":177,\"Title\":\"Another Amazing Game\",\"ConsoleID\":19,\"ImageIcon\":\"/Images/112233.png\","
      "\"Achievements\":[],\"Leaderboards\":["
       "{\"ID\":4401,\"Title\":\"Leaderboard1\",\"Description\":\"Desc1\","
        "\"Mem\":\"0=1\",\"Format\":\"SCORE\"},"
       "{\"ID\":4402,\"Title\":\"Leaderboard2\",\"Description\":\"Desc2\","
        "\"Mem\":\"0=1\",\"Format\":\"SECS\",\"LowerIsBetter\":false,\"Hidden\":true},"
       "{\"ID\":4403,\"Title\":\"Leaderboard3\",\"Description\":\"Desc3\","
        "\"Mem\":\"0=1\",\"Format\":\"UNKNOWN\",\"LowerIsBetter\":true,\"Hidden\":false}"
      "]}}";
  rc_api_leaderboard_definition_t* leaderboard;

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_STR_EQUALS(fetch_game_data_response.title, "Another Amazing Game");
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 19);
  ASSERT_STR_EQUALS(fetch_game_data_response.image_name, "112233");
  ASSERT_STR_EQUALS(fetch_game_data_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 3);

  ASSERT_PTR_NOT_NULL(fetch_game_data_response.leaderboards);
  leaderboard = fetch_game_data_response.leaderboards;

  ASSERT_NUM_EQUALS(leaderboard->id, 4401);
  ASSERT_STR_EQUALS(leaderboard->title, "Leaderboard1");
  ASSERT_STR_EQUALS(leaderboard->description, "Desc1");
  ASSERT_STR_EQUALS(leaderboard->definition, "0=1");
  ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
  ASSERT_NUM_EQUALS(leaderboard->lower_is_better, 0);
  ASSERT_NUM_EQUALS(leaderboard->hidden, 0);

  ++leaderboard;
  ASSERT_NUM_EQUALS(leaderboard->id, 4402);
  ASSERT_STR_EQUALS(leaderboard->title, "Leaderboard2");
  ASSERT_STR_EQUALS(leaderboard->description, "Desc2");
  ASSERT_STR_EQUALS(leaderboard->definition, "0=1");
  ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SECONDS);
  ASSERT_NUM_EQUALS(leaderboard->lower_is_better, 0);
  ASSERT_NUM_EQUALS(leaderboard->hidden, 1);

  ++leaderboard;
  ASSERT_NUM_EQUALS(leaderboard->id, 4403);
  ASSERT_STR_EQUALS(leaderboard->title, "Leaderboard3");
  ASSERT_STR_EQUALS(leaderboard->description, "Desc3");
  ASSERT_STR_EQUALS(leaderboard->definition, "0=1");
  ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_VALUE);
  ASSERT_NUM_EQUALS(leaderboard->lower_is_better, 1);
  ASSERT_NUM_EQUALS(leaderboard->hidden, 0);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_rich_presence() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
      "\"ID\":177,\"Title\":\"Some Other Game\",\"ConsoleID\":2,\"ImageIcon\":\"/Images/000001.png\","
      "\"ImageIconURL\":\"http://host/Images/000001.png\","
      "\"Achievements\":[],\"Leaderboards\":[],"
      "\"RichPresencePatch\":\"Display:\\r\\nTest\\r\\n\""
      "}}";

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_STR_EQUALS(fetch_game_data_response.title, "Some Other Game");
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 2);
  ASSERT_STR_EQUALS(fetch_game_data_response.image_name, "000001");
  ASSERT_STR_EQUALS(fetch_game_data_response.image_url, "http://host/Images/000001.png");
  ASSERT_STR_EQUALS(fetch_game_data_response.rich_presence_script, "Display:\r\nTest\r\n");
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 0);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_rich_presence_null() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
      "\"ID\":177,\"Title\":\"Some Other Game\",\"ConsoleID\":2,\"ImageIcon\":\"/Images/000001.png\","
      "\"Achievements\":[],\"Leaderboards\":[],"
      "\"RichPresencePatch\":null"
      "}}";

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_STR_EQUALS(fetch_game_data_response.title, "Some Other Game");
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 2);
  ASSERT_STR_EQUALS(fetch_game_data_response.image_name, "000001");
  ASSERT_STR_EQUALS(fetch_game_data_response.image_url, "https://media.retroachievements.org/Images/000001.png");
  ASSERT_STR_EQUALS(fetch_game_data_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 0);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void test_process_fetch_game_data_response_rich_presence_tab() {
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const char* server_response = "{\"Success\":true,\"PatchData\":{"
      "\"ID\":177,\"Title\":\"Some Other Game\",\"ConsoleID\":2,\"ImageIcon\":\"/Images/000001.png\","
      "\"Achievements\":[],\"Leaderboards\":[],"
      "\"RichPresencePatch\":\"Display:\\r\\nTest\\tTab\\r\\n\""
      "}}";

  memset(&fetch_game_data_response, 0, sizeof(fetch_game_data_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_data_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_data_response.response.error_message);
  ASSERT_STR_EQUALS(fetch_game_data_response.title, "Some Other Game");
  ASSERT_NUM_EQUALS(fetch_game_data_response.console_id, 2);
  ASSERT_STR_EQUALS(fetch_game_data_response.image_name, "000001");
  ASSERT_STR_EQUALS(fetch_game_data_response.image_url, "https://media.retroachievements.org/Images/000001.png");
  ASSERT_STR_EQUALS(fetch_game_data_response.rich_presence_script, "Display:\r\nTest\tTab\r\n");
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_game_data_response.num_leaderboards, 0);

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

/* ----- fetch game sets ----- */

static void test_init_fetch_game_sets_request() {
  rc_api_fetch_game_sets_request_t fetch_game_sets_request;
  rc_api_request_t request;

  memset(&fetch_game_sets_request, 0, sizeof(fetch_game_sets_request));
  fetch_game_sets_request.username = "Username";
  fetch_game_sets_request.api_token = "API_TOKEN";
  fetch_game_sets_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_fetch_game_sets_request(&request, &fetch_game_sets_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=achievementsets&u=Username&t=API_TOKEN&m=ABCDEF0123456789");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_game_sets_request_no_hash() {
  rc_api_fetch_game_sets_request_t fetch_game_sets_request;
  rc_api_request_t request;

  memset(&fetch_game_sets_request, 0, sizeof(fetch_game_sets_request));
  fetch_game_sets_request.username = "Username";
  fetch_game_sets_request.api_token = "API_TOKEN";

  ASSERT_NUM_EQUALS(rc_api_init_fetch_game_sets_request(&request, &fetch_game_sets_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_game_sets_request_by_id() {
  rc_api_fetch_game_sets_request_t fetch_game_sets_request;
  rc_api_request_t request;

  memset(&fetch_game_sets_request, 0, sizeof(fetch_game_sets_request));
  fetch_game_sets_request.username = "Username";
  fetch_game_sets_request.api_token = "API_TOKEN";
  fetch_game_sets_request.game_id = 953;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_game_sets_request(&request, &fetch_game_sets_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=achievementsets&u=Username&t=API_TOKEN&g=953");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_game_sets_request_by_hash_and_id() {
  rc_api_fetch_game_sets_request_t fetch_game_sets_request;
  rc_api_request_t request;

  memset(&fetch_game_sets_request, 0, sizeof(fetch_game_sets_request));
  fetch_game_sets_request.username = "Username";
  fetch_game_sets_request.api_token = "API_TOKEN";
  fetch_game_sets_request.game_id = 953;
  fetch_game_sets_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_fetch_game_sets_request(&request, &fetch_game_sets_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=achievementsets&u=Username&t=API_TOKEN&g=953");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_process_fetch_game_sets_response_empty() {
  rc_api_achievement_set_definition_t* set;
  rc_api_fetch_game_sets_response_t fetch_game_sets_response;
  const char server_response[] = "{\"Success\":true,"
    "\"GameId\":177,\"Title\":\"My Game\",\"ConsoleId\":23,"
    "\"ImageIconUrl\":\"http://server/Images/012345.png\","
    "\"RichPresenceGameId\":177,\"RichPresencePatch\":\"\",\"Sets\":[{"
        "\"AchievementSetId\":192,\"GameId\":177,\"Title\":null,\"Type\":\"core\","
        "\"ImageIconUrl\":\"http://server/Images/012345.png\","
        "\"Achievements\":[],\"Leaderboards\":[]"
    "}]}";

  rc_api_server_response_t fetch_game_sets_server_response;
  init_server_response(&fetch_game_sets_server_response, 200, server_response, sizeof(server_response) - 1);

  memset(&fetch_game_sets_response, 0, sizeof(fetch_game_sets_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_sets_server_response(&fetch_game_sets_response, &fetch_game_sets_server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_sets_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.id, 177);
  ASSERT_STR_EQUALS(fetch_game_sets_response.title, "My Game");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.console_id, 23);
  ASSERT_STR_EQUALS(fetch_game_sets_response.image_name, "012345");
  ASSERT_STR_EQUALS(fetch_game_sets_response.image_url, "http://server/Images/012345.png");
  ASSERT_STR_EQUALS(fetch_game_sets_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.session_game_id, 177);

  ASSERT_NUM_EQUALS(fetch_game_sets_response.num_sets, 1);
  set = &fetch_game_sets_response.sets[0];
  ASSERT_NUM_EQUALS(set->id, 192);
  ASSERT_NUM_EQUALS(set->game_id, 177);
  ASSERT_STR_EQUALS(set->title, "My Game");
  ASSERT_NUM_EQUALS(set->type, RC_ACHIEVEMENT_SET_TYPE_CORE);
  ASSERT_STR_EQUALS(set->image_name, "012345");
  ASSERT_STR_EQUALS(set->image_url, "http://server/Images/012345.png");
  ASSERT_NUM_EQUALS(set->num_achievements, 0);
  ASSERT_NUM_EQUALS(set->num_leaderboards, 0);

  rc_api_destroy_fetch_game_sets_response(&fetch_game_sets_response);
}

static void test_process_fetch_game_sets_response_invalid_credentials() {
  rc_api_fetch_game_sets_response_t fetch_game_sets_response;
  const char server_response[] = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";
  rc_api_server_response_t fetch_game_sets_server_response;
  init_server_response(&fetch_game_sets_server_response, 403, server_response, sizeof(server_response) - 1);

  memset(&fetch_game_sets_response, 0, sizeof(fetch_game_sets_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_sets_server_response(&fetch_game_sets_response, &fetch_game_sets_server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(fetch_game_sets_response.response.error_message, "Credentials invalid (0)");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.id, 0);
  ASSERT_PTR_NULL(fetch_game_sets_response.title);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.console_id, 0);
  ASSERT_PTR_NULL(fetch_game_sets_response.image_name);
  ASSERT_PTR_NULL(fetch_game_sets_response.rich_presence_script);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.num_sets, 0);

  rc_api_destroy_fetch_game_sets_response(&fetch_game_sets_response);
}

static void test_process_fetch_game_sets_response_not_found() {
  rc_api_fetch_game_sets_response_t fetch_game_sets_response;
  const char server_response[] = "{\"Success\":false,\"Error\":\"Unknown game\",\"Code\":\"not_found\",\"Status\":404}";
  rc_api_server_response_t fetch_game_sets_server_response;
  init_server_response(&fetch_game_sets_server_response, 404, server_response, sizeof(server_response) - 1);

  memset(&fetch_game_sets_response, 0, sizeof(fetch_game_sets_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_sets_server_response(&fetch_game_sets_response, &fetch_game_sets_server_response), RC_NOT_FOUND);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(fetch_game_sets_response.response.error_message, "Unknown game");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.id, 0);
  ASSERT_PTR_NULL(fetch_game_sets_response.title);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.console_id, 0);
  ASSERT_PTR_NULL(fetch_game_sets_response.image_name);
  ASSERT_PTR_NULL(fetch_game_sets_response.rich_presence_script);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.num_sets, 0);

  rc_api_destroy_fetch_game_sets_response(&fetch_game_sets_response);
}

static void test_process_fetch_game_sets_response_achievements() {
  rc_api_achievement_set_definition_t* set;
  rc_api_achievement_definition_t* achievement;
  rc_api_fetch_game_sets_response_t fetch_game_sets_response;
  const char server_response[] = "{\"Success\":true,"
    "\"GameId\":20,\"Title\":\"Another Amazing Game\",\"ConsoleId\":19,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":20,\"RichPresencePatch\":\"\",\"Sets\":[{"
        "\"AchievementSetId\":192,\"GameId\":20,\"Title\":null,\"Type\":\"core\","
        "\"ImageIconUrl\":\"http://server/Images/112233.png\","
        "\"Achievements\":["
            "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
             "\"MemAddr\":\"0=1\",\"Author\":\"User1\",\"BadgeName\":\"00234\",\"Type\":\"\","
             "\"Rarity\":100.0,\"RarityHardcore\":66.67,\"Created\":1367266583,\"Modified\":1376929305},"
            "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
             "\"MemAddr\":\"0=2\",\"Author\":\"User1\",\"BadgeName\":\"00235\",\"Type\":\"missable\","
             "\"Rarity\":57.43,\"RarityHardcore\":57.43,\"Created\":1376970283,\"Modified\":1376970283},"
            "{\"ID\":5503,\"Title\":\"Ach3\",\"Description\":\"Desc3\",\"Flags\":5,\"Points\":0,"
             "\"MemAddr\":\"0=3\",\"Author\":\"User2\",\"BadgeName\":\"00236\",\"Type\":\"progression\","
             "\"Rarity\":6.8,\"RarityHardcore\":0,\"Created\":1376969412,\"Modified\":1376969412},"
            "{\"ID\":5504,\"Title\":\"Ach4\",\"Description\":\"Desc4\",\"Flags\":3,\"Points\":10,"
             "\"MemAddr\":\"0=4\",\"Author\":null,\"BadgeName\":\"00236\",\"Type\":\"win_condition\","
             "\"Created\":1504474554,\"Modified\":1504474554},"
            "{\"ID\":5505,\"Title\":\"Ach5 [m]\",\"Description\":\"Desc5\",\"Flags\":3,\"Points\":10,"
             "\"MemAddr\":\"0=4\",\"Author\":\"User1\",\"BadgeName\":\"00236\",\"Type\":\"\","
             "\"Created\":1504474554,\"Modified\":1504474554},"
            "{\"ID\":5506,\"Title\":\"[m] Ach6\",\"Description\":\"Desc6\",\"Flags\":3,\"Points\":10,"
             "\"MemAddr\":\"0=4\",\"Author\":\"User1\",\"BadgeName\":\"00236\",\"Type\":\"\","
             "\"Created\":1504474554,\"Modified\":1504474554},"
            "{\"ID\":5507,\"Title\":\"Ach7\",\"Description\":\"Desc7\",\"Flags\":3,\"Points\":5,"
             "\"MemAddr\":\"0=1\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
             "\"Created\":1367266583,\"Modified\":1376929305}"
        "],\"Leaderboards\":[]"
    "}]}";
  rc_api_server_response_t fetch_game_sets_server_response;
  init_server_response(&fetch_game_sets_server_response, 200, server_response, sizeof(server_response) - 1);

  memset(&fetch_game_sets_response, 0, sizeof(fetch_game_sets_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_sets_server_response(&fetch_game_sets_response, &fetch_game_sets_server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_sets_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.id, 20);
  ASSERT_STR_EQUALS(fetch_game_sets_response.title, "Another Amazing Game");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.console_id, 19);
  ASSERT_STR_EQUALS(fetch_game_sets_response.image_name, "112233");
  ASSERT_STR_EQUALS(fetch_game_sets_response.image_url, "http://server/Images/112233.png");
  ASSERT_STR_EQUALS(fetch_game_sets_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.session_game_id, 20);

  ASSERT_NUM_EQUALS(fetch_game_sets_response.num_sets, 1);
  set = &fetch_game_sets_response.sets[0];
  ASSERT_NUM_EQUALS(set->id, 192);
  ASSERT_NUM_EQUALS(set->game_id, 20);
  ASSERT_STR_EQUALS(set->title, "Another Amazing Game");
  ASSERT_NUM_EQUALS(set->type, RC_ACHIEVEMENT_SET_TYPE_CORE);
  ASSERT_STR_EQUALS(set->image_name, "112233");
  ASSERT_STR_EQUALS(set->image_url, "http://server/Images/112233.png");
  ASSERT_NUM_EQUALS(set->num_achievements, 7);
  ASSERT_NUM_EQUALS(set->num_leaderboards, 0);

  ASSERT_PTR_NOT_NULL(set->achievements);
  achievement = set->achievements;

  ASSERT_NUM_EQUALS(achievement->id, 5501);
  ASSERT_STR_EQUALS(achievement->title, "Ach1");
  ASSERT_STR_EQUALS(achievement->description, "Desc1");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 5);
  ASSERT_STR_EQUALS(achievement->definition, "0=1");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00234");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_STANDARD);
  ASSERT_FLOAT_EQUALS(achievement->rarity, 100.0f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 66.67f);
  ASSERT_NUM_EQUALS(achievement->created, 1367266583);
  ASSERT_NUM_EQUALS(achievement->updated, 1376929305);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5502);
  ASSERT_STR_EQUALS(achievement->title, "Ach2");
  ASSERT_STR_EQUALS(achievement->description, "Desc2");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 2);
  ASSERT_STR_EQUALS(achievement->definition, "0=2");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00235");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_MISSABLE);
  ASSERT_FLOAT_EQUALS(achievement->rarity, 57.43f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 57.43f);
  ASSERT_NUM_EQUALS(achievement->created, 1376970283);
  ASSERT_NUM_EQUALS(achievement->updated, 1376970283);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5503);
  ASSERT_STR_EQUALS(achievement->title, "Ach3");
  ASSERT_STR_EQUALS(achievement->description, "Desc3");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_UNOFFICIAL);
  ASSERT_NUM_EQUALS(achievement->points, 0);
  ASSERT_STR_EQUALS(achievement->definition, "0=3");
  ASSERT_STR_EQUALS(achievement->author, "User2");
  ASSERT_STR_EQUALS(achievement->badge_name, "00236");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_PROGRESSION);
  ASSERT_FLOAT_EQUALS(achievement->rarity, 6.8f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 0.0f);
  ASSERT_NUM_EQUALS(achievement->created, 1376969412);
  ASSERT_NUM_EQUALS(achievement->updated, 1376969412);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5504);
  ASSERT_STR_EQUALS(achievement->title, "Ach4");
  ASSERT_STR_EQUALS(achievement->description, "Desc4");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 10);
  ASSERT_STR_EQUALS(achievement->definition, "0=4");
  ASSERT_STR_EQUALS(achievement->author, ""); /* null author */
  ASSERT_STR_EQUALS(achievement->badge_name, "00236");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_WIN);
  ASSERT_FLOAT_EQUALS(achievement->rarity, 100.0f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 100.0f);
  ASSERT_NUM_EQUALS(achievement->created, 1504474554);
  ASSERT_NUM_EQUALS(achievement->updated, 1504474554);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5505);
  ASSERT_STR_EQUALS(achievement->title, "Ach5"); /* [m] stripped */
  ASSERT_STR_EQUALS(achievement->description, "Desc5");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 10);
  ASSERT_STR_EQUALS(achievement->definition, "0=4");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00236");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_MISSABLE);
  ASSERT_FLOAT_EQUALS(achievement->rarity, 100.0f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 100.0f);
  ASSERT_NUM_EQUALS(achievement->created, 1504474554);
  ASSERT_NUM_EQUALS(achievement->updated, 1504474554);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5506);
  ASSERT_STR_EQUALS(achievement->title, "Ach6"); /* [m] stripped */
  ASSERT_STR_EQUALS(achievement->description, "Desc6");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 10);
  ASSERT_STR_EQUALS(achievement->definition, "0=4");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00236");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_MISSABLE);
  ASSERT_FLOAT_EQUALS(achievement->rarity, 100.0f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 100.0f);
  ASSERT_NUM_EQUALS(achievement->created, 1504474554);
  ASSERT_NUM_EQUALS(achievement->updated, 1504474554);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5507);
  ASSERT_STR_EQUALS(achievement->title, "Ach7");
  ASSERT_STR_EQUALS(achievement->description, "Desc7");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 5);
  ASSERT_STR_EQUALS(achievement->definition, "0=1");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00234");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_STANDARD); /* no type specified */
  ASSERT_FLOAT_EQUALS(achievement->rarity, 100.0f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 100.0f);
  ASSERT_NUM_EQUALS(achievement->created, 1367266583);
  ASSERT_NUM_EQUALS(achievement->updated, 1376929305);

  rc_api_destroy_fetch_game_sets_response(&fetch_game_sets_response);
}

static void test_process_fetch_game_sets_response_leaderboards() {
  rc_api_achievement_set_definition_t* set;
  rc_api_leaderboard_definition_t* leaderboard;
  rc_api_fetch_game_sets_response_t fetch_game_sets_response;
  const char server_response[] = "{\"Success\":true,"
    "\"GameId\":20,\"Title\":\"Another Amazing Game\",\"ConsoleId\":19,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":20,\"RichPresencePatch\":\"\",\"Sets\":[{"
        "\"AchievementSetId\":192,\"GameId\":20,\"Title\":null,\"Type\":\"core\","
        "\"ImageIconUrl\":\"http://server/Images/112233.png\","
        "\"Achievements\":[],\"Leaderboards\":["
            "{\"ID\":4401,\"Title\":\"Leaderboard1\",\"Description\":\"Desc1\","
             "\"Mem\":\"0=1\",\"Format\":\"SCORE\"},"
            "{\"ID\":4402,\"Title\":\"Leaderboard2\",\"Description\":\"Desc2\","
             "\"Mem\":\"0=1\",\"Format\":\"SECS\",\"LowerIsBetter\":false,\"Hidden\":true},"
            "{\"ID\":4403,\"Title\":\"Leaderboard3\",\"Description\":\"Desc3\","
             "\"Mem\":\"0=1\",\"Format\":\"UNKNOWN\",\"LowerIsBetter\":true,\"Hidden\":false}"
        "]"
    "}]}";

  rc_api_server_response_t fetch_game_sets_server_response;
  init_server_response(&fetch_game_sets_server_response, 200, server_response, sizeof(server_response) - 1);

  memset(&fetch_game_sets_response, 0, sizeof(fetch_game_sets_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_sets_server_response(&fetch_game_sets_response, &fetch_game_sets_server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_sets_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.id, 20);
  ASSERT_STR_EQUALS(fetch_game_sets_response.title, "Another Amazing Game");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.console_id, 19);
  ASSERT_STR_EQUALS(fetch_game_sets_response.image_name, "112233");
  ASSERT_STR_EQUALS(fetch_game_sets_response.image_url, "http://server/Images/112233.png");
  ASSERT_STR_EQUALS(fetch_game_sets_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.session_game_id, 20);

  ASSERT_NUM_EQUALS(fetch_game_sets_response.num_sets, 1);
  set = &fetch_game_sets_response.sets[0];
  ASSERT_NUM_EQUALS(set->id, 192);
  ASSERT_NUM_EQUALS(set->game_id, 20);
  ASSERT_STR_EQUALS(set->title, "Another Amazing Game");
  ASSERT_NUM_EQUALS(set->type, RC_ACHIEVEMENT_SET_TYPE_CORE);
  ASSERT_STR_EQUALS(set->image_name, "112233");
  ASSERT_STR_EQUALS(set->image_url, "http://server/Images/112233.png");
  ASSERT_NUM_EQUALS(set->num_achievements, 0);
  ASSERT_NUM_EQUALS(set->num_leaderboards, 3);

  ASSERT_PTR_NOT_NULL(set->leaderboards);
  leaderboard = set->leaderboards;

  ASSERT_NUM_EQUALS(leaderboard->id, 4401);
  ASSERT_STR_EQUALS(leaderboard->title, "Leaderboard1");
  ASSERT_STR_EQUALS(leaderboard->description, "Desc1");
  ASSERT_STR_EQUALS(leaderboard->definition, "0=1");
  ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
  ASSERT_NUM_EQUALS(leaderboard->lower_is_better, 0);
  ASSERT_NUM_EQUALS(leaderboard->hidden, 0);

  ++leaderboard;
  ASSERT_NUM_EQUALS(leaderboard->id, 4402);
  ASSERT_STR_EQUALS(leaderboard->title, "Leaderboard2");
  ASSERT_STR_EQUALS(leaderboard->description, "Desc2");
  ASSERT_STR_EQUALS(leaderboard->definition, "0=1");
  ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SECONDS);
  ASSERT_NUM_EQUALS(leaderboard->lower_is_better, 0);
  ASSERT_NUM_EQUALS(leaderboard->hidden, 1);

  ++leaderboard;
  ASSERT_NUM_EQUALS(leaderboard->id, 4403);
  ASSERT_STR_EQUALS(leaderboard->title, "Leaderboard3");
  ASSERT_STR_EQUALS(leaderboard->description, "Desc3");
  ASSERT_STR_EQUALS(leaderboard->definition, "0=1");
  ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_VALUE);
  ASSERT_NUM_EQUALS(leaderboard->lower_is_better, 1);
  ASSERT_NUM_EQUALS(leaderboard->hidden, 0);

  rc_api_destroy_fetch_game_sets_response(&fetch_game_sets_response);
}

static void test_process_fetch_game_sets_response_rich_presence() {
  rc_api_achievement_set_definition_t* set;
  rc_api_fetch_game_sets_response_t fetch_game_sets_response;
  const char server_response[] = "{\"Success\":true,"
    "\"GameId\":99,\"Title\":\"Some Other Game\",\"ConsoleId\":2,"
    "\"ImageIconUrl\":\"http://server/Images/000001.png\","
    "\"RichPresenceGameId\":99,\"RichPresencePatch\":\"Display:\\r\\nTest\\r\\n\",\"Sets\":[{"
        "\"AchievementSetId\":106,\"GameId\":99,\"Title\":null,\"Type\":\"core\","
        "\"ImageIconUrl\":\"http://server/Images/000001.png\","
        "\"Achievements\":[],\"Leaderboards\":[]"
    "}]}";

  memset(&fetch_game_sets_response, 0, sizeof(fetch_game_sets_response));
  rc_api_server_response_t fetch_game_sets_server_response;
  init_server_response(&fetch_game_sets_server_response, 200, server_response, sizeof(server_response) - 1);

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_sets_server_response(&fetch_game_sets_response, &fetch_game_sets_server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_sets_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.id, 99);
  ASSERT_STR_EQUALS(fetch_game_sets_response.title, "Some Other Game");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.console_id, 2);
  ASSERT_STR_EQUALS(fetch_game_sets_response.image_name, "000001");
  ASSERT_STR_EQUALS(fetch_game_sets_response.image_url, "http://server/Images/000001.png");
  ASSERT_STR_EQUALS(fetch_game_sets_response.rich_presence_script, "Display:\r\nTest\r\n");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.session_game_id, 99);

  ASSERT_NUM_EQUALS(fetch_game_sets_response.num_sets, 1);
  set = &fetch_game_sets_response.sets[0];
  ASSERT_NUM_EQUALS(set->id, 106);
  ASSERT_NUM_EQUALS(set->game_id, 99);
  ASSERT_STR_EQUALS(set->title, "Some Other Game");
  ASSERT_NUM_EQUALS(set->type, RC_ACHIEVEMENT_SET_TYPE_CORE);
  ASSERT_STR_EQUALS(set->image_name, "000001");
  ASSERT_STR_EQUALS(set->image_url, "http://server/Images/000001.png");
  ASSERT_NUM_EQUALS(set->num_achievements, 0);
  ASSERT_NUM_EQUALS(set->num_leaderboards, 0);

  rc_api_destroy_fetch_game_sets_response(&fetch_game_sets_response);
}

static void test_process_fetch_game_sets_response_rich_presence_null() {
  rc_api_achievement_set_definition_t* set;
  rc_api_fetch_game_sets_response_t fetch_game_sets_response;
  const char server_response[] = "{\"Success\":true,"
    "\"GameId\":99,\"Title\":\"Some Other Game\",\"ConsoleId\":2,"
    "\"ImageIconUrl\":\"http://server/Images/000001.png\","
    "\"RichPresenceGameId\":99,\"RichPresencePatch\":null,\"Sets\":[{"
        "\"AchievementSetId\":106,\"GameId\":99,\"Title\":null,\"Type\":\"core\","
        "\"ImageIconUrl\":\"http://server/Images/000001.png\","
        "\"Achievements\":[],\"Leaderboards\":[]"
    "}]}";

  memset(&fetch_game_sets_response, 0, sizeof(fetch_game_sets_response));
  rc_api_server_response_t fetch_game_sets_server_response;
  init_server_response(&fetch_game_sets_server_response, 200, server_response, sizeof(server_response) - 1);

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_sets_server_response(&fetch_game_sets_response, &fetch_game_sets_server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_sets_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.id, 99);
  ASSERT_STR_EQUALS(fetch_game_sets_response.title, "Some Other Game");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.console_id, 2);
  ASSERT_STR_EQUALS(fetch_game_sets_response.image_name, "000001");
  ASSERT_STR_EQUALS(fetch_game_sets_response.image_url, "http://server/Images/000001.png");
  ASSERT_STR_EQUALS(fetch_game_sets_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.session_game_id, 99);

  ASSERT_NUM_EQUALS(fetch_game_sets_response.num_sets, 1);
  set = &fetch_game_sets_response.sets[0];
  ASSERT_NUM_EQUALS(set->id, 106);
  ASSERT_NUM_EQUALS(set->game_id, 99);
  ASSERT_STR_EQUALS(set->title, "Some Other Game");
  ASSERT_NUM_EQUALS(set->type, RC_ACHIEVEMENT_SET_TYPE_CORE);
  ASSERT_STR_EQUALS(set->image_name, "000001");
  ASSERT_STR_EQUALS(set->image_url, "http://server/Images/000001.png");
  ASSERT_NUM_EQUALS(set->num_achievements, 0);
  ASSERT_NUM_EQUALS(set->num_leaderboards, 0);

  rc_api_destroy_fetch_game_sets_response(&fetch_game_sets_response);
}

static void test_process_fetch_game_sets_response_specialty_subset() {
  rc_api_achievement_set_definition_t* set;
  rc_api_achievement_definition_t* achievement;
  rc_api_leaderboard_definition_t* leaderboard;
  rc_api_fetch_game_sets_response_t fetch_game_sets_response;
  const char server_response[] = "{\"Success\":true,"
    "\"GameId\":20,\"Title\":\"Another Amazing Game\",\"ConsoleId\":19,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":20,\"RichPresencePatch\":\"\",\"Sets\":["
      "{"
        "\"AchievementSetId\":98,\"GameId\":26,\"Title\":\"Low Level Run\",\"Type\":\"specialty\","
        "\"ImageIconUrl\":\"http://server/Images/112236.png\","
        "\"Achievements\":["
            "{\"ID\":5507,\"Title\":\"Ach7\",\"Description\":\"Desc7\",\"Flags\":3,\"Points\":5,"
             "\"MemAddr\":\"0=1\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
             "\"Created\":1367266583,\"Modified\":1376929305}"
        "],\"Leaderboards\":[]"
      "},{"
        "\"AchievementSetId\":192,\"GameId\":20,\"Title\":null,\"Type\":\"core\","
        "\"ImageIconUrl\":\"http://server/Images/112233.png\","
        "\"Achievements\":["
            "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
             "\"MemAddr\":\"0=1\",\"Author\":\"User1\",\"BadgeName\":\"00234\",\"Type\":\"progression\","
             "\"Rarity\":100.0,\"RarityHardcore\":66.67,\"Created\":1367266583,\"Modified\":1376929305},"
            "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
             "\"MemAddr\":\"0=2\",\"Author\":\"User1\",\"BadgeName\":\"00235\",\"Type\":\"missable\","
             "\"Rarity\":57.43,\"RarityHardcore\":57.43,\"Created\":1376970283,\"Modified\":1376970283},"
            "{\"ID\":5503,\"Title\":\"Ach3\",\"Description\":\"Desc3\",\"Flags\":5,\"Points\":0,"
             "\"MemAddr\":\"0=3\",\"Author\":\"User2\",\"BadgeName\":\"00236\",\"Type\":\"win_condition\","
             "\"Rarity\":6.8,\"RarityHardcore\":0,\"Created\":1376969412,\"Modified\":1376969412}"
        "],\"Leaderboards\":["
            "{\"ID\":4401,\"Title\":\"Leaderboard1\",\"Description\":\"Desc1\","
             "\"Mem\":\"0=1\",\"Format\":\"SCORE\"}"
        "]"
      "},{"
        "\"AchievementSetId\":77,\"GameId\":21,\"Title\":\"Bonus\",\"Type\":\"bonus\","
        "\"ImageIconUrl\":\"http://server/Images/112236.png\","
        "\"Achievements\":["
            "{\"ID\":5504,\"Title\":\"Ach4\",\"Description\":\"Desc4\",\"Flags\":3,\"Points\":10,"
             "\"MemAddr\":\"0=4\",\"Author\":null,\"BadgeName\":\"00236\",\"Type\":\"\","
             "\"Created\":1504474554,\"Modified\":1504474554},"
            "{\"ID\":5505,\"Title\":\"Ach5 [m]\",\"Description\":\"Desc5\",\"Flags\":3,\"Points\":10,"
             "\"MemAddr\":\"0=4\",\"Author\":\"User1\",\"BadgeName\":\"00236\",\"Type\":\"\","
             "\"Created\":1504474554,\"Modified\":1504474554},"
            "{\"ID\":5506,\"Title\":\"[m] Ach6\",\"Description\":\"Desc6\",\"Flags\":3,\"Points\":10,"
             "\"MemAddr\":\"0=4\",\"Author\":\"User1\",\"BadgeName\":\"00236\",\"Type\":\"\","
             "\"Created\":1504474554,\"Modified\":1504474554}"
        "],\"Leaderboards\":["
            "{\"ID\":4402,\"Title\":\"Leaderboard2\",\"Description\":\"Desc2\","
             "\"Mem\":\"0=1\",\"Format\":\"SECS\",\"LowerIsBetter\":false,\"Hidden\":true}"
        "]"
      "}"
    "]}";
  rc_api_server_response_t fetch_game_sets_server_response;
  init_server_response(&fetch_game_sets_server_response, 200, server_response, sizeof(server_response) - 1);

  memset(&fetch_game_sets_response, 0, sizeof(fetch_game_sets_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_sets_server_response(&fetch_game_sets_response, &fetch_game_sets_server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_sets_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.id, 20);
  ASSERT_STR_EQUALS(fetch_game_sets_response.title, "Another Amazing Game");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.console_id, 19);
  ASSERT_STR_EQUALS(fetch_game_sets_response.image_name, "112233");
  ASSERT_STR_EQUALS(fetch_game_sets_response.image_url, "http://server/Images/112233.png");
  ASSERT_STR_EQUALS(fetch_game_sets_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.session_game_id, 20);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.num_sets, 3);

  set = &fetch_game_sets_response.sets[0];
  ASSERT_NUM_EQUALS(set->id, 98);
  ASSERT_NUM_EQUALS(set->game_id, 26);
  ASSERT_STR_EQUALS(set->title, "Low Level Run");
  ASSERT_NUM_EQUALS(set->type, RC_ACHIEVEMENT_SET_TYPE_SPECIALTY);
  ASSERT_STR_EQUALS(set->image_name, "112236");
  ASSERT_STR_EQUALS(set->image_url, "http://server/Images/112236.png");
  ASSERT_NUM_EQUALS(set->num_achievements, 1);
  ASSERT_NUM_EQUALS(set->num_leaderboards, 0);

  ASSERT_PTR_NOT_NULL(set->achievements);
  achievement = set->achievements;

  ASSERT_NUM_EQUALS(achievement->id, 5507);
  ASSERT_STR_EQUALS(achievement->title, "Ach7");
  ASSERT_STR_EQUALS(achievement->description, "Desc7");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 5);
  ASSERT_STR_EQUALS(achievement->definition, "0=1");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00234");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_STANDARD); /* no type specified */
  ASSERT_NUM_EQUALS(achievement->created, 1367266583);
  ASSERT_NUM_EQUALS(achievement->updated, 1376929305);

  set = &fetch_game_sets_response.sets[1];
  ASSERT_NUM_EQUALS(set->id, 192);
  ASSERT_NUM_EQUALS(set->game_id, 20);
  ASSERT_STR_EQUALS(set->title, "Another Amazing Game");
  ASSERT_NUM_EQUALS(set->type, RC_ACHIEVEMENT_SET_TYPE_CORE);
  ASSERT_STR_EQUALS(set->image_name, "112233");
  ASSERT_STR_EQUALS(set->image_url, "http://server/Images/112233.png");
  ASSERT_NUM_EQUALS(set->num_achievements, 3);
  ASSERT_NUM_EQUALS(set->num_leaderboards, 1);

  ASSERT_PTR_NOT_NULL(set->achievements);
  achievement = set->achievements;
  ASSERT_NUM_EQUALS(achievement->id, 5501);
  ASSERT_STR_EQUALS(achievement->title, "Ach1");
  ASSERT_STR_EQUALS(achievement->description, "Desc1");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 5);
  ASSERT_STR_EQUALS(achievement->definition, "0=1");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00234");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_PROGRESSION);
  ASSERT_FLOAT_EQUALS(achievement->rarity, 100.0f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 66.67f);
  ASSERT_NUM_EQUALS(achievement->created, 1367266583);
  ASSERT_NUM_EQUALS(achievement->updated, 1376929305);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5502);
  ASSERT_STR_EQUALS(achievement->title, "Ach2");
  ASSERT_STR_EQUALS(achievement->description, "Desc2");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 2);
  ASSERT_STR_EQUALS(achievement->definition, "0=2");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00235");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_MISSABLE);
  ASSERT_FLOAT_EQUALS(achievement->rarity, 57.43f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 57.43f);
  ASSERT_NUM_EQUALS(achievement->created, 1376970283);
  ASSERT_NUM_EQUALS(achievement->updated, 1376970283);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5503);
  ASSERT_STR_EQUALS(achievement->title, "Ach3");
  ASSERT_STR_EQUALS(achievement->description, "Desc3");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_UNOFFICIAL);
  ASSERT_NUM_EQUALS(achievement->points, 0);
  ASSERT_STR_EQUALS(achievement->definition, "0=3");
  ASSERT_STR_EQUALS(achievement->author, "User2");
  ASSERT_STR_EQUALS(achievement->badge_name, "00236");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_WIN);
  ASSERT_FLOAT_EQUALS(achievement->rarity, 6.8f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 0.0f);
  ASSERT_NUM_EQUALS(achievement->created, 1376969412);
  ASSERT_NUM_EQUALS(achievement->updated, 1376969412);

  ASSERT_PTR_NOT_NULL(set->leaderboards);
  leaderboard = set->leaderboards;

  ASSERT_NUM_EQUALS(leaderboard->id, 4401);
  ASSERT_STR_EQUALS(leaderboard->title, "Leaderboard1");
  ASSERT_STR_EQUALS(leaderboard->description, "Desc1");
  ASSERT_STR_EQUALS(leaderboard->definition, "0=1");
  ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
  ASSERT_NUM_EQUALS(leaderboard->lower_is_better, 0);
  ASSERT_NUM_EQUALS(leaderboard->hidden, 0);

  set = &fetch_game_sets_response.sets[2];
  ASSERT_NUM_EQUALS(set->id, 77);
  ASSERT_NUM_EQUALS(set->game_id, 21);
  ASSERT_STR_EQUALS(set->title, "Bonus");
  ASSERT_NUM_EQUALS(set->type, RC_ACHIEVEMENT_SET_TYPE_BONUS);
  ASSERT_STR_EQUALS(set->image_name, "112236");
  ASSERT_STR_EQUALS(set->image_url, "http://server/Images/112236.png");
  ASSERT_NUM_EQUALS(set->num_achievements, 3);
  ASSERT_NUM_EQUALS(set->num_leaderboards, 1);

  ASSERT_PTR_NOT_NULL(set->achievements);
  achievement = set->achievements;
  ASSERT_NUM_EQUALS(achievement->id, 5504);
  ASSERT_STR_EQUALS(achievement->title, "Ach4");
  ASSERT_STR_EQUALS(achievement->description, "Desc4");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 10);
  ASSERT_STR_EQUALS(achievement->definition, "0=4");
  ASSERT_STR_EQUALS(achievement->author, ""); /* null author */
  ASSERT_STR_EQUALS(achievement->badge_name, "00236");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_STANDARD);
  ASSERT_FLOAT_EQUALS(achievement->rarity, 100.0f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 100.0f);
  ASSERT_NUM_EQUALS(achievement->created, 1504474554);
  ASSERT_NUM_EQUALS(achievement->updated, 1504474554);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5505);
  ASSERT_STR_EQUALS(achievement->title, "Ach5"); /* [m] stripped */
  ASSERT_STR_EQUALS(achievement->description, "Desc5");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 10);
  ASSERT_STR_EQUALS(achievement->definition, "0=4");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00236");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_MISSABLE);
  ASSERT_FLOAT_EQUALS(achievement->rarity, 100.0f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 100.0f);
  ASSERT_NUM_EQUALS(achievement->created, 1504474554);
  ASSERT_NUM_EQUALS(achievement->updated, 1504474554);

  ++achievement;
  ASSERT_NUM_EQUALS(achievement->id, 5506);
  ASSERT_STR_EQUALS(achievement->title, "Ach6"); /* [m] stripped */
  ASSERT_STR_EQUALS(achievement->description, "Desc6");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 10);
  ASSERT_STR_EQUALS(achievement->definition, "0=4");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00236");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_MISSABLE);
  ASSERT_FLOAT_EQUALS(achievement->rarity, 100.0f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 100.0f);
  ASSERT_NUM_EQUALS(achievement->created, 1504474554);
  ASSERT_NUM_EQUALS(achievement->updated, 1504474554);

  ASSERT_PTR_NOT_NULL(set->leaderboards);
  leaderboard = set->leaderboards;

  ASSERT_NUM_EQUALS(leaderboard->id, 4402);
  ASSERT_STR_EQUALS(leaderboard->title, "Leaderboard2");
  ASSERT_STR_EQUALS(leaderboard->description, "Desc2");
  ASSERT_STR_EQUALS(leaderboard->definition, "0=1");
  ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SECONDS);
  ASSERT_NUM_EQUALS(leaderboard->lower_is_better, 0);
  ASSERT_NUM_EQUALS(leaderboard->hidden, 1);

  rc_api_destroy_fetch_game_sets_response(&fetch_game_sets_response);
}

static void test_process_fetch_game_sets_response_exclusive_subset() {
  rc_api_achievement_set_definition_t* set;
  rc_api_achievement_definition_t* achievement;
  rc_api_fetch_game_sets_response_t fetch_game_sets_response;
  const char server_response[] = "{\"Success\":true,"
    "\"GameId\":20,\"Title\":\"Another Amazing Game\",\"ConsoleId\":19,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":26,\"RichPresencePatch\":\"\",\"Sets\":[{"
        "\"AchievementSetId\":98,\"GameId\":26,\"Title\":\"Low Level Run\",\"Type\":\"exclusive\","
        "\"ImageIconUrl\":\"http://server/Images/112236.png\","
        "\"Achievements\":["
            "{\"ID\":5507,\"Title\":\"Ach7\",\"Description\":\"Desc7\",\"Flags\":3,\"Points\":5,"
             "\"MemAddr\":\"0=1\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
             "\"Created\":1367266583,\"Modified\":1376929305}"
        "],\"Leaderboards\":[]"
    "}]}";
  rc_api_server_response_t fetch_game_sets_server_response;
  init_server_response(&fetch_game_sets_server_response, 200, server_response, sizeof(server_response) - 1);

  memset(&fetch_game_sets_response, 0, sizeof(fetch_game_sets_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_game_sets_server_response(&fetch_game_sets_response, &fetch_game_sets_server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_game_sets_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.id, 20);
  ASSERT_STR_EQUALS(fetch_game_sets_response.title, "Another Amazing Game");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.console_id, 19);
  ASSERT_STR_EQUALS(fetch_game_sets_response.image_name, "112233");
  ASSERT_STR_EQUALS(fetch_game_sets_response.image_url, "http://server/Images/112233.png");
  ASSERT_STR_EQUALS(fetch_game_sets_response.rich_presence_script, "");
  ASSERT_NUM_EQUALS(fetch_game_sets_response.session_game_id, 26);
  ASSERT_NUM_EQUALS(fetch_game_sets_response.num_sets, 1);

  set = &fetch_game_sets_response.sets[0];
  ASSERT_NUM_EQUALS(set->id, 98);
  ASSERT_NUM_EQUALS(set->game_id, 26);
  ASSERT_STR_EQUALS(set->title, "Low Level Run");
  ASSERT_NUM_EQUALS(set->type, RC_ACHIEVEMENT_SET_TYPE_EXCLUSIVE);
  ASSERT_STR_EQUALS(set->image_name, "112236");
  ASSERT_STR_EQUALS(set->image_url, "http://server/Images/112236.png");
  ASSERT_NUM_EQUALS(set->num_achievements, 1);
  ASSERT_NUM_EQUALS(set->num_leaderboards, 0);

  ASSERT_PTR_NOT_NULL(set->achievements);
  achievement = set->achievements;

  ASSERT_NUM_EQUALS(achievement->id, 5507);
  ASSERT_STR_EQUALS(achievement->title, "Ach7");
  ASSERT_STR_EQUALS(achievement->description, "Desc7");
  ASSERT_NUM_EQUALS(achievement->category, RC_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->points, 5);
  ASSERT_STR_EQUALS(achievement->definition, "0=1");
  ASSERT_STR_EQUALS(achievement->author, "User1");
  ASSERT_STR_EQUALS(achievement->badge_name, "00234");
  ASSERT_NUM_EQUALS(achievement->type, RC_ACHIEVEMENT_TYPE_STANDARD); /* no type specified */
  ASSERT_NUM_EQUALS(achievement->created, 1367266583);
  ASSERT_NUM_EQUALS(achievement->updated, 1376929305);

  rc_api_destroy_fetch_game_sets_response(&fetch_game_sets_response);
}

/* ----- ping ----- */

static void test_init_ping_request() {
  rc_api_ping_request_t ping_request;
  rc_api_request_t request;

  memset(&ping_request, 0, sizeof(ping_request));
  ping_request.username = "Username";
  ping_request.api_token = "API_TOKEN";
  ping_request.game_id = 1234;

  ASSERT_NUM_EQUALS(rc_api_init_ping_request(&request, &ping_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=ping&u=Username&t=API_TOKEN&g=1234");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_ping_request_no_game_id() {
  rc_api_ping_request_t ping_request;
  rc_api_request_t request;

  memset(&ping_request, 0, sizeof(ping_request));
  ping_request.username = "Username";
  ping_request.api_token = "API_TOKEN";

  ASSERT_NUM_EQUALS(rc_api_init_ping_request(&request, &ping_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_ping_request_rich_presence() {
  rc_api_ping_request_t ping_request;
  rc_api_request_t request;

  memset(&ping_request, 0, sizeof(ping_request));
  ping_request.username = "Username";
  ping_request.api_token = "API_TOKEN";
  ping_request.game_id = 1234;
  ping_request.rich_presence = "Level 1, 70% complete";

  ASSERT_NUM_EQUALS(rc_api_init_ping_request(&request, &ping_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=ping&u=Username&t=API_TOKEN&g=1234&m=Level+1%2c+70%25+complete");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_ping_request_rich_presence_unicode() {
  rc_api_ping_request_t ping_request;
  rc_api_request_t request;

  memset(&ping_request, 0, sizeof(ping_request));
  ping_request.username = "Username";
  ping_request.api_token = "API_TOKEN";
  ping_request.game_id = 1446;
  ping_request.rich_presence = "\xf0\x9f\x9a\xb6:3, 1st Quest";

  ASSERT_NUM_EQUALS(rc_api_init_ping_request(&request, &ping_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=ping&u=Username&t=API_TOKEN&g=1446&m=%f0%9f%9a%b6%3a3%2c+1st+Quest");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_ping_request_rich_presence_empty() {
  rc_api_ping_request_t ping_request;
  rc_api_request_t request;

  memset(&ping_request, 0, sizeof(ping_request));
  ping_request.username = "Username";
  ping_request.api_token = "API_TOKEN";
  ping_request.game_id = 1234;
  ping_request.rich_presence = "";

  ASSERT_NUM_EQUALS(rc_api_init_ping_request(&request, &ping_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=ping&u=Username&t=API_TOKEN&g=1234");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_ping_request_game_hash_softcore() {
  rc_api_ping_request_t ping_request;
  rc_api_request_t request;

  memset(&ping_request, 0, sizeof(ping_request));
  ping_request.username = "Username";
  ping_request.api_token = "API_TOKEN";
  ping_request.game_id = 1234;
  ping_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_ping_request(&request, &ping_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=ping&u=Username&t=API_TOKEN&g=1234&h=0&x=ABCDEF0123456789");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_ping_request_game_hash_hardcore() {
  rc_api_ping_request_t ping_request;
  rc_api_request_t request;

  memset(&ping_request, 0, sizeof(ping_request));
  ping_request.username = "Username";
  ping_request.api_token = "API_TOKEN";
  ping_request.game_id = 1234;
  ping_request.hardcore = 1;
  ping_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_ping_request(&request, &ping_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=ping&u=Username&t=API_TOKEN&g=1234&h=1&x=ABCDEF0123456789");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_process_ping_response() {
  rc_api_ping_response_t ping_response;
  const char* server_response = "{\"Success\":true}";

  memset(&ping_response, 0, sizeof(ping_response));

  ASSERT_NUM_EQUALS(rc_api_process_ping_response(&ping_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(ping_response.response.succeeded, 1);
  ASSERT_PTR_NULL(ping_response.response.error_message);

  rc_api_destroy_ping_response(&ping_response);
}

/* ----- award achievement ----- */

static void test_init_award_achievement_request_hardcore() {
  rc_api_award_achievement_request_t award_achievement_request;
  rc_api_request_t request;

  memset(&award_achievement_request, 0, sizeof(award_achievement_request));
  award_achievement_request.username = "Username";
  award_achievement_request.api_token = "API_TOKEN";
  award_achievement_request.achievement_id = 1234;
  award_achievement_request.hardcore = 1;
  award_achievement_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_award_achievement_request(&request, &award_achievement_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=awardachievement&u=Username&t=API_TOKEN&a=1234&h=1&m=ABCDEF0123456789&v=b8aefaad6f9659e2164bc60da0c3b64d");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_award_achievement_request_non_hardcore() {
  rc_api_award_achievement_request_t award_achievement_request;
  rc_api_request_t request;

  memset(&award_achievement_request, 0, sizeof(award_achievement_request));
  award_achievement_request.username = "Username";
  award_achievement_request.api_token = "API_TOKEN";
  award_achievement_request.achievement_id = 1234;
  award_achievement_request.hardcore = 0;
  award_achievement_request.game_hash = "ABABCBCBDEDEFFFF";

  ASSERT_NUM_EQUALS(rc_api_init_award_achievement_request(&request, &award_achievement_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=awardachievement&u=Username&t=API_TOKEN&a=1234&h=0&m=ABABCBCBDEDEFFFF&v=ed81d6ecf825f8cbe3ae1edace098892");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_award_achievement_request_no_hash() {
  rc_api_award_achievement_request_t award_achievement_request;
  rc_api_request_t request;

  memset(&award_achievement_request, 0, sizeof(award_achievement_request));
  award_achievement_request.username = "Username";
  award_achievement_request.api_token = "API_TOKEN";
  award_achievement_request.achievement_id = 5432;
  award_achievement_request.hardcore = 1;

  ASSERT_NUM_EQUALS(rc_api_init_award_achievement_request(&request, &award_achievement_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=awardachievement&u=Username&t=API_TOKEN&a=5432&h=1&v=31048257ab1788386e71ab0c222aa5c8");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_award_achievement_request_no_achievement_id() {
  rc_api_award_achievement_request_t award_achievement_request;
  rc_api_request_t request;

  memset(&award_achievement_request, 0, sizeof(award_achievement_request));
  award_achievement_request.username = "Username";
  award_achievement_request.api_token = "API_TOKEN";
  award_achievement_request.hardcore = 1;
  award_achievement_request.game_hash = "ABABCBCBDEDEFFFF";

  ASSERT_NUM_EQUALS(rc_api_init_award_achievement_request(&request, &award_achievement_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_award_achievement_request_delayed() {
  rc_api_award_achievement_request_t award_achievement_request;
  rc_api_request_t request;

  memset(&award_achievement_request, 0, sizeof(award_achievement_request));
  award_achievement_request.username = "Username";
  award_achievement_request.api_token = "API_TOKEN";
  award_achievement_request.achievement_id = 1234;
  award_achievement_request.hardcore = 1;
  award_achievement_request.game_hash = "ABCDEF0123456789";
  award_achievement_request.seconds_since_unlock = 17;

  ASSERT_NUM_EQUALS(rc_api_init_award_achievement_request(&request, &award_achievement_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=awardachievement&u=Username&t=API_TOKEN&a=1234&h=1&m=ABCDEF0123456789&o=17&v=b2326b09d61e9264eb5d3607d947317d");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_process_award_achievement_response_success() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":true,\"Score\":119102,\"SoftcoreScore\":777,\"AchievementID\":56481,\"AchievementsRemaining\":11}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 1);
  ASSERT_PTR_NULL(award_achievement_response.response.error_message);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 119102);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score_softcore, 777);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 56481);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 11);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_hardcore_already_unlocked() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":119210,\"SoftcoreScore\":777,\"AchievementID\":56494,\"AchievementsRemaining\":17}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 1);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "User already has hardcore and regular achievements awarded.");
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 119210);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score_softcore, 777);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 56494);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 17);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_non_hardcore_already_unlocked() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"User already has this achievement awarded.\",\"Score\":119210,\"SoftcoreScore\":777,\"AchievementID\":56494}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 1);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "User already has this achievement awarded.");
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 119210);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score_softcore, 777);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 56494);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0xFFFFFFFF);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_generic_failure() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":false}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_PTR_NULL(award_achievement_response.response.error_message);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score_softcore, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_empty() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_NO_RESPONSE);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_PTR_NULL(award_achievement_response.response.error_message);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score_softcore, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_invalid_credentials() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "Credentials invalid (0)");
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score_softcore, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_text() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "You do not have access to that resource";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "You do not have access to that resource");
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score_softcore, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_no_fields() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":true}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 1);
  ASSERT_PTR_NULL(award_achievement_response.response.error_message);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score_softcore, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0xFFFFFFFF);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_429() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response =
      "<html>\n"
      "<head><title>429 Too Many Requests</title></head>\n"
      "<body>\n"
      "<center><h1>429 Too Many Requests</h1></center>\n"
      "<hr><center>nginx</center>\n"
      "</body>\n"
      "</html>";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "429 Too Many Requests");
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score_softcore, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_429_json() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response =
      "{\"Success\": false,\"Error\":\"Too Many Attempts\"}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "Too Many Attempts");
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_503_fancy() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "  <meta charset='utf-8'>\n"
    "  <meta name='viewport' content='width=device-width, initial-scale=1'>\n"
    "  <style>\n"
    "  body {\n"
    "    background-color: #1a1a1a;\n"
    "    color: white; width: 100%;\n"
    "  }\n"
    "  a {\n"
    "    color: #cc9900;\n"
    "    text-decoration: none;\n"
    "  }\n"
    "  .center {\n"
    "    margin: 0;\n"
    "    padding: 0;\n"
    "    text - align: center;\n"
    "    position: absolute;\n"
    "    top: 50%;\n"
    "    left: 50%;\n"
    "    transform: translateX(-50%) translateY(-50%);\n"
    "  }\n"
    "  </style>\n"
    "  <title>503 Service Temporarily Unavailable</title>\n"
    "</head>\n"
    "<body style='background-color: #1a1a1a; color:white; width:100%;'>\n"
    "<div class='center'>\n"
    "  <img style='text-align: center; width: 75%;' src = 'https://static.retroachievements.org/assets/images/ra-logo-sm.webp'>\n"
    "  <h1>503</h1>\n"
    "  <h2>Service Temporarily Unavailable</h2>\n"
    "  <img src='https://static.retroachievements.org/assets/images/cheevo/sad.webp' alt='Sad Cheevo'>\n"
    "  <p>The RetroAchievements website is currently offline, we apologize for any inconvenience caused.</p>\n"
    "  <p><strong><u>You can still earn achievements in any supported emulator.</u></strong></p>\n"
    "  <p><strong>Make sure that the emulator has informed you that you have successfully logged in before you begin playing to avoid missing unlocks.</strong></p>\n"
    "  <p>For more information and updates, please join our <a href='https://discord.gg/retroachievements'>Discord</a>.</p>\n"
    "  <br/>\n"
    "  <a href='https://retroachievements.org'>retroachievements.org</a>\n"
    "</div>\n"
    "</body>\n"
    "</html>";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "503 Service Temporarily Unavailable");
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score_softcore, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_522_simple() {
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "error code: 522";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "error code: 522");
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.new_player_score_softcore, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);
  ASSERT_UNUM_EQUALS(award_achievement_response.achievements_remaining, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

/* ----- submit lboard entry ----- */

static void test_init_submit_lboard_entry_request() {
  rc_api_submit_lboard_entry_request_t submit_lboard_entry_request;
  rc_api_request_t request;

  memset(&submit_lboard_entry_request, 0, sizeof(submit_lboard_entry_request));
  submit_lboard_entry_request.username = "Username";
  submit_lboard_entry_request.api_token = "API_TOKEN";
  submit_lboard_entry_request.leaderboard_id = 1234;
  submit_lboard_entry_request.score = 10999;
  submit_lboard_entry_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_submit_lboard_entry_request(&request, &submit_lboard_entry_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=submitlbentry&u=Username&t=API_TOKEN&i=1234&s=10999&m=ABCDEF0123456789&v=e13c9132ee651256f9d2ee8f06f75d76");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_submit_lboard_entry_request_zero_value() {
  rc_api_submit_lboard_entry_request_t submit_lboard_entry_request;
  rc_api_request_t request;

  memset(&submit_lboard_entry_request, 0, sizeof(submit_lboard_entry_request));
  submit_lboard_entry_request.username = "Username";
  submit_lboard_entry_request.api_token = "API_TOKEN";
  submit_lboard_entry_request.leaderboard_id = 1111;
  submit_lboard_entry_request.score = 0;
  submit_lboard_entry_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_submit_lboard_entry_request(&request, &submit_lboard_entry_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=submitlbentry&u=Username&t=API_TOKEN&i=1111&s=0&m=ABCDEF0123456789&v=9c2ac665157d68b8a26e83bb71dd8aaf");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_submit_lboard_entry_request_negative_value() {
  rc_api_submit_lboard_entry_request_t submit_lboard_entry_request;
  rc_api_request_t request;

  memset(&submit_lboard_entry_request, 0, sizeof(submit_lboard_entry_request));
  submit_lboard_entry_request.username = "Username";
  submit_lboard_entry_request.api_token = "API_TOKEN";
  submit_lboard_entry_request.leaderboard_id = 1111;
  submit_lboard_entry_request.score = -234781;
  submit_lboard_entry_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_submit_lboard_entry_request(&request, &submit_lboard_entry_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=submitlbentry&u=Username&t=API_TOKEN&i=1111&s=-234781&m=ABCDEF0123456789&v=fbe290266f2d121a7a37942e1e90f453");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_submit_lboard_entry_request_no_leaderboard_id() {
  rc_api_submit_lboard_entry_request_t submit_lboard_entry_request;
  rc_api_request_t request;

  memset(&submit_lboard_entry_request, 0, sizeof(submit_lboard_entry_request));
  submit_lboard_entry_request.username = "Username";
  submit_lboard_entry_request.api_token = "API_TOKEN";
  submit_lboard_entry_request.score = 12345;
  submit_lboard_entry_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_submit_lboard_entry_request(&request, &submit_lboard_entry_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_submit_lboard_entry_request_delayed() {
  rc_api_submit_lboard_entry_request_t submit_lboard_entry_request;
  rc_api_request_t request;

  memset(&submit_lboard_entry_request, 0, sizeof(submit_lboard_entry_request));
  submit_lboard_entry_request.username = "Username";
  submit_lboard_entry_request.api_token = "API_TOKEN";
  submit_lboard_entry_request.leaderboard_id = 1234;
  submit_lboard_entry_request.score = 10999;
  submit_lboard_entry_request.game_hash = "ABCDEF0123456789";
  submit_lboard_entry_request.seconds_since_completion = 33;

  ASSERT_NUM_EQUALS(rc_api_init_submit_lboard_entry_request(&request, &submit_lboard_entry_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=submitlbentry&u=Username&t=API_TOKEN&i=1234&s=10999&m=ABCDEF0123456789&o=33&v=7971fc37cf38026f99dd4bae84360ac1");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_process_submit_lb_entry_response_success() {
  rc_api_submit_lboard_entry_response_t submit_lb_entry_response;
  rc_api_lboard_entry_t* entry;
  const char* server_response = "{\"Success\":true,\"Response\":{\"Score\":1234,\"BestScore\":2345,"
	  "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":8765,\"Rank\":1},{\"User\":\"Player2\",\"Score\":7654,\"Rank\":2}],"
	  "\"RankInfo\":{\"Rank\":5,\"NumEntries\":\"17\"}}}";

  memset(&submit_lb_entry_response, 0, sizeof(submit_lb_entry_response));

  ASSERT_NUM_EQUALS(rc_api_process_submit_lboard_entry_response(&submit_lb_entry_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.response.succeeded, 1);
  ASSERT_PTR_NULL(submit_lb_entry_response.response.error_message);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.submitted_score, 1234);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.best_score, 2345);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.new_rank, 5);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.num_entries, 17);

  ASSERT_NUM_EQUALS(submit_lb_entry_response.num_top_entries, 2);
  entry = &submit_lb_entry_response.top_entries[0];
  ASSERT_NUM_EQUALS(entry->rank, 1);
  ASSERT_STR_EQUALS(entry->username, "Player1");
  ASSERT_NUM_EQUALS(entry->score, 8765);
  entry = &submit_lb_entry_response.top_entries[1];
  ASSERT_NUM_EQUALS(entry->rank, 2);
  ASSERT_STR_EQUALS(entry->username, "Player2");
  ASSERT_NUM_EQUALS(entry->score, 7654);

  rc_api_destroy_submit_lboard_entry_response(&submit_lb_entry_response);
}

static void test_process_submit_lb_entry_response_no_entries() {
  rc_api_submit_lboard_entry_response_t submit_lb_entry_response;
  const char* server_response = "{\"Success\":true,\"Response\":{\"Score\":1234,\"BestScore\":2345,"
	  "\"TopEntries\":[],"
	  "\"RankInfo\":{\"Rank\":5,\"NumEntries\":\"17\"}}}";

  memset(&submit_lb_entry_response, 0, sizeof(submit_lb_entry_response));

  ASSERT_NUM_EQUALS(rc_api_process_submit_lboard_entry_response(&submit_lb_entry_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.response.succeeded, 1);
  ASSERT_PTR_NULL(submit_lb_entry_response.response.error_message);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.submitted_score, 1234);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.best_score, 2345);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.new_rank, 5);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.num_entries, 17);

  ASSERT_NUM_EQUALS(submit_lb_entry_response.num_top_entries, 0);
  ASSERT_PTR_NULL(submit_lb_entry_response.top_entries);

  rc_api_destroy_submit_lboard_entry_response(&submit_lb_entry_response);
}

static void test_process_submit_lb_entry_response_invalid_credentials() {
  rc_api_submit_lboard_entry_response_t submit_lb_entry_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";

  memset(&submit_lb_entry_response, 0, sizeof(submit_lb_entry_response));

  ASSERT_NUM_EQUALS(rc_api_process_submit_lboard_entry_response(&submit_lb_entry_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(submit_lb_entry_response.response.error_message, "Credentials invalid (0)");
  ASSERT_NUM_EQUALS(submit_lb_entry_response.submitted_score, 0);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.best_score, 0);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.new_rank, 0);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.num_entries, 0);

  ASSERT_NUM_EQUALS(submit_lb_entry_response.num_top_entries, 0);
  ASSERT_PTR_NULL(submit_lb_entry_response.top_entries);

  rc_api_destroy_submit_lboard_entry_response(&submit_lb_entry_response);
}

static void test_process_submit_lb_entry_response_entries_not_array() {
  rc_api_submit_lboard_entry_response_t submit_lb_entry_response;
  const char* server_response = "{\"Success\":true,\"Response\":{\"Score\":1234,\"BestScore\":2345,"
	  "\"TopEntries\":{\"User\":\"Player1\",\"Score\":8765,\"Rank\":1},"
	  "\"RankInfo\":{\"Rank\":5,\"NumEntries\":\"17\"}}}";

  memset(&submit_lb_entry_response, 0, sizeof(submit_lb_entry_response));

  ASSERT_NUM_EQUALS(rc_api_process_submit_lboard_entry_response(&submit_lb_entry_response, server_response), RC_MISSING_VALUE);
  ASSERT_NUM_EQUALS(submit_lb_entry_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(submit_lb_entry_response.response.error_message, "TopEntries not found in response");

  rc_api_destroy_submit_lboard_entry_response(&submit_lb_entry_response);
}

/* ----- harness ----- */

void test_rapi_runtime(void) {
  TEST_SUITE_BEGIN();

  /* gameid */
  TEST(test_init_resolve_hash_request);
  TEST(test_init_resolve_hash_request_no_credentials);
  TEST(test_init_resolve_hash_request_no_hash);
  TEST(test_init_resolve_hash_request_empty_hash);

  TEST(test_process_resolve_hash_response_match);
  TEST(test_process_resolve_hash_response_no_match);

  /* patch */
  TEST(test_init_fetch_game_data_request);
  TEST(test_init_fetch_game_data_request_no_id);
  TEST(test_init_fetch_game_data_request_by_hash);
  TEST(test_init_fetch_game_data_request_by_id_and_hash);

  TEST(test_process_fetch_game_data_response_empty);
  TEST(test_process_fetch_game_data_response_invalid_credentials);
  TEST(test_process_fetch_game_data_response_not_found);
  TEST(test_process_fetch_game_data_response_achievements);
  TEST(test_process_fetch_game_data_response_achievement_types);
  TEST(test_process_fetch_game_data_response_achievement_rarity);
  TEST(test_process_fetch_game_data_response_achievement_null_author);
  TEST(test_process_fetch_game_data_response_leaderboards);
  TEST(test_process_fetch_game_data_response_rich_presence);
  TEST(test_process_fetch_game_data_response_rich_presence_null);
  TEST(test_process_fetch_game_data_response_rich_presence_tab);

  /* hashdata */
  TEST(test_init_fetch_game_sets_request);
  TEST(test_init_fetch_game_sets_request_no_hash);
  TEST(test_init_fetch_game_sets_request_by_id);
  TEST(test_init_fetch_game_sets_request_by_hash_and_id);

  TEST(test_process_fetch_game_sets_response_empty);
  TEST(test_process_fetch_game_sets_response_invalid_credentials);
  TEST(test_process_fetch_game_sets_response_not_found);
  TEST(test_process_fetch_game_sets_response_achievements);
  TEST(test_process_fetch_game_sets_response_leaderboards);
  TEST(test_process_fetch_game_sets_response_rich_presence);
  TEST(test_process_fetch_game_sets_response_rich_presence_null);
  TEST(test_process_fetch_game_sets_response_specialty_subset);
  TEST(test_process_fetch_game_sets_response_exclusive_subset);

  /* ping */
  TEST(test_init_ping_request);
  TEST(test_init_ping_request_no_game_id);
  TEST(test_init_ping_request_rich_presence);
  TEST(test_init_ping_request_rich_presence_unicode);
  TEST(test_init_ping_request_rich_presence_empty);
  TEST(test_init_ping_request_game_hash_softcore);
  TEST(test_init_ping_request_game_hash_hardcore);

  TEST(test_process_ping_response);

  /* awardachievement */
  TEST(test_init_award_achievement_request_hardcore);
  TEST(test_init_award_achievement_request_non_hardcore);
  TEST(test_init_award_achievement_request_no_hash);
  TEST(test_init_award_achievement_request_no_achievement_id);
  TEST(test_init_award_achievement_request_delayed);

  TEST(test_process_award_achievement_response_success);
  TEST(test_process_award_achievement_response_hardcore_already_unlocked);
  TEST(test_process_award_achievement_response_non_hardcore_already_unlocked);
  TEST(test_process_award_achievement_response_generic_failure);
  TEST(test_process_award_achievement_response_empty);
  TEST(test_process_award_achievement_response_invalid_credentials);
  TEST(test_process_award_achievement_response_text);
  TEST(test_process_award_achievement_response_no_fields);
  TEST(test_process_award_achievement_response_429);
  TEST(test_process_award_achievement_response_429_json);
  TEST(test_process_award_achievement_response_503_fancy);
  TEST(test_process_award_achievement_response_522_simple);

  /* submitlbentry */
  TEST(test_init_submit_lboard_entry_request);
  TEST(test_init_submit_lboard_entry_request_zero_value);
  TEST(test_init_submit_lboard_entry_request_negative_value);
  TEST(test_init_submit_lboard_entry_request_no_leaderboard_id);
  TEST(test_init_submit_lboard_entry_request_delayed);

  TEST(test_process_submit_lb_entry_response_success);
  TEST(test_process_submit_lb_entry_response_no_entries);
  TEST(test_process_submit_lb_entry_response_invalid_credentials);
  TEST(test_process_submit_lb_entry_response_entries_not_array);

  TEST_SUITE_END();
}
