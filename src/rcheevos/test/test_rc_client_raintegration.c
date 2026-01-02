#include "rc_client.h"

#include "rc_consoles.h"
#include "rc_hash.h"
#include "rc_internal.h"
#include "rc_api_runtime.h"

#include "../src/rc_client_internal.h"
#include "../src/rc_version.h"

#include "rhash/data.h"
#include "test_framework.h"

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION

static rc_client_t* g_client;
static const char* g_integration_event;
static void* g_callback_userdata = &g_client; /* dummy object to use for callback userdata validation */

/* begin from test_rc_client.c */

extern void rc_client_server_call(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client);
extern void rc_client_server_call_async(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client);
extern void reset_mock_api_handlers(void);
extern void mock_api_response(const char* request_params, const char* response_body);
extern void mock_api_error(const char* request_params, const char* response_body, int http_status_code);
extern void async_api_response(const char* request_params, const char* response_body);
extern void async_api_error(const char* request_params, const char* response_body, int http_status_code);

/* end from test_rc_client.c */

static uint32_t rc_client_read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client)
{
  return 0;
}

static rc_client_t* mock_client_with_integration()
{
  rc_client_t* client = rc_client_create(rc_client_read_memory, rc_client_server_call);
  client->state.raintegration = (rc_client_raintegration_t*)
      rc_buffer_alloc(&client->state.buffer, sizeof(*client->state.raintegration));
  memset(client->state.raintegration, 0, sizeof(*client->state.raintegration));

  rc_api_set_host(NULL);
  reset_mock_api_handlers();
  g_integration_event = "none";

  return client;
}

static rc_client_t* mock_client_with_integration_async()
{
  rc_client_t* client = mock_client_with_integration();
  client->callbacks.server_call = rc_client_server_call_async;
  return client;
}

static void rc_client_callback_expect_success(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_OK);
  ASSERT_PTR_NULL(error_message);
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void rc_client_callback_expect_uncalled(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_FAIL("Callback should not have been called.");
}

static uint32_t g_uint32;

static void rc_client_integration_set_uint(uint32_t value)
{
  g_uint32 = value;
}

/* ----- version ----- */

extern int rc_client_version_less(const char* left, const char* right);

static void test_version_less(const char* left, const char* right, int expected)
{
  if (expected == 0) {
    ASSERT_FALSE(rc_client_version_less(left, right));
  }
  else {
    ASSERT_TRUE(rc_client_version_less(left, right));
  }
}

/* ----- login ----- */

static void assert_init_params(HWND hWnd, const char* client_name, const char* client_version)
{
  ASSERT_PTR_NOT_NULL((void*)hWnd);
  ASSERT_STR_EQUALS(client_name, "TestClient");
  ASSERT_STR_EQUALS(client_version, "1.0.1");
}

static int rc_client_integration_init(HWND hWnd, const char* client_name, const char* client_version)
{
  assert_init_params(hWnd, client_name, client_version);

  g_integration_event = "init";
  return 1;
}

static int rc_client_get_external_client(rc_client_external_t* client, int nVersion)
{
  if (strcmp(g_integration_event, "init") == 0)
    g_integration_event = "init2";

  return 1;
}

static const char* rc_client_integration_get_version(void)
{
  return "1.3.0";
}

static const char* rc_client_integration_get_host_url_offline(void)
{
  return "OFFLINE";
}

static void test_load_raintegration(void)
{
  g_client = mock_client_with_integration();
  g_client->state.raintegration->get_version = rc_client_integration_get_version;
  g_client->state.raintegration->init_client = rc_client_integration_init;
  g_client->state.raintegration->get_external_client = rc_client_get_external_client;

  mock_api_response("r=latestintegration", "{\"Success\":true,\"MinimumVersion\":\"1.3.0\"}");

  rc_client_begin_load_raintegration(g_client, L"C:\\Client", (HWND)0x1234, "TestClient", "1.0.1",
      rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_integration_event, "init2");

  rc_client_destroy(g_client);
}

static void test_load_raintegration_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_with_integration_async();
  g_client->state.raintegration->get_version = rc_client_integration_get_version;
  g_client->state.raintegration->init_client = rc_client_integration_init;
  g_client->state.raintegration->get_external_client = rc_client_get_external_client;

  handle = rc_client_begin_load_raintegration(g_client, L"C:\\Client", (HWND)0x1234, "TestClient", "1.0.1",
      rc_client_callback_expect_uncalled, g_callback_userdata);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=latestintegration", "{\"Success\":true,\"MinimumVersion\":\"1.3.0\"}");

  ASSERT_STR_EQUALS(g_integration_event, "none");

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_outdated_version(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_ABORTED);
  ASSERT_STR_EQUALS(error_message, "RA_Integration version 1.3.0 is lower than minimum version 1.3.1");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_load_raintegration_outdated_version(void)
{
  g_client = mock_client_with_integration();
  g_client->state.raintegration->get_version = rc_client_integration_get_version;
  g_client->state.raintegration->init_client = rc_client_integration_init;
  g_client->state.raintegration->get_external_client = rc_client_get_external_client;

  mock_api_response("r=latestintegration", "{\"Success\":true,\"MinimumVersion\":\"1.3.1\"}");

  rc_client_begin_load_raintegration(g_client, L"C:\\Client", (HWND)0x1234, "TestClient", "1.0.1",
      rc_client_callback_expect_outdated_version, g_callback_userdata);

  ASSERT_STR_EQUALS(g_integration_event, "none");

  rc_client_destroy(g_client);
}

static void test_load_raintegration_supported_version(void)
{
  g_client = mock_client_with_integration();
  g_client->state.raintegration->get_version = rc_client_integration_get_version;
  g_client->state.raintegration->init_client = rc_client_integration_init;
  g_client->state.raintegration->get_external_client = rc_client_get_external_client;

  mock_api_response("r=latestintegration", "{\"Success\":true,\"MinimumVersion\":\"1.2.1\"}");

  rc_client_begin_load_raintegration(g_client, L"C:\\Client", (HWND)0x1234, "TestClient", "1.0.1",
      rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_integration_event, "init2");

  rc_client_destroy(g_client);
}

static void test_load_raintegration_offline(void)
{
  g_client = mock_client_with_integration();
  g_client->state.raintegration->get_host_url = rc_client_integration_get_host_url_offline;
  g_client->state.raintegration->get_version = rc_client_integration_get_version;
  g_client->state.raintegration->init_client_offline = rc_client_integration_init;
  g_client->state.raintegration->get_external_client = rc_client_get_external_client;

  mock_api_response("r=latestintegration", "{\"Success\":true,\"MinimumVersion\":\"1.2.1\"}");

  rc_client_begin_load_raintegration(g_client, L"C:\\Client", (HWND)0x1234, "TestClient", "1.0.1",
    rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_integration_event, "init2");

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_after_login_failure(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "Cannot initialize RAIntegration after login");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_load_raintegration_after_login(void)
{
  g_client = mock_client_with_integration();
  g_client->state.raintegration->get_version = rc_client_integration_get_version;
  g_client->state.raintegration->init_client = rc_client_integration_init;
  g_client->state.raintegration->get_external_client = rc_client_get_external_client;
  g_client->state.user = RC_CLIENT_USER_STATE_LOGIN_REQUESTED;

  mock_api_response("r=latestintegration", "{\"Success\":true,\"MinimumVersion\":\"1.3.0\"}");

  rc_client_begin_load_raintegration(g_client, L"C:\\Client", (HWND)0x1234, "TestClient", "1.0.1",
      rc_client_callback_expect_after_login_failure, g_callback_userdata);

  ASSERT_STR_EQUALS(g_integration_event, "none");

  rc_client_destroy(g_client);
}

/* ----- windows ----- */

static HWND g_hWnd;

static void rc_client_integration_update_main_window_handle(HWND hWnd)
{
  g_hWnd = hWnd;
}

static void test_update_main_window_handle(void)
{
  HWND hWnd = (HWND)0x12345678;
  g_hWnd = NULL;

  g_client = mock_client_with_integration();
  g_client->state.raintegration->update_main_window_handle = rc_client_integration_update_main_window_handle;

  /* does nothing if raintegration hasn't been initialized */
  ASSERT_NUM_EQUALS(g_client->state.raintegration->bIsInited, 0);
  rc_client_raintegration_update_main_window_handle(g_client, hWnd);
  ASSERT_PTR_NULL(g_hWnd);

  g_client->state.raintegration->bIsInited = 1;
  rc_client_raintegration_update_main_window_handle(g_client, hWnd);
  ASSERT_PTR_EQUALS(g_hWnd, hWnd);

  g_hWnd = NULL;
}

/* ----- menu ----- */

static rc_client_raintegration_menu_t* g_menu;

static const rc_client_raintegration_menu_t* rc_client_integration_get_menu(void)
{
  return g_menu;
}

static void test_get_menu(void)
{
  const rc_client_raintegration_menu_t* menu;
  rc_client_raintegration_menu_t local_menu;
  rc_client_raintegration_menu_item_t local_menu_items[3];

  memset(&local_menu_items, 0, sizeof(local_menu_items));
  local_menu_items[0].id = 1234;
  local_menu_items[0].label = "Label 1";
  local_menu_items[0].checked = 1;
  local_menu_items[0].enabled = 1;
  local_menu_items[2].id = 2345;
  local_menu_items[2].label = "Label 2";

  local_menu.num_items = sizeof(local_menu_items) / sizeof(local_menu_items[0]);
  local_menu.items = local_menu_items;
  g_menu = &local_menu;

  g_client = mock_client_with_integration();
  g_client->state.raintegration->get_menu = rc_client_integration_get_menu;

  /* returns null if raintegration hasn't been initialized */
  ASSERT_NUM_EQUALS(g_client->state.raintegration->bIsInited, 0);
  menu = rc_client_raintegration_get_menu(g_client);
  ASSERT_PTR_NULL(menu);

  g_client->state.raintegration->bIsInited = 1;
  menu = rc_client_raintegration_get_menu(g_client);
  ASSERT_PTR_NOT_NULL(menu);

  ASSERT_NUM_EQUALS(menu->num_items, 3);
  ASSERT_NUM_EQUALS(menu->items[0].id, 1234);
  ASSERT_STR_EQUALS(menu->items[0].label, "Label 1");
  ASSERT_NUM_EQUALS(menu->items[0].checked, 1);
  ASSERT_NUM_EQUALS(menu->items[0].enabled, 1);
  ASSERT_NUM_EQUALS(menu->items[1].id, 0);
  ASSERT_PTR_NULL(menu->items[1].label);
  ASSERT_NUM_EQUALS(menu->items[1].checked, 0);
  ASSERT_NUM_EQUALS(menu->items[1].enabled, 0);
  ASSERT_NUM_EQUALS(menu->items[2].id, 2345);
  ASSERT_STR_EQUALS(menu->items[2].label, "Label 2");
  ASSERT_NUM_EQUALS(menu->items[2].checked, 0);
  ASSERT_NUM_EQUALS(menu->items[2].enabled, 0);

  g_menu = NULL;
}

static int rc_client_integration_activate_menu_item(uint32_t id)
{
  if (id < 1700)
    return 0;

  g_uint32 = id;
  return 1;
}

static void test_activate_menu_item(void)
{
  g_client = mock_client_with_integration();
  g_client->state.raintegration->activate_menu_item = rc_client_integration_activate_menu_item;

  g_uint32 = 0;
  ASSERT_NUM_EQUALS(rc_client_raintegration_activate_menu_item(g_client, 1600), 0);
  ASSERT_NUM_EQUALS(g_uint32, 0);

  ASSERT_NUM_EQUALS(rc_client_raintegration_activate_menu_item(g_client, 1704), 1);
  ASSERT_NUM_EQUALS(g_uint32, 1704);
}

static void rc_client_integration_set_console_id(int console_id)
{
  g_uint32 = console_id;
}

static void test_set_console_id(void)
{
  g_client = mock_client_with_integration();
  g_client->state.raintegration->set_console_id = rc_client_integration_set_console_id;

  g_uint32 = 0;
  rc_client_raintegration_set_console_id(g_client, RC_CONSOLE_NINTENDO);
  ASSERT_NUM_EQUALS(g_uint32, RC_CONSOLE_NINTENDO);

  rc_client_raintegration_set_console_id(g_client, RC_CONSOLE_PLAYSTATION);
  ASSERT_NUM_EQUALS(g_uint32, RC_CONSOLE_PLAYSTATION);

  rc_client_raintegration_set_console_id(g_client, RC_CONSOLE_MEGA_DRIVE);
  ASSERT_NUM_EQUALS(g_uint32, RC_CONSOLE_MEGA_DRIVE);
}

static int rc_client_integration_has_modifications(void)
{
  return (int)g_uint32;
}

static void test_has_modifications(void)
{
  g_client = mock_client_with_integration();
  g_client->state.raintegration->has_modifications = rc_client_integration_has_modifications;

  g_uint32 = 0;
  ASSERT_NUM_EQUALS(rc_client_raintegration_has_modifications(g_client), 0);

  g_uint32 = 1;
  ASSERT_NUM_EQUALS(rc_client_raintegration_has_modifications(g_client), 0);

  g_client->state.raintegration->bIsInited = 1;
  ASSERT_NUM_EQUALS(rc_client_raintegration_has_modifications(g_client), 1);

  g_uint32 = 0;
  ASSERT_NUM_EQUALS(rc_client_raintegration_has_modifications(g_client), 0);
}

/* ----- harness ----- */

void test_client_raintegration(void) {
  TEST_SUITE_BEGIN();

  /* version */
  TEST_PARAMS3(test_version_less, "0.0.0", "1.0.0", 1);
  TEST_PARAMS3(test_version_less, "1.0.0", "0.0.0", 0);
  TEST_PARAMS3(test_version_less, "1.2.0", "1.2.0", 0);
  TEST_PARAMS3(test_version_less, "1.2.0", "1.1.0", 0);
  TEST_PARAMS3(test_version_less, "1.2.0", "1.3.0", 1);
  TEST_PARAMS3(test_version_less, "1.2.0", "1.10.0", 1);
  TEST_PARAMS3(test_version_less, "1.2.0", "2.1.0", 1);
  TEST_PARAMS3(test_version_less, "2.1.0", "1.2.0", 0);

  /* login */
  TEST(test_load_raintegration);
  TEST(test_load_raintegration_aborted);
  TEST(test_load_raintegration_outdated_version);
  TEST(test_load_raintegration_supported_version);
  TEST(test_load_raintegration_offline);
  TEST(test_load_raintegration_after_login);

  /* windows */
  TEST(test_update_main_window_handle);

  /* menu */
  TEST(test_get_menu);
  TEST(test_activate_menu_item);

  /* set_console_id */
  TEST(test_set_console_id);

  /* has_modifications */
  TEST(test_has_modifications);

  TEST_SUITE_END();
}

#endif /* RC_CLIENT_SUPPORTS_RAINTEGRATION */
