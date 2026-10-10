// Save-file plumbing for the CARD layer: where card files live, and a dev round-trip check of the decomp's TCardManager.
#pragma once

#include <filesystem>

#include "../settings.h"

// Call after the disc is open (Aurora's card path uses the game name) and before gpApplication.initialize(), which runs
// CARDInit. Creates the directory and points Aurora's CARD layer at it.
void savesApplyDirectory(const sunshine::Settings& s, const std::filesystem::path& exeDir);

// SUNSHINE_DEV_CARD_TEST: writes flag values through the TCardManager save path, clobbers them, reads them back through
// the load path and compares. Call after gpApplication.initialize(). Returns 0 on PASS.
int savesRoundTripTest();
