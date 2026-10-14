#include <System/MarDirector.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JDrama/JDRCamera.hpp>
#include <System/Application.hpp>
#include <System/MSoundMainSide.hpp>
#include <System/MarioGamePad.hpp>
#include <System/PerformList.hpp>
#include <System/StageUtil.hpp>
#include <System/FlagManager.hpp>
#include <System/CardManager.hpp>
#include <Player/Mario.hpp>
#include <Player/WaterGun.hpp>
#include <Strategic/ObjHitCheck.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <THPPlayer/THPPlayer.h>
#include <GC2D/GCConsole2.hpp>
#include <GC2D/ConsoleStr.hpp>
#include <GC2D/ScrnFader.hpp>
#ifdef TARGET_PC
#include <dolphin/gx.h>
#include <JSystem/JDrama/JDRVideo.hpp>
#include <JSystem/JDrama/JDRFrmGXSet.hpp>
#endif
#include <GC2D/PauseMenu2.hpp>
#include <GC2D/CardSave.hpp>
#include <GC2D/Guide.hpp>
#include <GC2D/SunGlass.hpp>
#include <GC2D/Talk2D2.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/MarioPositionObj.hpp>
#include <MoveBG/Item.hpp>
#include <MoveBG/MapObjDolpic.hpp>
#include <NPC/NpcBase.hpp>
#include <dolphin/gx.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

extern OSThread gSetupThread;

#ifdef TARGET_PC
void sunshineLogicTick(); // sunshine-pc/src/main.cpp: cheats and mods, once per logic step
#endif

int TMarDirector::direct()
{
	int vsyncRate = 600 / (int)SMSGetVSyncTimesPerSec();

	if (unk260 == 0) {
		if (!OSIsThreadTerminated(&gSetupThread))
			return 0;

		u32 local_40;
		OSJoinThread(&gSetupThread, &local_40);
		if (local_40)
			return 4;

		setupObjects();
		unk260 = 1;
	}

	u32 desiredAppState = TApplication::APP_STATE_DEFAULT;

	JDrama::TGraphics local_140;
	char trash[0x48];
#ifdef TARGET_PC
	JDrama::SunshineApplyDisplay(unkC0, &local_140); // retail leans on stale stack data here, see JDRFrmGXSet.cpp
#endif

	u8 bVar2 = gpMSound->unkA8;
	unk54 += vsyncRate;

	int i = 0;
	for (;;) {
		if (!(unk4C & 0x4000)) {
			++i;
			if (i == 1)
				unk4C |= 0x2000;
			unk54 -= 5;
			if (unk54 < 5)
				unk4C |= 0x4000;

			// inline?
			u32 uVar8 = 0;
			u8 bVar7  = bVar2;
			if (unk4C & 0x4000) {
				if (unk258)
					unk258->stageLoop();
			} else {
				uVar8 |= 2;
				bVar7 &= ~0x1;
			}
			gpMSound->unkA8 = bVar7;

			switch (mState) {
			case STATE_PAUSE_MENU:
			case STATE_CARD_SAVE:
			case STATE_UNK12:
				uVar8 |= 2;
				uVar8 |= 1;
				break;

			case STATE_GUIDE:
				uVar8 |= 2;
				uVar8 |= 1;
				break;
			}

			if (!(uVar8 & 1))
				++mMoveTickCount;
			++mTickCount;
			if (unk4C & 0x2000) {
				if (mState == STATE_UNK4 || mState == STATE_UNK7) {
					SMSRumbleMgr->update();
				}
			} else {
				for (int i = 0; i < 4; ++i) {
					// TODO: inline?
					TMarioGamePad* pad    = unk18[i];
					pad->mButton.mTrigger = 0;
					pad->mButton.mRelease = 0;

					unk18[i]->updateMeaning();
					unk18[i]->offFlag(TMarioGamePad::PAD_FLAG_0x40);
				}
			}

			u32 tmp = 0;
			if (unk4C & 0x2000)
				tmp |= 1;
			if (unk4C & 0x4000)
				tmp |= 2;
			// asm 0x800ED050: sth r3,0x58(r1) is offset 0 of the TGraphics passed to perform (graphics->unk0 readers)
			local_140.unk0 = tmp;

			// inline
			bool bVar1 = true;
			if ((mMoveTickCount & 1) || (mMoveTickCount & 2))
				bVar1 = false;

			if (bVar1)
				gpObjHitCheck->checkActorsHit();
			else
				gpObjHitCheck->clearHitNum();

			u32 uVar11 = ~uVar8;
			u32 uVar4  = uVar11;
			// asm 0x800ED0A0: rlwinm r4,r4,0,20,18 / rlwinm r4,r4,0,19,17 clear CUE_MOVEMENT_GATE_A (0x1000) and
			// CUE_MOVEMENT_GATE_B (0x2000); the literals 0x100/0x200 cleared unrelated cues and left Mario's
			// gated CUE_MOVE stripped by TViewObj::testPerform.
			if (mMoveTickCount & 1)
				uVar4 &= ~CUE_MOVEMENT_GATE_A;
			if (mMoveTickCount & 2)
				uVar4 &= ~CUE_MOVEMENT_GATE_B;
			// asm 0x800ED0B0: unk4E & 1 selects 0x44 (mShinePfLstMov), otherwise 0x28 (mPerformListMovement, the list
			// that holds the player group); the decomp performed mShinePfLstMov in both branches, so Mario never moved.
			if (unk4E & 1)
				mShinePfLstMov->perform(uVar4, &local_140);
			else
				mPerformListMovement->perform(uVar4, &local_140);

			u32 uVar44 = 0;
			if (!(unk4C & 0x4000))
				uVar44 |= 2;
			unk30->perform(~uVar44, &local_140);
			movement();
#ifdef TARGET_PC
			sunshineLogicTick();
#endif
			if (!(uVar8 & 2)) {
				// asm 0x800ED130: unk4E & 1 selects 0x48 (mShinePfLstAnm), otherwise 0x2C (mPerformListCalcAnim).
				if (unk4E & 1)
					mShinePfLstAnm->perform(uVar11, &local_140);
				else
					mPerformListCalcAnim->perform(uVar11, &local_140);
			}

			if (unk4C & 0x4000) {
				// asm 0x800ED188: sth r0,0x58(r1), offset 0 = unk0
				local_140.unk0 = 0;
				unk34->perform(CUE_ALL, &local_140);
				break;
			}
		} else {
			// asm 0x800ED1B0: sth r24,0x58(r1), offset 0 = unk0
			local_140.unk0 = 0;
			unk40->perform(CUE_ALL, &local_140);
			unk38->perform(CUE_ALL, &local_140);
			unk3C->perform(CUE_ALL, &local_140);
			mPerformListGX->perform(CUE_ALL, &local_140);
			if ((gpSilhouetteManager->unk48 > 0.0f ? true : false)
			    || gpCamera->unk2C8 != -1) {
				mPerformListSilhouette->perform(CUE_ALL, &local_140);
			}
			mPerformListGXPost->perform(CUE_ALL, &local_140);
			GXInvalidateTexAll();
		}
		desiredAppState = changeState();
		unk4C &= ~0x6000;
	}

	gpMSound->unkA8 = bVar2;
	return desiredAppState;
}

static void decideNextStage()
{
	// TODO: inline hell. TFlagT is my mortal enemy
	char trash[8];
	TGameSequence local_3C;

	int stage = SMS_getShineStage(gpApplication.mCurrArea.getStage());
	switch (stage) {
	case 0:
		local_3C.set(1, 0xff, JDrama::TFlagT<u16>());
		break;
	case 1:
	case 2:
	default:
		local_3C.set(1, 0xff, JDrama::TFlagT<u16>());
		break;
	}
	gpApplication.setNextArea(local_3C);
}

static void decideNextStageOfMiss()
{
	const TGameSequence& curArea = gpApplication.mCurrArea;
	if (SMS_isExMap() || curArea.getStage() == 0 || curArea.getStage() == 60) {
		gpApplication.mNextArea.set(curArea.getStage(), 0, 0);
	} else {
		decideNextStage();
	}
}

static void checkDefeatShadowMarioAll() { }

// fabricated
static inline bool decideSomething()
{
	static u8 stages[] = { 0x6, 0x10, 0x1A, 0x24, 0x2E, 0x38, 0x42 };

	int i = 0;
	do {
		if (!TFlagManager::smInstance->getShineFlag(stages[i]))
			return false;
		++i;
	} while (i < 7);

	return true;
}

static int decideNextScenario(u8 param_1)
{
	char trash[0x18];
	if ((int)param_1 != 1)
		return 0;

	if (TFlagManager::smInstance->getBool(0x103AE))
		return 2;

	if (decideSomething())
		return 9;

	if (TFlagManager::smInstance->getBool(0x10389))
		return 8;

	if (TFlagManager::smInstance->getBool(0x10386)
	    && TFlagManager::smInstance->getBool(0x10387)) {
		if (TFlagManager::smInstance->getFlag(0x40000) >= 10)
			return 7;
		else
			return 6;
	}

	if (TFlagManager::smInstance->getBool(0x10385))
		return 5;

	if (TFlagManager::smInstance->getBool(0x10384))
		return 1;

	return 0;
}

int TMarDirector::changeState()
{
	int desiredAppState = TApplication::APP_STATE_DEFAULT;
	u8 nextState        = mState;
	switch (mState) {
	case STATE_UNK0:
		switch (gpApplication.mCurrArea.unk0) {
		case 0xf:
			nextState = STATE_UNK4;
			unk50 |= 1;
			break;

		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x8:
		case 0x9:
		case 0x34:
			nextState = STATE_UNK1;
			unk50 |= 0x6;
			break;

		case 1:
			if (unk4E & 2) {
				nextState = STATE_UNK1;
				unk50 |= 6;
			} else {
				nextState = STATE_UNK2;
			}
			break;

		default:
			if (JKRGetResource("/scene/map/camera/startcamera.bck")) {
				nextState = STATE_UNK1;
				unk50 |= 10;
			} else {
				nextState = STATE_UNK4;
			}
			break;
		}
		break;

	case STATE_UNK1:
		if (unk4E & 4) {
			if (mConsole->unk94->unk2BC == 4) {
				nextState = STATE_UNK3;
				unk4E &= ~0x4;
			}
		} else {
			TGameSequence& curArea = gpApplication.mCurrArea;
			TConsoleStr* str       = mConsole->unk94;
			f32 iVar2              = gpCamera->getRestDemoFrames() / 120.0f;
			if (iVar2 <= str->getWipeCloseTime()
			    || ((curArea.getStage() != 1 || curArea.getScenario() != 1)
			        && (curArea.getStage() != 1 || curArea.getScenario() != 9)
			        && unk18[0]->checkFrameMeaning(
			            TMarioGamePad::MEANING_START
			            | TMarioGamePad::MEANING_MENU_A
			            | TMarioGamePad::MEANING_MENU_B))) {
				unk4E |= 4;
				mConsole->unk94->startCloseWipe((unk50 & 8) != 0);
				unk50 &= ~0x8;
			}
		}
		break;

	case STATE_UNK3:
		if (gpApplication.mCurrArea.unk0 == 1) {
			if (mConsole->unk94->unk2BC == 6)
				nextState = STATE_UNK2;
		} else if (mConsole->unk94->unk2BC == 6
		           && !gpMarioOriginal->checkStatusType(
		               MARIO_STATUS_FLAG_UNK1000)) {
			nextState = STATE_UNK4;
		}
		break;

	case STATE_UNK2:
		if (!unkE0->unk26
		    && !gpMarioOriginal->checkStatusType(MARIO_STATUS_FLAG_UNK1000))
			nextState = STATE_UNK4;
		break;

	case STATE_UNK4:
		nextState = updateGameMode();
		break;

	case STATE_PAUSE_MENU:
		switch (unkAC->getNextState()) {
		case 0:
			nextState = STATE_UNK4;
			break;
		case 1:
			unkE4     = 4;
			nextState = STATE_UNK12;
			unkB4     = TApplication::APP_STATE_DONE;
			break;
		case 5:
			decideNextStage();
			unk4C &= ~0x100;
			moveStage();
			unkE4     = 2;
			nextState = STATE_UNK9;
			break;
		}
		break;

	case STATE_GUIDE:
		if (unk78->unkC4 && gpApplication.mFader->isFullyFadedIn())
			nextState = STATE_UNK4;
		break;

	case STATE_CARD_SAVE: {
		switch (unkAC->mCardSave->getNextState()) {
		case 0:
			if (unk261 == 7) {
				TFlagManager::smInstance->restore();
				TFlagManager::smInstance->setBool(true, 0x30001);
				if (!TFlagManager::smInstance->getFlag(0x40000)) {
					gpApplication.mNextArea.set(0, 0, 0);
				} else {
					gpApplication.mNextArea.set(1, 0xff, 0);
				}
				unk4C &= ~0x100;
				moveStage();
				gpApplication.mFader->setFadeStatus(
				    TSMSFader::FADE_STATUS_FULLY_FADED_OUT);
				desiredAppState = TApplication::APP_STATE_GAMEPLAY;
			} else {
				nextState = STATE_UNK4;
			}
			break;
		case 1:
			if (unk261 == 7) {
				gpApplication.mFader->setFadeStatus(
				    TSMSFader::FADE_STATUS_FULLY_FADED_OUT);
				desiredAppState = TApplication::APP_STATE_DONE;
			} else {
				unkE4     = 4;
				nextState = STATE_UNK12;
				unkB4     = TApplication::APP_STATE_DONE;
			}
			break;
		}
		break;
	}

	case STATE_UNK7:
		if (gpApplication.mFader->isFullyFadedOut()
		    && (MSBgm::getHandle(2) == 0 || mTickCount - unk60 >= 1200)) {
			if (TFlagManager::smInstance->getFlag(0x20001) >= 0) {
				TFlagManager::smInstance->setBool(true, 0x30002);
				decideNextStageOfMiss();
				unk4C &= ~0x100;
				moveStage();
				unkE4 = 0xf;
				gpApplication.mFader->setColor(JUtility::TColor(0, 0, 0, 0xff));
				nextState = STATE_UNK12;
			} else {
				gpApplication.mFader->startWipe(0xE, 0.3f, 0.0f);
				gpApplication.mFader->setColor(JUtility::TColor(0, 0, 0, 0xff));
				unk261    = 7;
				nextState = STATE_CARD_SAVE;
				mConsole->startDisappearStar();
				mConsole->startDisappearCoin();
			}
		}
		break;

	case STATE_UNK9:
	case STATE_UNK12:
		if (gpApplication.mFader->isFullyFadedOut()
		    && gpMSound->checkWaveOnAram(MS_WAVE_DEFAULT))
			desiredAppState = unkB4;
		break;
	}

	if (unk18[0]->isSomethingPushed()
	    && gpCardManager->getLastStatus() != CARD_RESULT_BUSY
	    && (unk4C & 0x4000) && !(unk50 & 0x10)) {
		nextState = STATE_UNK12;
		unk50 |= 0x10;
		unkE4 = 4;
		unkB4 = TApplication::APP_STATE_DONE;
	}

	if (nextState != mState) {
		currentStateFinalize(nextState);
		nextStateInitialize(nextState);
		mState = nextState;
	}

	return desiredAppState;
}

void TMarDirector::currentStateFinalize(u8 next_state)
{
	char trash[0x78];
	switch (mState) {
	case STATE_UNK0:
		static_cast<JDrama::TViewObj*>(JDrama::TNameRefGen::search("Group 2D"))
		    ->unkC.off(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
		static_cast<JDrama::TViewObj*>(JDrama::TNameRefGen::search("Guide"))
		    ->unkC.on(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);

		gpApplication.mFader->startWipe(unkE4, 0.4f, 0.0f);
		SMSRumbleMgr->reset();
		if (gpApplication.mCurrArea.unk0 == 1)
			THPPlayerPlay();
		break;

	case STATE_UNK1:
		unk18[0]->offFlag(TMarioGamePad::PAD_FLAG_MENU_INPUT);
		gpCamera->endDemoCamera();
		mConsole->unk94->startOpenWipe();
		MSMainProc::endStageEntranceDemo(gpApplication.mCurrArea.unk0,
		                                 gpApplication.mCurrArea.unk1);
		break;

	case STATE_UNK4:
		if (unk124 == 0)
			OSStopStopwatch(&unkE8);
		unk18[0]->offFlag(TMarioGamePad::PAD_FLAG_GAME_INPUT);
		break;

	case STATE_PAUSE_MENU:
		unk18[0]->offFlag(TMarioGamePad::PAD_FLAG_MENU_INPUT);
		SMSRumbleMgr->finishPause();
		if (gpApplication.mCurrArea.unk0 == 1)
			THPPlayerPlay();
		break;

	case STATE_GUIDE:
		unk18[0]->offFlag(TMarioGamePad::PAD_FLAG_MENU_INPUT);
		SMSRumbleMgr->finishPause();

		static_cast<JDrama::TViewObj*>(JDrama::TNameRefGen::search("Group 2D"))
		    ->unkC.off(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
		static_cast<JDrama::TViewObj*>(JDrama::TNameRefGen::search("Guide"))
		    ->unkC.on(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);

		SMSSwitch2DArchive("guide", gArBkConsole);
		if (gpApplication.mCurrArea.unk0 == 1)
			THPPlayerPlay();
		break;

	case STATE_CARD_SAVE:
		unk18[0]->offFlag(TMarioGamePad::PAD_FLAG_MENU_INPUT);
		SMSRumbleMgr->finishPause();
		if (gpApplication.mCurrArea.unk0 == 1)
			THPPlayerPlay();
		switch (unk261) {
		case 3:
			mConsole->startAppearBalloon(0xE0048, true);
			break;

		case 4:
			mConsole->startAppearBalloon(0xE0049, true);
			break;
		}
		break;
	}
}

void TMarDirector::setMario()
{
	char trash[0x50];
	bool cVar4 = false;
	if (TFlagManager::getInstance()->getBool(0x30006)) {
		TFlagManager::getInstance()->setBool(false, 0x30006);
		cVar4 = true;
	}

	u8 uVar10 = unkD0;

	TMarioPositionObj* marioSetPosition = static_cast<TMarioPositionObj*>(
	    JDrama::TNameRefGen::search("マリオセット位置"));
	if (!marioSetPosition || marioSetPosition->unkD0 == 0)
		uVar10 = 0;

	switch (unkD1) {
	case 1: {
		f32 fVar1 = 0.0f;

		const JGeometry::TVec3<f32>* pos = nullptr;

		if (uVar10) {
			fVar1 = marioSetPosition->getUnk70(uVar10 - 1).y;
			pos   = &marioSetPosition->getUnk10(uVar10 - 1);
		}
		gpMarioOriginal->rollingStart(pos, fVar1);
	} break;

	case 2: {
		int iVar9 = 0;
		switch (SMS_getShineStage(gpApplication.mPrevArea.getStage())) {
		case 5:
		case 6:
		case 7:
			iVar9 = 1;
			break;
		case 8:
			iVar9 = 2;
			break;
		}
		f32 fVar1 = 0.0f;

		const JGeometry::TVec3<f32>* pos = nullptr;

		if (uVar10) {
			fVar1 = marioSetPosition->getUnk70(uVar10 - 1).y;
			pos   = &marioSetPosition->getUnk10(uVar10 - 1);
		}
		gpMarioOriginal->returnStart(pos, fVar1, cVar4, iVar9);
	} break;

	case 4:
		gpMarioOriginal->toroccoStart();
		break;

	case 0:
	default: { // asm 0x800EBFC4: unkD1 0, 3 and >4 all take the waiting path
		const JGeometry::TVec3<f32>* pos = nullptr;
		if (uVar10)
			pos = &marioSetPosition->getUnk10(uVar10 - 1);
		gpMarioOriginal->waitingStart(pos, 0.0f);
	} break;
	}

	const TGameSequence& curArea = gpApplication.mCurrArea;
	if (gpMarioOriginal->checkFlag(MARIO_FLAG_HAS_FLUDD)) {
		switch (curArea.getStage()) {
		case 0x3C:
			gpMarioOriginal->mWaterGun->changeNozzle(TWaterGun::Rocket, true);
			break;
		// asm 0x800EC094: these stages keep the current nozzle
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 8:
		case 9:
		case 0x10:
		case 0x2C:
		case 0x34:
		case 0x39:
			break;
		default: { // asm 0x800EC094: flag nozzle, then Spray
			TWaterGun* waterGun = gpMarioOriginal->mWaterGun;
			waterGun->changeNozzle(
			    (TWaterGun::TNozzleType)TFlagManager::getInstance()->getFlag(0x40004),
			    true);
			gpMarioOriginal->mWaterGun->changeNozzle(TWaterGun::Spray, true);
		} break;
		}
	}

	u8 uVar6 = SMS_getShineIDofExStage(curArea.getStage());
	if (uVar6 != 0xff && TFlagManager::getInstance()->getShineFlag(uVar6) == 0)
		gpMarioOriginal->offFlag(MARIO_FLAG_HAS_FLUDD);
}

void TMarDirector::nextStateInitialize(u8 next_state)
{
	TGameSequence& currSeq = gpApplication.mCurrArea;

	switch (next_state) {
	case 1: {
		const char* pcVar8 = "startcamera";
		unk18[0]->onFlag(TMarioGamePad::PAD_FLAG_MENU_INPUT);
		unk68 = 0;
		if (gpApplication.mCurrArea.unk0 == 1 && (unk4E & 2)) {
			if (gpApplication.mCurrArea.unk1 == 8) {
				switch (TFlagManager::smInstance->getFlag(0x60003)) {
				case 0:
					if (TFlagManager::smInstance->getFlag(0x40000) >= 0x14)
						pcVar8 = "mareopen_startcamera";
					break;
				case 1:
					pcVar8 = "yoshi_startcamera";
					break;
				case 2:
					pcVar8 = "turbo_startcamera";
					break;
				case 3:
					pcVar8 = "rocket_startcamera";
					break;
				}
			} else {
				if (TFlagManager::smInstance->getBool(0x50001)) {
					pcVar8 = "sinkricco";
				} else {
					if (TFlagManager::smInstance->getBool(0x50002))
						pcVar8 = "sinkmamma";
				}
			}
		}
		gpCamera->startDemoCamera(pcVar8, nullptr, -1, 0.0f, true);
		if (unk50 & 4) {
			mConsole->unk94->startAppearScenario();
			unk50 &= ~0x4;
		}
		MSMainProc::startStageEntranceDemo(currSeq.unk0, currSeq.unk1);
		break;
	}

	case 3:
		unk68 = 0;
		if (!(unk50 & 1)) {
			MSMainProc::startStageBGM(currSeq.unk0, currSeq.unk1);
			setMario();
			unk50 |= 1;
		}
		break;

	case 2:
		if (!(unk50 & 1)) {
			MSMainProc::startStageBGM(currSeq.unk0, currSeq.unk1);
			setMario();
			unk50 |= 1;
		}
		if (mMap != 0xf) {
			mConsole->unkC.off(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
		}
		break;

	case 4:
		if (mState <= STATE_UNK3 && mMap != 0xf) {
			mConsole->unkC.off(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
		}
		if (unk50 & 2) {
			mConsole->unk94->startAppearGo();
			unk50 &= ~0x2;
		}
		if (!(unk50 & 1)) {
			MSMainProc::startStageBGM(currSeq.unk0, currSeq.unk1);
			setMario();
			unk50 |= 1;
		}
		if (!unk124)
			OSStartStopwatch(&unkE8);
		unk18[0]->onFlag(TMarioGamePad::PAD_FLAG_GAME_INPUT);
		break;

	case 12:
		if (currSeq.unk0 == 1)
			THPPlayerStop();
	// !!!fallthrough!!!
	case 9: {
		gpApplication.mFader->startWipe(unkE4, 0.4f, 0.0f);
		if (unkE4 == 8)
			SMSGetMSound()->startSoundSystemSE(MSD_SE_MA_INTO_DOKAN, 0, nullptr,
			                                   0);
		MSound* sound = gpMSound;
		sound->fadeOutAllSound(SMSGetVSyncTimesPerSec() * 0.4f);
		SMSRumbleMgr->reset();
		for (int i = 0; i < 4; ++i)
			JUTGamePad::CRumble::stopMotor(unk18[i]->mPortNum);
		break;
	}

	case 5:
		if (currSeq.unk0 == 1)
			THPPlayerPause();
		SMSRumbleMgr->startPause();
		unkAC->setDrawStart();
		for (int i = 0; i < 4; ++i)
			JUTGamePad::CRumble::stopMotor(unk18[i]->mPortNum);
		unk18[0]->onFlag(TMarioGamePad::PAD_FLAG_MENU_INPUT);
		break;

	case 10:
		if (currSeq.unk0 == 1)
			THPPlayerPause();
		SMSRumbleMgr->startPause();
		for (int i = 0; i < 4; ++i)
			JUTGamePad::CRumble::stopMotor(unk18[i]->mPortNum);
		unk18[0]->onFlag(TMarioGamePad::PAD_FLAG_MENU_INPUT);
		static_cast<JDrama::TViewObj*>(JDrama::TNameRefGen::search("Group 2D"))
		    ->unkC.on(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
		static_cast<JDrama::TViewObj*>(JDrama::TNameRefGen::search("Guide"))
		    ->unkC.off(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
		SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_WIPE_IN, 0, nullptr, 0);
		gpApplication.mFader->startWipe(6, 1.0f, 0.0f);
		unk78->setup(nullptr);
		unk78->startMoveCursor();
		break;

	case 11:
		if (currSeq.unk0 == 1)
			THPPlayerPause();
		SMSRumbleMgr->startPause();
		unkAC->mCardSave->init(unk261);
		for (int i = 0; i < 4; ++i)
			JUTGamePad::CRumble::stopMotor(unk18[i]->mPortNum);
		unk18[0]->onFlag(TMarioGamePad::PAD_FLAG_MENU_INPUT);
		break;

	case 7:
		gpMarDirector->mConsole->unk94->startAppearMiss();
		TFlagManager::smInstance->decFlag(0x20001, 1);
		unk60 = mTickCount;
		gpApplication.mFader->setColor(JUtility::TColor(0, 0, 0, 0xff));
		if (TFlagManager::smInstance->getFlag(0x20001) >= 0) {
			MSBgm::startBGM(MSD_BGM_BOSS);
			if (unk4E & 8)
				gpApplication.mFader->startWipe(2, 0.0f, 2.0f);
			else
				gpApplication.mFader->startWipe(10, 0.0f, 2.2f);
		} else {
			MSBgm::startBGM(MSD_BGM_BOSSHANA_2ND3RD);
			gpApplication.mFader->startWipe(0xD, 0.0f, 2.0f);
		}
		break;
	}
}

u8 TMarDirector::updateGameMode()
{
	char trash[0x90];
	u8 r29 = mState;

	switch (unk124) {
	case 0:
		if (!(unk4C & 0x1FFF)) {
			if (SMS_CheckMarioFlag(MARIO_FLAG_GAME_OVER)) {
				unk4C |= 0x20;
				break;
			}

			if (mMap != 15) {
				if (unk18[0]->mButton.mTrigger & 0x10) {
					r29 = STATE_GUIDE;
					break;
				}

				if (unk18[0]->checkFrameMeaning(TMarioGamePad::MEANING_START)) {
					if (gpMarioOriginal->checkActionThing3()) {
						r29 = STATE_PAUSE_MENU;
						break;
					}

					SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_NOT_COLLECT, 0,
					                                   nullptr, 0);
				}
			}
		} else {
			if (unk4C & 0x20) {
				unk4C &= ~0x20;
				r29 = STATE_UNK7;
				TFlagManager::getInstance()->setFlag(0x40002, 0);
				break;
			}

			if (unk4C & 0x1) {
				unk4C &= ~0x1;
				unk126 = 3;

				TGCConsole2* console = gpMarDirector->mConsole;
				console->unk94->startAppearShineGet();
				console->unk47 = 1;
				MSBgm::startBGM(MSD_BGM_CHUBOSS);
				TFlagManager::getInstance()->setBool(true, 0x30006);
				TFlagManager::getInstance()->setShineFlag(unk25C->getEventId());
				f32 fVar3     = unkDC->mRate;
				f32 fadeInSec = 1.0f;
				unkDC->registFadeout(fadeInSec * fVar3, fVar3 * 5.3333333f);
				unk4C |= 0x8202;
				unk261 = 6;
				decideNextStage();
				break;
			}

			if (unk4C & 0x40) {
				unk126 = 3;
				break;
			}

			if (unk4C & 0x200) {
				unk4C &= ~0x200;
				r29 = STATE_CARD_SAVE;
				break;
			}

			if (unk4C & 0x8) {
				unk4C &= ~0x8;
				unk4C |= 0x2;
				unk126 = 3;
				if (gpApplication.mNextArea.getStage() == 5) {
					fireStartDemoCamera("hodai_dpt_pinna1", nullptr, -1, 0.0f,
					                    false, nullptr, 0, nullptr, 0);
					if (unk254 != nullptr)
						unk254->startDemo();
					break;
				}

				if (gpApplication.mNextArea.getStage() == 6) {
					fireStartDemoCamera("camera_sirena_gate_in", nullptr, -1,
					                    0.0f, false, nullptr, 0, nullptr, 0);
					break;
				}

				if (gpApplication.mNextArea.getStage() == 8) {
					fireStartDemoCamera("camera_monte_gate_in", nullptr, -1,
					                    0.0f, false, nullptr, 0, nullptr, 0);
					break;
				}

				break;
			}

			if (unk4C & 0x4) {
				unk4C &= ~0x4;
				unk4C |= 0x2;
				unk126 = 3;
				fireStartDemoCamera(nullptr, nullptr, -1, 0.0f, false, nullptr,
				                    0, unk250, 1);
				break;
			}

			if (unk4C & 0x2) {
				moveStage();
				r29 = STATE_UNK9;
				break;
			}
		}
		break;

	case 2:
		if (unk4C & 0x40) {
			unk126 = 4;
		} else {
			if (unkB0->mTalkMode == 0)
				unk126 = 0;
		}
		break;

	case 3:
	case 4: {
		bool bVar5  = false;
		bool uVar15 = 0;
		if (unk4C & 0x80) {
			uVar15 = 1;
			bVar5  = true;
			unk4C &= ~0x80;
		} else {
			if (!gpCamera->getRestDemoFrames()) {
				if (!MSBgm::getHandle(2) || mTickCount - unk60 >= 720) { // asm 0x800EB494: cmpwi 0x2d0
					bVar5  = true;
					uVar15 = unk12C[unk24D].unk10;
				}
			}
		}

		if (bVar5) {
			u32 prev = unk24D++;
			unk24D &= 0x7;
			TDemoInfo* info = &unk12C[prev];
			if (unk24D != unk24C) {
				gpCamera->endDemoCamera();
				if (info->unk14 != nullptr)
					(*info->unk14)(info->unk18, 1);
				info = &unk12C[unk24D]; // asm 0x800EB510: retail starts the advanced index

				gpCamera->startDemoCamera(info->unk0, info->unk4, info->unk8,
				                          info->unkC, info->unk10);
				if (info->unk14 != nullptr)
					(*info->unk14)(info->unk18, 0);
			} else {
				unk4C &= ~0x40;
				unk126 = unk124 == 4 ? 2 : 0;
				if (uVar15 != 0)
					gpCamera->endDemoCamera();
				if (info->unk14 != nullptr)
					(*info->unk14)(info->unk18, 1);
			}
		}
	} break;
	}

	if (unk24D == unk24C)
		unk4C &= ~0x40; // asm 0x800EB5C4: rlwinm 26,24 clears 0x40, not 0x80

	unk125 = unk124;

	if (unk124 != unk126) {
		switch (unk124) {
		case 2:
			if (unk126 == 0) {
				unkA0 = 0;
				unkA4 = 0;
				unk18[0]->offFlag(TMarioGamePad::PAD_FLAG_TALK_SELECT);
				OSStartStopwatch(&unkE8);
			}
			break;

		case 3:
		case 4:
			if (unk124 == 4)
				MSMainProc::fromTalkingCameraDemo(unk124 == 4);
			else
				MSMainProc::fromInnerCameraDemo();
			unk18[0]->offFlag(TMarioGamePad::PAD_FLAG_NO_INPUT);
			OSStartStopwatch(&unkE8);
			break;
		}

		switch (unk126) {
		case 0:
			break;

		case 1:
			unkA0->onLiveFlag(LIVE_FLAG_UNK40000);
			unkA0->unkC.off(CUE_MOVE | CUE_CALC_ANIM);
			unk18[0]->onFlag(TMarioGamePad::PAD_FLAG_TALK_SELECT);
			OSStopStopwatch(&unkE8);
			break;

		case 2:
			if (unk124 == 1)
				unkB0->openTalkWindow(unkA0);
			break;

		case 3:
		case 4:
			if (unk124 == 4)
				MSMainProc::toTalkingCameraDemo();
			else
				MSMainProc::toInnerCameraDemo();
			unk18[0]->onFlag(TMarioGamePad::PAD_FLAG_NO_INPUT);
			if ((int)unk12C[unk24D].unk20.mValue == 1) {
				gpCamera->startGateDemoCamera(unk12C[unk24D].unk1C);
			} else {
				TDemoInfo* info = &unk12C[unk24D];
				gpCamera->startDemoCamera(info->unk0, info->unk4, info->unk8,
				                          info->unkC, info->unk10);
				if (unk12C[unk24D].unk14 != nullptr)
					(*unk12C[unk24D].unk14)(unk12C[unk24D].unk18, 0);
			}
			OSStopStopwatch(&unkE8);
			unk60 = mTickCount;
			break;
		}

		unk124 = unk126;
	}

	if (unk128 & 0x1) {
		unk128 &= ~0x1;
		unk128 |= 0x2;
	} else {
		unk128 &= ~0x2;
	}

	return r29;
}

struct BigTemp24 {
	char c[0x24];
};
BigTemp24 MakeBigTemp24();

void TMarDirector::moveStage()
{
	unkB4 = TApplication::APP_STATE_GAMEPLAY;
	unkE4 = 15;
	gpApplication.mFader->setColor(JUtility::TColor(0, 0, 0, 0xff));
	if (0) {
		(void)MakeBigTemp24();
	}

	u8 sVar4 = SMS_getShineStage(gpApplication.mNextArea.getStage());
	u8 sVar5 = SMS_getShineStage(gpApplication.mCurrArea.getStage());
	if (sVar4 != sVar5)
		TFlagManager::smInstance->setFlag(0x40002, 0);
	TGameSequence& nextArea = gpApplication.mNextArea;


	if (nextArea.unk1 == 0xff)
		switch (nextArea.unk0) {
		case 1:
			unkE4         = 2;
			nextArea.unk1 = decideNextScenario(nextArea.getStage());
			TFlagManager::smInstance->setFlag(0x40003, 0);
			break;

		case 13: {
			unkE4     = 2;
			u32 thing = 0;
			switch (TFlagManager::smInstance->getFlag(0x40003)) {
			case 0:
				thing = 0;
				break;
			case 2:
				thing = 1;
				break;
			case 4:
				thing = 2;
				break;
			case 5:
				thing = 3;
				break;
			case 6:
				thing = 4;
				break;
			case 7:
				thing = 5;
				break;
			}
			nextArea.unk1 = thing;
			break;
		}

		case 0x3A: {
			unkE4     = 2;
			u32 thing = 0;
			switch (TFlagManager::smInstance->getFlag(0x40003)) {
			case 0:
				thing = 1;
				break;
			case 7:
				thing = 0;
				break;
			}
			nextArea.unk1 = thing;
			break;
		}

		case 7: {
			unkE4     = 2;
			u32 thing = 0;
			switch (TFlagManager::smInstance->getFlag(0x40003)) {
			case 1:
				thing = 0;
				break;
			case 2:
				thing = 1;
				break;
			case 3:
			case 4:
				thing = 2;
				break;
			case 6:
				thing = 3;
				break;
			case 7:
				thing = 4;
				break;
			}
			nextArea.unk1 = thing;
			break;
		}

		case 14: {
			unkE4     = 2;
			u32 thing = 0;
			switch (TFlagManager::smInstance->getFlag(0x40003)) {
			case 3:
				thing = 0;
				break;
			case 4:
				thing = 1;
				break;
			}
			nextArea.unk1 = thing;
			break;
		}

		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 8:
			unkE4 = 8; // asm 0x800EAE04: retail sets unkE4=8, unkB4=TITLE
			unkB4 = TApplication::APP_STATE_TITLE;
			break;

		case 9:
			gpApplication.mFader->setColor(
			    JUtility::TColor(0xD2, 0xD2, 0xD2, 0xFF));
			unkB4 = TApplication::APP_STATE_TITLE;
			break;

		case 0x34:
			unkE4         = 8;
			nextArea.unk1 = 0;
			TFlagManager::smInstance->setFlag(0x40003, 0);
			break;

		case 0:
			nextArea.unk1 = 0;
			TFlagManager::smInstance->setFlag(0x40003, 0);
			break;

		default:
			unkE4         = 2;
			nextArea.unk1 = 0;
			break;
		}

	if (nextArea.unk1 != 0xff) {
		if (unk4C & 0x100) {
			unkE4 = 15;
			gpApplication.mFader->setColor(JUtility::TColor(0, 0, 0, 0xff));
			unkB4 = TApplication::APP_STATE_MOVIE;
		} else {
			unkB4 = TApplication::APP_STATE_GAMEPLAY;
		}
	}

	if (gpMarioOriginal->checkFlag(MARIO_FLAG_HAS_FLUDD)) {
		int r5 = gpMarioOriginal->mWaterGun->mSecondNozzle; // asm 0x800EAF2C: lbz 0x1C85, value unless 3
		if (r5 == 3)
			r5 = 4;
		TFlagManager::smInstance->setFlag(0x40004, r5);
	}
}

JStage::TObject* TMarDirector::JSGFindObject(const char* param_1,
                                             JStage::TEObject param_2) const
{
	if (strcmp("cam_int1", param_1) == 0) {
		TMarDirector* casted = const_cast<TMarDirector*>(this);
		JDrama::TCamera* cam = (JDrama::TCamera*)casted->search("camera 1");
		return cam;
	}

	if (strcmp("mario", param_1) == 0) {
		TMarDirector* casted  = const_cast<TMarDirector*>(this);
		JDrama::TActor* mario = (JDrama::TActor*)casted->search(
		        "マリオ");
		return mario;
	}

	return JDrama::TDirector::JSGFindObject(param_1, param_2);
}

#ifdef TARGET_PC
#include <JSystem/JDrama/JDRDrawBufObj.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DDrawBuffer.hpp>
#include <Map/PollutionManager.hpp>
#include <Map/MapDraw.hpp>
#include <MarioUtil/ShadowUtil.hpp>
#include <MarioUtil/ScreenUtil.hpp>
#include <Strategic/MirrorActor.hpp>
#include <JSystem/JDrama/JDRViewObjPtrList.hpp>
#include <stddef.h>
#include <string.h>

// Presentation replay (sunshine-pc/src/framerate/framerate.cpp): draws the current game frame again without advancing
// logic. DRAW_INIT clears the draw buffers, so the entry stage is re-run first with logic (MOVE) and animation (CALC_ANIM)
// masked out; then the draw lists (same mask) and the post-direct overlay run as in direct() and gameLoop.
// Only in gameplay: other states (menus, movies, title) keep the single present.
bool sunshineCanReplay()
{
	if (gpApplication.mAppState != TApplication::APP_STATE_GAMEPLAY || gpMarDirector == nullptr
	    || gpApplication.mDirector != gpMarDirector || gpMarDirector->mState != TMarDirector::STATE_UNK4)
		return false;
	const TMarDirector* d = gpMarDirector;
	// setupObjects() builds the lists in stages; replay only once every list it draws from exists.
	return d->unk34 && d->unk38 && d->unk3C && d->unk40 && d->mPerformListGX && d->mPerformListSilhouette
	       && d->mPerformListGXPost && d->unk260 == 1;
}

typedef JGadget::TSingleLinkList<TPerformLink, 0>::iterator SunshineLinkIt;

// A draw buffer that the draw lists clear only after drawing it: a bare CUE_DRAW_INIT link on a TDrawBufObj that no draw
// list makes current. The entry stage fills it (DrawBuf AfterIndirect Opa/Xlu: TMapStaticObj on CUE_ENTRY).
static bool sunshineIsPostDrawClear(TPerformList* const* lists, int count, TPerformLink& link)
{
	if (link.mCueFilter != CUE_DRAW_INIT || dynamic_cast<JDrama::TDrawBufObj*>(link.mPerformer) == nullptr)
		return false;
	for (int i = 0; i < count; ++i)
		for (SunshineLinkIt it = lists[i]->getChildren().begin(); it != lists[i]->getChildren().end(); it++)
			if (it->mPerformer == link.mPerformer && (it->mCueFilter & CUE_SET_DRAW_BUFFER))
				return false;
	return true;
}

void sunshineReplayFrame()
{
	TMarDirector* d = gpMarDirector;
	// One TGraphics for the whole frame, as in direct(): TFrmGXSet (entry list, CUE_DRAW) fills in the frame buffer and
	// render mode that the draw lists read.
	JDrama::TGraphics local;
	JDrama::SunshineApplyDisplay(d->unkC0, &local);
	local.unk0 = 0; // the flag halfword direct() writes (asm 0x800ED050: sth to TGraphics+0x0)
	// Post-draw-clear buffers still hold this tick's entries here. J3D links an entered packet in place, so entering a
	// model twice loops the packet list and its draw never ends (Aurora aborts on its full 2 MiB index buffer). Empty
	// them before the entry stage and skip their clear below: the next direct() draws the replay's entries and clears
	// them, as retail does with its own.
	TPerformList* const draws[] = { d->mPerformListGX, d->mPerformListSilhouette, d->mPerformListGXPost };
	for (int i = 0; i < 3; ++i)
		for (SunshineLinkIt it = draws[i]->getChildren().begin(); it != draws[i]->getChildren().end(); it++)
			if (sunshineIsPostDrawClear(draws, 3, *it))
				static_cast<JDrama::TDrawBufObj*>(it->mPerformer)->getDrawBuffer()->frameInit();
	// Circle shadows: the entry stage requests them (CUE_CALC_VIEW/CUE_ENTRY: TLiveActor::requestShadow; Mario's
	// TMBindShadowBody on CUE_ENTRY) and the draw lists consume them: TMBindShadowManager's CUE_CALC_VIEW link
	// (0x4000000e) runs calcVtx, which rewrites the requests in place (radius * 0.8, position put on the ground, square
	// shadows moved along the light, ship radius 1), then its draw link (0x20000008) draws them and reset()s. direct()
	// draws at the start of the next call what the last entry stage requested, so this tick's requests are still pending
	// here, not yet processed. Set them aside and reset, so the replay's entry stage requests them once and its draw lists
	// process and clear that copy; afterwards put the unprocessed requests and the counters (the six fields reset()
	// clears) back, so the next direct() processes and draws them once, as retail does.
	// The null test is no real guard: gpBindShadowManager is never cleared, it only covers boot before the first manager.
	static TCircleShadowRequest sSavedShadowReqs[0x200]; // capacity of mRequests (TMBindShadowManager ctor)
	TMBindShadowManager* const shadows = gpBindShadowManager;
	const int shadowReqs = shadows ? shadows->mRequestNum : 0;
	const int shadowQuadArys = shadows ? shadows->mQuadAryNum : 0;
	const int shadowSquares = shadows ? shadows->mSquareShadowNum : 0;
	const u16 shadowModels = shadows ? shadows->mModelShadowNum : 0;
	const u8 shadowUnk49 = shadows ? shadows->unk49 : 0;
	const bool shadowUnk65 = shadows ? shadows->unk65 : false;
	for (int i = 0; i < shadowReqs; ++i)
		sSavedShadowReqs[i] = shadows->mRequests[i];
	if (shadows)
		shadows->reset();
	d->unk34->perform(CUE_ALL & ~(CUE_MOVE | CUE_CALC_ANIM), &local);
	d->unk40->perform(CUE_ALL, &local);
	// On a stage with pollution layers unk38/unk3C are the pollution passes (initECTGft): they count goop, apply the
	// stamps queued by logic and the layer's per-frame change, and the copy is read back into the map that logic reads.
	// That is game logic run once per tick in retail, not a picture: the replay skips them and draws with the maps of the
	// last tick. Without layers unk38 is empty and unk3C holds only the bath water preprocessor (a draw-only depth
	// pre-pass and its GXCopyTex for the bath water draw), so the replay runs it as direct() does.
	if (gpPollution->getJointModelNum() == 0)
		d->unk3C->perform(CUE_ALL, &local);
	// Draw code that steps state once per drawn frame (retail draws once per tick): the console's rocket nozzle pressure
	// flash (TGCConsole2::drawWaterBack, ++unk30C), the camera-in-wall overlay fade (TMapDrawWall::perform, unk18) and
	// the screen blur easing (TAfterEffect::perform on CUE_SET_PROJECTION). Their state is put back after the draw lists,
	// so it steps once per tick and the next direct() draws the frame the replay drew.
	TGCConsole2* const console = d->mConsole;
	const u8 consoleFlash = console ? console->unk30C : 0;
	TMapDrawWall* const wall = static_cast<TMapDrawWall*>(JDrama::TNameRefGen::search("カメラ食い込み壁"));
	const int wallAlpha = wall ? wall->unk18 : 0;
	// The blur state from unk14 to the end (the vtable and the name ref stay as they are). The null test is no real guard
	// (gpAfterEffect is never cleared, scenecmn sets it per stage).
	const size_t afterEffectAt = offsetof(TAfterEffect, unk14);
	u8 savedAfterEffect[sizeof(TAfterEffect) - offsetof(TAfterEffect, unk14)];
	if (gpAfterEffect)
		memcpy(savedAfterEffect, reinterpret_cast<u8*>(gpAfterEffect) + afterEffectAt, sizeof(savedAfterEffect));
	const bool silhouette = (gpSilhouetteManager->unk48 > 0.0f ? true : false) || gpCamera->unk2C8 != -1;
	// Mirror actors (TMirrorActor; Mario's マリオin鏡 exists on every stage) copy their source model's animation matrices,
	// and check whether they are in the mirror, only on CUE_CALC_ANIM, which the mask below takes from the mirror scene's
	// link. direct() runs that link before the logic, so the replay would draw this tick's models with mirror images of
	// the last tick. Run the copy here: it reads only this tick's state, so the next direct() repeats it unchanged.
	typedef JDrama::TViewObjPtrListT<JDrama::TViewObj> SunshineMirrorScene;
	SunshineMirrorScene* const mirrorScene
	    = static_cast<SunshineMirrorScene*>(JDrama::TNameRefGen::search("鏡シーン"));
	// Same gate as the scene's own testPerform in the 0x206 link: a scene with CALC_ANIM masked copies nothing.
	if (mirrorScene && !mirrorScene->unkC.check(CUE_CALC_ANIM))
		for (JGadget::TList_pointer<JDrama::TViewObj*>::iterator it = mirrorScene->getChildren().begin();
		     it != mirrorScene->getChildren().end(); ++it)
			if (TMirrorActor* mirror = dynamic_cast<TMirrorActor*>(*it))
				mirror->testPerform(CUE_CALC_ANIM, &local);
	// Logic cues masked as on the entry stage: PerformList GX runs the mirror scene with CUE_CALC_ANIM (link 0x206 from
	// PerformLists.bin), and each such perform steps the animation of TMapStaticObj 太陽in鏡 (MActor::frameUpdate).
	for (int i = 0; i < 3; ++i) {
		if (i == 1 && !silhouette)
			continue;
		for (SunshineLinkIt it = draws[i]->getChildren().begin(); it != draws[i]->getChildren().end(); it++)
			if (!sunshineIsPostDrawClear(draws, 3, *it))
				it->perform(CUE_ALL & ~(CUE_MOVE | CUE_CALC_ANIM), &local);
	}
	GXInvalidateTexAll();
	if (console)
		console->unk30C = consoleFlash;
	if (wall)
		wall->unk18 = wallAlpha;
	if (gpAfterEffect)
		memcpy(reinterpret_cast<u8*>(gpAfterEffect) + afterEffectAt, savedAfterEffect, sizeof(savedAfterEffect));
	for (int i = 0; i < shadowReqs; ++i)
		shadows->mRequests[i] = sSavedShadowReqs[i];
	if (shadows) {
		shadows->mRequestNum      = shadowReqs;
		shadows->mQuadAryNum      = shadowQuadArys;
		shadows->mSquareShadowNum = shadowSquares;
		shadows->mModelShadowNum  = shadowModels;
		shadows->unk49            = shadowUnk49;
		shadows->unk65            = shadowUnk65;
	}

	// Same overlay as TApplication::gameLoop (Application.cpp), without advancing the fader or the sound mixer.
	JDrama::TVideo* video = gpApplication.mDisplay->unk60;
	GXSetViewport(0.0f, 0.0f, video->mNextRenderMode.fbWidth, video->mNextRenderMode.efbHeight, 0.0f, 1.0f);
	GXSetScissor(0, 0, video->mNextRenderMode.fbWidth, video->mNextRenderMode.efbHeight);
	Mtx ortho;
	C_MTXOrtho(ortho, 0.0f, (f32)video->mNextRenderMode.fbWidth, 0.0f, (f32)video->mNextRenderMode.efbHeight, -1.0f,
	           1.0f);
	GXSetProjection(ortho, GX_ORTHOGRAPHIC);
	gpApplication.mFader->draw(JDrama::TRect(0, 0, video->mNextRenderMode.fbWidth, video->mNextRenderMode.efbHeight));
}
#endif
