#include "logging.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

void installCrashHandler(const wchar_t* path);

int main(int argc, char** argv)
{
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	const auto crash = sunshine::initializeLogging();
	installCrashHandler(crash.c_str());
	std::printf("[test] stdout survives\n");
	std::fprintf(stderr, "[test] stderr survives\n");
	DWORD written;
	const char raw[] = "[test] Win32 stderr survives\n";
	WriteFile(GetStdHandle(STD_ERROR_HANDLE), raw, sizeof(raw)-1, &written, nullptr);
	if (argc > 1 && std::strcmp(argv[1], "--crash") == 0) {
		ULONG_PTR details[] = {0, 0x1234};
		RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 2, details);
	}
	if (argc > 1 && std::strcmp(argv[1], "--abort") == 0) {
		std::fprintf(stderr, "[test] fatal diagnostic before abort\n");
		std::abort();
	}
	if (argc > 1 && std::strcmp(argv[1], "--hold") == 0) Sleep(5000);
	std::printf("[test] normal exit\n");
}
