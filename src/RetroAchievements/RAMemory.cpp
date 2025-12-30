/*
#include "RAClient.h"
#include "../NDS.h"
#include <cstring>

uint32_t ra_read_memory(
    uint32_t address,
    uint8_t* buffer,
    uint32_t size,
    rc_client_t* client)
{
    RAContext* ctx =
        static_cast<RAContext*>(rc_client_get_userdata(client));

    if (!ctx)
        return 0;

    melonDS::NDS* nds = ctx->nds; // ← BEZ GetNDS()

    if (!nds) {
        memset(buffer, 0, size);
        return size;
    }

    // TODO: real mapping
    memset(buffer, 0, size);
    return size;
}
    */