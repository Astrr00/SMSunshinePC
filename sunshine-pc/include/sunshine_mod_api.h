/*
 * Sunshine PC mod C API, version 1.0.
 *
 * A mod is a folder mods/<id>/ next to sunshine.exe with:
 *   mod.json   manifest: {"id": "...", "sunshine_api": "1.0", "name", "version", "author", "description", "load_order"}
 *   mod.dll    the code (optional for data-only mods)
 *   files/     data overlays: each file replaces the DVD file at "/" + its path under files/
 *
 * The DLL exports (extern "C", __cdecl):
 *   uint32_t sunshine_mod_api_version(void);                       REQUIRED. Return SUNSHINE_MOD_API_VERSION.
 *   int32_t  sunshine_mod_init(SunshineModCtx*, const SunshineHostApi*);  REQUIRED. 0 = ok, else the mod is disabled.
 *   void     sunshine_mod_shutdown(SunshineModCtx*);               optional, called at exit.
 *   void     sunshine_mod_on_logic_tick(SunshineModCtx*);          optional, once per game logic frame (30 Hz).
 *   void     sunshine_mod_on_stage_load(SunshineModCtx*, uint32_t stage);  optional, first logic frame of a new stage.
 *   void     sunshine_mod_on_present(SunshineModCtx*);             optional, once per presented frame.
 *
 * Compatibility: the host major must equal the mod's major, and the mod's minor must not be newer than the host's.
 * Minor versions only add fields or functions at the end of SunshineHostApi, guarded by struct_size.
 *
 * Errors: a crash (access violation and similar, SEH) inside a callback disables that mod and is logged; the
 * game keeps running. C++ exceptions are not caught; a mod must not let them escape its exports.
 *
 * Threading: all callbacks run on the game thread. SunshineActorHandle values are valid only during the callback
 * that obtained them.
 *
 * Adapted from the modding design of Dusklight (CC0, https://github.com/TwilitRealm/dusklight): manifest with
 * a stable id, versioned C-ABI host table, lifecycle callbacks, and per-mod error isolation. The mechanism here
 * is DLL + mod.json, not Dusklight's Luau runtime.
 */
#ifndef SUNSHINE_MOD_API_H
#define SUNSHINE_MOD_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SUNSHINE_MOD_API_MAJOR 1u
#define SUNSHINE_MOD_API_MINOR 0u
#define SUNSHINE_MOD_API_VERSION ((SUNSHINE_MOD_API_MAJOR << 16) | SUNSHINE_MOD_API_MINOR)

#ifdef _WIN32
#define SUNSHINE_MOD_EXPORT __declspec(dllexport)
#else
#define SUNSHINE_MOD_EXPORT __attribute__((visibility("default")))
#endif

/* Log levels for SunshineHostApi.log. */
#define SUNSHINE_LOG_INFO 0
#define SUNSHINE_LOG_WARN 1
#define SUNSHINE_LOG_ERROR 2

/* Opaque per-mod context, created by the host. Pass it back to every host call. */
typedef struct SunshineModCtx SunshineModCtx;

/* Opaque handle to the player actor (Mario). Valid only during the current callback. */
typedef void* SunshineActorHandle;

typedef struct SunshineHostApi {
    uint32_t struct_size; /* sizeof(SunshineHostApi) as the host knows it */
    uint32_t api_version; /* SUNSHINE_MOD_API_VERSION of the host */

    /* Writes "[mod <id>] message" to the console and mods log. */
    void (*log)(SunshineModCtx* ctx, int32_t level, const char* message);

    /* Game flags (sms TFlagManager). flag encodes bank and index, see the decomp's FlagManager. */
    int32_t (*flag_get)(SunshineModCtx* ctx, uint32_t flag);
    void (*flag_set)(SunshineModCtx* ctx, uint32_t flag, int32_t value);

    /* Cheats by id (cheats.json). cheat_get returns 1 on, 0 off, -1 unknown id. cheat_set returns 0 ok, -1 unknown id. */
    int32_t (*cheat_get)(SunshineModCtx* ctx, const char* id);
    int32_t (*cheat_set)(SunshineModCtx* ctx, const char* id, int32_t enabled);

    /* Mario. mario() returns NULL when no stage with Mario is running. */
    SunshineActorHandle (*mario)(SunshineModCtx* ctx);
    int32_t (*mario_health)(SunshineModCtx* ctx, SunshineActorHandle mario);
    void (*mario_set_health)(SunshineModCtx* ctx, SunshineActorHandle mario, int32_t health);
    int32_t (*mario_water)(SunshineModCtx* ctx, SunshineActorHandle mario);
    void (*mario_set_water)(SunshineModCtx* ctx, SunshineActorHandle mario, int32_t water);
} SunshineHostApi;

/* Mod exports. Declare them in the mod with SUNSHINE_MOD_EXPORT. */
typedef uint32_t (*SunshineModApiVersionFn)(void);
typedef int32_t (*SunshineModInitFn)(SunshineModCtx* ctx, const SunshineHostApi* host);
typedef void (*SunshineModShutdownFn)(SunshineModCtx* ctx);
typedef void (*SunshineModLogicTickFn)(SunshineModCtx* ctx);
typedef void (*SunshineModStageLoadFn)(SunshineModCtx* ctx, uint32_t stage);
typedef void (*SunshineModPresentFn)(SunshineModCtx* ctx);

#ifdef __cplusplus
}
#endif

#endif /* SUNSHINE_MOD_API_H */
