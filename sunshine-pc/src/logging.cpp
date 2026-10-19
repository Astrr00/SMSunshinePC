#include "logging.h"

#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <vector>

#ifndef SUNSHINE_VERSION
#define SUNSHINE_VERSION "development"
#endif
#ifndef SUNSHINE_BUILD_TYPE
#define SUNSHINE_BUILD_TYPE "unspecified"
#endif

namespace sunshine {
namespace {
bool redirected(DWORD stream)
{
	const DWORD type = GetFileType(GetStdHandle(stream));
	return type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE;
}

bool redirect(FILE* stream, DWORD handle, int source)
{
	if (_fileno(stream) < 0) {
		FILE* opened = nullptr;
		if (_wfreopen_s(&opened, L"NUL", L"wb", stream) != 0) return false;
	}
	if (_dup2(source, _fileno(stream)) != 0) return false;
	return SetStdHandle(handle, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stream)))) != FALSE;
}

void prune(const std::filesystem::path& directory)
{
	std::error_code error;
	std::vector<std::filesystem::path> sessions;
	for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
		const auto name = it->path().filename().native();
		if (name.starts_with(L"session-") && name.ends_with(L".log") && name.size() > 12
		    && std::all_of(name.begin()+8, name.end()-4, [](wchar_t c) { return (c >= L'0' && c <= L'9') || c == L'-'; })
		    && it->is_regular_file(error))
			sessions.push_back(it->path());
	}
	std::sort(sessions.rbegin(), sessions.rend());
	// shortcut: five sessions, not a live file-size cap; add a cap if normal logging grows excessively.
	for (size_t i = 5; i < sessions.size(); ++i) {
		// An active session's handles deny deletion; never truncate another running instance's log.
		if (std::filesystem::remove(sessions[i], error)) {
			auto crash = sessions[i];
			std::filesystem::remove(crash.replace_extension(".crash.log"), error);
		}
	}
}
}

std::filesystem::path initializeLogging()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	std::setvbuf(stderr, nullptr, _IONBF, 0);
	const bool captureOut = !redirected(STD_OUTPUT_HANDLE);
	const bool captureErr = !redirected(STD_ERROR_HANDLE);
	std::filesystem::path crash = L"crash.log";
	std::array<wchar_t, 32768> executable{};
	const DWORD pathLength = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
	const auto exe = pathLength && pathLength < executable.size() ? std::filesystem::path(executable.data()) : std::filesystem::path();
	SYSTEMTIME now{};
	GetSystemTime(&now);
	int log = -1;
	bool capturedOut = false;
	if (captureOut || captureErr) {
		const wchar_t* appData = _wgetenv(L"APPDATA");
		const auto directory = appData && *appData ? std::filesystem::path(appData) / L"Sunshine" / L"logs" : exe.parent_path() / L"logs";
		std::error_code error;
		std::filesystem::create_directories(directory, error);
		wchar_t name[96];
		swprintf_s(name, L"session-%04u%02u%02u-%02u%02u%02u-%03u-%lu.log", now.wYear, now.wMonth, now.wDay,
		           now.wHour, now.wMinute, now.wSecond, now.wMilliseconds, GetCurrentProcessId());
		const auto path = directory / name;
		HANDLE file = error ? INVALID_HANDLE_VALUE : CreateFileW(path.c_str(), FILE_APPEND_DATA,
		    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file != INVALID_HANDLE_VALUE) {
			log = _open_osfhandle(reinterpret_cast<intptr_t>(file), _O_WRONLY | _O_APPEND | _O_BINARY);
			if (log < 0) CloseHandle(file);
		}
		if (log >= 0) {
			if (captureOut) capturedOut = redirect(stdout, STD_OUTPUT_HANDLE, log);
			const bool capturedErr = captureErr && redirect(stderr, STD_ERROR_HANDLE, log);
			crash = path;
			crash.replace_extension(".crash.log");
			if ((captureOut && !capturedOut) || (captureErr && !capturedErr))
				std::fprintf(stderr, "[logging] Could not redirect every output stream\n");
			prune(directory);
		} else {
			std::fprintf(stderr, "[logging] Automatic log capture unavailable; check the log directory and free disk space\n");
		}
	}
	WIN32_FILE_ATTRIBUTE_DATA info{};
	GetFileAttributesExW(exe.c_str(), GetFileExInfoStandard, &info);
	char header[512];
	std::snprintf(header, sizeof(header),
	    "[session] Sunshine %s %s Windows x64 GMSJ01 pid=%lu UTC=%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\n"
	    "[build] exe_bytes=%llu exe_mtime=%08lx%08lx logger_compiled=%s %s\n",
	    SUNSHINE_VERSION, SUNSHINE_BUILD_TYPE, GetCurrentProcessId(), now.wYear, now.wMonth, now.wDay,
	    now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
	    (static_cast<unsigned long long>(info.nFileSizeHigh) << 32) | info.nFileSizeLow,
	    info.ftLastWriteTime.dwHighDateTime, info.ftLastWriteTime.dwLowDateTime, __DATE__, __TIME__);
	std::fputs(header, stdout);
	if (log >= 0) {
		if (!capturedOut) _write(log, header, static_cast<unsigned>(std::char_traits<char>::length(header)));
		_close(log);
	}
	return crash;
}
}
