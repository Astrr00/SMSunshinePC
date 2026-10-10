// Minimal JSON reader for the port's own data files (cheats.json, mod.json). Full JSON syntax for input
// (objects, arrays, strings with escapes incl. \uXXXX, numbers, true/false/null), no writer, no streaming.
#pragma once

#include <string>
#include <vector>

namespace json {

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Value> array;
    std::vector<std::string> keys;  // object members; keys[i] belongs to values[i]
    std::vector<Value> values;

    // Object member lookup; nullptr when this is not an object or the key is absent.
    const Value* find(const char* key) const;
};

// Parses `text` into `out`. On failure returns false and sets `error` to "line:col: message".
bool parse(const std::string& text, Value& out, std::string& error);

}  // namespace json
