// Public cheat API of the port. Used by the launcher UI (list, toggle), the mod loader (mods_host.cpp)
// and the sms hooks (TMario::decHP guard, TMarDirector logic tick). Implemented in cheats_game.cpp.
// Data: <exe dir>/assets/cheats.json (shipped) and <exe dir>/cheats.user.json (optional overrides/additions).
#pragma once

#include <string>

struct CheatInfo {
    const char* id;
    const char* name;
    const char* description;
    const char* category;
    bool enabled;
    bool oneShot;  // one-shot cheats switch themselves off after they run
};

// Loads both data files from exeDir. Call once before the game starts. Returns false if the shipped list is
// missing or invalid (the cheat count is then 0). A broken user file is logged and skipped.
bool cheatsLoad(const std::string& exeDir);

int cheatCount();

// Fills out for the cheat at index (0..cheatCount()-1). Pointers stay valid until the next cheatsLoad().
bool cheatAt(int index, CheatInfo& out);

bool cheatEnabled(const char* id);

// Returns false when the id is unknown.
bool cheatSet(const char* id, bool enabled);

// True while an enabled cheat keeps Mario's health full. TMario::decHP returns early in that case (sms hook).
bool cheatsHealthLossBlocked();

// Runs the enabled cheats once. Called from sunshineLogicTick() once per logic frame (30 Hz).
void cheatsTick();

bool cheatExists(const char* id);
