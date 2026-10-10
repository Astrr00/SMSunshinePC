// Cheat registry and effects, free of game and OS dependencies so tests/port_tests.cpp can run it headless.
// The data format is JSON (assets/cheats.json, plus cheats.user.json next to the exe); effects are a fixed
// C++ table, selected from data by name.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cheats {

// Everything a cheat effect may touch. cheats_game.cpp binds it to the running game.
class World {
public:
    virtual ~World() = default;
    virtual bool marioPresent() = 0;
    virtual void refillHealth() = 0;
    virtual void refillWater() = 0;
    virtual int32_t getFlag(uint32_t flag) = 0;
    virtual void setFlag(uint32_t flag, int32_t value) = 0;
};

enum class Effect {
    KeepHealthFull,    // hold: refill Mario's health each logic frame; blocks health loss (see healthLossBlocked)
    KeepWaterFull,     // hold: refill FLUDD water each logic frame
    KeepLivesMax,      // hold: keep the lives counter at kMaxLives
    UnlockAllNozzles,  // one-shot: set the nozzle-unlocked flag for every stage, then the cheat switches itself off
};

constexpr uint32_t kLivesFlag = 0x20001;  // card int bank: lives (see sms incMario cap of 99)
constexpr int32_t kMaxLives = 99;
// Nozzle-unlocked flags: one per (shine stage 1..9, nozzle 0..1), index = base + (stage-1)*2 + nozzle.
// Same indices as sms TFlagManager::getNozzleRight/setNozzleRight (sms/src/System/FlagManager.cpp).
constexpr uint32_t kNozzleFlagBase = 0x10366;

struct Cheat {
    std::string id;
    std::string name;
    std::string description;
    std::string category;
    Effect effect = Effect::KeepHealthFull;
    bool oneShot = false;
    bool enabled = false;
};

class Registry {
public:
    // Replaces the content with the built-in list (assets/cheats.json). Returns false and sets error on bad data.
    bool loadBase(const std::string& json, std::string& error);

    // Applies the user file: an entry with a known id overrides the fields it sets (name, description,
    // category, effect, default); an unknown id adds a new cheat, which must give name and effect.
    bool applyUser(const std::string& json, std::string& error);

    const std::vector<Cheat>& list() const { return cheats_; }
    const Cheat* find(const std::string& id) const;
    bool set(const std::string& id, bool enabled);

    // True while a cheat with the KeepHealthFull effect is enabled (TMario::decHP consults this).
    bool healthLossBlocked() const;

    // Runs the enabled effects once. Called once per logic frame.
    void tick(World& world);

private:
    std::vector<Cheat> cheats_;
};

}  // namespace cheats
