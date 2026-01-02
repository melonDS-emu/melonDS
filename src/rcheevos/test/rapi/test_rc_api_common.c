#include "../rapi/rc_api_common.h"

#include "rc_api_runtime.h" /* for rc_fetch_image */

#include "../rc_compat.h"

#include "../test_framework.h"

#define IMAGEREQUEST_URL "https://media.retroachievements.org"

static void _assert_json_parse_response(rc_api_response_t* response, rc_json_field_t* field, const char* json, int expected_result) {
  int result;
  rc_api_server_response_t server_response;
  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Test")
  };
  rc_buffer_init(&response->buffer);

  memset(&server_response, 0, sizeof(server_response));
  server_response.body = json;
  server_response.body_length = strlen(json);

  result = rc_json_parse_server_response(response, &server_response, fields, sizeof(fields)/sizeof(fields[0]));
  ASSERT_NUM_EQUALS(result, expected_result);

  ASSERT_NUM_EQUALS(response->succeeded, 1);
  ASSERT_PTR_NULL(response->error_message);

  memcpy(field, &fields[2], sizeof(fields[2]));
}
#define assert_json_parse_response(response, field, json, expected_result) ASSERT_HELPER(_assert_json_parse_response(response, field, json, expected_result), "assert_json_parse_operand")

static void _assert_field_value(rc_json_field_t* field, const char* expected_value) {
  char buffer[256];

  ASSERT_PTR_NOT_NULL(field->value_start);
  ASSERT_PTR_NOT_NULL(field->value_end);
  ASSERT_NUM_LESS(field->value_end - field->value_start, sizeof(buffer));

  memcpy(buffer, field->value_start, field->value_end - field->value_start);
  buffer[field->value_end - field->value_start] = '\0';
  ASSERT_STR_EQUALS(buffer, expected_value);
}
#define assert_field_value(field, expected_value) ASSERT_HELPER(_assert_field_value(field, expected_value), "assert_field_value")

static void test_json_parse_response_empty() {
  rc_api_response_t response;
  rc_json_field_t field;

  assert_json_parse_response(&response, &field, "{}", RC_OK);

  ASSERT_STR_EQUALS(field.name, "Test");
  ASSERT_PTR_NULL(field.value_start);
  ASSERT_PTR_NULL(field.value_end);
}

static void test_json_parse_response_field(const char* json, const char* value) {
  rc_api_response_t response;
  rc_json_field_t field;

  assert_json_parse_response(&response, &field, json, RC_OK);

  ASSERT_STR_EQUALS(field.name, "Test");
  assert_field_value(&field, value);
}

static void test_json_parse_response_non_json() {
  int result;
  rc_api_server_response_t server_response;
  rc_api_response_t response;
  const char* error_message = "This is an error.";
  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Test")
  };
  rc_buffer_init(&response.buffer);

  memset(&server_response, 0, sizeof(server_response));
  server_response.body = error_message;
  server_response.body_length = strlen(error_message);

  result = rc_json_parse_server_response(&response, &server_response, fields, sizeof(fields) / sizeof(fields[0]));
  ASSERT_NUM_EQUALS(result, RC_INVALID_JSON);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "This is an error.");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_parse_response_non_json_bounded() {
  int result;
  rc_api_server_response_t server_response;
  rc_api_response_t response;
  const char* error_message = "This is an error.\r\n<title>Should not be seen</title>";
  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Test")
  };
  rc_buffer_init(&response.buffer);

  memset(&server_response, 0, sizeof(server_response));
  server_response.body = error_message;
  server_response.body_length = 16; /* "This is an error" (no period, newline, etc) */

  result = rc_json_parse_server_response(&response, &server_response, fields, sizeof(fields) / sizeof(fields[0]));
  ASSERT_NUM_EQUALS(result, RC_INVALID_JSON);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "This is an error");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_parse_response_451() {
  int result;
  rc_api_server_response_t server_response;
  rc_api_response_t response;
  const char* error_message = "<html>"
    "<head><title>Unavailable For Legal Reasons</title></head>"
    "<body>"
    "<h1>Unavailable For Legal Reasons</h1>"
    "<p>This request may not be serviced in the Roman Province"
    "of Judea due to the Lex Julia Majestatis, which disallows"
    "access to resources hosted on servers deemed to be"
    "operated by the People's Front of Judea.</p>"
    "</body>"
    "</html>";
  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Test")
  };
  rc_buffer_init(&response.buffer);

  memset(&server_response, 0, sizeof(server_response));
  server_response.body = error_message;
  server_response.body_length = strlen(error_message);
  server_response.http_status_code = 451;

  result = rc_json_parse_server_response(&response, &server_response, fields, sizeof(fields) / sizeof(fields[0]));
  ASSERT_NUM_EQUALS(result, RC_INVALID_JSON);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "Unavailable For Legal Reasons");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_parse_response_error_from_server() {
  int result;
  rc_api_server_response_t server_response;
  rc_api_response_t response;
  const char* json;
  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Test")
  };
  rc_buffer_init(&response.buffer);

  json = "{\"Success\":false,\"Error\":\"Oops\"}";
  memset(&server_response, 0, sizeof(server_response));
  server_response.body = json;
  server_response.body_length = strlen(json);

  result = rc_json_parse_server_response(&response, &server_response, fields, sizeof(fields)/sizeof(fields[0]));
  ASSERT_NUM_EQUALS(result, RC_OK);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "Oops");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_parse_response_incorrect_size() {
  int result;
  rc_api_server_response_t server_response;
  rc_api_response_t response;
  const char* json;
  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Test")
  };
  rc_buffer_init(&response.buffer);

  json = "{\"Success\":false,\"Error\":\"Oops\"}";
  memset(&server_response, 0, sizeof(server_response));
  server_response.body = json;
  server_response.body_length = strlen(json) - 1;

  result = rc_json_parse_server_response(&response, &server_response, fields, sizeof(fields) / sizeof(fields[0]));
  ASSERT_NUM_EQUALS(result, RC_INVALID_JSON);

  /* the error message was found before the parser failed */
  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "Oops");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_get_string(const char* escaped, const char* expected) {
  rc_api_response_t response;
  rc_json_field_t field;
  char buffer[256];
  const char *value = NULL;
  snprintf(buffer, sizeof(buffer), "{\"Test\":\"%s\"}", escaped);

  assert_json_parse_response(&response, &field, buffer, RC_OK);

  ASSERT_TRUE(rc_json_get_string(&value, &response.buffer, &field, "Test"));
  ASSERT_PTR_NOT_NULL(value);
  ASSERT_STR_EQUALS(value, expected);
}

static void test_json_get_optional_string() {
  rc_api_response_t response;
  rc_json_field_t field;
  const char *value = NULL;

  assert_json_parse_response(&response, &field, "{\"Test\":\"Value\"}", RC_OK);

  rc_json_get_optional_string(&value, &response, &field, "Test", "Default");
  ASSERT_PTR_NOT_NULL(value);
  ASSERT_STR_EQUALS(value, "Value");

  assert_json_parse_response(&response, &field, "{\"Test2\":\"Value\"}", RC_OK);

  rc_json_get_optional_string(&value, &response, &field, "Test", "Default");
  ASSERT_PTR_NOT_NULL(value);
  ASSERT_STR_EQUALS(value, "Default");
}

static void test_json_get_required_string() {
  rc_api_response_t response;
  rc_json_field_t field;
  const char *value = NULL;

  assert_json_parse_response(&response, &field, "{\"Test\":\"Value\"}", RC_OK);

  ASSERT_TRUE(rc_json_get_required_string(&value, &response, &field, "Test"));
  ASSERT_PTR_NOT_NULL(value);
  ASSERT_STR_EQUALS(value, "Value");

  ASSERT_PTR_NULL(response.error_message);
  ASSERT_NUM_EQUALS(response.succeeded, 1);

  assert_json_parse_response(&response, &field, "{\"Test2\":\"Value\"}", RC_OK);

  ASSERT_FALSE(rc_json_get_required_string(&value, &response, &field, "Test"));
  ASSERT_PTR_NULL(value);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "Test not found in response");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_get_num(const char* input, int expected) {
  rc_api_response_t response;
  rc_json_field_t field;
  char buffer[64];
  int value = 0;
  snprintf(buffer, sizeof(buffer), "{\"Test\":%s}", input);

  assert_json_parse_response(&response, &field, buffer, RC_OK);

  if (expected) {
    ASSERT_TRUE(rc_json_get_num(&value, &field, "Test"));
  }
  else {
    ASSERT_FALSE(rc_json_get_num(&value, &field, "Test"));
  }

  ASSERT_NUM_EQUALS(value, expected);
}

static void test_json_get_optional_num() {
  rc_api_response_t response;
  rc_json_field_t field;
  int value = 0;

  assert_json_parse_response(&response, &field, "{\"Test\":12345678}", RC_OK);

  rc_json_get_optional_num(&value, &field, "Test", 9999);
  ASSERT_NUM_EQUALS(value, 12345678);

  assert_json_parse_response(&response, &field, "{\"Test2\":12345678}", RC_OK);

  rc_json_get_optional_num(&value, &field, "Test", 9999);
  ASSERT_NUM_EQUALS(value, 9999);
}

static void test_json_get_required_num() {
  rc_api_response_t response;
  rc_json_field_t field;
  int value = 0;

  assert_json_parse_response(&response, &field, "{\"Test\":12345678}", RC_OK);

  ASSERT_TRUE(rc_json_get_required_num(&value, &response, &field, "Test"));
  ASSERT_NUM_EQUALS(value, 12345678);

  ASSERT_PTR_NULL(response.error_message);
  ASSERT_NUM_EQUALS(response.succeeded, 1);

  assert_json_parse_response(&response, &field, "{\"Test2\":12345678}", RC_OK);

  ASSERT_FALSE(rc_json_get_required_num(&value, &response, &field, "Test"));
  ASSERT_NUM_EQUALS(value, 0);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "Test not found in response");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_get_unum(const char* input, uint32_t expected) {
  rc_api_response_t response;
  rc_json_field_t field;
  char buffer[64];
  uint32_t value = 0;
  snprintf(buffer, sizeof(buffer), "{\"Test\":%s}", input);

  assert_json_parse_response(&response, &field, buffer, RC_OK);

  if (expected) {
    ASSERT_TRUE(rc_json_get_unum(&value, &field, "Test"));
  }
  else {
    ASSERT_FALSE(rc_json_get_unum(&value, &field, "Test"));
  }

  ASSERT_NUM_EQUALS(value, expected);
}

static void test_json_get_optional_unum() {
  rc_api_response_t response;
  rc_json_field_t field;
  uint32_t value = 0;

  assert_json_parse_response(&response, &field, "{\"Test\":12345678}", RC_OK);

  rc_json_get_optional_unum(&value, &field, "Test", 9999);
  ASSERT_NUM_EQUALS(value, 12345678);

  assert_json_parse_response(&response, &field, "{\"Test2\":12345678}", RC_OK);

  rc_json_get_optional_unum(&value, &field, "Test", 9999);
  ASSERT_NUM_EQUALS(value, 9999);
}

static void test_json_get_required_unum() {
  rc_api_response_t response;
  rc_json_field_t field;
  uint32_t value = 0;

  assert_json_parse_response(&response, &field, "{\"Test\":12345678}", RC_OK);

  ASSERT_TRUE(rc_json_get_required_unum(&value, &response, &field, "Test"));
  ASSERT_NUM_EQUALS(value, 12345678);

  ASSERT_PTR_NULL(response.error_message);
  ASSERT_NUM_EQUALS(response.succeeded, 1);

  assert_json_parse_response(&response, &field, "{\"Test2\":12345678}", RC_OK);

  ASSERT_FALSE(rc_json_get_required_unum(&value, &response, &field, "Test"));
  ASSERT_NUM_EQUALS(value, 0);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "Test not found in response");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_get_float(const char* input, float expected)
{
  rc_api_response_t response;
  rc_json_field_t field;
  char buffer[64];
  float value = 0.0;
  snprintf(buffer, sizeof(buffer), "{\"Test\":%s}", input);

  assert_json_parse_response(&response, &field, buffer, RC_OK);

  if (expected)
  {
    ASSERT_TRUE(rc_json_get_float(&value, &field, "Test"));
  }
  else
  {
    ASSERT_FALSE(rc_json_get_float(&value, &field, "Test"));
  }

  ASSERT_FLOAT_EQUALS(value, expected);
}

static void test_json_get_optional_float()
{
  rc_api_response_t response;
  rc_json_field_t field;
  float value = 0.0;

  assert_json_parse_response(&response, &field, "{\"Test\":1.5}", RC_OK);

  rc_json_get_optional_float(&value, &field, "Test", 9999);
  ASSERT_FLOAT_EQUALS(value, 1.5);

  assert_json_parse_response(&response, &field, "{\"Test2\":1.5}", RC_OK);

  rc_json_get_optional_float(&value, &field, "Test", 2.5);
  ASSERT_FLOAT_EQUALS(value, 2.5);
}

static void test_json_get_required_float()
{
  rc_api_response_t response;
  rc_json_field_t field;
  float value = 0.0;

  assert_json_parse_response(&response, &field, "{\"Test\":1.5}", RC_OK);

  ASSERT_TRUE(rc_json_get_required_float(&value, &response, &field, "Test"));
  ASSERT_FLOAT_EQUALS(value, 1.5f);

  ASSERT_PTR_NULL(response.error_message);
  ASSERT_NUM_EQUALS(response.succeeded, 1);

  assert_json_parse_response(&response, &field, "{\"Test2\":1.5}", RC_OK);

  ASSERT_FALSE(rc_json_get_required_float(&value, &response, &field, "Test"));
  ASSERT_FLOAT_EQUALS(value, 0.0f);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "Test not found in response");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_get_bool(const char* input, int expected) {
  rc_api_response_t response;
  rc_json_field_t field;
  char buffer[64];
  int value = 2;
  snprintf(buffer, sizeof(buffer), "{\"Test\":%s}", input);

  assert_json_parse_response(&response, &field, buffer, RC_OK);

  if (expected != -1) {
    ASSERT_TRUE(rc_json_get_bool(&value, &field, "Test"));
    ASSERT_NUM_EQUALS(value, expected);
  }
  else {
    ASSERT_FALSE(rc_json_get_bool(&value, &field, "Test"));
    ASSERT_NUM_EQUALS(value, 0);
  }
}

static void test_json_get_optional_bool() {
  rc_api_response_t response;
  rc_json_field_t field;
  int value = 3;

  assert_json_parse_response(&response, &field, "{\"Test\":true}", RC_OK);

  rc_json_get_optional_bool(&value, &field, "Test", 2);
  ASSERT_NUM_EQUALS(value, 1);

  assert_json_parse_response(&response, &field, "{\"Test2\":true}", RC_OK);

  rc_json_get_optional_bool(&value, &field, "Test", 2);
  ASSERT_NUM_EQUALS(value, 2);
}

static void test_json_get_required_bool() {
  rc_api_response_t response;
  rc_json_field_t field;
  int value = 3;

  assert_json_parse_response(&response, &field, "{\"Test\":true}", RC_OK);

  ASSERT_TRUE(rc_json_get_required_bool(&value, &response, &field, "Test"));
  ASSERT_NUM_EQUALS(value, 1);

  ASSERT_PTR_NULL(response.error_message);
  ASSERT_NUM_EQUALS(response.succeeded, 1);

  assert_json_parse_response(&response, &field, "{\"Test2\":True}", RC_OK);

  ASSERT_FALSE(rc_json_get_required_bool(&value, &response, &field, "Test"));
  ASSERT_NUM_EQUALS(value, 0);

  ASSERT_PTR_NOT_NULL(response.error_message);
  ASSERT_STR_EQUALS(response.error_message, "Test not found in response");
  ASSERT_NUM_EQUALS(response.succeeded, 0);
}

static void test_json_get_datetime(const char* input, int expected) {
  rc_api_response_t response;
  rc_json_field_t field;
  char buffer[64];
  time_t value = 2;
  snprintf(buffer, sizeof(buffer), "{\"Test\":\"%s\"}", input);

  assert_json_parse_response(&response, &field, buffer, RC_OK);

  if (expected != -1) {
    ASSERT_TRUE(rc_json_get_datetime(&value, &field, "Test"));
    ASSERT_NUM_EQUALS(value, (time_t)expected);
  }
  else {
    ASSERT_FALSE(rc_json_get_datetime(&value, &field, "Test"));
    ASSERT_NUM_EQUALS(value, 0);
  }
}

static void test_json_get_unum_array(const char* input, uint32_t expected_count, int expected_result) {
  rc_api_response_t response;
  rc_json_field_t field;
  int result;
  uint32_t count = 0xFFFFFFFF;
  uint32_t*values;
  char buffer[128];

  snprintf(buffer, sizeof(buffer), "{\"Test\":%s}", input);
  assert_json_parse_response(&response, &field, buffer, RC_OK);

  result = rc_json_get_required_unum_array(&values, &count, &response, &field, "Test");
  ASSERT_NUM_EQUALS(result, expected_result);
  ASSERT_NUM_EQUALS(count, expected_count);

  rc_buffer_destroy(&response.buffer);
}

static void test_json_get_unum_array_trailing_comma() {
  rc_api_response_t response;
  rc_json_field_t field;

  assert_json_parse_response(&response, &field, "{\"Test\":[1,2,3,]}", RC_INVALID_JSON);
}

static void test_url_build_dorequest_url_default_host() {
  rc_api_request_t request;
  rc_api_fetch_image_request_t api_params;

  rc_api_url_build_dorequest_url(&request, NULL);
  ASSERT_STR_EQUALS(request.url, "https://retroachievements.org/dorequest.php");
  rc_api_destroy_request(&request);

  api_params.image_type = RC_IMAGE_TYPE_ACHIEVEMENT;
  api_params.image_name = "12345";
  rc_api_init_fetch_image_request_hosted(&request, &api_params, NULL);
  ASSERT_STR_EQUALS(request.url, "https://media.retroachievements.org/Badge/12345.png");
  rc_api_destroy_request(&request);
}

static void test_url_build_dorequest_url_default_host_nonssl() {
  rc_api_request_t request;
  rc_api_fetch_image_request_t api_params;
  rc_api_host_t host;

  memset(&host, 0, sizeof(host));
  host.host = "http://retroachievements.org";

  rc_api_url_build_dorequest_url(&request, &host);
  ASSERT_STR_EQUALS(request.url, "http://retroachievements.org/dorequest.php");
  rc_api_destroy_request(&request);

  api_params.image_type = RC_IMAGE_TYPE_ACHIEVEMENT;
  api_params.image_name = "12345";
  rc_api_init_fetch_image_request_hosted(&request, &api_params, &host);
  ASSERT_STR_EQUALS(request.url, "http://media.retroachievements.org/Badge/12345.png");
  rc_api_destroy_request(&request);
}

static void test_url_build_dorequest_url_custom_host() {
  rc_api_request_t request;
  rc_api_fetch_image_request_t api_params;
  rc_api_host_t host;

  memset(&host, 0, sizeof(host));
  host.host = "http://localhost";

  rc_api_url_build_dorequest_url(&request, &host);
  ASSERT_STR_EQUALS(request.url, "http://localhost/dorequest.php");
  rc_api_destroy_request(&request);

  api_params.image_type = RC_IMAGE_TYPE_ACHIEVEMENT;
  api_params.image_name = "12345";
  rc_api_init_fetch_image_request_hosted(&request, &api_params, &host);
  ASSERT_STR_EQUALS(request.url, "http://localhost/Badge/12345.png");
  rc_api_destroy_request(&request);
}

static void test_url_build_dorequest_url_custom_host_no_protocol() {
  rc_api_request_t request;
  rc_api_fetch_image_request_t api_params;
  rc_api_host_t host;

  memset(&host, 0, sizeof(host));
  host.host = "my.host";

  rc_api_url_build_dorequest_url(&request, &host);
  ASSERT_STR_EQUALS(request.url, "http://my.host/dorequest.php");
  rc_api_destroy_request(&request);

  api_params.image_type = RC_IMAGE_TYPE_ACHIEVEMENT;
  api_params.image_name = "12345";
  rc_api_init_fetch_image_request_hosted(&request, &api_params, &host);
  ASSERT_STR_EQUALS(request.url, "http://my.host/Badge/12345.png");
  rc_api_destroy_request(&request);
}

static void test_url_builder_append_encoded_str(const char* input, const char* expected) {
  rc_api_url_builder_t builder;
  rc_buffer_t buffer;
  const char* output;

  rc_buffer_init(&buffer);
  rc_url_builder_init(&builder, &buffer, 128);
  rc_url_builder_append_encoded_str(&builder, input);
  output = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(output, expected);

  rc_buffer_destroy(&buffer);
}

static void test_url_builder_append_str_param() {
  rc_api_url_builder_t builder;
  rc_buffer_t buffer;
  const char* output;

  rc_buffer_init(&buffer);
  rc_url_builder_init(&builder, &buffer, 64);
  rc_url_builder_append_str_param(&builder, "a", "Apple");
  rc_url_builder_append_str_param(&builder, "b", "Banana");
  rc_url_builder_append_str_param(&builder, "t", "Test 1");
  output = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(output, "a=Apple&b=Banana&t=Test+1");

  rc_buffer_destroy(&buffer);
}

static void test_url_builder_append_unum_param() {
  rc_api_url_builder_t builder;
  rc_buffer_t buffer;
  const char* output;

  rc_buffer_init(&buffer);
  rc_url_builder_init(&builder, &buffer, 32);
  rc_url_builder_append_unum_param(&builder, "a", 0);
  rc_url_builder_append_unum_param(&builder, "b", 123456);
  rc_url_builder_append_unum_param(&builder, "t", (unsigned)-1);
  output = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(output, "a=0&b=123456&t=4294967295");

  rc_buffer_destroy(&buffer);
}

static void test_url_builder_append_num_param() {
  rc_api_url_builder_t builder;
  rc_buffer_t buffer;
  const char* output;

  rc_buffer_init(&buffer);
  rc_url_builder_init(&builder, &buffer, 32);
  rc_url_builder_append_num_param(&builder, "a", 0);
  rc_url_builder_append_num_param(&builder, "b", 123456);
  rc_url_builder_append_num_param(&builder, "t", -1);
  output = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(output, "a=0&b=123456&t=-1");

  rc_buffer_destroy(&buffer);
}

static void test_init_fetch_image_request_game() {
  rc_api_fetch_image_request_t fetch_image_request;
  rc_api_request_t request;

  memset(&fetch_image_request, 0, sizeof(fetch_image_request));
  fetch_image_request.image_name = "0123324";
  fetch_image_request.image_type = RC_IMAGE_TYPE_GAME;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_image_request(&request, &fetch_image_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, IMAGEREQUEST_URL "/Images/0123324.png");
  ASSERT_PTR_NULL(request.post_data);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_image_request_achievement() {
  rc_api_fetch_image_request_t fetch_image_request;
  rc_api_request_t request;

  memset(&fetch_image_request, 0, sizeof(fetch_image_request));
  fetch_image_request.image_name = "135764";
  fetch_image_request.image_type = RC_IMAGE_TYPE_ACHIEVEMENT;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_image_request(&request, &fetch_image_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, IMAGEREQUEST_URL "/Badge/135764.png");
  ASSERT_PTR_NULL(request.post_data);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_image_request_achievement_locked() {
  rc_api_fetch_image_request_t fetch_image_request;
  rc_api_request_t request;

  memset(&fetch_image_request, 0, sizeof(fetch_image_request));
  fetch_image_request.image_name = "135764";
  fetch_image_request.image_type = RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_image_request(&request, &fetch_image_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, IMAGEREQUEST_URL "/Badge/135764_lock.png");
  ASSERT_PTR_NULL(request.post_data);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_image_request_user() {
  rc_api_fetch_image_request_t fetch_image_request;
  rc_api_request_t request;

  memset(&fetch_image_request, 0, sizeof(fetch_image_request));
  fetch_image_request.image_name = "Username";
  fetch_image_request.image_type = RC_IMAGE_TYPE_USER;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_image_request(&request, &fetch_image_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, IMAGEREQUEST_URL "/UserPic/Username.png");
  ASSERT_PTR_NULL(request.post_data);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_image_request_unknown() {
  rc_api_fetch_image_request_t fetch_image_request;
  rc_api_request_t request;

  memset(&fetch_image_request, 0, sizeof(fetch_image_request));
  fetch_image_request.image_name = "12345";
  fetch_image_request.image_type = -1;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_image_request(&request, &fetch_image_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_image_request_custom_host()
{
  rc_api_fetch_image_request_t fetch_image_request;
  rc_api_request_t request;
  rc_api_host_t host;

  memset(&host, 0, sizeof(host));
  host.host = "http://localhost";

  memset(&fetch_image_request, 0, sizeof(fetch_image_request));
  fetch_image_request.image_name = "0123324";
  fetch_image_request.image_type = RC_IMAGE_TYPE_GAME;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_image_request_hosted(&request, &fetch_image_request, &host), RC_OK);
  ASSERT_STR_EQUALS(request.url, "http://localhost/Images/0123324.png");
  ASSERT_PTR_NULL(request.post_data);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_image_request_explicit_default_host()
{
  rc_api_fetch_image_request_t fetch_image_request;
  rc_api_request_t request;
  rc_api_host_t host;

  memset(&host, 0, sizeof(host));
  host.host = rc_api_default_host();

  memset(&fetch_image_request, 0, sizeof(fetch_image_request));
  fetch_image_request.image_name = "0123324";
  fetch_image_request.image_type = RC_IMAGE_TYPE_GAME;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_image_request_hosted(&request, &fetch_image_request, &host), RC_OK);
  ASSERT_STR_EQUALS(request.url, "https://media.retroachievements.org/Images/0123324.png");
  ASSERT_PTR_NULL(request.post_data);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_image_request_explicit_nonssl_host()
{
  rc_api_fetch_image_request_t fetch_image_request;
  rc_api_request_t request;
  rc_api_host_t host;

  memset(&host, 0, sizeof(host));
  host.host = "http://retroachievements.org";

  memset(&fetch_image_request, 0, sizeof(fetch_image_request));
  fetch_image_request.image_name = "0123324";
  fetch_image_request.image_type = RC_IMAGE_TYPE_GAME;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_image_request_hosted(&request, &fetch_image_request, &host), RC_OK);
  ASSERT_STR_EQUALS(request.url, "http://media.retroachievements.org/Images/0123324.png");
  ASSERT_PTR_NULL(request.post_data);

  rc_api_destroy_request(&request);
}

void test_rapi_common(void) {
  TEST_SUITE_BEGIN();

  /* rc_json_parse_response */
  TEST(test_json_parse_response_empty);
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":\"Test\"}", "\"Test\""); /* string */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":\"Te\\\"st\"}", "\"Te\\\"st\""); /* escaped string */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":12345678}", "12345678"); /* integer */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":+12345678}", "+12345678"); /* positive integer */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":-12345678}", "-12345678"); /* negatvie integer */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":1234.5678}", "1234.5678"); /* decimal */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":+1234.5678}", "+1234.5678"); /* positive decimal */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":-1234.5678}", "-1234.5678"); /* negatvie decimal */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":[1,2,3]}", "[1,2,3]"); /* array */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":{\"Foo\":1}}", "{\"Foo\":1}"); /* object */
  TEST_PARAMS2(test_json_parse_response_field, "{\"Test\":null}", "null"); /* null */
  TEST_PARAMS2(test_json_parse_response_field, "{ \"Test\" : 0 }", "0"); /* ignore whitespace */
  TEST_PARAMS2(test_json_parse_response_field, "{ \"Other\" : 1, \"Test\" : 2 }", "2"); /* preceding field */
  TEST_PARAMS2(test_json_parse_response_field, "{ \"Test\" : 1, \"Other\" : 2 }", "1"); /* trailing field */
  TEST(test_json_parse_response_non_json);
  TEST(test_json_parse_response_non_json_bounded);
  TEST(test_json_parse_response_451);
  TEST(test_json_parse_response_error_from_server);
  TEST(test_json_parse_response_incorrect_size);

  /* rc_json_get_string */
  TEST_PARAMS2(test_json_get_string, "", "");
  TEST_PARAMS2(test_json_get_string, "Banana", "Banana");
  TEST_PARAMS2(test_json_get_string, "A \\\"Quoted\\\" String", "A \"Quoted\" String");
  TEST_PARAMS2(test_json_get_string, "This\\r\\nThat", "This\r\nThat");
  TEST_PARAMS2(test_json_get_string, "This\\/That", "This/That");
  TEST_PARAMS2(test_json_get_string, "\\u0065", "e");
  TEST_PARAMS2(test_json_get_string, "\\u00a9", "\xc2\xa9");
  TEST_PARAMS2(test_json_get_string, "\\u2260", "\xe2\x89\xa0");
  TEST_PARAMS2(test_json_get_string, "\\ud83d\\udeb6", "\xf0\x9f\x9a\xb6"); /* surrogate pair */
  TEST_PARAMS2(test_json_get_string, "\\ud83d", "\xef\xbf\xbd"); /* surrogate lead with no tail */
  TEST_PARAMS2(test_json_get_string, "\\udeb6", "\xef\xbf\xbd"); /* surrogate tail with no lead */
  TEST(test_json_get_optional_string);
  TEST(test_json_get_required_string);

  /* rc_json_get_num */
  TEST_PARAMS2(test_json_get_num, "Banana", 0);
  TEST_PARAMS2(test_json_get_num, "True", 0);
  TEST_PARAMS2(test_json_get_num, "2468", 2468);
  TEST_PARAMS2(test_json_get_num, "+55", 55);
  TEST_PARAMS2(test_json_get_num, "-16", -16);
  TEST_PARAMS2(test_json_get_num, "3.14159", 3);
  TEST(test_json_get_optional_num);
  TEST(test_json_get_required_num);

  /* rc_json_get_unum */
  TEST_PARAMS2(test_json_get_unum, "Banana", 0);
  TEST_PARAMS2(test_json_get_unum, "True", 0);
  TEST_PARAMS2(test_json_get_unum, "2468", 2468);
  TEST_PARAMS2(test_json_get_unum, "+55", 0);
  TEST_PARAMS2(test_json_get_unum, "-16", 0);
  TEST_PARAMS2(test_json_get_unum, "3.14159", 3);
  TEST(test_json_get_optional_unum);
  TEST(test_json_get_required_unum);

  /* rc_json_get_num */
  TEST_PARAMS2(test_json_get_float, "Banana", 0.0f);
  TEST_PARAMS2(test_json_get_float, "True", 0.0f);
  TEST_PARAMS2(test_json_get_float, "2468", 2468.0f);
  TEST_PARAMS2(test_json_get_float, "+55", 55.0f);
  TEST_PARAMS2(test_json_get_float, "-16", -16.0f);
  TEST_PARAMS2(test_json_get_float, "3.14159", 3.14159f);
  TEST_PARAMS2(test_json_get_float, "-6.7", -6.7f);
  TEST(test_json_get_optional_float);
  TEST(test_json_get_required_float);

  /* rc_json_get_bool */
  TEST_PARAMS2(test_json_get_bool, "true", 1);
  TEST_PARAMS2(test_json_get_bool, "false", 0);
  TEST_PARAMS2(test_json_get_bool, "TRUE", 1);
  TEST_PARAMS2(test_json_get_bool, "True", 1);
  TEST_PARAMS2(test_json_get_bool, "Banana", -1);
  TEST_PARAMS2(test_json_get_bool, "1", 1);
  TEST_PARAMS2(test_json_get_bool, "0", 0);
  TEST(test_json_get_optional_bool);
  TEST(test_json_get_required_bool);

  /* rc_json_get_datetime */
  TEST_PARAMS2(test_json_get_datetime, "", -1);
  TEST_PARAMS2(test_json_get_datetime, "2015-01-01 08:15:00", 1420100100);
  TEST_PARAMS2(test_json_get_datetime, "2016-02-29 20:01:47", 1456776107);

  /* rc_json_get_unum_array */
  TEST_PARAMS3(test_json_get_unum_array, "[]", 0, RC_OK);
  TEST_PARAMS3(test_json_get_unum_array, "1", 0, RC_MISSING_VALUE);
  TEST_PARAMS3(test_json_get_unum_array, "[1]", 1, RC_OK);
  TEST_PARAMS3(test_json_get_unum_array, "[ 1 ]", 1, RC_OK);
  TEST_PARAMS3(test_json_get_unum_array, "[1,2,3,4]", 4, RC_OK);
  TEST_PARAMS3(test_json_get_unum_array, "[ 1 , 2 ]", 2, RC_OK);
  TEST_PARAMS3(test_json_get_unum_array, "[1,1,1]", 3, RC_OK);
  TEST_PARAMS3(test_json_get_unum_array, "[A,B,C]", 3, RC_MISSING_VALUE);
  TEST(test_json_get_unum_array_trailing_comma);

  /* rc_api_url_build_dorequest_url / rc_api_url_build_dorequest_url_hosted */
  TEST(test_url_build_dorequest_url_default_host);
  TEST(test_url_build_dorequest_url_default_host_nonssl);
  TEST(test_url_build_dorequest_url_custom_host);
  TEST(test_url_build_dorequest_url_custom_host_no_protocol);

  /* rc_api_url_builder_append_encoded_str */
  TEST_PARAMS2(test_url_builder_append_encoded_str, "", "");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "Apple", "Apple");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "Test 1", "Test+1");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "Test+1", "Test%2b1");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "Test%1", "Test%251");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "%Test%", "%25Test%25");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "%%", "%25%25");

  /* rc_api_url_builder_append_param */
  TEST(test_url_builder_append_str_param);
  TEST(test_url_builder_append_num_param);
  TEST(test_url_builder_append_unum_param);

  /* fetch_image */
  TEST(test_init_fetch_image_request_game);
  TEST(test_init_fetch_image_request_achievement);
  TEST(test_init_fetch_image_request_achievement_locked);
  TEST(test_init_fetch_image_request_user);
  TEST(test_init_fetch_image_request_unknown);
  TEST(test_init_fetch_image_request_custom_host);
  TEST(test_init_fetch_image_request_explicit_default_host);
  TEST(test_init_fetch_image_request_explicit_nonssl_host);


  TEST_SUITE_END();
}
