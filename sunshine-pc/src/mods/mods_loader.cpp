// Mod loader (Win32). See mods_loader.h. Pure parsing and merging live in mod_manifest.cpp; this file does the I/O.
// Logs go to stdout and <exe>/sunshine_mods.log, one line per event, prefixed "[mods]" or "[mod <id>]".
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "aurora/dvd.h"
#include "mods/mod_manifest.h"
#include "mods/mods_host.h"
#include "mods/mods_loader.h"
#include "sunshine_mod_api.h"
#include "util/read_file.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <set>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

extern "C" void sunshineEnterSehGuard();
extern "C" void sunshineLeaveSehGuard();

namespace {

FILE* gLogFile = nullptr;

struct ActiveMod {
    mods::Manifest manifest;
    HMODULE module = nullptr;
    SunshineModCtx* ctx = nullptr;
    SunshineModShutdownFn shutdown = nullptr;
    SunshineModLogicTickFn onLogicTick = nullptr;
    SunshineModStageLoadFn onStageLoad = nullptr;
    SunshineModPresentFn onPresent = nullptr;
    bool alive = false;
};

std::vector<ActiveMod> gMods;
uint32_t gLastStage = 0xFFFFFFFFu;

// deque: element addresses stay valid, and AuroraOverlayFile::userData points into it.
struct OverlaySource {
    fs::path hostPath;
    std::atomic<bool> announced{false};
};
std::deque<OverlaySource> gOverlaySources;
std::deque<std::string> gOverlayNames;  // AuroraOverlayFile::fileName points into this
// Every overlay offered by an accepted mod, in load order. applyOverlays() merges the live subset.
std::vector<mods::OverlayFile> gOverlayCandidates;

// Calls fn(args...) under SEH. Returns false and the exception code when the mod faults.
// Only trivially destructible values live in this frame: __try cannot unwind C++ objects.
template <typename... Args>
bool guardedVoid(void (*fn)(Args...), unsigned long& code, Args... args) {
    sunshineEnterSehGuard();
    __try {
        fn(args...);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        code = GetExceptionCode();
        sunshineLeaveSehGuard();
        return false;
    }
    sunshineLeaveSehGuard();
    return true;
}

template <typename R, typename... Args>
bool guardedRet(R (*fn)(Args...), unsigned long& code, R& result, Args... args) {
    sunshineEnterSehGuard();
    __try {
        result = fn(args...);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        code = GetExceptionCode();
        sunshineLeaveSehGuard();
        return false;
    }
    sunshineLeaveSehGuard();
    return true;
}

void applyOverlays(bool report);

void disableMod(ActiveMod& m, const char* where, unsigned long code) {
    m.alive = false;
    char buf[160];
    std::snprintf(buf, sizeof(buf), "[mod %s] faulted in %s (exception 0x%08lX), disabled for this session",
                  m.manifest.id.c_str(), where, code);
    modsLogLine(SUNSHINE_LOG_ERROR, buf);
    applyOverlays(false);
}


// Loads mod.dll, checks its API version and runs sunshine_mod_init. False: the mod is not active.
bool loadDll(const mods::Manifest& m, const fs::path& dll, ActiveMod& out) {
    const std::string tag = "[mod " + m.id + "] ";
    HMODULE module = LoadLibraryW(dll.c_str());
    if (!module) {
        modsLogLine(SUNSHINE_LOG_ERROR, tag + "LoadLibrary failed (error " + std::to_string(GetLastError()) + ")");
        return false;
    }
    auto fail = [&](const std::string& why) {
        modsLogLine(SUNSHINE_LOG_ERROR, tag + why);
        FreeLibrary(module);
        return false;
    };

    auto apiFn = reinterpret_cast<SunshineModApiVersionFn>(GetProcAddress(module, "sunshine_mod_api_version"));
    auto initFn = reinterpret_cast<SunshineModInitFn>(GetProcAddress(module, "sunshine_mod_init"));
    if (!apiFn || !initFn) {
        return fail("missing sunshine_mod_api_version or sunshine_mod_init export");
    }

    unsigned long code = 0;
    uint32_t raw = 0;
    if (!guardedRet(apiFn, code, raw)) {
        return fail("faulted in sunshine_mod_api_version");
    }
    mods::ApiVersion dllApi{raw >> 16, raw & 0xFFFFu};
    std::string why;
    if (!mods::apiCompatible(dllApi, mods::kHostApi, why)) {
        return fail("DLL API " + std::to_string(dllApi.major) + "." + std::to_string(dllApi.minor) + ": " + why);
    }
    if (dllApi.major != m.api.major || dllApi.minor != m.api.minor) {
        modsLogLine(SUNSHINE_LOG_WARN, tag + "mod.json says sunshine_api differs from the DLL; using the DLL value");
    }

    SunshineModCtx* ctx = new SunshineModCtx{m.id};
    int32_t rc = -1;
    if (!guardedRet(initFn, code, rc, ctx, &modsHostApi())) {
        delete ctx;
        return fail("faulted in sunshine_mod_init");
    }
    if (rc != 0) {
        delete ctx;
        return fail("sunshine_mod_init returned " + std::to_string(rc));
    }

    out.manifest = m;
    out.module = module;
    out.ctx = ctx;
    out.shutdown = reinterpret_cast<SunshineModShutdownFn>(GetProcAddress(module, "sunshine_mod_shutdown"));
    out.onLogicTick = reinterpret_cast<SunshineModLogicTickFn>(GetProcAddress(module, "sunshine_mod_on_logic_tick"));
    out.onStageLoad = reinterpret_cast<SunshineModStageLoadFn>(GetProcAddress(module, "sunshine_mod_on_stage_load"));
    out.onPresent = reinterpret_cast<SunshineModPresentFn>(GetProcAddress(module, "sunshine_mod_on_present"));
    out.alive = true;
    modsLogLine(SUNSHINE_LOG_INFO, tag + "loaded " + m.name + " " + m.version);
    return true;
}

// Adds <mod>/files/** as overlay candidates. Paths are relative to files/ and map to the DVD root.
void collectOverlays(const mods::Manifest& m, std::vector<mods::OverlayFile>& out) {
    const fs::path files = fs::path(m.dir) / "files";
    std::error_code ec;
    if (!fs::is_directory(files, ec)) {
        return;
    }
    for (fs::recursive_directory_iterator it(files, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) {
            continue;
        }
        mods::OverlayFile f;
        f.dvdPath = mods::overlayDvdPath(fs::relative(it->path(), files, ec).generic_string());
        f.hostPath = it->path().string();
        f.size = static_cast<uint64_t>(it->file_size(ec));
        f.modId = m.id;
        out.push_back(f);
    }
}

void* overlayOpen(void* userdata) {
    auto* src = static_cast<OverlaySource*>(userdata);
    if (!src->announced.exchange(true)) {
        modsLogLine(SUNSHINE_LOG_INFO, "[mods] overlay in use: " + src->hostPath.string());
    }
    return _wfopen(src->hostPath.c_str(), L"rb");
}

void overlayClose(void* handle) {
    std::fclose(static_cast<FILE*>(handle));
}

int64_t overlayRead(void* handle, uint8_t* buf, size_t len) {
    FILE* f = static_cast<FILE*>(handle);
    size_t n = std::fread(buf, 1, len, f);
    if (n == 0 && std::ferror(f)) {
        return -1;
    }
    return static_cast<int64_t>(n);
}

int64_t overlaySeek(void* handle, int64_t offset, int32_t whence) {
    FILE* f = static_cast<FILE*>(handle);
    if (_fseeki64(f, offset, whence) != 0) {
        return -1;
    }
    return _ftelli64(f);
}

void registerOverlays(const std::vector<mods::OverlayFile>& files) {
    static const AuroraOverlayCallbacks callbacks{overlayOpen, overlayClose, overlayRead, overlaySeek};
    aurora_dvd_overlay_callbacks(&callbacks);

    std::vector<AuroraOverlayFile> entries;
    entries.reserve(files.size());
    for (const mods::OverlayFile& f : files) {
        gOverlayNames.push_back(f.dvdPath);
        gOverlaySources.emplace_back();
        gOverlaySources.back().hostPath = fs::path(f.hostPath);

        AuroraOverlayFile e{};
        e.fileName = gOverlayNames.back().c_str();
        e.userData = &gOverlaySources.back();
        e.size = static_cast<size_t>(f.size);
        entries.push_back(e);
    }

    std::vector<s32> entryNums(entries.size(), -1);
    aurora_dvd_overlay_files(entries.data(), entries.size(), entryNums.data());

    for (size_t i = 0; i < files.size(); ++i) {
        if (entryNums[i] == -1) {
            modsLogLine(SUNSHINE_LOG_WARN, "[mods] overlay rejected (bad path, over 4 GiB, or a directory clash): " +
                                               files[i].dvdPath + " (from " + files[i].modId + ")");
        }
    }
}

bool modIsLive(const std::string& id) {
    for (const ActiveMod& m : gMods) {
        if (m.manifest.id == id) {
            return m.alive;
        }
    }
    return true;  // data-only mod: no DLL that could fault
}

// Re-merges the candidates of live mods (load order, last wins) and hands the set to Aurora.
// Runs at load and after every disableMod, so a disabled mod's files leave the DVD view.
// ponytail: rebuilds the whole FST per disable; fine for a rare fault, not a per-frame path.
void applyOverlays(bool report) {
    std::vector<mods::OverlayFile> live;
    for (const mods::OverlayFile& f : gOverlayCandidates) {
        if (modIsLive(f.modId)) {
            live.push_back(f);
        }
    }
    std::vector<std::string> replaced;
    const std::vector<mods::OverlayFile> merged = mods::mergeOverlays(live, replaced);
    if (report) {
        for (const std::string& r : replaced) {
            modsLogLine(SUNSHINE_LOG_INFO, "[mods] overlay replaced " + r);
        }
    }
    registerOverlays(merged);  // empty set clears Aurora's overlays
    modsLogLine(SUNSHINE_LOG_INFO, "[mods] " + std::to_string(merged.size()) + " overlay file(s) active");
}

}  // namespace

void modsLogLine(int32_t level, const std::string& line) {
    const char* tag = level == SUNSHINE_LOG_ERROR ? "ERROR" : level == SUNSHINE_LOG_WARN ? "WARN" : "INFO";
    std::printf("%s\n", line.c_str());
    if (gLogFile) {
        std::fprintf(gLogFile, "[%s] %s\n", tag, line.c_str());
        std::fflush(gLogFile);
    }
}

std::vector<mods::Manifest> scanModManifests(const std::string& exeDir) {
    std::vector<mods::Manifest> manifests;
    const fs::path root = fs::path(exeDir) / "mods";
    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        return manifests;
    }
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec)) {
            continue;
        }
        const fs::path manifestPath = it->path() / "mod.json";
        if (!fs::exists(manifestPath, ec)) {
            modsLogLine(SUNSHINE_LOG_WARN, "[mods] " + it->path().string() + " has no mod.json, skipped");
            continue;
        }
        std::string text;
        if (!readWholeFile(manifestPath, text)) {
            modsLogLine(SUNSHINE_LOG_ERROR, "[mods] cannot read " + manifestPath.string());
            continue;
        }
        mods::Manifest m;
        std::string err;
        if (!mods::parseManifest(text, m, err)) {
            modsLogLine(SUNSHINE_LOG_ERROR, "[mods] " + manifestPath.string() + ": " + err);
            continue;
        }
        m.dir = it->path().string();
        manifests.push_back(m);
    }
    return manifests;
}

bool modsLoad(const std::string& exeDir, const std::map<std::string, bool>& modSwitches) {
    const fs::path exe = fs::path(exeDir);
    gLogFile = _wfopen((exe / L"sunshine_mods.log").c_str(), L"w");

    const fs::path root = exe / "mods";
    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        modsLogLine(SUNSHINE_LOG_INFO, "[mods] no mods folder (" + root.string() + "), 0 mods loaded");
        return true;
    }

    std::vector<mods::Manifest> manifests = scanModManifests(exeDir);
    mods::sortLoadOrder(manifests);

    std::set<std::string> seen;
    std::vector<mods::OverlayFile> overlays;
    for (const mods::Manifest& m : manifests) {
        const auto sw = modSwitches.find(m.id);
        if (sw != modSwitches.end() && !sw->second) {
            modsLogLine(SUNSHINE_LOG_INFO, "[mods] " + m.id + " disabled by settings");
            continue;
        }
        if (!seen.insert(m.id).second) {
            modsLogLine(SUNSHINE_LOG_ERROR, "[mods] duplicate mod id '" + m.id + "' in " + m.dir + ", skipped");
            continue;
        }
        std::string why;
        if (!mods::apiCompatible(m.api, mods::kHostApi, why)) {
            modsLogLine(SUNSHINE_LOG_ERROR, "[mod " + m.id + "] skipped: " + why);
            continue;
        }
        bool accepted = true;
        const fs::path dll = fs::path(m.dir) / "mod.dll";
        if (fs::exists(dll, ec)) {
            ActiveMod am;
            accepted = loadDll(m, dll, am);
            if (accepted) {
                gMods.push_back(am);
            }
        }
        if (accepted) {
            collectOverlays(m, overlays);
        }
    }

    gOverlayCandidates = std::move(overlays);
    applyOverlays(true);
    modsLogLine(SUNSHINE_LOG_INFO, "[mods] " + std::to_string(gMods.size()) + " DLL mod(s) active");
    return true;
}

void modsLogicTick() {
    const uint32_t stage = modsCurrentStage();
    if (stage != gLastStage) {
        gLastStage = stage;
        for (ActiveMod& m : gMods) {
            unsigned long code = 0;
            if (m.alive && m.onStageLoad && !guardedVoid(m.onStageLoad, code, m.ctx, stage)) {
                disableMod(m, "sunshine_mod_on_stage_load", code);
            }
        }
    }
    for (ActiveMod& m : gMods) {
        unsigned long code = 0;
        if (m.alive && m.onLogicTick && !guardedVoid(m.onLogicTick, code, m.ctx)) {
            disableMod(m, "sunshine_mod_on_logic_tick", code);
        }
    }
}

void modsPresent() {
    for (ActiveMod& m : gMods) {
        unsigned long code = 0;
        if (m.alive && m.onPresent && !guardedVoid(m.onPresent, code, m.ctx)) {
            disableMod(m, "sunshine_mod_on_present", code);
        }
    }
}

void modsShutdown() {
    for (ActiveMod& m : gMods) {
        unsigned long code = 0;
        if (m.alive && m.shutdown && !guardedVoid(m.shutdown, code, m.ctx)) {
            disableMod(m, "sunshine_mod_shutdown", code);
        }
        m.alive = false;
        delete m.ctx;
        m.ctx = nullptr;
        if (m.module) {
            FreeLibrary(m.module);
            m.module = nullptr;
        }
    }
    gMods.clear();
    if (gLogFile) {
        std::fclose(gLogFile);
        gLogFile = nullptr;
    }
}
