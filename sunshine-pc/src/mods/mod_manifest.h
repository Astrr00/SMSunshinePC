// Pure mod logic: parse mod.json, check the C API version, order mods, merge overlay file lists.
// No OS calls, so tests/port_tests.cpp runs it headless. The loader (mods_loader.cpp) does the I/O.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mods {

struct ApiVersion {
    uint32_t major = 0;
    uint32_t minor = 0;
};

// Host C API version. Keep in sync with SUNSHINE_MOD_API_MAJOR / SUNSHINE_MOD_API_MINOR in sunshine_mod_api.h.
constexpr ApiVersion kHostApi{1, 0};

struct Manifest {
    std::string id;           // [A-Za-z0-9_-], unique; used in logs and as the mod's folder name convention
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    ApiVersion api;          // "sunshine_api": "major.minor" in mod.json
    int32_t loadOrder = 0;   // lower loads first; ties are broken by id
    std::string dir;         // folder the manifest was read from (set by the loader)
};

// Parses mod.json text. Required: id, sunshine_api. Optional: name, version, author, description, load_order.
bool parseManifest(const std::string& text, Manifest& out, std::string& error);

// A mod is compatible when the major version is equal and the mod's minor is not newer than the host's.
bool apiCompatible(ApiVersion mod, ApiVersion host, std::string& why);

// Sorts by (load_order, id). Stable for equal keys.
void sortLoadOrder(std::vector<Manifest>& manifests);

// One file a mod supplies to replace a DVD file. dvdPath is "/" + path under the mod's files/ folder.
struct OverlayFile {
    std::string dvdPath;
    std::string hostPath;
    uint64_t size = 0;
    std::string modId;
};

// "files/" relative path -> DVD path ("/" + path with '/' separators). Returns "" for an empty path.
std::string overlayDvdPath(const std::string& relative);

// Input is in load order. A path supplied twice (case-insensitive) keeps the later entry; each replacement is
// reported in `replaced` as "path: <old mod> -> <new mod>".
std::vector<OverlayFile> mergeOverlays(const std::vector<OverlayFile>& inLoadOrder, std::vector<std::string>& replaced);

}  // namespace mods
