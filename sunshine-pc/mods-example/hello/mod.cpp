// Example mod: logs its lifecycle, reads the game state through the host table and toggles a cheat.
// Build with the SUNSHINE_BUILD_EXAMPLE_MODS option. The DLL goes to <exe>/mod-examples/hello (not discovered);
// copy the folder into <exe>/mods/ to load it.
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

SUNSHINE_MOD_EXPORT void sunshine_mod_on_logic_tick(SunshineModCtx* ctx) {
    // One log line per second (30 logic ticks), so the tick rate can be read from the log.
    static uint32_t ticks = 0;
    ticks++;
    if (ticks % 30 == 0) {
        char msg[64];
        std::snprintf(msg, sizeof msg, "logic tick %u (%u s)", ticks, ticks / 30);
        gHost->log(ctx, SUNSHINE_LOG_INFO, msg);
    }
}

SUNSHINE_MOD_EXPORT void sunshine_mod_shutdown(SunshineModCtx* ctx) {
    gHost->log(ctx, SUNSHINE_LOG_INFO, "shutting down");
}

}  // extern "C"
