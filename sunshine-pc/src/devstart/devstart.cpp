// Developer switch: SUNSHINE_DEV_START=<stage>/<scenario> (e.g. bianco/0) boots straight into that
// gameplay scenario, skipping the title screen and file select. Unset (default) keeps the normal flow.
// The stage name is matched by the game's own scenario table in sms (TApplication::mountStageArchive
// indexes it the same way), so this parser only splits the string.

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Dev runs redirect stdout to a file; unbuffered so the game's log lines appear even if the process is killed.
static const bool kUnbufferedStdout = [] {
	if (std::getenv("SUNSHINE_DEV_START") != nullptr)
		std::setvbuf(stdout, nullptr, _IONBF, 0);
	return true;
}();

bool sunshineDevStart(char* stageName, unsigned stageNameSize, int* scenario)
{
	static bool parsed = false;
	static bool active = false;
	static char stage[32];
	static int scen = 0;

	if (!parsed) {
		parsed = true;
		const char* env = std::getenv("SUNSHINE_DEV_START");
		if (env != nullptr && *env != '\0') {
			const char* slash = std::strchr(env, '/');
			size_t nameLen    = slash ? (size_t)(slash - env) : std::strlen(env);
			if (nameLen > 0 && nameLen < sizeof(stage)) {
				std::memcpy(stage, env, nameLen);
				stage[nameLen] = '\0';
				scen           = slash ? std::atoi(slash + 1) : 0;
				active         = true;
				std::printf("[devstart] SUNSHINE_DEV_START: stage=%s scenario=%d\n", stage, scen);
				std::fflush(stdout);
			} else {
				std::printf("[devstart] SUNSHINE_DEV_START=%s is not <stage>/<scenario>; ignored\n", env);
			}
		}
	}

	if (!active)
		return false;

	if (stageName != nullptr && stageNameSize > 0) {
		std::strncpy(stageName, stage, stageNameSize - 1);
		stageName[stageNameSize - 1] = '\0';
	}
	if (scenario != nullptr)
		*scenario = scen;
	return true;
}
