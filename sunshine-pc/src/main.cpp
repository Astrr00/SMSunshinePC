// Entry point of the PC port. Replaces sms/src/main.cpp (excluded in excluded-units.txt): Aurora brings up the
// window, GX and the disc image, then the decompiled game runs its own initialize/proc/finalize.
#include <aurora/aurora.h>
#include <aurora/dvd.h>
#include <aurora/main.h>

#include <System/Application.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

// BSS object defined by the decomp's main.cpp.
TApplication gpApplication;

static void log_callback(AuroraLogLevel level, const char* module, const char* message, unsigned int)
{
	std::fprintf(level >= LOG_ERROR ? stderr : stdout, "[%d %s] %s\n", (int)level, module, message);
	if (level == LOG_FATAL)
		std::abort();
}

static const char* findDvdArg(int argc, char** argv)
{
	for (int i = 1; i + 1 < argc; i++) {
		if (std::strcmp(argv[i], "--dvd") == 0)
			return argv[i + 1];
	}
	return nullptr;
}

int main(int argc, char* argv[])
{
	const AuroraConfig config = { .appName = "Sunshine", .logCallback = &log_callback };
	aurora_initialize(argc, argv, &config);

	const char* disc = findDvdArg(argc, argv);
	if (disc == nullptr || !aurora_dvd_open(disc)) {
		std::fprintf(stderr, "usage: sunshine --dvd <path to disc image (iso/rvz/wia/...)>\n");
		aurora_shutdown();
		return 1;
	}

	gpApplication.initialize();
	gpApplication.proc();
	gpApplication.finalize();

	aurora_shutdown();
	return 0;
}
