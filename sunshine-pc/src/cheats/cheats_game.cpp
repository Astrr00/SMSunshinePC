// Binds the cheat effects to the running game (sms): Mario health, FLUDD water, the flag manager.
// Loads assets/cheats.json and cheats.user.json from the exe directory.
#include "cheats/cheats.h"

#include "cheats/cheats_core.h"
#include "util/read_file.h"
#include "Player/Mario.hpp"
#include "Player/WaterGun.hpp"
#include "System/FlagManager.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

bool SMS_isOptionMap(); // sms/src/System/StageUtil.cpp

namespace {

class GameWorld : public cheats::World {
public:
    bool marioPresent() override { return gpMarioOriginal != nullptr; }

    void refillHealth() override {
        const int16_t before = gpMarioOriginal->mHealth;
        gpMarioOriginal->mHealth = gpMarioOriginal->mDeParams.mHpMax.get();
        if (before != gpMarioOriginal->mHealth) {
            std::printf("[cheats] keep_health_full: health %d -> %d\n", (int)before, (int)gpMarioOriginal->mHealth);
        }
    }

    void refillWater() override {
        auto* water = gpMarioOriginal->mWaterGun;
        if (water == nullptr) {
            return;
        }
        const int32_t before = water->getCurrentWater();
        water->resetWaterToFull();
        const int32_t after = water->getCurrentWater();
        static int logged = 0;  // a spray changes the water every frame; cap the log
        if (before != after && logged < 40) {
            logged++;
            std::printf("[cheats] keep_water_full: water %d -> %d\n", (int)before, (int)after);
        }
    }

    int32_t getFlag(uint32_t flag) override {
        TFlagManager* flags = TFlagManager::getInstance();
        return flags != nullptr ? flags->getFlag(flag) : 0;
    }

    void setFlag(uint32_t flag, int32_t value) override {
        TFlagManager* flags = TFlagManager::getInstance();
        if (flags != nullptr) {
            std::printf("[cheats] flag 0x%X: %d -> %d\n", (unsigned)flag, (int)flags->getFlag(flag), (int)value);
            flags->setFlag(flag, value);
            // Nozzle unlocks are one-shot and not saved: without the saved copy a game over before the next save
            // (TFlagManager::restore, MarDirectorDirect.cpp:409) takes them back (observed, run mc_go2).
            if (flag >= cheats::kNozzleFlagBase && flag < cheats::kNozzleFlagBase + 18) {
                flags->setSavedCardBool(flag, value);
            }
        }
    }
};

cheats::Registry& registry() {
    static cheats::Registry instance;
    return instance;
}

int activeCount() {
    int n = 0;
    for (const cheats::Cheat& c : registry().list()) {
        n += c.enabled ? 1 : 0;
    }
    return n;
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

    std::printf("[cheats] %d cheats loaded, %d active\n", (int)registry().list().size(), activeCount());
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

bool cheatsHealthLossBlocked(int damage) {
    if (!registry().healthLossBlocked()) {
        return false;
    }
    std::printf("[cheats] keep_health_full: blocked %d damage (health %d)\n", damage,
                gpMarioOriginal != nullptr ? (int)gpMarioOriginal->mHealth : -1);
    return true;
}

void cheatsTick() {
    static GameWorld world;
    static uint32_t ticks = 0;
    static const bool statusLog = [] {
        const char* v = std::getenv("SUNSHINE_DEV_STATUS");
        return v != nullptr && v[0] != '\0' && v[0] != '0';
    }();
    // The title map (option map 15) runs the file select. A new file resets the nozzle flags there (observed); a
    // loaded save is read there too (TCardLoad runs only on map 15: CardLoad.cpp:95, MarDirectorSetup2.cpp:107-113).
    // So the cheats wait for the first stage after it (observed with a new file and a loaded save); without this a
    // one-shot picked in the launcher ran on the title and was lost.
    if (!SMS_isOptionMap()) {
        registry().tick(world);
    }
    ticks++;
    if (!statusLog || ticks % 30 != 0 || gpMarioOriginal == nullptr) {
        return;
    }
    int nozzleMask = 0;
    for (uint32_t i = 0; i < 18; i++) {
        if (world.getFlag(cheats::kNozzleFlagBase + i) != 0) {
            nozzleMask |= 1 << i;
        }
    }
    const int water = gpMarioOriginal->mWaterGun != nullptr ? gpMarioOriginal->mWaterGun->getCurrentWater() : -1;
    std::printf("[cheats-state] t=%us health=%d water=%d lives=%d coins=%d nozzles=0x%05X active=%d\n", ticks / 30,
                (int)gpMarioOriginal->mHealth, water, (int)world.getFlag(cheats::kLivesFlag),
                (int)world.getFlag(cheats::kCoinFlag), nozzleMask, activeCount());
}

bool cheatExists(const char* id) {
    return id != nullptr && registry().find(id) != nullptr;
}
