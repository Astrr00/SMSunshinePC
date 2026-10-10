// Internal seam between the mod loader (Win32, no sms headers) and the game binding (mods_host.cpp, sms headers).
#pragma once

#include "sunshine_mod_api.h"

#include <cstdint>
#include <string>

// Per-mod context handed to the mod as an opaque SunshineModCtx*. The C header declares it opaque; this is its body.
struct SunshineModCtx {
    std::string id;
};

// The host table passed to every mod's sunshine_mod_init. Static storage; valid for the process lifetime.
const SunshineHostApi& modsHostApi();

// Stage id of the current game area (TApplication mCurrArea). Always available once the game is running.
uint32_t modsCurrentStage();

// Writes one line to the console and the mods log. level is SUNSHINE_LOG_*.
void modsLogLine(int32_t level, const std::string& line);
