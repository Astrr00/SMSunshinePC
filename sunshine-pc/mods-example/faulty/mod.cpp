// Example mod that crashes on its first logic tick, to prove the loader isolates a faulty mod.
// Expected: "[mod faulty] faulted in on_logic_tick" in the log, the game keeps running, hello is unaffected.
#include "sunshine_mod_api.h"

extern "C" {

SUNSHINE_MOD_EXPORT uint32_t sunshine_mod_api_version(void) {
    return SUNSHINE_MOD_API_VERSION;
}

SUNSHINE_MOD_EXPORT int32_t sunshine_mod_init(SunshineModCtx*, const SunshineHostApi*) {
    return 0;
}

SUNSHINE_MOD_EXPORT void sunshine_mod_on_logic_tick(SunshineModCtx*) {
    volatile int* p = nullptr;
    *p = 1;  // access violation; the loader catches it with SEH and disables this mod
}

}  // extern "C"
