#pragma once
#include <cstdint>
#include <rcheevos/include/rc_client.h>

struct RAContext;

extern "C" uint32_t ra_read_memory(uint32_t addr, uint8_t* buf, uint32_t size, rc_client_t* client);
extern "C" void ra_server_call(const rc_api_request_t* request,
                               rc_client_server_callback_t callback,
                               void* userdata,
                               rc_client_t* client);
