#pragma once

// Dev-only read-only observations for the original Pinna Episode3 ships.
#include <cstdio>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <MoveBG/MapObjPinna.hpp>
#include <System/Application.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>

static inline bool pinnaShipSceneReady()
{
	return gpApplication.mAppState == TApplication::APP_STATE_GAMEPLAY
	    && gpMarDirector && gpApplication.mDirector == gpMarDirector
	    && gpMarDirector->unk260 == 1
	    && gpApplication.mCurrArea.getStage() == 13
	    && gpApplication.mCurrArea.getScenario() == 1;
}

static inline const TViking* findPinnaShip(int index)
{
	if (!pinnaShipSceneReady()) return nullptr;
	// Existing registry only; never construct a singleton or retain a scene pointer.
	auto* registry = JDrama::TNameRefGen::instance;
	auto* root = registry ? registry->getRootNameRef() : nullptr;
	return root ? dynamic_cast<const TViking*>(root->search(index == 0 ? "viking 0" : "viking 1")) : nullptr;
}

static inline void logPinnaShips(const char* tag, int tick)
{
	if (!pinnaShipSceneReady()) return;
	if (!tag) tag = "pinna";
	const auto* flags = TFlagManager::smInstance;
	const int redcoins = flags ? flags->getFlag(0x60000) : -1;
	for (int i = 0; i < 2; ++i) {
		const auto* ship = findPinnaShip(i);
		if (!ship) {
			std::printf("[%s-pinna3-ship] i=%d found=0 redcoins=%d tick=%d\n", tag, i, redcoins, tick);
			continue;
		}
		// THorizontalViking initializes both fields; rate is degrees per MOVE.
		std::printf("[%s-pinna3-ship] i=%d found=1 angle=%.3f rate=%.6f pos=(%.1f,%.1f,%.1f) redcoins=%d tick=%d\n",
		    tag, i, ship->unk148, ship->unk144,
		    ship->mPosition.x, ship->mPosition.y, ship->mPosition.z, redcoins, tick);
	}
}
