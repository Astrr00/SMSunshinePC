#pragma once

// Dev-only read-only telemetry for the original Pinna Episode2 secret.
#include <cstdio>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <MoveBG/MapObjRailBlock.hpp>
#include <System/Application.hpp>
#include <System/MarDirector.hpp>

static inline bool pinnaSecretSceneReady()
{
	// Same setup-thread join guard as devscript::sceneReady, before any scene lookup.
	return gpApplication.mAppState == TApplication::APP_STATE_GAMEPLAY
	    && gpMarDirector && gpApplication.mDirector == gpMarDirector
	    && gpMarDirector->unk260 == 1
	    && gpApplication.mCurrArea.getStage() == 50
	    && gpApplication.mCurrArea.getScenario() == 0;
}

static inline bool pinnaSecretWoodWait(const char* name, int remaining, bool atLeast)
{
	if (!pinnaSecretSceneReady()) return false;
	// Observe the current hidden wait in MOVE updates; never retain scene pointers.
	auto* registry = JDrama::TNameRefGen::instance;
	auto* root = registry ? registry->getRootNameRef() : nullptr;
	const auto* block = root ? dynamic_cast<const TWoodBlock*>(root->search(name)) : nullptr;
	return block && block->unk158 != 0 && block->unk152 != 0
	    && (atLeast ? block->unk152 >= remaining : block->unk152 <= remaining);
}

static inline void logSecretWoodBlocks(const char* tag, int tick)
{
	if (!pinnaSecretSceneReady()) return;
	// Read the already-existing registry pointer; do not construct any singleton.
	auto* registry = JDrama::TNameRefGen::instance;
	auto* root = registry ? registry->getRootNameRef() : nullptr;
	if (!root) return;
	if (!tag) tag = "secret";
	const char* names[] = { "WoodBlockCube 0", "WoodBlockCube 1", "WoodBlockCube 8", "WoodBlockCube 12" };
	for (const char* name : names) {
		// Fresh lookup on every log call: never retain scene-owned actor pointers.
		const auto* block = dynamic_cast<const TWoodBlock*>(root->search(name));
		if (!block) {
			std::printf("[%s-secret-woodblock] name=%s missing=1 tick=%d\n", tag, name, tick);
			continue;
		}
		// Initialization proof: TPlacement constructor/load initializes position;
		// TRailMapObj constructor initializes unk148=0 and unk14A=180;
		// TNormalLift constructor initializes unk150=0, unk158=0 and unk152=0.
		// All are ordinary typed fields; no nerve query or actor method with side effects.
		std::printf("[%s-secret-woodblock] name=%s pos=(%.1f,%.1f,%.1f) unk150=%u unk148=%d unk14A=%d unk158=%d unk152=%u tick=%d\n",
		    tag, name, block->mPosition.x, block->mPosition.y, block->mPosition.z,
		    static_cast<unsigned>(block->unk150), static_cast<int>(block->unk148),
		    static_cast<int>(block->unk14A), static_cast<int>(block->unk158),
		    static_cast<unsigned>(block->unk152), tick);
	}
}
