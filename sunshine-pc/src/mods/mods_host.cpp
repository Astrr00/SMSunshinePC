// Game side of the mod host API: maps SunshineHostApi calls onto sms state (flags, Mario, FLUDD, cheats).
// Thin wrappers only; the rules live in the game code and cheats/. No OS calls here.
#include "mods/mods_host.h"

#include "cheats/cheats.h"
#include "Player/Mario.hpp"
#include "Player/WaterGun.hpp"
#include "System/Application.hpp"
#include "System/FlagManager.hpp"

namespace {

void hostLog(SunshineModCtx* ctx, int32_t level, const char* message) {
    modsLogLine(level, "[mod " + ctx->id + "] " + (message ? message : ""));
}

int32_t hostFlagGet(SunshineModCtx*, uint32_t flag) {
    TFlagManager* flags = TFlagManager::getInstance();
    return flags != nullptr ? flags->getFlag(flag) : 0;
}

void hostFlagSet(SunshineModCtx*, uint32_t flag, int32_t value) {
    TFlagManager* flags = TFlagManager::getInstance();
    if (flags != nullptr) {
        flags->setFlag(flag, value);
    }
}

int32_t hostCheatGet(SunshineModCtx*, const char* id) {
    if (id == nullptr || !cheatExists(id)) {
        return -1;
    }
    return cheatEnabled(id) ? 1 : 0;
}

int32_t hostCheatSet(SunshineModCtx*, const char* id, int32_t enabled) {
    return cheatSet(id, enabled != 0) ? 0 : -1;
}

SunshineActorHandle hostMario(SunshineModCtx*) {
    return gpMarioOriginal;
}

int32_t hostMarioHealth(SunshineModCtx*, SunshineActorHandle mario) {
    return mario != nullptr ? static_cast<TMario*>(mario)->mHealth : 0;
}

void hostMarioSetHealth(SunshineModCtx*, SunshineActorHandle mario, int32_t health) {
    if (mario != nullptr) {
        static_cast<TMario*>(mario)->mHealth = (s16)health;
    }
}

int32_t hostMarioWater(SunshineModCtx*, SunshineActorHandle mario) {
    TMario* m = static_cast<TMario*>(mario);
    return (m != nullptr && m->mWaterGun != nullptr) ? m->mWaterGun->getCurrentWater() : 0;
}

void hostMarioSetWater(SunshineModCtx*, SunshineActorHandle mario, int32_t water) {
    TMario* m = static_cast<TMario*>(mario);
    if (m != nullptr && m->mWaterGun != nullptr) {
        m->mWaterGun->mCurrentWater = water;
    }
}

}  // namespace

const SunshineHostApi& modsHostApi() {
    static const SunshineHostApi api = {
        sizeof(SunshineHostApi),
        SUNSHINE_MOD_API_VERSION,
        hostLog,
        hostFlagGet,
        hostFlagSet,
        hostCheatGet,
        hostCheatSet,
        hostMario,
        hostMarioHealth,
        hostMarioSetHealth,
        hostMarioWater,
        hostMarioSetWater,
    };
    return api;
}

uint32_t modsCurrentStage() {
    return gpApplication.mCurrArea.getStage();
}
