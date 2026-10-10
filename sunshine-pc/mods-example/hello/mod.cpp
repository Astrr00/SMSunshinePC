// Example mod: logs its lifecycle, reads the game state through the host table and toggles a cheat.
// Build with the SUNSHINE_BUILD_EXAMPLE_MODS option, the DLL is copied to mods/hello/mod.dll.
#include <cstdio>

#include "sunshine_mod_api.h"

namespace {

const SunshineHostApi* gHost = nullptr;

}  // namespace

extern "C" {

SUNSHINE_MOD_EXPORT uint32_t sunshine_mod_api_version(void) {
    return SUNSHINE_MOD_API_VERSION;
}

SUNSHINE_MOD_EXPORT int32_t sunshine_mod_init(SunshineModCtx* ctx, const SunshineHostApi* host) {
    gHost = host;
    char msg[96];
    std::snprintf(msg, sizeof msg, "hello from the mod API, host API %u.%u", host->api_version >> 16,
                  host->api_version & 0xFFFFu);
    host->log(ctx, SUNSHINE_LOG_INFO, msg);
    return 0;
}

SUNSHINE_MOD_EXPORT void sunshine_mod_on_stage_load(SunshineModCtx* ctx, uint32_t stage) {
    char msg[64];
    std::snprintf(msg, sizeof msg, "entered stage %u", stage);
    gHost->log(ctx, SUNSHINE_LOG_INFO, msg);

    // Turn on the infinite-lives cheat from the data file, if it exists.
    if (gHost->cheat_set(ctx, "infinite_lives", 1) != 0) {
        gHost->log(ctx, SUNSHINE_LOG_WARN, "cheat infinite_lives not found");
    }
}

SUNSHINE_MOD_EXPORT void sunshine_mod_shutdown(SunshineModCtx* ctx) {
    gHost->log(ctx, SUNSHINE_LOG_INFO, "shutting down");
}

}  // extern "C"
