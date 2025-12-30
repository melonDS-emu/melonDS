#include "rc_client.h"

#include "../src/rc_client_external_versions.h"
#include "../src/rc_client_internal.h"
#include "../src/rc_version.h"
#include "rc_consoles.h"
#include "rhash/data.h"

#include "test_framework.h"

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL

static rc_client_t* g_client;
static const char* g_external_event;
static int g_external_int = 0;
static void* g_callback_userdata = &g_client; /* dummy object to use for callback userdata validation */

/* begin from test_rc_client.c */

extern void rc_client_server_call(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client);
extern void reset_mock_api_handlers(void);
extern void mock_api_response(const char* request_params, const char* response_body);
extern void mock_api_error(const char* request_params, const char* response_body, int http_status_code);

/* end from test_rc_client.c */

#define RC_OFFSETOF(s,f) ((uint32_t)((uint8_t*)&(((s*)(0))->f) - ((uint8_t*)0)))

#define ASSERT_FIELD_OFFSET(struct1, struct2, field) { \
  const uint32_t __o1 = RC_OFFSETOF(struct1, field); \
  const uint32_t __o2 = RC_OFFSETOF(struct2, field); \
  if (__o1 != __o2) { \
    ASSERT_FAIL("Expected: " #struct1 "." #field " at offset %u (%s:%d)\n  Found: " #struct2 "." #field " at offset %u", \
                __o1, test_framework_basename(__FILE__), __LINE__, __o2); \
  } \
}

static uint32_t rc_client_read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client)
{
  return 0;
}

static rc_client_t* mock_client_with_external()
{
  rc_client_t* client = rc_client_create(rc_client_read_memory, rc_client_server_call);
  client->state.external_client = (rc_client_external_t*)
      rc_buffer_alloc(&client->state.buffer, sizeof(*client->state.external_client));
  memset(client->state.external_client, 0, sizeof(*client->state.external_client));

  rc_api_set_host(NULL);
  reset_mock_api_handlers();
  g_external_event = "none";
  g_external_int = 0;

  return client;
}

static void rc_client_callback_expect_success(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_OK);
  ASSERT_PTR_NULL(error_message);
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void rc_client_external_unload_game(void)
{
  g_external_event = "unload_game";
}

/* ----- settings ----- */

static int rc_client_external_get_int(void)
{
  return g_external_int;
}

static void rc_client_external_set_int(int value)
{
  g_external_int = value;
}

static void test_hardcore_enabled(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->get_hardcore_enabled = rc_client_external_get_int;
  g_client->state.external_client->set_hardcore_enabled = rc_client_external_set_int;

  g_external_int = 0;
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);

  g_external_int = 1;
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);

  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(g_external_int, 0);

  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(g_external_int, 1);

  rc_client_destroy(g_client);
}

static void test_unofficial_enabled(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->get_unofficial_enabled = rc_client_external_get_int;
  g_client->state.external_client->set_unofficial_enabled = rc_client_external_set_int;

  g_external_int = 0;
  ASSERT_NUM_EQUALS(rc_client_get_unofficial_enabled(g_client), 0);

  g_external_int = 1;
  ASSERT_NUM_EQUALS(rc_client_get_unofficial_enabled(g_client), 1);

  rc_client_set_unofficial_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(g_external_int, 0);

  rc_client_set_unofficial_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(g_external_int, 1);

  rc_client_destroy(g_client);
}

static void test_encore_mode_enabled(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->get_encore_mode_enabled = rc_client_external_get_int;
  g_client->state.external_client->set_encore_mode_enabled = rc_client_external_set_int;

  g_external_int = 0;
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 0);

  g_external_int = 1;
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);

  rc_client_set_encore_mode_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(g_external_int, 0);

  rc_client_set_encore_mode_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(g_external_int, 1);

  rc_client_destroy(g_client);
}

static void test_spectator_mode_enabled(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->get_spectator_mode_enabled = rc_client_external_get_int;
  g_client->state.external_client->set_spectator_mode_enabled = rc_client_external_set_int;

  g_external_int = 0;
  ASSERT_NUM_EQUALS(rc_client_get_spectator_mode_enabled(g_client), 0);

  g_external_int = 1;
  ASSERT_NUM_EQUALS(rc_client_get_spectator_mode_enabled(g_client), 1);

  rc_client_set_spectator_mode_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(g_external_int, 0);

  rc_client_set_spectator_mode_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(g_external_int, 1);

  rc_client_destroy(g_client);
}

static void rc_client_external_log_message(const char* message, const rc_client_t* client)
{
}

static void rc_client_external_enable_logging(rc_client_t* client, int level, rc_client_message_callback_t callback)
{
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_NUM_EQUALS(level, RC_CLIENT_LOG_LEVEL_INFO);
  ASSERT_PTR_EQUALS(callback, rc_client_external_log_message);

  g_external_event = "enable_logging";
}

static void test_enable_logging(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->enable_logging = rc_client_external_enable_logging;

  rc_client_enable_logging(g_client, RC_CLIENT_LOG_LEVEL_INFO, rc_client_external_log_message);

  ASSERT_STR_EQUALS(g_external_event, "enable_logging");

  rc_client_destroy(g_client);
}

static void rc_client_external_event_handler(const rc_client_event_t* event, rc_client_t* client)
{
}

static void rc_client_external_set_event_handler(rc_client_t* client, rc_client_event_handler_t handler)
{
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(handler, rc_client_external_event_handler);

  g_external_event = "event_handler";
}

static void test_event_handler(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->set_event_handler = rc_client_external_set_event_handler;

  rc_client_set_event_handler(g_client, rc_client_external_event_handler);

  ASSERT_STR_EQUALS(g_external_event, "event_handler");

  rc_client_destroy(g_client);
}

static uint32_t rc_client_external_read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client)
{
  return 0;
}

static void rc_client_external_set_read_memory_function(rc_client_t* client, rc_client_read_memory_func_t handler)
{
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(handler, rc_client_external_read_memory);

  g_external_event = "read_memory";
}

static void test_read_memory(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->set_read_memory = rc_client_external_set_read_memory_function;

  rc_client_set_read_memory_function(g_client, rc_client_external_read_memory);

  ASSERT_STR_EQUALS(g_external_event, "read_memory");

  rc_client_destroy(g_client);
}

static rc_clock_t rc_client_external_now_millisecs(const rc_client_t* client)
{
  return (rc_clock_t)12345678;
}

static void rc_client_external_set_get_time_millisecs(rc_client_t* client, rc_get_time_millisecs_func_t handler)
{
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(handler, rc_client_external_now_millisecs);

  g_external_event = "set_milli";
}

static void test_get_time_millisecs(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->set_get_time_millisecs = rc_client_external_set_get_time_millisecs;

  rc_client_set_get_time_millisecs_function(g_client, rc_client_external_now_millisecs);

  ASSERT_STR_EQUALS(g_external_event, "set_milli");

  rc_client_destroy(g_client);
}

static void rc_client_external_set_host(const char* hostname)
{
  ASSERT_STR_EQUALS(hostname, "localhost");

  g_external_event = "set_host";
}

static void test_set_host(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->set_host = rc_client_external_set_host;

  rc_client_set_host(g_client, "localhost");

  ASSERT_STR_EQUALS(g_external_event, "set_host");

  rc_api_set_host(NULL);
  rc_api_set_image_host(NULL);

  rc_client_destroy(g_client);
}

static size_t rc_client_external_get_user_agent_clause(char buffer[], size_t buffer_size)
{
  return snprintf(buffer, buffer_size, "external/2.1");
}

static void test_get_user_agent_clause(void)
{
  char expected_clause[] = "external/2.1 rc_client/" RCHEEVOS_VERSION_STRING;
  char buffer[64];

  g_client = mock_client_with_external();
  g_client->state.external_client->get_user_agent_clause = rc_client_external_get_user_agent_clause;

  ASSERT_NUM_EQUALS(rc_client_get_user_agent_clause(g_client, buffer, sizeof(buffer)), sizeof(expected_clause) - 1);
  ASSERT_STR_EQUALS(buffer, expected_clause);

  /* snprintf will return the number of characters it wants, even if the buffer is too small,
   * but will only fill as much of the buffer is available */
  ASSERT_NUM_EQUALS(rc_client_get_user_agent_clause(g_client, buffer, 8), sizeof(expected_clause) - 1);
  ASSERT_STR_EQUALS(buffer, "externa");

  ASSERT_NUM_EQUALS(rc_client_get_user_agent_clause(g_client, buffer, 20), sizeof(expected_clause) - 1);
  ASSERT_STR_EQUALS(buffer, "external/2.1 rc_cli");

  rc_client_destroy(g_client);
}

static void rc_client_external_add_game_hash(const char* hash, uint32_t game_id)
{
  ASSERT_STR_EQUALS(hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_NUM_EQUALS(game_id, 1234);

  g_external_event = "add_game_hash";
  g_external_int = game_id;
}

static void test_add_game_hash(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->add_game_hash = rc_client_external_add_game_hash;

  rc_client_add_game_hash(g_client, "6a2305a2b6675a97ff792709be1ca857", 1234);

  /* hash should be loaded in both local client and external client */
  ASSERT_PTR_NOT_NULL(g_client->hashes);
  ASSERT_STR_EQUALS(g_client->hashes->hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_NUM_EQUALS(g_client->hashes->game_id, 1234);

  ASSERT_STR_EQUALS(g_external_event, "add_game_hash");

  rc_client_destroy(g_client);
}

static void test_set_allow_background_memory_reads(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->set_allow_background_memory_reads = rc_client_external_set_int;

  rc_client_set_allow_background_memory_reads(g_client, 0);
  ASSERT_NUM_EQUALS(g_external_int, 0);

  rc_client_set_allow_background_memory_reads(g_client, 1);
  ASSERT_NUM_EQUALS(g_external_int, 1);

  rc_client_destroy(g_client);
}

/* ----- login ----- */

static void test_v1_user_field_offsets(void)
{
  ASSERT_FIELD_OFFSET(rc_client_user_t, v1_rc_client_user_t, display_name);
  ASSERT_FIELD_OFFSET(rc_client_user_t, v1_rc_client_user_t, username);
  ASSERT_FIELD_OFFSET(rc_client_user_t, v1_rc_client_user_t, token);
  ASSERT_FIELD_OFFSET(rc_client_user_t, v1_rc_client_user_t, score);
  ASSERT_FIELD_OFFSET(rc_client_user_t, v1_rc_client_user_t, score_softcore);
  ASSERT_FIELD_OFFSET(rc_client_user_t, v1_rc_client_user_t, num_unread_messages);
}

static void test_v3_user_field_offsets(void)
{
  ASSERT_FIELD_OFFSET(rc_client_user_t, v3_rc_client_user_t, display_name);
  ASSERT_FIELD_OFFSET(rc_client_user_t, v3_rc_client_user_t, username);
  ASSERT_FIELD_OFFSET(rc_client_user_t, v3_rc_client_user_t, token);
  ASSERT_FIELD_OFFSET(rc_client_user_t, v3_rc_client_user_t, score);
  ASSERT_FIELD_OFFSET(rc_client_user_t, v3_rc_client_user_t, score_softcore);
  ASSERT_FIELD_OFFSET(rc_client_user_t, v3_rc_client_user_t, num_unread_messages);
  ASSERT_FIELD_OFFSET(rc_client_user_t, v3_rc_client_user_t, avatar_url);
}

static void assert_login_with_password(rc_client_t* client, const char* username, const char* password)
{
  ASSERT_PTR_EQUALS(client, g_client);

  ASSERT_STR_EQUALS(username, "User");
  ASSERT_STR_EQUALS(password, "Pa$$word");
}

static rc_client_async_handle_t* rc_client_external_login_with_password(rc_client_t* client,
    const char* username, const char* password, rc_client_callback_t callback, void* callback_userdata)
{
  assert_login_with_password(client, username, password);

  g_external_event = "login";

  callback(RC_OK, NULL, client, callback_userdata);
  return NULL;
}

static const rc_client_user_t* rc_client_external_get_user_info_v1(void)
{
  v1_rc_client_user_t* user = (v1_rc_client_user_t*)
      rc_buffer_alloc(&g_client->state.buffer, sizeof(v1_rc_client_user_t));

  memset(user, 0, sizeof(*user));
  user->display_name = "User";
  user->username = "User";
  user->token = "ApiToken";
  user->score = 12345;
  user->score_softcore = 123;
  user->num_unread_messages = 2;

  return (rc_client_user_t*)user;
}

static const rc_client_user_t* rc_client_external_get_user_info_v3(void)
{
  v3_rc_client_user_t* user = (v3_rc_client_user_t*)
    rc_buffer_alloc(&g_client->state.buffer, sizeof(v3_rc_client_user_t));

  memset(user, 0, sizeof(*user));
  user->display_name = "User";
  user->username = "User";
  user->token = "ApiToken";
  user->score = 12345;
  user->score_softcore = 123;
  user->num_unread_messages = 2;
  user->avatar_url = "/UserPic/User.png";

  return (rc_client_user_t*)user;
}

static void test_login_with_password(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_login_with_password = rc_client_external_login_with_password;
  g_client->state.external_client->get_user_info = rc_client_external_get_user_info_v1;
  g_client->state.external_client->get_user_info_v3 = rc_client_external_get_user_info_v3;

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "login");

  /* user data should come from external client. validate structure */
  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "User");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->score_softcore, 123);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);
  ASSERT_STR_EQUALS(user->avatar_url, "/UserPic/User.png");

  /* ensure non-external client user was not initialized */
  ASSERT_PTR_NULL(g_client->user.username);

  rc_client_destroy(g_client);
}

static void assert_login_with_token(rc_client_t* client, const char* username, const char* token)
{
  ASSERT_PTR_EQUALS(client, g_client);

  ASSERT_STR_EQUALS(username, "User");
  ASSERT_STR_EQUALS(token, "ApiToken");
}

static rc_client_async_handle_t* rc_client_external_login_with_token(rc_client_t* client,
  const char* username, const char* token, rc_client_callback_t callback, void* callback_userdata)
{
  assert_login_with_token(client, username, token);

  g_external_event = "login";

  callback(RC_OK, NULL, client, callback_userdata);
  return NULL;
}

static void test_login_with_token_v1(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_login_with_token = rc_client_external_login_with_token;
  g_client->state.external_client->get_user_info = rc_client_external_get_user_info_v1;

  rc_client_begin_login_with_token(g_client, "User", "ApiToken", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "login");

  /* user data should come from external client. validate structure */
  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "User");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->score_softcore, 123);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);
  ASSERT_STR_EQUALS(user->avatar_url, "https://media.retroachievements.org/UserPic/User.png");

  /* ensure non-external client user was not initialized */
  ASSERT_PTR_NULL(g_client->user.username);

  rc_client_destroy(g_client);
}

static const rc_client_user_t* rc_client_external_get_user_info_v1_long_name(void)
{
  v1_rc_client_user_t* user = (v1_rc_client_user_t*)
    rc_buffer_alloc(&g_client->state.buffer, sizeof(v1_rc_client_user_t));

  memset(user, 0, sizeof(*user));
  user->display_name = "TwentyCharUserNameXX";
  user->username = "TwentyCharUserNameXX";
  user->token = "ApiToken";
  user->score = 12345;
  user->score_softcore = 123;
  user->num_unread_messages = 2;

  return (rc_client_user_t*)user;
}

static rc_client_async_handle_t* rc_client_external_login_with_token_long_name(rc_client_t* client,
  const char* username, const char* token, rc_client_callback_t callback, void* callback_userdata)
{
  g_external_event = "login";

  callback(RC_OK, NULL, client, callback_userdata);
  return NULL;
}

static void test_login_with_token_v1_long_username(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_login_with_token = rc_client_external_login_with_token_long_name;
  g_client->state.external_client->get_user_info = rc_client_external_get_user_info_v1_long_name;

  rc_client_begin_login_with_token(g_client, "TwentyCharUserNameXX", "ApiToken", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "login");

  /* user data should come from external client. validate structure */
  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "TwentyCharUserNameXX");
  ASSERT_STR_EQUALS(user->display_name, "TwentyCharUserNameXX");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->score_softcore, 123);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);
  ASSERT_STR_EQUALS(user->avatar_url, "https://media.retroachievements.org/UserPic/TwentyCharUserNameXX.png");

  /* ensure non-external client user was not initialized */
  ASSERT_PTR_NULL(g_client->user.username);

  rc_client_destroy(g_client);
}

static const rc_client_user_t* rc_client_external_get_user_info_v1_too_long_name(void)
{
  v1_rc_client_user_t* user = (v1_rc_client_user_t*)
    rc_buffer_alloc(&g_client->state.buffer, sizeof(v1_rc_client_user_t));

  memset(user, 0, sizeof(*user));
  user->display_name = "ThisUserNameIsTooLongToFitIntoTheUserAvatarBufferWithoutOverflowing";
  user->username = "ThisUserNameIsTooLongToFitIntoTheUserAvatarBufferWithoutOverflowing";
  user->token = "ApiToken";
  user->score = 12345;
  user->score_softcore = 123;
  user->num_unread_messages = 2;

  return (rc_client_user_t*)user;
}

static void test_login_with_token_v1_too_long_username(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_login_with_token = rc_client_external_login_with_token_long_name;
  g_client->state.external_client->get_user_info = rc_client_external_get_user_info_v1_too_long_name;

  rc_client_begin_login_with_token(g_client, "ThisUserNameIsTooLongToFitIntoTheUserAvatarBufferWithoutOverflowing", "ApiToken", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "login");

  /* user data should come from external client. validate structure */
  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "ThisUserNameIsTooLongToFitIntoTheUserAvatarBufferWithoutOverflowing");
  ASSERT_STR_EQUALS(user->display_name, "ThisUserNameIsTooLongToFitIntoTheUserAvatarBufferWithoutOverflowing");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->score_softcore, 123);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);
  /* overly long URL will be truncated, but should not cause an exception.
   * test_login_with_token_v1_long_username validates the longest allowed username, so this shouldn't occur anyway */
  ASSERT_STR_EQUALS(user->avatar_url, "https://media.retroachievements.org/UserPic/ThisUserNameIsTooLongToFitIntoTheUs");

  /* ensure non-external client user was not initialized */
  ASSERT_PTR_NULL(g_client->user.username);

  rc_client_destroy(g_client);
}

static void test_login_with_token(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_login_with_token = rc_client_external_login_with_token;
  g_client->state.external_client->get_user_info_v3 = rc_client_external_get_user_info_v3;

  rc_client_begin_login_with_token(g_client, "User", "ApiToken", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "login");

  /* user data should come from external client. validate structure */
  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "User");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->score_softcore, 123);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);
  ASSERT_STR_EQUALS(user->avatar_url, "/UserPic/User.png");

  /* ensure non-external client user was not initialized */
  ASSERT_PTR_NULL(g_client->user.username);

  rc_client_destroy(g_client);
}

static void rc_client_external_logout(void)
{
  g_external_event = "logout";
}

static void test_logout(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->logout = rc_client_external_logout;

  /* external client should maintain its own state, but use the singular instance*/
  g_client->state.user = RC_CLIENT_USER_STATE_LOGGED_IN;

  rc_client_logout(g_client);
  ASSERT_STR_EQUALS(g_external_event, "logout");

  /* ensure non-external client user was not modified */
  ASSERT_NUM_EQUALS(g_client->state.user, RC_CLIENT_USER_STATE_LOGGED_IN);

  rc_client_destroy(g_client);
}

/* ----- load game ----- */

static void test_v1_game_field_offsets(void)
{
  ASSERT_FIELD_OFFSET(rc_client_game_t, v1_rc_client_game_t, id);
  ASSERT_FIELD_OFFSET(rc_client_game_t, v1_rc_client_game_t, console_id);
  ASSERT_FIELD_OFFSET(rc_client_game_t, v1_rc_client_game_t, title);
  ASSERT_FIELD_OFFSET(rc_client_game_t, v1_rc_client_game_t, hash);
  ASSERT_FIELD_OFFSET(rc_client_game_t, v1_rc_client_game_t, badge_name);
}

static void test_v3_game_field_offsets(void)
{
  ASSERT_FIELD_OFFSET(rc_client_game_t, v3_rc_client_game_t, id);
  ASSERT_FIELD_OFFSET(rc_client_game_t, v3_rc_client_game_t, console_id);
  ASSERT_FIELD_OFFSET(rc_client_game_t, v3_rc_client_game_t, title);
  ASSERT_FIELD_OFFSET(rc_client_game_t, v3_rc_client_game_t, hash);
  ASSERT_FIELD_OFFSET(rc_client_game_t, v3_rc_client_game_t, badge_name);
  ASSERT_FIELD_OFFSET(rc_client_game_t, v3_rc_client_game_t, badge_url);
}

static const rc_client_game_t* rc_client_external_get_game_info_v1(void)
{
  v1_rc_client_game_t* game = (v1_rc_client_game_t*)
    rc_buffer_alloc(&g_client->state.buffer, sizeof(v1_rc_client_game_t));

  memset(game, 0, sizeof(*game));
  game->id = 1234;
  game->console_id = RC_CONSOLE_PLAYSTATION;
  game->title = "Game Title";
  game->hash = "GAME_HASH";
  game->badge_name = "BDG001";

  return (const rc_client_game_t*)game;
}

static const rc_client_game_t* rc_client_external_get_game_info_v1_not_found(void)
{
  return NULL;
}

static const rc_client_game_t* rc_client_external_get_game_info_v3(void)
{
  v3_rc_client_game_t* game = (v3_rc_client_game_t*)
    rc_buffer_alloc(&g_client->state.buffer, sizeof(v3_rc_client_game_t));

  memset(game, 0, sizeof(*game));
  game->id = 1234;
  game->console_id = RC_CONSOLE_PLAYSTATION;
  game->title = "Game Title";
  game->hash = "GAME_HASH";
  game->badge_name = "BDG001";
  game->badge_url = "/Badge/BDG001.png";

  return (const rc_client_game_t*)game;
}

#ifdef RC_CLIENT_SUPPORTS_HASH

static void assert_identify_and_load_game(rc_client_t* client,
    uint32_t console_id, const char* file_path, const uint8_t* data, size_t data_size)
{
  ASSERT_PTR_EQUALS(client, g_client);

  ASSERT_NUM_EQUALS(console_id, RC_CONSOLE_GAMEBOY);
  ASSERT_STR_EQUALS(file_path, "foo.zip#foo.gb");
  ASSERT_PTR_NOT_NULL(data);
  ASSERT_NUM_EQUALS(32768, data_size);
}

static rc_client_async_handle_t* rc_client_external_identify_and_load_game(rc_client_t* client,
    uint32_t console_id, const char* file_path, const uint8_t* data, size_t data_size,
    rc_client_callback_t callback, void* callback_userdata)
{
  assert_identify_and_load_game(client, console_id, file_path, data, data_size);

  g_external_event = "load_game";

  callback(RC_OK, NULL, client, callback_userdata);
  return NULL;
}

static void test_identify_and_load_game_v1(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);
  const rc_client_game_t* game;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_identify_and_load_game = rc_client_external_identify_and_load_game;
  g_client->state.external_client->get_game_info = rc_client_external_get_game_info_v1;

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_GAMEBOY, "foo.zip#foo.gb",
    image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "load_game");

  /* user data should come from external client. validate structure */
  game = rc_client_get_game_info(g_client);
  ASSERT_PTR_NOT_NULL(game);
  ASSERT_NUM_EQUALS(game->id, 1234);
  ASSERT_NUM_EQUALS(game->console_id, RC_CONSOLE_PLAYSTATION);
  ASSERT_STR_EQUALS(game->title, "Game Title");
  ASSERT_STR_EQUALS(game->hash, "GAME_HASH");
  ASSERT_STR_EQUALS(game->badge_name, "BDG001");
  ASSERT_STR_EQUALS(game->badge_url, "https://media.retroachievements.org/Images/BDG001.png");

  /* ensure non-external client game was not initialized */
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);
  const rc_client_game_t* game;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_identify_and_load_game = rc_client_external_identify_and_load_game;
  g_client->state.external_client->get_game_info = rc_client_external_get_game_info_v1;
  g_client->state.external_client->get_game_info_v3 = rc_client_external_get_game_info_v3;

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_GAMEBOY, "foo.zip#foo.gb",
    image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "load_game");

  /* user data should come from external client. validate structure */
  game = rc_client_get_game_info(g_client);
  ASSERT_PTR_NOT_NULL(game);
  ASSERT_NUM_EQUALS(game->id, 1234);
  ASSERT_NUM_EQUALS(game->console_id, RC_CONSOLE_PLAYSTATION);
  ASSERT_STR_EQUALS(game->title, "Game Title");
  ASSERT_STR_EQUALS(game->hash, "GAME_HASH");
  ASSERT_STR_EQUALS(game->badge_name, "BDG001");
  ASSERT_STR_EQUALS(game->badge_url, "/Badge/BDG001.png");
  /* ensure non-external client game was not initialized */
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_reload_game(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);
  const rc_client_game_t* game;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_identify_and_load_game = rc_client_external_identify_and_load_game;
  g_client->state.external_client->get_game_info = rc_client_external_get_game_info_v1;
  g_client->state.external_client->get_game_info_v3 = rc_client_external_get_game_info_v3;
  g_client->state.external_client->unload_game = rc_client_external_unload_game;

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_GAMEBOY, "foo.zip#foo.gb",
    image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "load_game");

  /* user data should come from external client. validate structure */
  game = rc_client_get_game_info(g_client);
  ASSERT_PTR_NOT_NULL(game);
  ASSERT_NUM_EQUALS(game->id, 1234);
  ASSERT_NUM_EQUALS(game->console_id, RC_CONSOLE_PLAYSTATION);
  ASSERT_STR_EQUALS(game->title, "Game Title");
  ASSERT_STR_EQUALS(game->hash, "GAME_HASH");
  ASSERT_STR_EQUALS(game->badge_name, "BDG001");
  ASSERT_STR_EQUALS(game->badge_url, "/Badge/BDG001.png");
  /* ensure non-external client game was not initialized */
  ASSERT_PTR_NULL(g_client->game);

  rc_client_unload_game(g_client);

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_GAMEBOY, "foo.zip#foo.gb",
    image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "load_game");

  /* user data should come from external client. validate structure */
  game = rc_client_get_game_info(g_client);
  ASSERT_PTR_NOT_NULL(game);
  ASSERT_NUM_EQUALS(game->id, 1234);
  ASSERT_NUM_EQUALS(game->console_id, RC_CONSOLE_PLAYSTATION);
  ASSERT_STR_EQUALS(game->title, "Game Title");
  ASSERT_STR_EQUALS(game->hash, "GAME_HASH");
  ASSERT_STR_EQUALS(game->badge_name, "BDG001");
  ASSERT_STR_EQUALS(game->badge_url, "/Badge/BDG001.png");
  /* ensure non-external client game was not initialized */
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

#endif /* RC_CLIENT_SUPPORTS_HASH */

static void assert_load_game(rc_client_t* client, const char* hash)
{
  ASSERT_PTR_EQUALS(client, g_client);

  ASSERT_STR_EQUALS(hash, "6a2305a2b6675a97ff792709be1ca857");
}

static rc_client_async_handle_t* rc_client_external_load_game(rc_client_t* client,
  const char* hash, rc_client_callback_t callback, void* callback_userdata)
{
  assert_load_game(client, hash);

  g_external_event = "load_game";

  callback(RC_OK, NULL, client, callback_userdata);
  return NULL;
}

static void test_load_game_v1(void)
{
  const rc_client_game_t* game;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_load_game = rc_client_external_load_game;
  g_client->state.external_client->get_game_info = rc_client_external_get_game_info_v1;

  rc_client_begin_load_game(g_client, "6a2305a2b6675a97ff792709be1ca857", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "load_game");

  /* game data should come from external client. validate structure */
  game = rc_client_get_game_info(g_client);
  ASSERT_PTR_NOT_NULL(game);
  ASSERT_NUM_EQUALS(game->id, 1234);
  ASSERT_NUM_EQUALS(game->console_id, RC_CONSOLE_PLAYSTATION);
  ASSERT_STR_EQUALS(game->title, "Game Title");
  ASSERT_STR_EQUALS(game->hash, "GAME_HASH");
  ASSERT_STR_EQUALS(game->badge_name, "BDG001");
  ASSERT_STR_EQUALS(game->badge_url, "https://media.retroachievements.org/Images/BDG001.png");

  /* ensure non-external client user was not initialized */
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_load_game(void)
{
  const rc_client_game_t* game;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_load_game = rc_client_external_load_game;
  g_client->state.external_client->get_game_info = rc_client_external_get_game_info_v1;
  g_client->state.external_client->get_game_info_v3 = rc_client_external_get_game_info_v3;

  rc_client_begin_load_game(g_client, "6a2305a2b6675a97ff792709be1ca857", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "load_game");

  /* game data should come from external client. validate structure */
  game = rc_client_get_game_info(g_client);
  ASSERT_PTR_NOT_NULL(game);
  ASSERT_NUM_EQUALS(game->id, 1234);
  ASSERT_NUM_EQUALS(game->console_id, RC_CONSOLE_PLAYSTATION);
  ASSERT_STR_EQUALS(game->title, "Game Title");
  ASSERT_STR_EQUALS(game->hash, "GAME_HASH");
  ASSERT_STR_EQUALS(game->badge_name, "BDG001");
  ASSERT_STR_EQUALS(game->badge_url, "/Badge/BDG001.png");

  /* ensure non-external client user was not initialized */
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_get_game_info_v1_no_game_loaded(void)
{
  const rc_client_game_t* game;

  g_client = mock_client_with_external();
  g_client->state.external_client->get_game_info = rc_client_external_get_game_info_v1_not_found;

  /* game data should come from external client. validate structure */
  game = rc_client_get_game_info(g_client);
  ASSERT_PTR_NULL(game);

  rc_client_destroy(g_client);
}

static void test_v1_user_game_summary_field_offsets(void)
{
  ASSERT_FIELD_OFFSET(rc_client_user_game_summary_t, v1_rc_client_user_game_summary_t, num_core_achievements);
  ASSERT_FIELD_OFFSET(rc_client_user_game_summary_t, v1_rc_client_user_game_summary_t, num_unofficial_achievements);
  ASSERT_FIELD_OFFSET(rc_client_user_game_summary_t, v1_rc_client_user_game_summary_t, num_unlocked_achievements);
  ASSERT_FIELD_OFFSET(rc_client_user_game_summary_t, v1_rc_client_user_game_summary_t, num_unsupported_achievements);
  ASSERT_FIELD_OFFSET(rc_client_user_game_summary_t, v1_rc_client_user_game_summary_t, points_core);
  ASSERT_FIELD_OFFSET(rc_client_user_game_summary_t, v1_rc_client_user_game_summary_t, points_unlocked);
}

static void test_v5_user_game_summary_field_offsets(void)
{
  ASSERT_FIELD_OFFSET(rc_client_user_game_summary_t, v5_rc_client_user_game_summary_t, num_core_achievements);
  ASSERT_FIELD_OFFSET(rc_client_user_game_summary_t, v5_rc_client_user_game_summary_t, num_unofficial_achievements);
  ASSERT_FIELD_OFFSET(rc_client_user_game_summary_t, v5_rc_client_user_game_summary_t, num_unlocked_achievements);
  ASSERT_FIELD_OFFSET(rc_client_user_game_summary_t, v5_rc_client_user_game_summary_t, num_unsupported_achievements);
  ASSERT_FIELD_OFFSET(rc_client_user_game_summary_t, v5_rc_client_user_game_summary_t, points_core);
  ASSERT_FIELD_OFFSET(rc_client_user_game_summary_t, v5_rc_client_user_game_summary_t, points_unlocked);
  ASSERT_FIELD_OFFSET(rc_client_user_game_summary_t, v5_rc_client_user_game_summary_t, beaten_time);
  ASSERT_FIELD_OFFSET(rc_client_user_game_summary_t, v5_rc_client_user_game_summary_t, completed_time);
}

static void rc_client_external_get_user_game_summary(rc_client_user_game_summary_t* summary)
{
  summary->num_core_achievements = 20;
  summary->num_unlocked_achievements = 6;
  summary->num_unofficial_achievements = 3;
  summary->num_unsupported_achievements = 1;
  summary->points_core = 100;
  summary->points_unlocked = 23;
}

static void rc_client_external_get_user_game_summary_v5(rc_client_user_game_summary_t* summary)
{
  summary->num_core_achievements = 20;
  summary->num_unlocked_achievements = 6;
  summary->num_unofficial_achievements = 3;
  summary->num_unsupported_achievements = 1;
  summary->points_core = 100;
  summary->points_unlocked = 23;
  summary->beaten_time = 1234567890;
  summary->completed_time = 1234598760;
}

static void test_get_user_game_summary(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_with_external();
  g_client->state.external_client->get_user_game_summary = rc_client_external_get_user_game_summary;

  rc_client_get_user_game_summary(g_client, &summary);

  ASSERT_NUM_EQUALS(summary.num_core_achievements, 20);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 6);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 3);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 1);
  ASSERT_NUM_EQUALS(summary.points_core, 100);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 23);
  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_v5(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_with_external();
  g_client->state.external_client->get_user_game_summary_v5 = rc_client_external_get_user_game_summary_v5;

  rc_client_get_user_game_summary(g_client, &summary);

  ASSERT_NUM_EQUALS(summary.num_core_achievements, 20);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 6);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 3);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 1);
  ASSERT_NUM_EQUALS(summary.points_core, 100);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 23);
  ASSERT_NUM_EQUALS(summary.beaten_time, 1234567890);
  ASSERT_NUM_EQUALS(summary.completed_time, 1234598760);

  rc_client_destroy(g_client);
}

static void rc_client_external_get_user_subset_summary(uint32_t subset_id, rc_client_user_game_summary_t* summary)
{
  if (subset_id == 6) {
    summary->num_core_achievements = 20;
    summary->num_unlocked_achievements = 6;
    summary->num_unofficial_achievements = 3;
    summary->num_unsupported_achievements = 1;
    summary->points_core = 100;
    summary->points_unlocked = 23;
    summary->beaten_time = 1234567890;
    summary->completed_time = 1234598760;
  }
}

static void test_get_user_subset_summary(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_with_external();
  g_client->state.external_client->get_user_subset_summary = rc_client_external_get_user_subset_summary;

  rc_client_get_user_subset_summary(g_client, 1, &summary);

  ASSERT_NUM_EQUALS(summary.num_core_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.points_core, 0);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 0);
  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_get_user_subset_summary(g_client, 6, &summary);

  ASSERT_NUM_EQUALS(summary.num_core_achievements, 20);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 6);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 3);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 1);
  ASSERT_NUM_EQUALS(summary.points_core, 100);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 23);
  ASSERT_NUM_EQUALS(summary.beaten_time, 1234567890);
  ASSERT_NUM_EQUALS(summary.completed_time, 1234598760);

  rc_client_destroy(g_client);
}

#ifdef RC_CLIENT_SUPPORTS_HASH

static void test_identify_and_load_game_external_hash(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);
  const rc_client_game_t* game;

  g_client = mock_client_with_external();
  g_client->state.external_client->add_game_hash = rc_client_external_add_game_hash;
  g_client->state.external_client->begin_load_game = rc_client_external_load_game;
  g_client->state.external_client->get_game_info_v3 = rc_client_external_get_game_info_v3;

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");
  g_external_int = 0;

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_GAMEBOY, "foo.zip#foo.gb",
    image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "load_game"); /* begin_load_game called */
  ASSERT_NUM_EQUALS(g_external_int, 1234); /* add_game_hash called */
  ASSERT_PTR_NULL(g_client->state.load);

  /* user data should come from external client. validate structure */
  game = rc_client_get_game_info(g_client);
  ASSERT_PTR_NOT_NULL(game);
  ASSERT_NUM_EQUALS(game->id, 1234);
  ASSERT_NUM_EQUALS(game->console_id, RC_CONSOLE_PLAYSTATION);
  ASSERT_STR_EQUALS(game->title, "Game Title");
  ASSERT_STR_EQUALS(game->hash, "GAME_HASH");
  ASSERT_STR_EQUALS(game->badge_name, "BDG001");
  ASSERT_STR_EQUALS(game->badge_url, "/Badge/BDG001.png");

  /* ensure internal client game was initialized to hold media hashes */
  ASSERT_PTR_NOT_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_reload_game_external_hash(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);
  const rc_client_game_t* game;

  g_client = mock_client_with_external();
  g_client->state.external_client->add_game_hash = rc_client_external_add_game_hash;
  g_client->state.external_client->begin_load_game = rc_client_external_load_game;
  g_client->state.external_client->get_game_info_v3 = rc_client_external_get_game_info_v3;
  g_client->state.external_client->unload_game = rc_client_external_unload_game;

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");
  g_external_int = 0;

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_GAMEBOY, "foo.zip#foo.gb",
    image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "load_game"); /* begin_load_game called */
  ASSERT_NUM_EQUALS(g_external_int, 1234); /* add_game_hash called */
  ASSERT_PTR_NULL(g_client->state.load);

  /* user data should come from external client. validate structure */
  game = rc_client_get_game_info(g_client);
  ASSERT_PTR_NOT_NULL(game);
  ASSERT_NUM_EQUALS(game->id, 1234);
  ASSERT_NUM_EQUALS(game->console_id, RC_CONSOLE_PLAYSTATION);
  ASSERT_STR_EQUALS(game->title, "Game Title");
  ASSERT_STR_EQUALS(game->hash, "GAME_HASH");
  ASSERT_STR_EQUALS(game->badge_name, "BDG001");
  ASSERT_STR_EQUALS(game->badge_url, "/Badge/BDG001.png");

  /* ensure internal client game was initialized to hold media hashes */
  ASSERT_PTR_NOT_NULL(g_client->game);

  rc_client_unload_game(g_client);

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_GAMEBOY, "foo.zip#foo.gb",
    image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "load_game"); /* begin_load_game called */
  ASSERT_NUM_EQUALS(g_external_int, 1234); /* add_game_hash called */
  ASSERT_PTR_NULL(g_client->state.load);

  /* user data should come from external client. validate structure */
  game = rc_client_get_game_info(g_client);
  ASSERT_PTR_NOT_NULL(game);
  ASSERT_NUM_EQUALS(game->id, 1234);
  ASSERT_NUM_EQUALS(game->console_id, RC_CONSOLE_PLAYSTATION);
  ASSERT_STR_EQUALS(game->title, "Game Title");
  ASSERT_STR_EQUALS(game->hash, "GAME_HASH");
  ASSERT_STR_EQUALS(game->badge_name, "BDG001");
  ASSERT_STR_EQUALS(game->badge_url, "/Badge/BDG001.png");

  /* ensure internal client game was initialized to hold media hashes */
  ASSERT_PTR_NOT_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void assert_change_media(rc_client_t* client, const char* file_path, const uint8_t* data, size_t data_size)
{
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_STR_EQUALS(file_path, "foo.zip#foo.gb");
  ASSERT_PTR_NOT_NULL(data);
  ASSERT_NUM_EQUALS(data_size, 32768);
}

static rc_client_async_handle_t* rc_client_external_begin_identify_and_change_media(rc_client_t* client, const char* file_path,
  const uint8_t* data, size_t data_size, rc_client_callback_t callback, void* callback_userdata)
{
  assert_change_media(client, file_path, data, data_size);

  g_external_event = "change_media";

  callback(RC_OK, NULL, client, callback_userdata);
  return NULL;
}

static void test_change_media(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_identify_and_change_media = rc_client_external_begin_identify_and_change_media;

  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "change_media");

  rc_client_destroy(g_client);
  free(image);
}

#endif

static void assert_change_media_from_hash(rc_client_t* client, const char* hash)
{
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_STR_EQUALS(hash, "6a2305a2b6675a97ff792709be1ca857");
}

static rc_client_async_handle_t* rc_client_external_begin_change_media(rc_client_t* client, const char* hash,
    rc_client_callback_t callback, void* callback_userdata)
{
  assert_change_media_from_hash(client, hash);

  g_external_event = "change_media_from_hash";

  callback(RC_OK, NULL, client, callback_userdata);
  return NULL;
}

static void test_change_media_from_hash(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->begin_change_media = rc_client_external_begin_change_media;

  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "change_media_from_hash");

  rc_client_destroy(g_client);
}

static const rc_client_subset_t* rc_client_external_get_subset_info_v1(uint32_t subset_id)
{
  v1_rc_client_subset_t* subset = (v1_rc_client_subset_t*)
    rc_buffer_alloc(&g_client->state.buffer, sizeof(v1_rc_client_subset_t));

  memset(subset, 0, sizeof(*subset));
  subset->id = subset_id;
  subset->title = "Subset Title";
  snprintf(subset->badge_name, sizeof(subset->badge_name), "%s", "BDG001");
  subset->num_achievements = 2;
  subset->num_leaderboards = 1;

  return (const rc_client_subset_t*)subset;
}

static const rc_client_subset_t* rc_client_external_get_subset_info_v3(uint32_t subset_id)
{
  v3_rc_client_subset_t* subset = (v3_rc_client_subset_t*)
    rc_buffer_alloc(&g_client->state.buffer, sizeof(v3_rc_client_subset_t));

  memset(subset, 0, sizeof(*subset));
  subset->id = subset_id;
  subset->title = "Subset Title";
  snprintf(subset->badge_name, sizeof(subset->badge_name), "%s", "BDG001");
  subset->num_achievements = 2;
  subset->num_leaderboards = 1;
  subset->badge_url = "/Badge/BDG001.png";

  return (const rc_client_subset_t*)subset;
}

static void test_v1_subset_field_offsets(void)
{
  ASSERT_FIELD_OFFSET(rc_client_subset_t, v1_rc_client_subset_t, id);
  ASSERT_FIELD_OFFSET(rc_client_subset_t, v1_rc_client_subset_t, title);
  ASSERT_FIELD_OFFSET(rc_client_subset_t, v1_rc_client_subset_t, badge_name);
  ASSERT_FIELD_OFFSET(rc_client_subset_t, v1_rc_client_subset_t, num_achievements);
  ASSERT_FIELD_OFFSET(rc_client_subset_t, v1_rc_client_subset_t, num_leaderboards);
}

static void test_v3_subset_field_offsets(void)
{
  ASSERT_FIELD_OFFSET(rc_client_subset_t, v3_rc_client_subset_t, id);
  ASSERT_FIELD_OFFSET(rc_client_subset_t, v3_rc_client_subset_t, title);
  ASSERT_FIELD_OFFSET(rc_client_subset_t, v3_rc_client_subset_t, badge_name);
  ASSERT_FIELD_OFFSET(rc_client_subset_t, v3_rc_client_subset_t, num_achievements);
  ASSERT_FIELD_OFFSET(rc_client_subset_t, v3_rc_client_subset_t, num_leaderboards);
  ASSERT_FIELD_OFFSET(rc_client_subset_t, v3_rc_client_subset_t, badge_url);
}

static void test_get_subset_info_v1(void)
{
  const rc_client_subset_t* subset;

  g_client = mock_client_with_external();
  g_client->state.external_client->get_subset_info = rc_client_external_get_subset_info_v1;

  /* subset data should come from external client. validate structure */
  subset = rc_client_get_subset_info(g_client, 1234);
  ASSERT_PTR_NOT_NULL(subset);
  ASSERT_NUM_EQUALS(subset->id, 1234);
  ASSERT_STR_EQUALS(subset->title, "Subset Title");
  ASSERT_STR_EQUALS(subset->badge_name, "BDG001");
  ASSERT_NUM_EQUALS(subset->num_achievements, 2);
  ASSERT_NUM_EQUALS(subset->num_leaderboards, 1);
  ASSERT_STR_EQUALS(subset->badge_url, "https://media.retroachievements.org/Images/BDG001.png");

  rc_client_destroy(g_client);
}

static void test_get_subset_info(void)
{
  const rc_client_subset_t* subset;

  g_client = mock_client_with_external();
  g_client->state.external_client->get_subset_info_v3 = rc_client_external_get_subset_info_v3;

  /* subset data should come from external client. validate structure */
  subset = rc_client_get_subset_info(g_client, 1234);
  ASSERT_PTR_NOT_NULL(subset);
  ASSERT_NUM_EQUALS(subset->id, 1234);
  ASSERT_STR_EQUALS(subset->title, "Subset Title");
  ASSERT_STR_EQUALS(subset->badge_name, "BDG001");
  ASSERT_NUM_EQUALS(subset->num_achievements, 2);
  ASSERT_NUM_EQUALS(subset->num_leaderboards, 1);
  ASSERT_STR_EQUALS(subset->badge_url, "/Badge/BDG001.png");

  rc_client_destroy(g_client);
}

static void rc_client_external_destroy_subset_list(rc_client_subset_list_info_t* list)
{
  g_external_event = "destroyed";
  free(list);
}

static rc_client_subset_list_info_t* rc_client_external_create_subset_list_v6()
{
  rc_client_subset_list_info_t* list;

  list = (rc_client_subset_list_info_t*)calloc(1, sizeof(*list) + sizeof(rc_client_subset_t*) * 2);
  if (list) {
    const rc_client_subset_t** subset;
    rc_client_subset_t* mutable_subset;
    list->public_.num_subsets = 2;
    list->public_.subsets = subset = (const rc_client_subset_t**)((uint8_t*)list + sizeof(*list));
    *subset++ = rc_client_external_get_subset_info_v3(1111);
    *subset = rc_client_external_get_subset_info_v3(2345);
    mutable_subset = (rc_client_subset_t*)*subset;
    mutable_subset->title = "Bonus";
    mutable_subset->num_achievements = 1;
    mutable_subset->num_leaderboards = 0;

    list->destroy_func = rc_client_external_destroy_subset_list;
  }

  return list;
}

static void test_create_subset_list(void)
{
  rc_client_subset_list_t* list;

  g_client = mock_client_with_external();
  g_client->state.external_client->create_subset_list = rc_client_external_create_subset_list_v6;

  list = rc_client_create_subset_list(g_client);
  ASSERT_PTR_NOT_NULL(list);
  ASSERT_NUM_EQUALS(list->num_subsets, 2);
  ASSERT_PTR_NOT_NULL(list->subsets);
  ASSERT_NUM_EQUALS(list->subsets[0]->id, 1111);
  ASSERT_STR_EQUALS(list->subsets[0]->title, "Subset Title");
  ASSERT_STR_EQUALS(list->subsets[0]->badge_name, "BDG001");
  ASSERT_NUM_EQUALS(list->subsets[0]->num_achievements, 2);
  ASSERT_NUM_EQUALS(list->subsets[0]->num_leaderboards, 1);
  ASSERT_STR_EQUALS(list->subsets[0]->badge_url, "/Badge/BDG001.png");
  ASSERT_NUM_EQUALS(list->subsets[1]->id, 2345);
  ASSERT_STR_EQUALS(list->subsets[1]->title, "Bonus");
  ASSERT_STR_EQUALS(list->subsets[1]->badge_name, "BDG001");
  ASSERT_NUM_EQUALS(list->subsets[1]->num_achievements, 1);
  ASSERT_NUM_EQUALS(list->subsets[1]->num_leaderboards, 0);
  ASSERT_STR_EQUALS(list->subsets[1]->badge_url, "/Badge/BDG001.png");

  rc_client_destroy_subset_list(list);

  ASSERT_STR_EQUALS(g_external_event, "destroyed");

  rc_client_destroy(g_client);
}

static void test_unload_game(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->unload_game = rc_client_external_unload_game;

  rc_client_unload_game(g_client);

  ASSERT_STR_EQUALS(g_external_event, "unload_game");

  rc_client_destroy(g_client);
}

/* ----- achievements ----- */

static void test_v1_achievement_field_offsets(void)
{
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, id);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, description);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, badge_name);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, measured_progress);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, measured_percent);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, id);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, points);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, unlock_time);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, state);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, category);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, bucket);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, unlocked);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, rarity);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, rarity_hardcore);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v1_rc_client_achievement_t, type);
}

static void test_v3_achievement_field_offsets(void)
{
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, id);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, description);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, badge_name);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, measured_progress);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, measured_percent);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, id);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, points);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, unlock_time);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, state);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, category);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, bucket);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, unlocked);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, rarity);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, rarity_hardcore);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, type);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, badge_url);
  ASSERT_FIELD_OFFSET(rc_client_achievement_t, v3_rc_client_achievement_t, badge_locked_url);
}

static const rc_client_achievement_t* rc_client_external_get_achievement_info_v1(uint32_t id)
{
  v1_rc_client_achievement_t* achievement = (v1_rc_client_achievement_t*)
    rc_buffer_alloc(&g_client->state.buffer, sizeof(v1_rc_client_achievement_t));

  memset(achievement, 0, sizeof(*achievement));
  achievement->id = id;
  achievement->title = "Achievement Title";
  achievement->description = "Do something cool";
  memcpy(achievement->badge_name, "BDG1234", 8);
  achievement->measured_percent = 33.5;
  achievement->points = 5;
  achievement->state = RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE;
  achievement->category = RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE;
  achievement->bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED;
  achievement->unlocked = RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE;
  achievement->rarity = 75.0f;
  achievement->rarity_hardcore = 66.66f;
  achievement->type = RC_CLIENT_ACHIEVEMENT_TYPE_MISSABLE;

  return (const rc_client_achievement_t*)achievement;
}

static const rc_client_achievement_t* rc_client_external_get_achievement_info_v1_not_found(uint32_t id)
{
  return NULL;
}

static const rc_client_achievement_t* rc_client_external_get_achievement_info_v3(uint32_t id)
{
  v3_rc_client_achievement_t* achievement = (v3_rc_client_achievement_t*)
    rc_buffer_alloc(&g_client->state.buffer, sizeof(v3_rc_client_achievement_t));

  memset(achievement, 0, sizeof(*achievement));
  achievement->id = id;
  achievement->title = "Achievement Title";
  achievement->description = "Do something cool";
  memcpy(achievement->badge_name, "BDG1234", 8);
  achievement->measured_percent = 33.5;
  achievement->points = 5;
  achievement->state = RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE;
  achievement->category = RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE;
  achievement->bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED;
  achievement->unlocked = RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE;
  achievement->rarity = 75.0f;
  achievement->rarity_hardcore = 66.66f;
  achievement->type = RC_CLIENT_ACHIEVEMENT_TYPE_MISSABLE;
  achievement->badge_url = "/Badge/000234.png";
  achievement->badge_locked_url = "/Badge/000234_locked.png";
  return (const rc_client_achievement_t*)achievement;
}

static void test_has_achievements(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->has_achievements = rc_client_external_get_int;

  g_external_int = 0;
  ASSERT_NUM_EQUALS(rc_client_has_achievements(g_client), 0);

  g_external_int = 1;
  ASSERT_NUM_EQUALS(rc_client_has_achievements(g_client), 1);

  rc_client_destroy(g_client);
}

static void test_get_achievement_info_v1(void)
{
  const rc_client_achievement_t* achievement;

  g_client = mock_client_with_external();
  g_client->state.external_client->get_achievement_info = rc_client_external_get_achievement_info_v1;

  achievement = rc_client_get_achievement_info(g_client, 4);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->id, 4);
  ASSERT_STR_EQUALS(achievement->title, "Achievement Title");
  ASSERT_STR_EQUALS(achievement->description, "Do something cool");
  ASSERT_STR_EQUALS(achievement->badge_name, "BDG1234");
  ASSERT_FLOAT_EQUALS(achievement->measured_percent, 33.5);
  ASSERT_NUM_EQUALS(achievement->points, 5);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(achievement->category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
  ASSERT_FLOAT_EQUALS(achievement->rarity, 75.0f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 66.66f);
  ASSERT_NUM_EQUALS(achievement->type, RC_CLIENT_ACHIEVEMENT_TYPE_MISSABLE);
  ASSERT_STR_EQUALS(achievement->badge_url, "https://media.retroachievements.org/Badge/BDG1234.png");
  ASSERT_STR_EQUALS(achievement->badge_locked_url, "https://media.retroachievements.org/Badge/BDG1234_lock.png");

  rc_client_destroy(g_client);
}

static void test_get_achievement_info_v1_not_found(void)
{
  const rc_client_achievement_t* achievement;

  g_client = mock_client_with_external();
  g_client->state.external_client->get_achievement_info = rc_client_external_get_achievement_info_v1_not_found;

  achievement = rc_client_get_achievement_info(g_client, 4);
  ASSERT_PTR_NULL(achievement);

  rc_client_destroy(g_client);
}

static void test_get_achievement_info(void)
{
  const rc_client_achievement_t* achievement;

  g_client = mock_client_with_external();
  g_client->state.external_client->get_achievement_info = rc_client_external_get_achievement_info_v1;
  g_client->state.external_client->get_achievement_info_v3 = rc_client_external_get_achievement_info_v3;

  achievement = rc_client_get_achievement_info(g_client, 4);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->id, 4);
  ASSERT_STR_EQUALS(achievement->title, "Achievement Title");
  ASSERT_STR_EQUALS(achievement->description, "Do something cool");
  ASSERT_STR_EQUALS(achievement->badge_name, "BDG1234");
  ASSERT_FLOAT_EQUALS(achievement->measured_percent, 33.5);
  ASSERT_NUM_EQUALS(achievement->points, 5);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(achievement->category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
  ASSERT_FLOAT_EQUALS(achievement->rarity, 75.0f);
  ASSERT_FLOAT_EQUALS(achievement->rarity_hardcore, 66.66f);
  ASSERT_NUM_EQUALS(achievement->type, RC_CLIENT_ACHIEVEMENT_TYPE_MISSABLE);
  ASSERT_STR_EQUALS(achievement->badge_url, "/Badge/000234.png");
  ASSERT_STR_EQUALS(achievement->badge_locked_url, "/Badge/000234_locked.png");

  rc_client_destroy(g_client);
}

static void test_v1_achievement_list_field_offsets(void)
{
  ASSERT_FIELD_OFFSET(rc_client_achievement_list_info_t, v1_rc_client_achievement_list_info_t, public_);
  ASSERT_FIELD_OFFSET(rc_client_achievement_list_info_t, v1_rc_client_achievement_list_info_t, destroy_func);

  ASSERT_FIELD_OFFSET(rc_client_achievement_list_t, v1_rc_client_achievement_list_t, buckets);
  ASSERT_FIELD_OFFSET(rc_client_achievement_list_t, v1_rc_client_achievement_list_t, num_buckets);

  ASSERT_FIELD_OFFSET(rc_client_achievement_bucket_t, v1_rc_client_achievement_bucket_t, achievements);
  ASSERT_FIELD_OFFSET(rc_client_achievement_bucket_t, v1_rc_client_achievement_bucket_t, num_achievements);
  ASSERT_FIELD_OFFSET(rc_client_achievement_bucket_t, v1_rc_client_achievement_bucket_t, label);
  ASSERT_FIELD_OFFSET(rc_client_achievement_bucket_t, v1_rc_client_achievement_bucket_t, subset_id);
  ASSERT_FIELD_OFFSET(rc_client_achievement_bucket_t, v1_rc_client_achievement_bucket_t, bucket_type);
}

static void test_v3_achievement_list_field_offsets(void)
{
  ASSERT_FIELD_OFFSET(rc_client_achievement_list_info_t, v3_rc_client_achievement_list_info_t, public_);
  ASSERT_FIELD_OFFSET(rc_client_achievement_list_info_t, v3_rc_client_achievement_list_info_t, destroy_func);

  ASSERT_FIELD_OFFSET(rc_client_achievement_list_t, v3_rc_client_achievement_list_t, buckets);
  ASSERT_FIELD_OFFSET(rc_client_achievement_list_t, v3_rc_client_achievement_list_t, num_buckets);

  ASSERT_FIELD_OFFSET(rc_client_achievement_bucket_t, v3_rc_client_achievement_bucket_t, achievements);
  ASSERT_FIELD_OFFSET(rc_client_achievement_bucket_t, v3_rc_client_achievement_bucket_t, num_achievements);
  ASSERT_FIELD_OFFSET(rc_client_achievement_bucket_t, v3_rc_client_achievement_bucket_t, label);
  ASSERT_FIELD_OFFSET(rc_client_achievement_bucket_t, v3_rc_client_achievement_bucket_t, subset_id);
  ASSERT_FIELD_OFFSET(rc_client_achievement_bucket_t, v3_rc_client_achievement_bucket_t, bucket_type);
}

static void assert_achievement_list_category_grouping(int category, int grouping)
{
  ASSERT_NUM_EQUALS(category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
  ASSERT_NUM_EQUALS(grouping, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
}

static void rc_client_external_destroy_achievement_list(rc_client_achievement_list_info_t* list)
{
  g_external_event = "destroyed";
  free(list);
}

static rc_client_achievement_list_info_t* rc_client_external_create_achievement_list_v1(int category, int grouping)
{
  v1_rc_client_achievement_list_info_t* list;

  assert_achievement_list_category_grouping(category, grouping);

  list = (v1_rc_client_achievement_list_info_t*)calloc(1, sizeof(*list) + sizeof(v1_rc_client_achievement_bucket_t) + sizeof(v1_rc_client_achievement_t*) * 2);
  if (list) {
    list->public_.num_buckets = 1;
    list->public_.buckets = (v1_rc_client_achievement_bucket_t*)((uint8_t*)list + sizeof(*list));
    list->public_.buckets[0].achievements = (v1_rc_client_achievement_t**)((uint8_t*)list->public_.buckets + sizeof(*list->public_.buckets));
    list->public_.buckets[0].achievements[0] = (v1_rc_client_achievement_t*)rc_client_external_get_achievement_info_v1(1234);
    list->public_.buckets[0].achievements[1] = (v1_rc_client_achievement_t*)rc_client_external_get_achievement_info_v1(1235);
    list->public_.buckets[0].num_achievements = 2;
    list->public_.buckets[0].bucket_type = RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED;
    list->public_.buckets[0].label = "Locked";
    list->public_.buckets[0].subset_id = 1234;

    list->destroy_func = rc_client_external_destroy_achievement_list;
  }

  return (rc_client_achievement_list_info_t*)list;
}

static rc_client_achievement_list_info_t* rc_client_external_create_achievement_list_v3(int category, int grouping)
{
  v3_rc_client_achievement_list_info_t* list;

  assert_achievement_list_category_grouping(category, grouping);

  list = (v3_rc_client_achievement_list_info_t*)calloc(1, sizeof(*list) + sizeof(v3_rc_client_achievement_bucket_t) + sizeof(v3_rc_client_achievement_t*) * 2);
  if (list) {
    v3_rc_client_achievement_bucket_t* bucket;
    list->public_.num_buckets = 1;
    list->public_.buckets = bucket = (v3_rc_client_achievement_bucket_t*)((uint8_t*)list + sizeof(*list));
    bucket->achievements = (const v3_rc_client_achievement_t**)((uint8_t*)list->public_.buckets + sizeof(*list->public_.buckets));
    bucket->achievements[0] = (const v3_rc_client_achievement_t*)rc_client_external_get_achievement_info_v3(1234);
    bucket->achievements[1] = (const v3_rc_client_achievement_t*)rc_client_external_get_achievement_info_v3(1235);
    bucket->num_achievements = 2;
    bucket->bucket_type = RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED;
    bucket->label = "Locked";
    bucket->subset_id = 1234;

    list->destroy_func = rc_client_external_destroy_achievement_list;
  }

  return (rc_client_achievement_list_info_t*)list;
}

static void test_create_achievement_list_v1(void)
{
  rc_client_achievement_list_t* list;

  g_client = mock_client_with_external();
  g_client->state.external_client->create_achievement_list = rc_client_external_create_achievement_list_v1;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  ASSERT_NUM_EQUALS(list->num_buckets, 1);
  ASSERT_PTR_NOT_NULL(list->buckets);
  ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);
  ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 1234);
  ASSERT_NUM_EQUALS(list->buckets[0].achievements[1]->id, 1235);
  ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1234);
  ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");

  ASSERT_STR_EQUALS(list->buckets[0].achievements[0]->badge_url, "https://media.retroachievements.org/Badge/BDG1234.png");
  ASSERT_STR_EQUALS(list->buckets[0].achievements[0]->badge_locked_url, "https://media.retroachievements.org/Badge/BDG1234_lock.png");

  rc_client_destroy_achievement_list(list);

  ASSERT_STR_EQUALS(g_external_event, "destroyed");

  rc_client_destroy(g_client);
}

static void test_create_achievement_list(void)
{
  rc_client_achievement_list_t* list;

  g_client = mock_client_with_external();
  g_client->state.external_client->create_achievement_list = rc_client_external_create_achievement_list_v1;
  g_client->state.external_client->create_achievement_list_v3 = rc_client_external_create_achievement_list_v3;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  ASSERT_NUM_EQUALS(list->num_buckets, 1);
  ASSERT_PTR_NOT_NULL(list->buckets);
  ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);
  ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 1234);
  ASSERT_NUM_EQUALS(list->buckets[0].achievements[1]->id, 1235);
  ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1234);
  ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");

  // only difference between v1 and v3 create_achievement_list is the badge_url/badge_unlocked_url fields on each achievement
  ASSERT_STR_EQUALS(list->buckets[0].achievements[0]->badge_url, "/Badge/000234.png");
  ASSERT_STR_EQUALS(list->buckets[0].achievements[0]->badge_locked_url, "/Badge/000234_locked.png");

  rc_client_destroy_achievement_list(list);

  ASSERT_STR_EQUALS(g_external_event, "destroyed");

  rc_client_destroy(g_client);
}

/* ----- leaderboards ----- */

typedef struct v1_rc_client_leaderboard_t {
  const char* title;
  const char* description;
  const char* tracker_value;
  uint32_t id;
  uint8_t state;
  uint8_t format;
  uint8_t lower_is_better;
} v1_rc_client_leaderboard_t;

static const rc_client_leaderboard_t* rc_client_external_get_leaderboard_info(uint32_t id)
{
  v1_rc_client_leaderboard_t* leaderboard = (v1_rc_client_leaderboard_t*)
    rc_buffer_alloc(&g_client->state.buffer, sizeof(v1_rc_client_leaderboard_t));

  memset(leaderboard, 0, sizeof(*leaderboard));
  leaderboard->id = 1234;
  leaderboard->title = "Leaderboard Title";
  leaderboard->description = "Do something cool";
  leaderboard->tracker_value = "000250";
  leaderboard->state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
  leaderboard->format = RC_CLIENT_LEADERBOARD_FORMAT_SCORE;
  leaderboard->lower_is_better = 1;

  return (const rc_client_leaderboard_t*)leaderboard;
}

typedef struct v1_rc_client_leaderboard_bucket_t {
  rc_client_leaderboard_t** leaderboards;
  uint32_t num_leaderboards;

  const char* label;
  uint32_t subset_id;
  uint8_t bucket_type;
} v1_rc_client_leaderboard_bucket_t;

typedef struct v1_rc_client_leaderboard_list_t {
  v1_rc_client_leaderboard_bucket_t* buckets;
  uint32_t num_buckets;
} v1_rc_client_leaderboard_list_t;

typedef struct v1_rc_client_leaderboard_list_info_t {
  v1_rc_client_leaderboard_list_t public_;
  rc_client_destroy_leaderboard_list_func_t destroy_func;
} v1_rc_client_leaderboard_list_info_t;

static void assert_leaderboard_list_grouping(int grouping)
{
  ASSERT_NUM_EQUALS(grouping, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
}

static void rc_client_external_destroy_leaderboard_list(rc_client_leaderboard_list_info_t* list)
{
  g_external_event = "destroyed";
  free(list);
}

static rc_client_leaderboard_list_info_t* rc_client_external_create_leaderboard_list(int grouping)
{
  v1_rc_client_leaderboard_list_info_t* list;

  assert_leaderboard_list_grouping(grouping);

  list = (v1_rc_client_leaderboard_list_info_t*)calloc(1, sizeof(*list) + sizeof(v1_rc_client_leaderboard_bucket_t));
  if (list) {
    list->public_.num_buckets = 1;
    list->public_.buckets = (v1_rc_client_leaderboard_bucket_t*)((uint8_t*)list + sizeof(*list));
    list->public_.buckets[0].num_leaderboards = 2; /* didn't actually allocate these */
    list->public_.buckets[0].bucket_type = RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE;
    list->public_.buckets[0].label = "Inactive";
    list->public_.buckets[0].subset_id = 1234;

    list->destroy_func = rc_client_external_destroy_leaderboard_list;
  }

  return (rc_client_leaderboard_list_info_t*)list;
}

static void test_create_leaderboard_list(void)
{
  rc_client_leaderboard_list_t* list;

  g_client = mock_client_with_external();
  g_client->state.external_client->create_leaderboard_list = rc_client_external_create_leaderboard_list;

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  ASSERT_NUM_EQUALS(list->num_buckets, 1);
  ASSERT_PTR_NOT_NULL(list->buckets);
  ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 2);
  ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
  ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1234);
  ASSERT_STR_EQUALS(list->buckets[0].label, "Inactive");

  rc_client_destroy_leaderboard_list(list);

  rc_client_destroy(g_client);
}

static void test_has_leaderboards(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->has_leaderboards = rc_client_external_get_int;

  g_external_int = 0;
  ASSERT_NUM_EQUALS(rc_client_has_leaderboards(g_client), 0);

  g_external_int = 1;
  ASSERT_NUM_EQUALS(rc_client_has_leaderboards(g_client), 1);

  rc_client_destroy(g_client);
}

static void test_get_leaderboard_info(void)
{
  const rc_client_leaderboard_t* leaderboard;

  g_client = mock_client_with_external();
  g_client->state.external_client->get_leaderboard_info = rc_client_external_get_leaderboard_info;

  leaderboard = rc_client_get_leaderboard_info(g_client, 4);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->id, 1234);
  ASSERT_STR_EQUALS(leaderboard->title, "Leaderboard Title");
  ASSERT_STR_EQUALS(leaderboard->description, "Do something cool");
  ASSERT_STR_EQUALS(leaderboard->tracker_value, "000250");
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(leaderboard->format, RC_CLIENT_LEADERBOARD_FORMAT_SCORE);
  ASSERT_NUM_EQUALS(leaderboard->lower_is_better, 1);

  rc_client_destroy(g_client);
}

/* ----- rich presence ----- */

static size_t rc_client_external_get_rich_presence_message(char buffer[], size_t buffer_size)
{
  size_t result = snprintf(buffer, buffer_size, "Playing Game Title");
  if (result >= buffer_size)
    return (buffer_size - 1);
  return result;
}

static void test_get_rich_presence_message(void)
{
  char buffer[16];
  size_t result;

  g_client = mock_client_with_external();
  g_client->state.external_client->get_rich_presence_message = rc_client_external_get_rich_presence_message;

  result = rc_client_get_rich_presence_message(g_client, buffer, sizeof(buffer));

  ASSERT_STR_EQUALS(buffer, "Playing Game Ti");
  ASSERT_NUM_EQUALS(result, 15);

  rc_client_destroy(g_client);
}

static void test_has_rich_presence(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->has_rich_presence = rc_client_external_get_int;

  g_external_int = 0;
  ASSERT_NUM_EQUALS(rc_client_has_rich_presence(g_client), 0);

  g_external_int = 1;
  ASSERT_NUM_EQUALS(rc_client_has_rich_presence(g_client), 1);

  rc_client_destroy(g_client);
}

/* ----- processing ----- */

static void test_is_processing_required(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->is_processing_required = rc_client_external_get_int;

  g_external_int = 0;
  ASSERT_NUM_EQUALS(rc_client_is_processing_required(g_client), 0);

  g_external_int = 1;
  ASSERT_NUM_EQUALS(rc_client_is_processing_required(g_client), 1);

  rc_client_destroy(g_client);
}

static void rc_client_external_do_frame(void)
{
  g_external_event = "do_frame";
}

static void test_do_frame(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->do_frame = rc_client_external_do_frame;

  rc_client_do_frame(g_client);

  ASSERT_STR_EQUALS(g_external_event, "do_frame");

  rc_client_destroy(g_client);
}

static void rc_client_external_idle(void)
{
  g_external_event = "idle";
}

static void test_idle(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->idle = rc_client_external_idle;

  rc_client_idle(g_client);

  ASSERT_STR_EQUALS(g_external_event, "idle");

  rc_client_destroy(g_client);
}

static void rc_client_external_reset(void)
{
  g_external_event = "reset";
}

static int rc_client_external_can_pause(uint32_t* frames_remaining)
{
  *frames_remaining = g_external_int ? 0 : 10;

  return g_external_int;
}

static void test_can_pause(void)
{
  uint32_t frames_remaining;
  g_client = mock_client_with_external();
  g_client->state.external_client->can_pause = rc_client_external_can_pause;

  g_external_int = 0;
  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 0);
  ASSERT_NUM_EQUALS(frames_remaining, 10);

  g_external_int = 1;
  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);

  rc_client_destroy(g_client);
}

static void test_reset(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->reset = rc_client_external_reset;

  rc_client_reset(g_client);

  ASSERT_STR_EQUALS(g_external_event, "reset");

  rc_client_destroy(g_client);
}

/* ----- progress ----- */

static size_t rc_client_external_progress_size(void)
{
  return 12345678;
}

static void test_progress_size(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->progress_size = rc_client_external_progress_size;

  ASSERT_NUM_EQUALS(rc_client_progress_size(g_client), 12345678);

  rc_client_destroy(g_client);
}

static int rc_client_external_serialize_progress(uint8_t* buffer, size_t size)
{
  memcpy(buffer, "SAVED", 6);

  g_external_event = "serialize_progress";

  return RC_OK;
}

static void test_serialize_progress(void)
{
  int result;
  uint8_t buffer[8] = { 0 };

  g_client = mock_client_with_external();
  g_client->state.external_client->serialize_progress = rc_client_external_serialize_progress;

  result = rc_client_serialize_progress(g_client, buffer);

  ASSERT_STR_EQUALS(g_external_event, "serialize_progress");
  ASSERT_STR_EQUALS(buffer, "SAVED");
  ASSERT_NUM_EQUALS(result, RC_OK);

  rc_client_destroy(g_client);
}

static int rc_client_external_deserialize_progress(const uint8_t* buffer, size_t size)
{
  if (memcmp(buffer, "SAVE", 5) == 0)
    g_external_event = "deserialize_progress";

  return RC_OK;
}

static void test_deserialize_progress(void)
{
  int result;
  uint8_t buffer[8] = {'S', 'A', 'V', 'E'};

  g_client = mock_client_with_external();
  g_client->state.external_client->deserialize_progress = rc_client_external_deserialize_progress;

  result = rc_client_deserialize_progress(g_client, buffer);

  ASSERT_STR_EQUALS(g_external_event, "deserialize_progress");
  ASSERT_NUM_EQUALS(result, RC_OK);

  rc_client_destroy(g_client);
}

/* ----- harness ----- */

void test_client_external(void) {
  TEST_SUITE_BEGIN();

  /* settings */
  TEST(test_hardcore_enabled);
  TEST(test_unofficial_enabled);
  TEST(test_encore_mode_enabled);
  TEST(test_spectator_mode_enabled);
  TEST(test_enable_logging);
  TEST(test_event_handler);
  TEST(test_read_memory);
  TEST(test_get_time_millisecs);
  TEST(test_set_host);
  TEST(test_get_user_agent_clause);
  TEST(test_set_allow_background_memory_reads);

  /* login */
  TEST(test_v1_user_field_offsets);
  TEST(test_v3_user_field_offsets);
  TEST(test_login_with_password);
  TEST(test_login_with_token_v1);
  TEST(test_login_with_token_v1_long_username);
  TEST(test_login_with_token_v1_too_long_username);
  TEST(test_login_with_token);

  TEST(test_logout);

  /* load game */
  TEST(test_v1_game_field_offsets);
  TEST(test_v3_game_field_offsets);

#ifdef RC_CLIENT_SUPPORTS_HASH
  TEST(test_identify_and_load_game_v1);
  TEST(test_identify_and_load_game);
  TEST(test_identify_and_reload_game);
#endif
  TEST(test_load_game_v1);
  TEST(test_load_game);
  TEST(test_get_game_info_v1_no_game_loaded);
  TEST(test_v1_user_game_summary_field_offsets);
  TEST(test_v5_user_game_summary_field_offsets);
  TEST(test_get_user_game_summary);
  TEST(test_get_user_game_summary_v5);
  TEST(test_get_user_subset_summary);
#ifdef RC_CLIENT_SUPPORTS_HASH
  TEST(test_identify_and_load_game_external_hash);
  TEST(test_identify_and_reload_game_external_hash);
  TEST(test_change_media);
#endif
  TEST(test_change_media_from_hash);
  TEST(test_add_game_hash);

  TEST(test_unload_game);

  /* subsets */
  TEST(test_v1_subset_field_offsets);
  TEST(test_v3_subset_field_offsets);
  TEST(test_get_subset_info_v1);
  TEST(test_get_subset_info);
  TEST(test_create_subset_list);

  /* achievements */
  TEST(test_v1_achievement_field_offsets);
  TEST(test_v3_achievement_field_offsets);
  TEST(test_has_achievements);
  TEST(test_get_achievement_info_v1);
  TEST(test_get_achievement_info_v1_not_found);
  TEST(test_get_achievement_info);

  TEST(test_v1_achievement_list_field_offsets);
  TEST(test_v3_achievement_list_field_offsets);
  TEST(test_create_achievement_list_v1);
  TEST(test_create_achievement_list);

  /* leaderboards */
  TEST(test_create_leaderboard_list);
  TEST(test_has_leaderboards);
  TEST(test_get_leaderboard_info);

  /* rich presence */
  TEST(test_get_rich_presence_message);
  TEST(test_has_rich_presence);

  /* processing */
  TEST(test_is_processing_required);
  TEST(test_do_frame);
  TEST(test_idle);
  TEST(test_can_pause);
  TEST(test_reset);

  /* progress */
  TEST(test_progress_size);
  TEST(test_serialize_progress);
  TEST(test_deserialize_progress);

  TEST_SUITE_END();
}

#endif /* RC_CLIENT_SUPPORTS_EXTERNAL */
