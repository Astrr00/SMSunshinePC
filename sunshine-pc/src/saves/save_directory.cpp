#include "saves.hpp"

// Aurora's header, by path: the game's include directories shadow <dolphin/...> (see launcher/launcher.cpp).
#include "../../extern/aurora/include/dolphin/card.h"

#include <cstdio>
#include <system_error>

void savesApplyDirectory(const sunshine::Settings& s, const std::filesystem::path& exeDir)
{
	const std::filesystem::path dir = sunshine::resolveSaveDirectory(s, exeDir);
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	if (ec) {
		// Keep Aurora's default card location rather than letting CARDSetBasePath strip the last path component.
		std::fprintf(stderr, "saves: cannot create %s (%s); using Aurora's default card path\n",
		             dir.string().c_str(), ec.message().c_str());
		return;
	}
	CARDSetBasePath(dir.string().c_str(), -1); // -1: both slots
}
