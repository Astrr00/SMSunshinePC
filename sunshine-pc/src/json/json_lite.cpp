#include "json/json_lite.h"

#include <cstdint>
#include <cstdlib>

namespace json {

const Value* Value::find(const char* key) const {
    if (type != Type::Object) {
        return nullptr;
    }
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i] == key) {
            return &values[i];
        }
    }
    return nullptr;
}

namespace {

constexpr int kMaxDepth = 64;

void appendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out += (char)cp;
    } else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

struct Parser {
    const std::string& text;
    size_t pos = 0;
    std::string error;

    explicit Parser(const std::string& t) : text(t) {}

    bool fail(const char* message) {
        if (error.empty()) {
            size_t line = 1;
            size_t col = 1;
            for (size_t i = 0; i < pos && i < text.size(); i++) {
                if (text[i] == '\n') {
                    line++;
                    col = 1;
                } else {
                    col++;
                }
            }
            error = std::to_string(line) + ":" + std::to_string(col) + ": " + message;
        }
        return false;
    }

    void skipWs() {
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' || text[pos] == '\r')) {
            pos++;
        }
    }

    bool hex4(uint32_t& out) {
        if (pos + 4 > text.size()) {
            return fail("truncated \\u escape");
        }
        out = 0;
        for (int i = 0; i < 4; i++) {
            char c = text[pos++];
            uint32_t d;
            if (c >= '0' && c <= '9') {
                d = (uint32_t)(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                d = (uint32_t)(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                d = (uint32_t)(c - 'A' + 10);
            } else {
                return fail("bad hex digit in \\u escape");
            }
            out = (out << 4) | d;
        }
        return true;
    }

    bool parseString(std::string& out) {
        // Caller has checked text[pos] == '"'.
        pos++;
        while (true) {
            if (pos >= text.size()) {
                return fail("unterminated string");
            }
            char c = text[pos++];
            if (c == '"') {
                return true;
            }
            if ((unsigned char)c < 0x20) {
                return fail("control character in string");
            }
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos >= text.size()) {
                return fail("unterminated escape");
            }
            char e = text[pos++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    uint32_t cp;
                    if (!hex4(cp)) {
                        return false;
                    }
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        // High surrogate: must be followed by \uDC00..\uDFFF.
                        uint32_t lo;
                        if (pos + 2 > text.size() || text[pos] != '\\' || text[pos + 1] != 'u') {
                            return fail("lone high surrogate");
                        }
                        pos += 2;
                        if (!hex4(lo)) {
                            return false;
                        }
                        if (lo < 0xDC00 || lo > 0xDFFF) {
                            return fail("bad low surrogate");
                        }
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default:
                    return fail("bad escape character");
            }
        }
    }

    bool parseNumber(Value& out) {
        char* end = nullptr;
        const char* start = text.c_str() + pos;
        out.number = std::strtod(start, &end);
        if (end == start) {
            return fail("bad number");
        }
        pos += (size_t)(end - start);
        out.type = Value::Type::Number;
        return true;
    }

    bool parseLiteral(const char* word, Value& out) {
        size_t n = std::char_traits<char>::length(word);
        if (text.compare(pos, n, word) != 0) {
            return fail("bad literal");
        }
        pos += n;
        if (word[0] == 'n') {
            out.type = Value::Type::Null;
        } else {
            out.type = Value::Type::Bool;
            out.boolean = (word[0] == 't');
        }
        return true;
    }

    bool parseValue(Value& out, int depth) {
        if (depth > kMaxDepth) {
            return fail("nesting too deep");
        }
        skipWs();
        if (pos >= text.size()) {
            return fail("unexpected end of input");
        }
        char c = text[pos];
        if (c == '{') {
            pos++;
            out.type = Value::Type::Object;
            skipWs();
            if (pos < text.size() && text[pos] == '}') {
                pos++;
                return true;
            }
            while (true) {
                skipWs();
                if (pos >= text.size() || text[pos] != '"') {
                    return fail("expected object key");
                }
                std::string key;
                if (!parseString(key)) {
                    return false;
                }
                skipWs();
                if (pos >= text.size() || text[pos] != ':') {
                    return fail("expected ':'");
                }
                pos++;
                Value member;
                if (!parseValue(member, depth + 1)) {
                    return false;
                }
                out.keys.push_back(std::move(key));
                out.values.push_back(std::move(member));
                skipWs();
                if (pos < text.size() && text[pos] == ',') {
                    pos++;
                    continue;
                }
                if (pos < text.size() && text[pos] == '}') {
                    pos++;
                    return true;
                }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            pos++;
            out.type = Value::Type::Array;
            skipWs();
            if (pos < text.size() && text[pos] == ']') {
                pos++;
                return true;
            }
            while (true) {
                Value element;
                if (!parseValue(element, depth + 1)) {
                    return false;
                }
                out.array.push_back(std::move(element));
                skipWs();
                if (pos < text.size() && text[pos] == ',') {
                    pos++;
                    continue;
                }
                if (pos < text.size() && text[pos] == ']') {
                    pos++;
                    return true;
                }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') {
            out.type = Value::Type::String;
            return parseString(out.string);
        }
        if (c == 't') {
            return parseLiteral("true", out);
        }
        if (c == 'f') {
            return parseLiteral("false", out);
        }
        if (c == 'n') {
            return parseLiteral("null", out);
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return parseNumber(out);
        }
        return fail("unexpected character");
    }
};

}  // namespace

bool parse(const std::string& text, Value& out, std::string& error) {
    Parser p(text);
    out = Value{};
    bool ok = p.parseValue(out, 0);
    if (ok) {
        p.skipWs();
        if (p.pos != text.size()) {
            ok = p.fail("trailing characters after value");
        }
    }
    if (!ok) {
        error = p.error;
    }
    return ok;
}

}  // namespace json
