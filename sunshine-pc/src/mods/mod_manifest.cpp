#include "mods/mod_manifest.h"

#include "json/json_lite.h"

#include <algorithm>
#include <cctype>

namespace mods {
namespace {

bool validId(const std::string& id) {
    if (id.empty() || id.size() > 64) {
        return false;
    }
    for (char c : id) {
        if (!(std::isalnum((unsigned char)c) || c == '_' || c == '-')) {
            return false;
        }
    }
    return true;
}

bool parseNumber(const std::string& s, uint32_t& out) {
    if (s.empty() || s.size() > 9) {
        return false;
    }
    out = 0;
    for (char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
        out = out * 10 + (uint32_t)(c - '0');
    }
    return true;
}

bool parseApi(const std::string& s, ApiVersion& out) {
    size_t dot = s.find('.');
    if (dot == std::string::npos) {
        return false;
    }
    return parseNumber(s.substr(0, dot), out.major) && parseNumber(s.substr(dot + 1), out.minor);
}

bool readString(const json::Value& root, const char* key, std::string& out, std::string& error) {
    const json::Value* v = root.find(key);
    if (v == nullptr) {
        return true;
    }
    if (v->type != json::Value::Type::String) {
        error = std::string("'") + key + "' must be a string";
        return false;
    }
    out = v->string;
    return true;
}

std::string lowerAscii(std::string s) {
    for (char& c : s) {
        c = (char)std::tolower((unsigned char)c);
    }
    return s;
}

}  // namespace

bool parseManifest(const std::string& text, Manifest& out, std::string& error) {
    json::Value root;
    if (!json::parse(text, root, error)) {
        return false;
    }
    if (root.type != json::Value::Type::Object) {
        error = "mod.json must be an object";
        return false;
    }

    Manifest m;
    if (!readString(root, "id", m.id, error) || !readString(root, "name", m.name, error) ||
        !readString(root, "version", m.version, error) || !readString(root, "author", m.author, error) ||
        !readString(root, "description", m.description, error)) {
        return false;
    }
    if (!validId(m.id)) {
        error = "'id' is missing or contains characters other than letters, digits, '_' and '-'";
        return false;
    }
    if (m.name.empty()) {
        m.name = m.id;
    }

    std::string api;
    if (!readString(root, "sunshine_api", api, error)) {
        return false;
    }
    if (api.empty() || !parseApi(api, m.api)) {
        error = "'sunshine_api' must be \"major.minor\"";
        return false;
    }

    if (const json::Value* order = root.find("load_order")) {
        if (order->type != json::Value::Type::Number || order->number < -2147483648.0 || order->number > 2147483647.0 ||
            order->number != (double)(int64_t)order->number) {
            error = "'load_order' must be an integer";
            return false;
        }
        m.loadOrder = (int32_t)order->number;
    }

    out = std::move(m);
    return true;
}

bool apiCompatible(ApiVersion mod, ApiVersion host, std::string& why) {
    if (mod.major != host.major) {
        why = "needs C API major " + std::to_string(mod.major) + ", host is " + std::to_string(host.major);
        return false;
    }
    if (mod.minor > host.minor) {
        why = "needs C API " + std::to_string(mod.major) + "." + std::to_string(mod.minor) + ", host is " +
              std::to_string(host.major) + "." + std::to_string(host.minor);
        return false;
    }
    return true;
}

void sortLoadOrder(std::vector<Manifest>& manifests) {
    std::stable_sort(manifests.begin(), manifests.end(), [](const Manifest& a, const Manifest& b) {
        if (a.loadOrder != b.loadOrder) {
            return a.loadOrder < b.loadOrder;
        }
        return a.id < b.id;
    });
}

std::string overlayDvdPath(const std::string& relative) {
    if (relative.empty()) {
        return "";
    }
    std::string path = "/";
    for (char c : relative) {
        path += (c == '\\') ? '/' : c;
    }
    return path;
}

std::vector<OverlayFile> mergeOverlays(const std::vector<OverlayFile>& inLoadOrder, std::vector<std::string>& replaced) {
    std::vector<OverlayFile> merged;
    for (const OverlayFile& file : inLoadOrder) {
        std::string key = lowerAscii(file.dvdPath);
        bool found = false;
        for (OverlayFile& existing : merged) {
            if (lowerAscii(existing.dvdPath) == key) {
                replaced.push_back(file.dvdPath + ": " + existing.modId + " -> " + file.modId);
                existing = file;
                found = true;
                break;
            }
        }
        if (!found) {
            merged.push_back(file);
        }
    }
    return merged;
}

}  // namespace mods
