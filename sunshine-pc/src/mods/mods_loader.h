// Mod loader (Win32): scans <exe>/mods/<id>/mod.json, loads mod.dll, runs the mod callbacks under SEH,
// and registers data overlays (<id>/files/) with Aurora's DVD layer. Log: <exe>/sunshine_mods.log.
#pragma once

#include "mods/mod_manifest.h"

#include <map>
#include <string>
#include <vector>

// Parses every <exeDir>/mods/<id>/mod.json. Loads no DLLs. Used by the launcher's Mods pane and by modsLoad.
std::vector<mods::Manifest> scanModManifests(const std::string& exeDir);

// Loads every compatible mod under exeDir/mods. modSwitches: mod id -> false disables it; absent = on.
// Call once after aurora_dvd_open, before the game starts.
bool modsLoad(const std::string& exeDir, const std::map<std::string, bool>& modSwitches);

// Once per game logic frame. Fires on_stage_load when the stage id changes, then on_logic_tick.
void modsLogicTick();

// Once per presented frame.
void modsPresent();

// Calls sunshine_mod_shutdown, then frees the DLLs. Call once at exit.
void modsShutdown();
