#pragma once

// Dev-only prerequisite telemetry. No controller inputs or actor mutations.
#include <cmath>
#include <cstdio>
#include <typeinfo>
#include <Enemy/BombHei.hpp>
#include <Enemy/Cannon.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Enemy/TamaNoko.hpp>
#include <Player/Mario.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/Spine.hpp>
#include <System/Application.hpp>
#include <System/MarDirector.hpp>

static inline bool pinnaPrerequisitesSceneReady(int scenario)
{
	// Match devscript's setup-thread join guard before dereferencing scene actors.
	return gpApplication.mAppState == TApplication::APP_STATE_GAMEPLAY
	    && gpMarDirector && gpApplication.mDirector == gpMarDirector
	    && gpMarDirector->unk260 == 1 && gpMarioOriginal && gpConductor
	    && gpApplication.mCurrArea.getStage() == 5
	    && gpApplication.mCurrArea.getScenario() == scenario;
}

static inline bool pinnaBombReady()
{
	if (!pinnaPrerequisitesSceneReady(1)) return false;
	// A nearby pickup candidate, not a replacement for Mario's normal canTake checks.
	// Release R before pressing B. The original 1000-step fuse leaves at least 400 steps.
	constexpr float pi = 3.14159265358979323846f;
	const auto& mario = *gpMarioOriginal;
	const float facing = static_cast<float>(mario.mFaceAngle.y) * (pi / 32768.f);
	for (auto it = gpConductor->unk20.begin(); it != gpConductor->unk20.end(); ++it) {
		const auto* manager = *it;
		if (!manager) continue;
		for (int i = 0; i < manager->getObjNum(); ++i) {
			const auto* bomb = dynamic_cast<const TBombHei*>(manager->getObj(i));
			if (!bomb || bomb->checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN)
			    || bomb->mHolder || bomb->unk1A4 != 0 || bomb->mCurrentBckAnm != 6
			    || bomb->unk164 == 0 || bomb->unk198 < 0 || bomb->unk198 > 600) continue;
			const float dx = bomb->mPosition.x - mario.mPosition.x;
			const float dy = bomb->mPosition.y - mario.mPosition.y;
			const float dz = bomb->mPosition.z - mario.mPosition.z;
			if (!(dx * dx + dy * dy + dz * dz <= 120.f * 120.f)) continue;
			const float angle = std::remainder(std::atan2(dx, dz) - facing, 2.f * pi);
			if (std::fabs(angle) < pi / 3.f) return true;
		}
	}
	return false;
}

static inline const TBombHei* nearestPinnaBomb(float radius)
{
	if (!pinnaPrerequisitesSceneReady(1)) return nullptr;
	const TBombHei* nearest = nullptr;
	const auto& mario = gpMarioOriginal->mPosition;
	for (auto it = gpConductor->unk20.begin(); it != gpConductor->unk20.end(); ++it) {
		const auto* manager = *it;
		if (!manager) continue;
		for (int i = 0; i < manager->getObjNum(); ++i) {
			const auto* bomb = dynamic_cast<const TBombHei*>(manager->getObj(i));
			if (!bomb || bomb->checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN)
			    || bomb->mHolder || bomb->unk1A4 != 0 || bomb->unk198 < 0 || bomb->unk198 > 600) continue;
			const int bck = bomb->mCurrentBckAnm;
			if (bck != 2 && bck != 3 && bck != 4 && !(bck == 6 && bomb->unk164 != 0)) continue;
			if (!(std::fabs(bomb->mPosition.y - mario.y) <= 300.f)) continue;
			const float distance = std::hypot(bomb->mPosition.x - mario.x, bomb->mPosition.z - mario.z);
			if (distance < radius) {
				radius = distance;
				nearest = bomb;
			}
		}
	}
	return nearest;
}

static inline void logPinnaPrerequisites(const char* tag, int tick)
{
	const bool cannonScene = pinnaPrerequisitesSceneReady(1);
	if (!cannonScene && !pinnaPrerequisitesSceneReady(3)) return;
	if (!tag) tag = "pinna";
	int cannons = 0, bombs = 0, tamaAlive = 0, tamaTotal = 0;
	// TEnemyManager registers itself in TConductor::unk20.
	for (auto it = gpConductor->unk20.begin(); it != gpConductor->unk20.end(); ++it) {
		const auto* manager = *it;
		if (!manager) continue;
		for (int i = 0; i < manager->getObjNum(); ++i) {
			const auto* object = manager->getObj(i);
			if (cannonScene) {
				if (const auto* cannon = dynamic_cast<const TCannon*>(object)) {
					++cannons;
					const bool dead = cannon->checkLiveFlag(LIVE_FLAG_DEAD);
					const bool hidden = cannon->checkLiveFlag(LIVE_FLAG_HIDDEN);
					const auto* nerve = cannon->mSpine ? cannon->mSpine->getCurrentNerve() : nullptr;
					std::printf("[%s-pinna-cannon] i=%d hp=%u dead=%d hidden=%d pos=(%.1f,%.1f,%.1f) nerve=%s time=%d bombMode=%u dom=%d bck=%d tick=%d\n",
					    tag, i, static_cast<unsigned>(cannon->getHitPoints()), static_cast<int>(dead), static_cast<int>(hidden),
					    cannon->mPosition.x, cannon->mPosition.y, cannon->mPosition.z,
					    nerve ? typeid(*nerve).name() : "none", cannon->mSpine ? cannon->mSpine->getTime() : -1,
					    static_cast<unsigned>(cannon->unk290), cannon->mDomIdx, cannon->mCurrentBckAnm, tick);
				}
				if (const auto* bomb = dynamic_cast<const TBombHei*>(object)) {
					if (bomb->checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN)) continue;
					++bombs;
					const auto* nerve = bomb->mSpine ? bomb->mSpine->getCurrentNerve() : nullptr;
					// Inspect the existing nerve; never call theNerve() or isDamageToCannon().
					const bool frozen = dynamic_cast<const TNerveSmallEnemyFreeze*>(nerve) != nullptr;
					// BombHeiWaitExplosion sets animation6 and unk164 on a successful water stun.
					std::printf("[%s-pinna-bomb] i=%d pos=(%.1f,%.1f,%.1f) nerve=%s time=%d bck=%d frozen=%d waterStunned=%d stunFlag=%u fuse=%d waterCd=%d held=%d heldByMario=%d marioHolding=%d pickedByMario=%u tick=%d\n",
					    tag, i, bomb->mPosition.x, bomb->mPosition.y, bomb->mPosition.z,
					    nerve ? typeid(*nerve).name() : "none", bomb->mSpine ? bomb->mSpine->getTime() : -1,
					    bomb->mCurrentBckAnm, static_cast<int>(frozen), bomb->mCurrentBckAnm == 6 && bomb->unk164 != 0,
					    static_cast<unsigned>(bomb->unk164), bomb->unk198,
					    bomb->mSprayedByWaterCooldown, bomb->mHolder != nullptr, bomb->mHolder == gpMarioOriginal,
					    gpMarioOriginal->mHeldObject == bomb, static_cast<unsigned>(bomb->unk1A4), tick);
				}
			} else if (const auto* tama = dynamic_cast<const TTamaNoko*>(object)) {
				++tamaTotal;
				const bool dead = tama->checkLiveFlag(LIVE_FLAG_DEAD);
				const bool hidden = tama->checkLiveFlag(LIVE_FLAG_HIDDEN);
				if (!dead && !hidden) ++tamaAlive;
				const auto* nerve = tama->mSpine ? tama->mSpine->getCurrentNerve() : nullptr;
				std::printf("[%s-pinna-tama] i=%d dead=%d hidden=%d pos=(%.1f,%.1f,%.1f) nerve=%s time=%d bck=%d sunk=%d jumpAttack=%d airTimer=%d wakeTimer=%d waterCd=%d waterHit=%d stunFlag=%u tick=%d\n",
				    tag, i, static_cast<int>(dead), static_cast<int>(hidden), tama->mPosition.x, tama->mPosition.y, tama->mPosition.z,
				    nerve ? typeid(*nerve).name() : "none", tama->mSpine ? tama->mSpine->getTime() : -1,
				    tama->mCurrentBckAnm, static_cast<int>(tama->mIsSunkInGround), static_cast<int>(tama->mIsDoingJumpAttack),
				    // The air timer is first initialized when a jump attack starts.
				    tama->mIsDoingJumpAttack ? tama->mAirAttackTimer : -1,
				    tama->mWakeUpTimer, tama->mSprayedByWaterCooldown, static_cast<int>(tama->unk165),
				    static_cast<unsigned>(tama->unk164), tick);
			}
		}
	}
	std::printf("[%s-pinna-prerequisites] scenario=%d cannons=%d liveBombs=%d tamaAlive=%d tamaTotal=%d tick=%d\n",
	    tag, cannonScene ? 1 : 3, cannons, bombs, tamaAlive, tamaTotal, tick);
}
