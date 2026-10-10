#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "util/read_file.h"

#include <algorithm>
#include <cstdint>

bool readWholeFile(const std::filesystem::path& path, std::string& out) {
    out.clear();
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size{};
    constexpr LONGLONG kMaxBytes = 256LL << 20;  // mod and cheat files are small; refuse anything absurd
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > kMaxBytes) {
        CloseHandle(file);
        return false;
    }
    out.resize(static_cast<size_t>(size.QuadPart));
    // Bounded: each pass either advances or stops (EOF or error), so an unreadable file cannot spin.
    size_t done = 0;
    bool ok = true;
    while (done < out.size()) {
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(out.size() - done, 1u << 20));
        DWORD got = 0;
        if (!ReadFile(file, &out[done], chunk, &got, nullptr) || got == 0) {
            ok = false;
            break;
        }
        done += got;
    }
    CloseHandle(file);
    if (!ok) {
        out.clear();
    }
    return ok;
}
