#include "rc_api_user.h"

#include "../src/rapi/rc_api_common.h"
#include "../test_framework.h"
#include "../rc_compat.h"
#include "../rc_version.h"

#define DOREQUEST_URL "https://retroachievements.org/dorequest.php"

static void test_init_start_session_request()
{
  rc_api_start_session_request_t start_session_request;
  rc_api_request_t request;

  memset(&start_session_request, 0, sizeof(start_session_request));
  start_session_request.username = "Username";
  start_session_request.api_token = "API_TOKEN";
  start_session_request.game_id = 1234;

  ASSERT_NUM_EQUALS(rc_api_init_start_session_request(&request, &start_session_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data,  "r=startsession&u=Username&t=API_TOKEN&g=1234&l=" RCHEEVOS_VERSION_STRING);
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_start_session_request_no_game()
{
  rc_api_start_session_request_t start_session_request;
  rc_api_request_t request;

  memset(&start_session_request, 0, sizeof(start_session_request));
  start_session_request.username = "Username";
  start_session_request.api_token = "API_TOKEN";

  ASSERT_NUM_EQUALS(rc_api_init_start_session_request(&request, &start_session_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_start_session_request_game_hash_softcore()
{
  rc_api_start_session_request_t start_session_request;
  rc_api_request_t request;

  memset(&start_session_request, 0, sizeof(start_session_request));
  start_session_request.username = "Username";
  start_session_request.api_token = "API_TOKEN";
  start_session_request.game_id = 1234;
  start_session_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_start_session_request(&request, &start_session_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=startsession&u=Username&t=API_TOKEN&g=1234&h=0&m=ABCDEF0123456789&l=" RCHEEVOS_VERSION_STRING);
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_start_session_request_game_hash_hardcore()
{
  rc_api_start_session_request_t start_session_request;
  rc_api_request_t request;

  memset(&start_session_request, 0, sizeof(start_session_request));
  start_session_request.username = "Username";
  start_session_request.api_token = "API_TOKEN";
  start_session_request.game_id = 1234;
  start_session_request.hardcore = 1;
  start_session_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_start_session_request(&request, &start_session_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=startsession&u=Username&t=API_TOKEN&g=1234&h=1&m=ABCDEF0123456789&l=" RCHEEVOS_VERSION_STRING);
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_process_start_session_response_legacy()
{
  rc_api_start_session_response_t start_session_response;
  const char* server_response = "{\"Success\":true}";

  memset(&start_session_response, 0, sizeof(start_session_response));

  ASSERT_NUM_EQUALS(rc_api_process_start_session_response(&start_session_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(start_session_response.response.succeeded, 1);
  ASSERT_PTR_NULL(start_session_response.response.error_message);
  ASSERT_NUM_EQUALS(start_session_response.num_unlocks, 0);
  ASSERT_NUM_EQUALS(start_session_response.num_hardcore_unlocks, 0);
  ASSERT_NUM_EQUALS(start_session_response.server_now, 0);

  rc_api_destroy_start_session_response(&start_session_response);
}

static void test_process_start_session_response()
{
  rc_api_start_session_response_t start_session_response;
  /* startsession API only returns HardcoreUnlocks if an achievement has been earned in hardcore,
   * even if the softcore unlock has a different timestamp. Unlocks are only returned for things
   * only unlocked in softcore. */
  const char* server_response = "{\"Success\":true,\"HardcoreUnlocks\":["
      "{\"ID\":111,\"When\":1234567890},"
      "{\"ID\":112,\"When\":1234567891},"
      "{\"ID\":113,\"When\":1234567860}"
    "],\"Unlocks\":["
      "{\"ID\":114,\"When\":1234567840}"
    "],\"ServerNow\":1234577777}";

  memset(&start_session_response, 0, sizeof(start_session_response));

  ASSERT_NUM_EQUALS(rc_api_process_start_session_response(&start_session_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(start_session_response.response.succeeded, 1);
  ASSERT_PTR_NULL(start_session_response.response.error_message);
  ASSERT_NUM_EQUALS(start_session_response.num_unlocks, 1);
  ASSERT_NUM_EQUALS(start_session_response.unlocks[0].achievement_id, 114);
  ASSERT_NUM_EQUALS(start_session_response.unlocks[0].when, 1234567840);
  ASSERT_NUM_EQUALS(start_session_response.num_hardcore_unlocks, 3);
  ASSERT_NUM_EQUALS(start_session_response.hardcore_unlocks[0].achievement_id, 111);
  ASSERT_NUM_EQUALS(start_session_response.hardcore_unlocks[0].when, 1234567890);
  ASSERT_NUM_EQUALS(start_session_response.hardcore_unlocks[1].achievement_id, 112);
  ASSERT_NUM_EQUALS(start_session_response.hardcore_unlocks[1].when, 1234567891);
  ASSERT_NUM_EQUALS(start_session_response.hardcore_unlocks[2].achievement_id, 113);
  ASSERT_NUM_EQUALS(start_session_response.hardcore_unlocks[2].when, 1234567860);
  ASSERT_NUM_EQUALS(start_session_response.server_now, 1234577777);

  rc_api_destroy_start_session_response(&start_session_response);
}

static void test_process_start_session_response_invalid_credentials()
{
  rc_api_start_session_response_t start_session_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";

  memset(&start_session_response, 0, sizeof(start_session_response));

  ASSERT_NUM_EQUALS(rc_api_process_start_session_response(&start_session_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(start_session_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(start_session_response.response.error_message, "Credentials invalid (0)");

  rc_api_destroy_start_session_response(&start_session_response);
}

static void test_init_login_request_password()
{
  rc_api_login_request_t login_request;
  rc_api_request_t request;

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "Username";
  login_request.password = "Pa$$w0rd!";

  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=login2&u=Username&p=Pa%24%24w0rd%21");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_login_request_password_long()
{
  char buffer[1024], *ptr, *password_start;
  rc_api_login_request_t login_request;
  rc_api_request_t request;
  int i;

  /* this generates a password that's 830 characters long */
  ptr = password_start = buffer + snprintf(buffer, sizeof(buffer), "r=login2&u=ThisUsernameIsAlsoReallyLongAtRoughlyFiftyCharacters&p=");
  for (i = 0; i < 30; i++)
	ptr += snprintf(ptr, sizeof(buffer) - (ptr - buffer), "%dABCDEFGHIJKLMNOPQRSTUVWXYZ", i);

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "ThisUsernameIsAlsoReallyLongAtRoughlyFiftyCharacters";
  login_request.password = password_start;

  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, buffer);
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_login_request_token()
{
  rc_api_login_request_t login_request;
  rc_api_request_t request;

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "Username";
  login_request.api_token = "ABCDEFGHIJKLMNOP";

  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=login2&u=Username&t=ABCDEFGHIJKLMNOP");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_login_request_password_and_token()
{
  rc_api_login_request_t login_request;
  rc_api_request_t request;

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "Username";
  login_request.password = "Pa$$w0rd!";
  login_request.api_token = "ABCDEFGHIJKLMNOP";

  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=login2&u=Username&p=Pa%24%24w0rd%21");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_login_request_no_password_or_token()
{
  rc_api_login_request_t login_request;
  rc_api_request_t request;

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "Username";

  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_login_request_alternate_host()
{
  rc_api_login_request_t login_request;
  rc_api_request_t request;

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "Username";
  login_request.password = "Pa$$w0rd!";

  rc_api_set_host("localhost");
  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, "http://localhost/dorequest.php");
  ASSERT_STR_EQUALS(request.post_data, "r=login2&u=Username&p=Pa%24%24w0rd%21");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_set_host(NULL);
  rc_api_destroy_request(&request);
}

static void test_process_login_response_success()
{
  rc_api_login_response_t login_response;
  rc_api_server_response_t response_obj;
  const char* server_response = "{\"Success\":true,\"User\":\"USER\",\"Token\":\"ApiTOKEN\",\"Score\":1234,\"SoftcoreScore\":789,\"Messages\":2}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 1);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_STR_EQUALS(login_response.username, "USER");
  ASSERT_STR_EQUALS(login_response.api_token, "ApiTOKEN");
  ASSERT_NUM_EQUALS(login_response.score, 1234);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 789);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 2);
  ASSERT_STR_EQUALS(login_response.display_name, "USER");

  rc_api_destroy_login_response(&login_response);

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);
  response_obj.http_status_code = 200;

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_server_response(&login_response, &response_obj), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 1);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_STR_EQUALS(login_response.username, "USER");
  ASSERT_STR_EQUALS(login_response.api_token, "ApiTOKEN");
  ASSERT_NUM_EQUALS(login_response.score, 1234);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 789);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 2);
  ASSERT_STR_EQUALS(login_response.display_name, "USER");

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_unique_display_name()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true,\"User\":\"GamingHero\",\"AvatarUrl\":\"http://host/UserPic/USER.png\",\"Token\":\"ApiTOKEN\",\"Score\":1234,\"SoftcoreScore\":789,\"Messages\":2}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 1);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_STR_EQUALS(login_response.username, "GamingHero");
  ASSERT_STR_EQUALS(login_response.api_token, "ApiTOKEN");
  ASSERT_NUM_EQUALS(login_response.score, 1234);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 789);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 2);
  ASSERT_STR_EQUALS(login_response.display_name, "GamingHero");
  ASSERT_STR_EQUALS(login_response.avatar_url, "http://host/UserPic/USER.png");

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_invalid_credentials()
{
  rc_api_login_response_t login_response;
  rc_api_server_response_t response_obj;
  const char* server_response = "{\"Success\":false,\"Error\":\"Invalid User/Password combination. Please try again\", \"Code\":\"invalid_credentials\"}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_INVALID_CREDENTIALS);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "Invalid User/Password combination. Please try again");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);
  response_obj.http_status_code = 401;

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_server_response(&login_response, &response_obj), RC_INVALID_CREDENTIALS);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "Invalid User/Password combination. Please try again");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_access_denied()
{
  rc_api_login_response_t login_response;
  rc_api_server_response_t response_obj;
  const char* server_response = "{\"Success\":false,\"Error\":\"Access denied.\",\"Code\":\"access_denied\"}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_ACCESS_DENIED);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "Access denied.");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);
  response_obj.http_status_code = 403;

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_server_response(&login_response, &response_obj), RC_ACCESS_DENIED);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "Access denied.");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_expired_token()
{
  rc_api_login_response_t login_response;
  rc_api_server_response_t response_obj;
  const char* server_response = "{\"Success\":false,\"Error\":\"The access token has expired. Please log in again.\",\"Code\":\"expired_token\"}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_EXPIRED_TOKEN);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "The access token has expired. Please log in again.");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);
  response_obj.http_status_code = 401;

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_server_response(&login_response, &response_obj), RC_EXPIRED_TOKEN);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "The access token has expired. Please log in again.");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_generic_failure()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":false}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_empty()
{
  rc_api_login_response_t login_response;
  const char* server_response = "";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_NO_RESPONSE);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_text()
{
  rc_api_login_response_t login_response;
  const char* server_response = "You do not have access to that resource";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "You do not have access to that resource");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_html()
{
  rc_api_login_response_t login_response;
  const char* server_response = "<b>You do not have access to that resource</b>";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "<b>You do not have access to that resource</b>");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_no_required_fields()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_MISSING_VALUE);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "User not found in response");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_no_token()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true,\"User\":\"Username\"}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_MISSING_VALUE);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "Token not found in response");
  ASSERT_STR_EQUALS(login_response.username, "Username");
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_PTR_NULL(login_response.display_name);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_no_optional_fields()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true,\"User\":\"USER\",\"Token\":\"ApiTOKEN\"}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 1);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_STR_EQUALS(login_response.username, "USER");
  ASSERT_STR_EQUALS(login_response.api_token, "ApiTOKEN");
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_STR_EQUALS(login_response.display_name, "USER");

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_null_score()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true,\"User\":\"USER\",\"Token\":\"ApiTOKEN\",\"Score\":null,\"SoftcoreScore\":null}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 1);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_STR_EQUALS(login_response.username, "USER");
  ASSERT_STR_EQUALS(login_response.api_token, "ApiTOKEN");
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.score_softcore, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);
  ASSERT_STR_EQUALS(login_response.display_name, "USER");

  rc_api_destroy_login_response(&login_response);
}

static void test_init_fetch_user_unlocks_request_non_hardcore()
{
  rc_api_fetch_user_unlocks_request_t fetch_user_unlocks_request;
  rc_api_request_t request;

  memset(&fetch_user_unlocks_request, 0, sizeof(fetch_user_unlocks_request));
  fetch_user_unlocks_request.username = "Username";
  fetch_user_unlocks_request.api_token = "API_TOKEN";
  fetch_user_unlocks_request.game_id = 1234;
  fetch_user_unlocks_request.hardcore = 0;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_user_unlocks_request(&request, &fetch_user_unlocks_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=unlocks&u=Username&t=API_TOKEN&g=1234&h=0");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_user_unlocks_request_hardcore()
{
  rc_api_fetch_user_unlocks_request_t fetch_user_unlocks_request;
  rc_api_request_t request;

  memset(&fetch_user_unlocks_request, 0, sizeof(fetch_user_unlocks_request));
  fetch_user_unlocks_request.username = "Username";
  fetch_user_unlocks_request.api_token = "API_TOKEN";
  fetch_user_unlocks_request.game_id = 2345;
  fetch_user_unlocks_request.hardcore = 1;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_user_unlocks_request(&request, &fetch_user_unlocks_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=unlocks&u=Username&t=API_TOKEN&g=2345&h=1");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_user_unlocks_response_empty_array()
{
  rc_api_fetch_user_unlocks_response_t fetch_user_unlocks_response;
  const char* server_response = "{\"Success\":true,\"UserUnlocks\":[],\"GameID\":11277,\"HardcoreMode\":false}";
  memset(&fetch_user_unlocks_response, 0, sizeof(fetch_user_unlocks_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_user_unlocks_response(&fetch_user_unlocks_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_user_unlocks_response.response.error_message);
  ASSERT_PTR_NULL(fetch_user_unlocks_response.achievement_ids);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.num_achievement_ids, 0);

  rc_api_destroy_fetch_user_unlocks_response(&fetch_user_unlocks_response);
}

static void test_init_fetch_user_unlocks_response_invalid_credentials()
{
  rc_api_fetch_user_unlocks_response_t fetch_user_unlocks_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";
  memset(&fetch_user_unlocks_response, 0, sizeof(fetch_user_unlocks_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_user_unlocks_response(&fetch_user_unlocks_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(fetch_user_unlocks_response.response.error_message, "Credentials invalid (0)");
  ASSERT_PTR_NULL(fetch_user_unlocks_response.achievement_ids);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.num_achievement_ids, 0);

  rc_api_destroy_fetch_user_unlocks_response(&fetch_user_unlocks_response);
}

static void test_init_fetch_user_unlocks_response_one_item()
{
  rc_api_fetch_user_unlocks_response_t fetch_user_unlocks_response;
  const char* server_response = "{\"Success\":true,\"UserUnlocks\":[1234],\"GameID\":11277,\"HardcoreMode\":false}";
  memset(&fetch_user_unlocks_response, 0, sizeof(fetch_user_unlocks_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_user_unlocks_response(&fetch_user_unlocks_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_user_unlocks_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.num_achievement_ids, 1);
  ASSERT_PTR_NOT_NULL(fetch_user_unlocks_response.achievement_ids);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.achievement_ids[0], 1234);

  rc_api_destroy_fetch_user_unlocks_response(&fetch_user_unlocks_response);
}

static void test_init_fetch_user_unlocks_response_several_items()
{
  rc_api_fetch_user_unlocks_response_t fetch_user_unlocks_response;
  const char* server_response = "{\"Success\":true,\"UserUnlocks\":[1,2,3,4],\"GameID\":11277,\"HardcoreMode\":false}";
  memset(&fetch_user_unlocks_response, 0, sizeof(fetch_user_unlocks_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_user_unlocks_response(&fetch_user_unlocks_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_user_unlocks_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.num_achievement_ids, 4);
  ASSERT_PTR_NOT_NULL(fetch_user_unlocks_response.achievement_ids);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.achievement_ids[0], 1);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.achievement_ids[1], 2);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.achievement_ids[2], 3);
  ASSERT_NUM_EQUALS(fetch_user_unlocks_response.achievement_ids[3], 4);

  rc_api_destroy_fetch_user_unlocks_response(&fetch_user_unlocks_response);
}

static void test_init_fetch_followed_users_request()
{
  rc_api_fetch_followed_users_request_t fetch_followed_users_request;
  rc_api_request_t request;

  memset(&fetch_followed_users_request, 0, sizeof(fetch_followed_users_request));
  fetch_followed_users_request.username = "Username";
  fetch_followed_users_request.api_token = "API_TOKEN";

  ASSERT_NUM_EQUALS(rc_api_init_fetch_followed_users_request(&request, &fetch_followed_users_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=getfriendlist&u=Username&t=API_TOKEN");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_followed_users_response_empty_array()
{
  rc_api_fetch_followed_users_response_t fetch_followed_users_response;
  rc_api_server_response_t server_response;
  memset(&fetch_followed_users_response, 0, sizeof(fetch_followed_users_response));

  memset(&server_response, 0, sizeof(server_response));
  server_response.body = "{\"Success\":true,\"Friends\":[]}";
  server_response.body_length = strlen(server_response.body);

  ASSERT_NUM_EQUALS(rc_api_process_fetch_followed_users_server_response(&fetch_followed_users_response, &server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_followed_users_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_followed_users_response.response.error_message);
  ASSERT_PTR_NULL(fetch_followed_users_response.users);
  ASSERT_NUM_EQUALS(fetch_followed_users_response.num_users, 0);

  rc_api_destroy_fetch_followed_users_response(&fetch_followed_users_response);
}

static void test_init_fetch_followed_users_response_invalid_credentials()
{
  rc_api_fetch_followed_users_response_t fetch_followed_users_response;
  rc_api_server_response_t server_response;
  memset(&fetch_followed_users_response, 0, sizeof(fetch_followed_users_response));

  memset(&server_response, 0, sizeof(server_response));
  server_response.body = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";
  server_response.body_length = strlen(server_response.body);

  ASSERT_NUM_EQUALS(rc_api_process_fetch_followed_users_server_response(&fetch_followed_users_response, &server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_followed_users_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(fetch_followed_users_response.response.error_message, "Credentials invalid (0)");
  ASSERT_PTR_NULL(fetch_followed_users_response.users);
  ASSERT_NUM_EQUALS(fetch_followed_users_response.num_users, 0);

  rc_api_destroy_fetch_followed_users_response(&fetch_followed_users_response);
}

static void test_init_fetch_followed_users_response_several_items()
{
  rc_api_fetch_followed_users_response_t fetch_followed_users_response;
  rc_api_server_response_t server_response;
  memset(&fetch_followed_users_response, 0, sizeof(fetch_followed_users_response));

  memset(&server_response, 0, sizeof(server_response));
  server_response.body = "{\"Success\":true,\"Friends\":["
                           "{\"Friend\":\"Bob\",\"AvatarUrl\":\"/User/Bob.png\",\"RAPoints\":1234,\"LastSeen\":\"Doing stuff\"}," /* legacy format */
                           "{\"Friend\":\"Jane\",\"AvatarUrl\":\"/User/Jane.png\",\"RAPoints\":5,\"LastSeen\":\"Winning\","
                            "\"LastSeenTime\":1234567890,\"LastGameId\":6,\"LastGameTitle\":\"The Game\",\"LastGameIconUrl\":\"/Badges/000006.png\"},"
                           "{\"Friend\":\"Bill\",\"AvatarUrl\":\"/User/Bill.png\",\"RAPoints\":0,\"LastSeen\":\"Unknown\","
                            "\"LastSeenTime\":1234567800,\"LastGameId\":null,\"LastGameTitle\":null,\"LastGameIconUrl\":null}"
                         "]}";
  server_response.body_length = strlen(server_response.body);

  ASSERT_NUM_EQUALS(rc_api_process_fetch_followed_users_server_response(&fetch_followed_users_response, &server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_followed_users_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_followed_users_response.response.error_message);
  ASSERT_PTR_NOT_NULL(fetch_followed_users_response.users);
  ASSERT_NUM_EQUALS(fetch_followed_users_response.num_users, 3);

  ASSERT_STR_EQUALS(fetch_followed_users_response.users[0].display_name, "Bob");
  ASSERT_STR_EQUALS(fetch_followed_users_response.users[0].avatar_url, "/User/Bob.png");
  ASSERT_NUM_EQUALS(fetch_followed_users_response.users[0].score, 1234);
  ASSERT_STR_EQUALS(fetch_followed_users_response.users[0].recent_activity.description, "Doing stuff");
  ASSERT_PTR_NULL(fetch_followed_users_response.users[0].recent_activity.context);
  ASSERT_PTR_NULL(fetch_followed_users_response.users[0].recent_activity.context_image_url);
  ASSERT_NUM_EQUALS(fetch_followed_users_response.users[0].recent_activity.context_id, 0);
  ASSERT_NUM_EQUALS(fetch_followed_users_response.users[0].recent_activity.when, 0);

  ASSERT_STR_EQUALS(fetch_followed_users_response.users[1].display_name, "Jane");
  ASSERT_STR_EQUALS(fetch_followed_users_response.users[1].avatar_url, "/User/Jane.png");
  ASSERT_NUM_EQUALS(fetch_followed_users_response.users[1].score, 5);
  ASSERT_STR_EQUALS(fetch_followed_users_response.users[1].recent_activity.description, "Winning");
  ASSERT_STR_EQUALS(fetch_followed_users_response.users[1].recent_activity.context, "The Game");
  ASSERT_STR_EQUALS(fetch_followed_users_response.users[1].recent_activity.context_image_url, "/Badges/000006.png");
  ASSERT_NUM_EQUALS(fetch_followed_users_response.users[1].recent_activity.context_id, 6);
  ASSERT_NUM_EQUALS(fetch_followed_users_response.users[1].recent_activity.when, 1234567890);

  ASSERT_STR_EQUALS(fetch_followed_users_response.users[2].display_name, "Bill");
  ASSERT_STR_EQUALS(fetch_followed_users_response.users[2].avatar_url, "/User/Bill.png");
  ASSERT_NUM_EQUALS(fetch_followed_users_response.users[2].score, 0);
  ASSERT_STR_EQUALS(fetch_followed_users_response.users[2].recent_activity.description, "Unknown");
  ASSERT_PTR_NULL(fetch_followed_users_response.users[2].recent_activity.context);
  ASSERT_PTR_NULL(fetch_followed_users_response.users[2].recent_activity.context_image_url);
  ASSERT_NUM_EQUALS(fetch_followed_users_response.users[2].recent_activity.context_id, 0);
  ASSERT_NUM_EQUALS(fetch_followed_users_response.users[2].recent_activity.when, 1234567800);

  rc_api_destroy_fetch_followed_users_response(&fetch_followed_users_response);
}

static void test_init_fetch_all_user_progress_request()
{
  rc_api_fetch_all_user_progress_request_t fetch_all_user_progress_request;
  rc_api_request_t request;

  memset(&fetch_all_user_progress_request, 0, sizeof(fetch_all_user_progress_request));
  fetch_all_user_progress_request.username = "Username";
  fetch_all_user_progress_request.api_token = "API_TOKEN";
  fetch_all_user_progress_request.console_id = 1;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_all_user_progress_request(&request, &fetch_all_user_progress_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=allprogress&u=Username&t=API_TOKEN&c=1");
  ASSERT_STR_EQUALS(request.content_type, RC_CONTENT_TYPE_URLENCODED);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_all_user_progress_response_empty_array()
{
  rc_api_fetch_all_user_progress_response_t fetch_all_user_progress_response;
  rc_api_server_response_t response_obj;
  const char* server_response = "{\"Success\":true,\"Response\":[]}";

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);
  response_obj.http_status_code = 200;

  memset(&fetch_all_user_progress_response, 0, sizeof(fetch_all_user_progress_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_all_user_progress_server_response(&fetch_all_user_progress_response, &response_obj), RC_OK);

  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_all_user_progress_response.response.error_message);
  ASSERT_PTR_NULL(fetch_all_user_progress_response.entries);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.num_entries, 0);

  rc_api_destroy_fetch_all_user_progress_response(&fetch_all_user_progress_response);
}

static void test_init_fetch_all_user_progress_response_invalid_credentials()
{
  rc_api_fetch_all_user_progress_response_t fetch_all_user_progress_response;
  rc_api_server_response_t response_obj;
  const char* server_response = "{\"Success\":false,\"Error\":\"Credentials invalid (0)\"}";
  memset(&fetch_all_user_progress_response, 0, sizeof(fetch_all_user_progress_response));

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);
  response_obj.http_status_code = 200;

  memset(&fetch_all_user_progress_response, 0, sizeof(fetch_all_user_progress_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_all_user_progress_server_response(&fetch_all_user_progress_response, &response_obj), RC_OK);

  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(fetch_all_user_progress_response.response.error_message, "Credentials invalid (0)");
  ASSERT_PTR_NULL(fetch_all_user_progress_response.entries);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.num_entries, 0);

  rc_api_destroy_fetch_all_user_progress_response(&fetch_all_user_progress_response);
}

static void test_init_fetch_all_user_progress_response_one_item()
{
  rc_api_fetch_all_user_progress_response_t fetch_all_user_progress_response;
  rc_api_server_response_t response_obj;
  const char* server_response = "{\"Success\":true,\"Response\":{\"10\":{\"Achievements\":11}}}";

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);
  response_obj.http_status_code = 200;

  memset(&fetch_all_user_progress_response, 0, sizeof(fetch_all_user_progress_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_all_user_progress_server_response(&fetch_all_user_progress_response, &response_obj), RC_OK);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_all_user_progress_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.num_entries, 1);
  ASSERT_PTR_NOT_NULL(fetch_all_user_progress_response.entries);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[0].game_id, 10);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[0].num_achievements, 11);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[0].num_unlocked_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[0].num_unlocked_achievements_hardcore, 0);

  rc_api_destroy_fetch_all_user_progress_response(&fetch_all_user_progress_response);
}

static void test_init_fetch_all_user_progress_response_several_items()
{
  rc_api_fetch_all_user_progress_response_t fetch_all_user_progress_response;
  rc_api_server_response_t response_obj;

  const char* server_response = "{\"Success\":true,\"Response\":{\"10\":{\"Achievements\":11},"
    "\"20\":{\"Achievements\":21,\"Unlocked\":22},"
    "\"30\":{\"Achievements\":31,\"Unlocked\":32,\"UnlockedHardcore\":33},"
    "\"40\":{\"Achievements\":41,\"UnlockedHardcore\":43}}}";

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);
  response_obj.http_status_code = 200;

  memset(&fetch_all_user_progress_response, 0, sizeof(fetch_all_user_progress_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_all_user_progress_server_response(&fetch_all_user_progress_response, &response_obj), RC_OK);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_all_user_progress_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.num_entries, 4);
  ASSERT_PTR_NOT_NULL(fetch_all_user_progress_response.entries);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[0].game_id, 10);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[0].num_achievements, 11);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[0].num_unlocked_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[0].num_unlocked_achievements_hardcore, 0);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[1].game_id, 20);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[1].num_achievements, 21);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[1].num_unlocked_achievements, 22);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[1].num_unlocked_achievements_hardcore, 0);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[2].game_id, 30);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[2].num_achievements, 31);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[2].num_unlocked_achievements, 32);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[2].num_unlocked_achievements_hardcore, 33);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[3].game_id, 40);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[3].num_achievements, 41);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[3].num_unlocked_achievements, 0);
  ASSERT_NUM_EQUALS(fetch_all_user_progress_response.entries[3].num_unlocked_achievements_hardcore, 43);

  rc_api_destroy_fetch_all_user_progress_response(&fetch_all_user_progress_response);
}

void test_rapi_user(void) {
  TEST_SUITE_BEGIN();

  /* start session */
  TEST(test_init_start_session_request);
  TEST(test_init_start_session_request_no_game);
  TEST(test_init_start_session_request_game_hash_softcore);
  TEST(test_init_start_session_request_game_hash_hardcore);

  TEST(test_process_start_session_response_legacy);
  TEST(test_process_start_session_response);
  TEST(test_process_start_session_response_invalid_credentials);

  /* login */
  TEST(test_init_login_request_password);
  TEST(test_init_login_request_password_long);
  TEST(test_init_login_request_token);
  TEST(test_init_login_request_password_and_token);
  TEST(test_init_login_request_no_password_or_token);
  TEST(test_init_login_request_alternate_host);

  TEST(test_process_login_response_success);
  TEST(test_process_login_response_unique_display_name);
  TEST(test_process_login_response_invalid_credentials);
  TEST(test_process_login_response_access_denied);
  TEST(test_process_login_response_generic_failure);
  TEST(test_process_login_response_expired_token);
  TEST(test_process_login_response_empty);
  TEST(test_process_login_response_text);
  TEST(test_process_login_response_html);
  TEST(test_process_login_response_no_required_fields);
  TEST(test_process_login_response_no_token);
  TEST(test_process_login_response_no_optional_fields);
  TEST(test_process_login_response_null_score);

  /* unlocks */
  TEST(test_init_fetch_user_unlocks_request_non_hardcore);
  TEST(test_init_fetch_user_unlocks_request_hardcore);

  TEST(test_init_fetch_user_unlocks_response_empty_array);
  TEST(test_init_fetch_user_unlocks_response_invalid_credentials);
  TEST(test_init_fetch_user_unlocks_response_one_item);
  TEST(test_init_fetch_user_unlocks_response_several_items);

  /* followed users */
  TEST(test_init_fetch_followed_users_request);

  TEST(test_init_fetch_followed_users_response_empty_array);
  TEST(test_init_fetch_followed_users_response_invalid_credentials);
  TEST(test_init_fetch_followed_users_response_several_items);

  /* all user progress */
  TEST(test_init_fetch_all_user_progress_request);

  TEST(test_init_fetch_all_user_progress_response_empty_array);
  TEST(test_init_fetch_all_user_progress_response_invalid_credentials);
  TEST(test_init_fetch_all_user_progress_response_one_item);
  TEST(test_init_fetch_all_user_progress_response_several_items);

  TEST_SUITE_END();
}
