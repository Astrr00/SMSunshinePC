#include "cheats/cheats_core.h"

#include "json/json_lite.h"

namespace cheats {
namespace {

struct EffectName {
    const char* name;
    Effect effect;
    bool oneShot;
};

constexpr EffectName kEffects[] = {
    {"keep_health_full", Effect::KeepHealthFull, false},
    {"keep_water_full", Effect::KeepWaterFull, false},
    {"keep_lives_max", Effect::KeepLivesMax, false},
    {"unlock_all_nozzles", Effect::UnlockAllNozzles, true},
};

// Stages 1..9 are the shine-stage indices of the nozzle flags (sms FlagManager setNozzleRight clamps stage 1..9).
constexpr uint8_t kFirstStage = 1;
constexpr uint8_t kLastStage = 9;
constexpr uint8_t kNozzleCount = 2;

bool effectFromName(const std::string& name, Effect& effect, bool& oneShot) {
    for (const EffectName& e : kEffects) {
        if (name == e.name) {
            effect = e.effect;
            oneShot = e.oneShot;
            return true;
        }
    }
    return false;
}

bool readString(const json::Value& obj, const char* key, std::string& out, std::string& error) {
    const json::Value* v = obj.find(key);
    if (v == nullptr) {
        return true;
    }
    if (v->type != json::Value::Type::String) {
        error = std::string("field '") + key + "' must be a string";
        return false;
    }
    out = v->string;
    return true;
}

bool readBool(const json::Value& obj, const char* key, bool& out, bool& present, std::string& error) {
    const json::Value* v = obj.find(key);
    present = (v != nullptr);
    if (v == nullptr) {
        return true;
    }
    if (v->type != json::Value::Type::Bool) {
        error = std::string("field '") + key + "' must be true or false";
        return false;
    }
    out = v->boolean;
    return true;
}

// Reads the "cheats" array of a cheats file. Calls visit(entry) for each object.
template <typename Visit>
bool forEachEntry(const std::string& text, std::string& error, Visit visit) {
    json::Value root;
    if (!json::parse(text, root, error)) {
        return false;
    }
    if (root.type != json::Value::Type::Object) {
        error = "top level must be an object";
        return false;
    }
    const json::Value* list = root.find("cheats");
    if (list == nullptr || list->type != json::Value::Type::Array) {
        error = "missing 'cheats' array";
        return false;
    }
    for (const json::Value& entry : list->array) {
        if (entry.type != json::Value::Type::Object) {
            error = "cheat entry must be an object";
            return false;
        }
        const json::Value* id = entry.find("id");
        if (id == nullptr || id->type != json::Value::Type::String || id->string.empty()) {
            error = "cheat entry without a string 'id'";
            return false;
        }
        if (!visit(entry, id->string)) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool Registry::loadBase(const std::string& text, std::string& error) {
    std::vector<Cheat> loaded;
    bool ok = forEachEntry(text, error, [&](const json::Value& entry, const std::string& id) {
        for (const Cheat& c : loaded) {
            if (c.id == id) {
                error = "duplicate cheat id '" + id + "'";
                return false;
            }
        }
        Cheat c;
        c.id = id;
        std::string effectName;
        if (!readString(entry, "name", c.name, error) || !readString(entry, "description", c.description, error) ||
            !readString(entry, "category", c.category, error) || !readString(entry, "effect", effectName, error)) {
            return false;
        }
        if (c.name.empty()) {
            error = "cheat '" + id + "' has no name";
            return false;
        }
        if (!effectFromName(effectName, c.effect, c.oneShot)) {
            error = "cheat '" + id + "' has unknown effect '" + effectName + "'";
            return false;
        }
        bool present = false;
        if (!readBool(entry, "default", c.enabled, present, error)) {
            return false;
        }
        loaded.push_back(c);
        return true;
    });
    if (ok) {
        cheats_ = std::move(loaded);
    }
    return ok;
}

bool Registry::applyUser(const std::string& text, std::string& error) {
    std::vector<Cheat> next = cheats_;
    bool ok = forEachEntry(text, error, [&](const json::Value& entry, const std::string& id) {
        Cheat* existing = nullptr;
        for (Cheat& c : next) {
            if (c.id == id) {
                existing = &c;
                break;
            }
        }
        Cheat fresh;
        Cheat& target = existing ? *existing : fresh;
        target.id = id;

        std::string effectName;
        bool hasEffect = entry.find("effect") != nullptr;
        bool hasDefault = false;
        bool defaultValue = false;
        if (!readString(entry, "name", target.name, error) ||
            !readString(entry, "description", target.description, error) ||
            !readString(entry, "category", target.category, error) ||
            !readString(entry, "effect", effectName, error) ||
            !readBool(entry, "default", defaultValue, hasDefault, error)) {
            return false;
        }
        if (hasEffect && !effectFromName(effectName, target.effect, target.oneShot)) {
            error = "cheat '" + id + "' has unknown effect '" + effectName + "'";
            return false;
        }
        if (existing == nullptr) {
            if (target.name.empty() || !hasEffect) {
                error = "new cheat '" + id + "' needs 'name' and 'effect'";
                return false;
            }
            next.push_back(target);
        }
        if (hasDefault) {
            // Only the default changes the enabled state; a cheat that is already on keeps running otherwise.
            for (Cheat& c : next) {
                if (c.id == id) {
                    c.enabled = defaultValue;
                }
            }
        }
        return true;
    });
    if (ok) {
        cheats_ = std::move(next);
    }
    return ok;
}

const Cheat* Registry::find(const std::string& id) const {
    for (const Cheat& c : cheats_) {
        if (c.id == id) {
            return &c;
        }
    }
    return nullptr;
}

bool Registry::set(const std::string& id, bool enabled) {
    for (Cheat& c : cheats_) {
        if (c.id == id) {
            c.enabled = enabled;
            return true;
        }
    }
    return false;
}

bool Registry::healthLossBlocked() const {
    for (const Cheat& c : cheats_) {
        if (c.enabled && c.effect == Effect::KeepHealthFull) {
            return true;
        }
    }
    return false;
}

void Registry::tick(World& world) {
    for (Cheat& c : cheats_) {
        if (!c.enabled) {
            continue;
        }
        switch (c.effect) {
            case Effect::KeepHealthFull:
                if (world.marioPresent()) {
                    world.refillHealth();
                }
                break;
            case Effect::KeepWaterFull:
                if (world.marioPresent()) {
                    world.refillWater();
                }
                break;
            case Effect::KeepLivesMax:
                if (world.getFlag(kLivesFlag) != kMaxLives) {
                    world.setFlag(kLivesFlag, kMaxLives);
                }
                break;
            case Effect::UnlockAllNozzles:
                for (uint8_t stage = kFirstStage; stage <= kLastStage; stage++) {
                    for (uint8_t nozzle = 0; nozzle < kNozzleCount; nozzle++) {
                        world.setNozzleRight(stage, nozzle);
                    }
                }
                break;
        }
        if (c.oneShot) {
            c.enabled = false;
        }
    }
}

}  // namespace cheats
