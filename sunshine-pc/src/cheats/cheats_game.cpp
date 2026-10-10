// Binds the cheat effects to the running game (sms): Mario health, FLUDD water, the flag manager.
// Loads assets/cheats.json and cheats.user.json from the exe directory.
#include "cheats/cheats.h"

#include "cheats/cheats_core.h"
#include "util/read_file.h"
#include "Player/Mario.hpp"
#include "Player/WaterGun.hpp"
#include "System/FlagManager.hpp"

#include <cstdio>
#include <filesystem>

namespace {

class GameWorld : public cheats::World {
public:
    bool marioPresent() override { return gpMarioOriginal != nullptr; }

    void refillHealth() override { gpMarioOriginal->mHealth = gpMarioOriginal->mDeParams.mHpMax.get(); }

    void refillWater() override {
        if (gpMarioOriginal->mWaterGun != nullptr) {
            gpMarioOriginal->mWaterGun->resetWaterToFull();
        }
    }

    int32_t getFlag(uint32_t flag) override {
        TFlagManager* flags = TFlagManager::getInstance();
        return flags != nullptr ? flags->getFlag(flag) : 0;
    }

    void setFlag(uint32_t flag, int32_t value) override {
        TFlagManager* flags = TFlagManager::getInstance();
        if (flags != nullptr) {
            flags->setFlag(flag, value);
        }
    }

    void setNozzleRight(uint8_t stage, uint8_t nozzle) override {
        TFlagManager* flags = TFlagManager::getInstance();
        if (flags != nullptr) {
            flags->setNozzleRight(stage, nozzle);
        }
    }
};

cheats::Registry& registry() {
    static cheats::Registry instance;
    return instance;
}


}  // namespace

bool cheatsLoad(const std::string& exeDir) {
    namespace fs = std::filesystem;
    const fs::path base(exeDir);
    registry() = cheats::Registry{};

    std::string text;
    std::string error;
    const fs::path shipped = base / "assets" / "cheats.json";
    if (!readWholeFile(shipped, text)) {
        std::printf("[cheats] missing %s; no cheats available\n", shipped.string().c_str());
        return false;
    }
    if (!registry().loadBase(text, error)) {
        std::printf("[cheats] %s: %s; no cheats available\n", shipped.string().c_str(), error.c_str());
        return false;
    }

    const fs::path user = base / "cheats.user.json";
    if (fs::exists(user)) {
        if (!readWholeFile(user, text)) {
            std::printf("[cheats] cannot read %s; ignored\n", user.string().c_str());
        } else if (!registry().applyUser(text, error)) {
            std::printf("[cheats] %s: %s; ignored\n", user.string().c_str(), error.c_str());
        } else {
            std::printf("[cheats] applied %s\n", user.string().c_str());
        }
    }

    std::printf("[cheats] %d cheats loaded\n", (int)registry().list().size());
    return true;
}

int cheatCount() {
    return (int)registry().list().size();
}

bool cheatAt(int index, CheatInfo& out) {
    const std::vector<cheats::Cheat>& list = registry().list();
    if (index < 0 || index >= (int)list.size()) {
        return false;
    }
    const cheats::Cheat& c = list[index];
    out = CheatInfo{c.id.c_str(), c.name.c_str(), c.description.c_str(), c.category.c_str(), c.enabled, c.oneShot};
    return true;
}

bool cheatEnabled(const char* id) {
    if (id == nullptr) {
        return false;
    }
    const cheats::Cheat* c = registry().find(id);
    return c != nullptr && c->enabled;
}

bool cheatSet(const char* id, bool enabled) {
    if (id == nullptr || !registry().set(id, enabled)) {
        return false;
    }
    std::printf("[cheats] %s = %s\n", id, enabled ? "on" : "off");
    return true;
}

bool cheatsHealthLossBlocked() {
    return registry().healthLossBlocked();
}

void cheatsTick() {
    static GameWorld world;
    registry().tick(world);
}

bool cheatExists(const char* id) {
    return id != nullptr && registry().find(id) != nullptr;
}
