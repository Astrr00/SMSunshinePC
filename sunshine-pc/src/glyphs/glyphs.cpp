#include "glyphs.h"

#include <windows.h>
#include <zlib.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

GlyphStyle g_glyphStyle = GlyphStyle::Xbox;

namespace {

constexpr const char* kStyleDir[] = {"xbox", "playstation", "switch", "gamecube", "keyboard"};
constexpr const char* kButtonFile[] = {"a", "b", "x", "y", "z", "start", "l", "r",
                                       "dpad_up", "dpad_down", "dpad_left", "dpad_right",
                                       "c_up", "c_down", "c_left", "c_right", "stick_l", "stick_r"};
static_assert(sizeof(kStyleDir) / sizeof(kStyleDir[0]) == (size_t)GlyphStyle::Count);
static_assert(sizeof(kButtonFile) / sizeof(kButtonFile[0]) == (size_t)PadButton::Count);

struct Entry {
    bool tried = false;
    bool ok = false;
    GlyphImage image{};
    std::vector<uint8_t> pixels;
};

Entry g_entries[(size_t)GlyphStyle::Count][(size_t)PadButton::Count];

bool readFile(const char* path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    bool ok = false;
    if (size > 0) {
        out.resize((size_t)size);
        ok = std::fread(out.data(), 1, out.size(), f) == out.size();
    }
    std::fclose(f);
    return ok;
}

uint32_t readBE32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

uint8_t paeth(int a, int b, int c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) {
        return uint8_t(a);
    }
    return uint8_t(pb <= pc ? b : c);
}

// Decodes an 8-bit RGBA, non-interlaced PNG into `pixels` (top row first). Supports all five PNG filters.
// ponytail: single-threaded inflate of whole file; ceiling is startup/first-use cost on huge PNGs, fine for 128x128 icons.
bool decodePng(const std::vector<uint8_t>& file, uint32_t& width, uint32_t& height, std::vector<uint8_t>& pixels) {
    static const uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (file.size() < 8 || std::memcmp(file.data(), kSignature, 8) != 0) {
        return false;
    }

    std::vector<uint8_t> idat;
    bool haveHeader = false;
    size_t pos = 8;
    while (pos + 12 <= file.size()) {
        uint32_t len = readBE32(&file[pos]);
        if (len > file.size() - pos - 12) {
            return false;
        }
        const uint8_t* type = &file[pos + 4];
        const uint8_t* data = &file[pos + 8];
        if (std::memcmp(type, "IHDR", 4) == 0) {
            if (len != 13 || data[8] != 8 || data[9] != 6 || data[12] != 0) {
                return false;
            }
            width = readBE32(data);
            height = readBE32(data + 4);
            haveHeader = true;
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), data, data + len);
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += 12 + len;
    }
    if (!haveHeader || width == 0 || height == 0) {
        return false;
    }

    const size_t stride = size_t(width) * 4;
    std::vector<uint8_t> raw(size_t(height) * (stride + 1));
    uLongf rawLen = (uLongf)raw.size();
    if (uncompress(raw.data(), &rawLen, idat.data(), (uLong)idat.size()) != Z_OK || rawLen != raw.size()) {
        return false;
    }

    pixels.assign(size_t(height) * stride, 0);
    for (size_t y = 0; y < height; ++y) {
        const uint8_t filter = raw[y * (stride + 1)];
        const uint8_t* src = &raw[y * (stride + 1) + 1];
        uint8_t* cur = &pixels[y * stride];
        const uint8_t* prev = y ? &pixels[(y - 1) * stride] : nullptr;
        for (size_t i = 0; i < stride; ++i) {
            int left = i >= 4 ? cur[i - 4] : 0;
            int up = prev ? prev[i] : 0;
            int upLeft = (prev && i >= 4) ? prev[i - 4] : 0;
            int pred;
            switch (filter) {
                case 0: pred = 0; break;
                case 1: pred = left; break;
                case 2: pred = up; break;
                case 3: pred = (left + up) / 2; break;
                case 4: pred = paeth(left, up, upLeft); break;
                default: return false;
            }
            cur[i] = uint8_t(src[i] + pred);
        }
    }
    return true;
}

bool loadEntry(GlyphStyle style, PadButton button, Entry& e) {
    char exe[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, exe, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return false;
    }
    std::string path(exe, n);
    path.erase(path.find_last_of('\\') + 1);
    path += "assets\\glyphs\\";
    path += kStyleDir[(size_t)style];
    path += '\\';
    path += kButtonFile[(size_t)button];
    path += ".png";

    std::vector<uint8_t> file;
    uint32_t w = 0, h = 0;
    if (!readFile(path.c_str(), file) || !decodePng(file, w, h, e.pixels)) {
        std::fprintf(stderr, "glyphs: cannot load %s\n", path.c_str());
        e.pixels.clear();
        return false;
    }
    e.image = GlyphImage{w, h, e.pixels.data()};
    std::printf("[glyph] %s/%s %ux%u\n", kStyleDir[(size_t)style], kButtonFile[(size_t)button], w, h);
    return true;
}

} // namespace

void glyphsSetStyle(GlyphStyle style) {
    g_glyphStyle = style;
}

const GlyphImage* glyphFor(GlyphStyle style, PadButton button) {
    if (style >= GlyphStyle::Count || button >= PadButton::Count) {
        return nullptr;
    }
    Entry& e = g_entries[(size_t)style][(size_t)button];
    if (!e.tried) {
        e.tried = true;
        e.ok = loadEntry(style, button, e);
    }
    return e.ok ? &e.image : nullptr;
}
