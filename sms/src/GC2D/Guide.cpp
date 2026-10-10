#include <GC2D/Guide.hpp>
#include <stdio.h>
#include <string.h>
#include <JSystem/J2D/J2DOrthoGraph.hpp>
#include <JSystem/J2D/J2DPicture.hpp>
#include <JSystem/J2D/J2DScreen.hpp>
#include <JSystem/J2D/J2DTextBox.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JKernel/JKRMemArchive.hpp>
#include <JSystem/JUtility/JUTPoint.hpp>
#include <JSystem/JUtility/JUTResFont.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <GC2D/BoundPane.hpp>
#include <GC2D/ExPane.hpp>
#include <GC2D/MessageUtil.hpp>
#include <GC2D/ScrnFader.hpp>
#include <MSound/MSound.hpp>
#include <Player/MarioAccess.hpp>
#include <System/Application.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <System/MarioGamePad.hpp>
#include <System/StageUtil.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <System/DummyStrings.hpp>

static u8 setup_wait;

static u32 scNormalStageTable[] = {
	0, 1, 2, 3, 4, 0xD, 6, 8, 9, 0xA,
};

void TGuide::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (setup_wait) {
		setup_wait--;
		if (setup_wait == 0) {
			SMSSwitch2DArchive("game_6", gArBkGuide);
			unkC4 = 0;

			s16 stage = SMS_getShineStage(gpMarDirector->mMap);
			mStage    = stage;
			resetObjects();
			changeBotStatus(stage);

			for (int i = 0; i < 10; ++i) {
				if (i == stage)
					mMarioIcon[i]->show();
				else
					mMarioIcon[i]->hide();
			}

			unk164 = 0;
		} else {
			return;
		}
	}

	if ((cue & CUE_DRAW) && mState != 9 && mState != 8) {
		J2DOrthoGraph ortho(graphics->getViewport());
		ortho.setup2D();
		mScreen->draw(0, 0, &ortho);
	}

	if (cue & CUE_MOVE) {
		bool done = true;
		switch (mState) {
		case 9: {
			if (unkC5 && gpApplication.mFader->isFullyFadedOut()) {
				gpApplication.mFader->startWipe(5, 1.0f, 0.0f);
				mState = 10;
			}

			JUTRect rect
			    = mStagePane[SMS_getShineStage(gpMarDirector->mMap)]->getBounds();
			mCursor[0]->getPane()->move(rect.x1 + 6, rect.y1 - 1);
			mCursor[1]->getPane()->move(rect.x1 + 6, rect.y1 - 1);
			break;
		}
		case 10:
			if (gpApplication.mFader->isFullyFadedIn()) {
				mState = 0;
				unk428 = nullptr;
				unk424 = nullptr;
				mCursor[0]->getPane()->setAlpha(255);
				mCursor[1]->getPane()->setAlpha(80);
			}
			break;
		case 0:
			linkSelect();
			break;
		case 1:
			done &= unk424->update();
			for (int i = 0; i < 2; ++i)
				done &= mCursor[i]->update();

			if (done) {
				if (unk428->update())
					mState = 2;
			}
			break;
		case 2:
			if (unkC0->checkFrameMeaning(TMarioGamePad::MEANING_MENU_A
			                             | TMarioGamePad::MEANING_MENU_B)) {
				s16 stage = mStage;
				unk428->setPaneAlpha(20, 0, 255);
				JUTRect from = unk218[stage];
				JUTRect to   = mStagePane[stage]->getBounds();
				SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_TALK_MODE_OUT, 0,
				                                   nullptr, 0);

				unk424->setCenteredSize(20, 0, 0, from.getWidth(),
				                        from.getHeight());

				// NOTE: retail uses from.x1 for both axes
				unk424->setPaneOffset(20, to.x1 - from.x1, to.y1 - from.x1 - 40,
				                      0, 0);

				mCursor[0]->setPaneAlpha(20, 255, 0);
				mCursor[1]->setPaneAlpha(20, 80, 0);
				mState = 3;
			} else if (unkC0->testTrigger(JUTGamePad::Z)) {
				mState = 7;
			}
			break;
		case 3:
			if (unk428->update()) {
				done &= unk424->update();
				for (int i = 0; i < 2; ++i)
					done &= mCursor[i]->update();

				if (done) {
					unk424->getPane()->hide();
					unk428->getPane()->hide();
					mState = 0;
					mFrame = 0;
				}
			}
			break;
		case 7:
			gpApplication.mFader->startWipe(6, 1.0f, 0.0f);
			unkC0->offFlag(TMarioGamePad::PAD_FLAG_GUIDE_INPUT);
			SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_WIPE_OUT, 0, nullptr,
			                                   0);
			mState = 11;
			break;
		case 11:
			if (gpApplication.mFader->isFullyFadedOut()) {
				gpApplication.mFader->startWipe(5, 1.0f, 0.0f);
				if (unk424 && unk424->getPane()->isVisible())
					unk424->getPane()->hide();
				if (unk428 && unk428->getPane()->isVisible())
					unk428->getPane()->hide();
				unkC4  = 1;
				mState = 8;
			}
			break;
		}
	}
}

void TGuide::appearGuidePane(int index)
{
	unk424 = unk1C0[index];
	unk428 = unk378[index];

	JUTRect from = unk218[index];
	JUTRect to   = mStagePane[index]->getBounds();
	char trash[8];

	unk424->getPane()->show();

	unk424->setCenteredSize(20, from.getWidth(), from.getHeight(), 0, 0);

	// NOTE: retail uses from.x1 for both axes
	unk424->setPaneOffset(20, 0, 0, to.x1 - from.x1, to.y1 - from.x1 - 40);

	unk428->getPane()->setAlpha(0);
	unk428->getPane()->show();
	unk428->setPaneAlpha(20, 255, 0);
	mCursor[0]->setPaneAlpha(20, 0, 255);
	mCursor[1]->setPaneAlpha(20, 0, 80);

	if (index == 1)
		placeMario();

	SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_TALK_MODE_IN, 0, nullptr, 0);

	mStage = index;
	mState = 1;
	if (index != -1 && index < 10) {
		unk164 = 0;
		unk44C[index]->setAlpha(255);
	}
}

void TGuide::placeMario()
{
	if (SMS_getShineStage(gpMarDirector->mMap) != 1) {
		unk430->hide();
		return;
	}

	JGeometry::TVec3<f32> pos = *gpMarioPos;
	int mapWidth              = unk434.getWidth();
	int mapHeight             = unk434.getHeight();
	pos.set(pos.x * mapWidth / 25000.0f, 0.0f, pos.z * mapHeight / 21200.0f);

	J2DPane* icon = unk430;
	int iconWidth  = icon->getWidth();
	int iconHeight = icon->getHeight();
	int x = pos.x + mapWidth * 0.5f - iconWidth * 0.5f - 2.0f;
	int y = pos.z + mapHeight * 0.5f + iconHeight * 0.5f;

	if (x > mapWidth - iconWidth)
		x = mapWidth - iconWidth;
	if (x < 0)
		x = 0;
	if (y > mapHeight - iconHeight)
		y = mapHeight - iconHeight;
	if (y < 0)
		y = 0;

	icon->show();
	unk430->move(x, y);

	for (int i = 2; i < 10; ++i) {
		if (TFlagManager::getInstance()->getBool(0x103A5 + i))
			mScreen->search('01g1' + (i - 2))->show();
		else
			mScreen->search('01g1' + (i - 2))->hide();
	}
}

void TGuide::changeBotStatus(int stage)
{
	if (stage == -1 || stage >= 10) {
		mStageNameText->hide();
		return;
	}

	if (mStageInfo[stage].unk0 == 0) {
		mStageNameText->show();
		unkF4->show();
		strncpy(mStageNameText->getStringPtr(),
		        SMSGetMessageData(unk474, stage), 26);

		int shines = mStageInfo[stage].mShines;
		if (shines < 0)
			shines = 0;
		if (shines > 99)
			shines = 99;

		if (shines < 10) {
			unkF8[1]->hide();
			unkF8[0]->changeTexture(mNumberTex[shines]->getTexInfo(), 0);
		} else {
			unkF8[1]->show();
			unkF8[0]->changeTexture(mNumberTex[shines / 10]->getTexInfo(), 0);
			unkF8[1]->changeTexture(mNumberTex[shines % 10]->getTexInfo(), 0);
		}

		if (stage == 0 || stage == 1 || mStageInfo[stage].mEtcShines == 0) {
			unk100->hide();
			unk104[0]->hide();
			unk104[1]->hide();
		} else if (mStageInfo[stage].mEtcShines == 1) {
			unk100->show();
			unk104[0]->show();
			unk104[1]->hide();
		} else {
			unk100->show();
			unk104[0]->show();
			unk104[1]->show();
		}

		int coins = mStageInfo[stage].mCoins;
		if (coins < 0)
			coins = 0;
		if (coins > 999)
			coins = 999;

		if (coins < 100) {
			unk10C[2]->hide();
			unk10C[0]->changeTexture(mNumberTex[coins / 10]->getTexInfo(), 0);
			unk10C[1]->changeTexture(mNumberTex[coins % 10]->getTexInfo(), 0);
		} else {
			unk10C[2]->show();
			unk10C[0]->changeTexture(mNumberTex[coins / 100]->getTexInfo(), 0);
			coins -= coins / 100 * 100;
			unk10C[1]->changeTexture(mNumberTex[coins / 10]->getTexInfo(), 0);
			unk10C[2]->changeTexture(mNumberTex[coins % 10]->getTexInfo(), 0);
		}

		if (mStageInfo[stage].mEtcShine0)
			unk118->show();
		else
			unk118->hide();

		int blueCoins = mStageInfo[stage].mBlueCoins;
		if (blueCoins < 0)
			blueCoins = 0;
		if (blueCoins > 99)
			blueCoins = 99;

		if (stage == 0) {
			mScreen->search('sb_i')->hide();
			mScreen->search('sc_t')->hide();
		} else {
			mScreen->search('sb_i')->show();
			mScreen->search('sc_t')->show();
		}

		if (blueCoins < 10) {
			unk11C[1]->hide();
			unk11C[0]->changeTexture(mNumberTex[blueCoins % 10]->getTexInfo(),
			                         0);
		} else {
			unk11C[1]->show();
			unk11C[0]->changeTexture(mNumberTex[blueCoins / 10]->getTexInfo(),
			                         0);
			unk11C[1]->changeTexture(mNumberTex[blueCoins % 10]->getTexInfo(),
			                         0);
		}
	} else {
		mStageNameText->show();
		unkF4->hide();

		int blueCoins = mStageInfo[stage].mBlueCoins;
		if (blueCoins < 0)
			blueCoins = 0;
		if (blueCoins > 99)
			blueCoins = 99;

		if (blueCoins < 10) {
			unk11C[1]->hide();
			unk11C[0]->changeTexture(mNumberTex[blueCoins % 10]->getTexInfo(),
			                         0);
		} else {
			unk11C[1]->show();
			unk11C[0]->changeTexture(mNumberTex[blueCoins / 10]->getTexInfo(),
			                         0);
			unk11C[1]->changeTexture(mNumberTex[blueCoins % 10]->getTexInfo(),
			                         0);
		}

		mScreen->search('sb_i')->show();
		mScreen->search('sc_t')->hide();
		mScreen->search('sq_i')->hide();
		strncpy(mStageNameText->getStringPtr(),
		        SMSGetMessageData(unk474, stage), 26);
	}
}

int TGuide::checkPoint(int x, int y)
{
	int result = -1;

	for (int i = 0; i < 14; ++i) {
		JUTRect rect = mStagePane[i]->getBounds();
		if (x > rect.x1 && x < rect.x2 && y > rect.y1 && y < rect.y2) {
			result = i;
			break;
		}
	}

	if (result == -1) {
		for (int i = 0; i < 10; ++i) {
			JUTRect rect = unk44C[i]->getBounds();
			if (x > rect.x1 && x < rect.x2 && y > rect.y1 && y < rect.y2) {
				result = i;
				break;
			}
		}
	}

	if (result >= 0 && result < 10) {
		if (!unk44C[result]->isVisible())
			result = -1;
	}

	return result;
}

inline void TGuide::changePattern(J2DPicture* picture, s16 period, u32 frame)
{
	if (frame % period == 0) {
		if (frame / period % 2 == 0) {
			picture->setBlendKonstColor(0.0f, 1.0f, 0.0f, 0.0f);
			picture->setBlendKonstAlpha(0.0f, 1.0f, 0.0f, 0.0f);
		} else {
			picture->setBlendKonstColor(1.0f, 0.0f, 0.0f, 0.0f);
			picture->setBlendKonstAlpha(1.0f, 0.0f, 0.0f, 0.0f);
		}
	}
}

inline void TGuide::mirrorPattern(J2DPicture* picture, s16 period, u32 frame)
{
	if (frame % period == 0) {
		if (frame / period % 2 == 0)
			picture->mMirror = MIRROR0;
		else
			picture->mMirror = J2DMirror_X;
	}
}

inline void TGuide::rotatePattern(J2DPicture* picture, s16 period, u32 frame,
                                  s16 angle)
{
	if (frame % period == 0) {
		if (frame / period % 2 == 0)
			picture->mRotation = 0.0f;
		else
			picture->mRotation = angle;
	}
}

inline void TGuide::shinePattern(TBoundPane* pane, s16 period, u32 frame)
{
	u32 phase = frame % period;
	if (phase == 0) {
		pane->setPanePosition(period / 2, JUTPoint(0, 0), JUTPoint(0, -5),
		                      JUTPoint(0, 0));
	} else if (phase == period / 2) {
		pane->setPanePosition(period / 2, JUTPoint(0, 0), JUTPoint(0, 5),
		                      JUTPoint(0, 0));
	}
}

void TGuide::linkSelect()
{
	unkC0->onFlag(TMarioGamePad::PAD_FLAG_GUIDE_INPUT);

	if (unkC0->checkFrameMeaning(TMarioGamePad::MEANING_MENU_B)
	    || unkC0->testTrigger(JUTGamePad::Z))
		mState = 7;

	J2DPane* cursor = mCursor[0]->getPane();
	int x           = cursor->mBounds.x1;
	int y           = cursor->mBounds.y1;
	x += (s16)(unkC0->mCompSPos[8] * 3.2f);
	y += (s16)(-3.2f * unkC0->mCompSPos[9]);
	if (x > 0x238)
		x = 0x238;
	if (x < 0)
		x = 0;
	if (y > 0x168)
		y = 0x168;
	if (y < 0x38)
		y = 0x38;

	cursor->move(x, y);
	mCursor[1]->getPane()->move(x + 7, y + 4);

	int hit = checkPoint(x - 2, y + 10);
	if (hit != -1 && unkC0->checkMeaning(TMarioGamePad::MEANING_MENU_A))
		appearGuidePane(hit);

	if (hit != -1 && hit < 10) {
		J2DPane** slot = &unk44C[hit];
		int alpha      = (*slot)->getAlpha();
		alpha          = unk164 ? alpha + 4 : alpha - 4;
		if (alpha < 30) {
			unk164 = 1;
			alpha  = 30;
		} else if (alpha > 255) {
			unk164 = 0;
			alpha  = 255;
		}
		(*slot)->setAlpha(alpha);
	}

	changePattern((J2DPicture*)mCursor[0]->getPane(), 45, mFrame);
	changePattern((J2DPicture*)mCursor[1]->getPane(), 45, mFrame);

	if (unk480 != hit) {
		changeBotStatus(hit);
		if (unk480 != -1 && unk480 < 10)
			unk44C[unk480]->setAlpha(255);

		if (hit == -1) {
			J2DPicture* picture = (J2DPicture*)mCursor[0]->getPane();
			picture->setBlendKonstColor(1.0f, 0.0f, 0.0f, 0.0f);
			picture->setBlendKonstAlpha(1.0f, 0.0f, 0.0f, 0.0f);

			picture = (J2DPicture*)mCursor[1]->getPane();
			picture->setBlendKonstColor(1.0f, 0.0f, 0.0f, 0.0f);
			picture->setBlendKonstAlpha(1.0f, 0.0f, 0.0f, 0.0f);
		}

		unk164 = 0;
		unk480 = hit;
	}

	changePattern(unk134, 90, mFrame);
	mirrorPattern(unk148, 90, mFrame);
	rotatePattern(unk154, 90, mFrame, 30);
	changePattern(unk138, 90, mFrame);
	mirrorPattern(unk14C, 90, mFrame);
	mirrorPattern(unk150, 90, mFrame);
	changePattern(unk13C, 90, mFrame);
	changePattern(unk140, 90, mFrame);
	changePattern(unk144, 90, mFrame);
	rotatePattern(unk158, 90, mFrame, -45);

	TBoundPane* bound = unk444;
	u32 frame         = mFrame;
	shinePattern(bound, 90, frame);
	unk448->setAlpha(frame % 180 < 130 ? 255 : 0);
	bound->update();

	// NOTE: written out by hand; as a nested inline the setPaneAlpha expansion
	// below would lose its clapAlpha inlining
	u32 markFrame = mFrame;
	if (markFrame % 270 == 0) {
		if (markFrame / 270 % 2 == 0)
			unk478->setPaneAlpha(270, 0, unk47C);
		else
			unk478->setPaneAlpha(270, unk47C, 0);
	}
	unk478->update();

	if (unk15C) {
		mAlpha += 3;
		if (mAlpha > 300)
			unk15C = false;
	} else {
		mAlpha -= 3;
		if (mAlpha < 30)
			unk15C = true;
	}

	u8 alpha;
	if (mAlpha < 30)
		alpha = 30;
	else if (mAlpha > 255)
		alpha = 255;
	else
		alpha = mAlpha;

	for (int i = 0; i < 10; ++i)
		mStagePane[i]->setAlpha(alpha);

	mFrame++;
	if (mFrame > 540)
		mFrame = 0;
}

void TGuide::startMoveCursor()
{
	mState = 9;
	unk164 = 0;
}

JKRMemArchive* TGuide::setup(JKRMemArchive* archive)
{
	if (archive)
		SMSMountAramArchive(archive, gArBkGuide);
	else
		setup_wait = 16;

	unkC4 = 0;
	return archive;
}

void TGuide::resetScore()
{
	u8 etcTotal = 0;
	u8 total    = 0;

	for (int i = 0; i < 10; ++i) {
		if (i == 9)
			continue;

		if (TFlagManager::getInstance()->getBool(0x103A5 + i))
			mScreen->search('0_mn' + (i << 24))->show();
		else
			mScreen->search('0_mn' + (i << 24))->hide();

		if (!(i == 0 || i == 1)) {
			for (int j = 0; j < 8; ++j) {
				if (j < mStageInfo[i].mShines)
					mScreen->search('0ss1' + (i << 24) + j)->show();
				else
					mScreen->search('0ss1' + (i << 24) + j)->hide();
			}

			J2DPane* etc1 = mScreen->search('0sq1' + (i << 24));
			etc1->hide();
			J2DPane* etc2 = mScreen->search('0sq2' + (i << 24));
			etc2->hide();
			if (mStageInfo[i].mEtcShines != 0)
				etc1->show();
			if (mStageInfo[i].mEtcShines > 1)
				etc2->show();

			etcTotal += mStageInfo[i].mEtcShines;
			total += mStageInfo[i].mShines;
		}
	}

	total += etcTotal;
	if (etcTotal == 0)
		mScreen->search('lqus')->hide();
	else
		mScreen->search('lqus')->show();

	for (int i = 1; i < 10; ++i) {
		mMarioIcon[i] = mScreen->search('mi00' + i);
		if (i == 9)
			continue;

		u16 coins = mStageInfo[i].mCoins;
		u32 tag   = '0c_1' + (i << 24);
		if (coins > 999)
			coins = 999;

		J2DPicture* digit1 = (J2DPicture*)mScreen->search(tag);
		J2DPicture* digit2 = (J2DPicture*)mScreen->search(tag + 1);
		J2DPicture* digit3 = (J2DPicture*)mScreen->search(tag + 2);

		if (coins < 100) {
			digit1->hide();
			digit2->changeTexture(mNumberTex[coins / 10]->getTexInfo(), 0);
			digit3->changeTexture(mNumberTex[coins % 10]->getTexInfo(), 0);
		} else {
			digit1->show();
			digit1->changeTexture(mNumberTex[coins / 100]->getTexInfo(), 0);
			coins -= coins / 100 * 100;
			digit2->changeTexture(mNumberTex[coins / 10]->getTexInfo(), 0);
			digit3->changeTexture(mNumberTex[coins % 10]->getTexInfo(), 0);
		}

		if (mStageInfo[i].mEtcShine0) {
			mScreen->search('0c_s' + (i << 24))->show();
			total++;
		} else {
			mScreen->search('0c_s' + (i << 24))->hide();
		}
	}

	mMarioIcon[0] = mScreen->search('mi00');
	unk448        = mScreen->search('clic');

	s16 secret = 0;
	if (TFlagManager::getInstance()->getBool(0x10056))
		secret += 1;
	if (TFlagManager::getInstance()->getBool(0x10058))
		secret += 1;

	const ResTIMG* secretTex = mNumberTex[secret]->getTexInfo();
	((J2DPicture*)mScreen->search('0s_1'))->changeTexture(secretTex, 0);
	total += secret;

	int totalShines = TFlagManager::getInstance()->getFlag(0x40000);
	u8 rest         = totalShines - total;
	if (rest > 99)
		rest = 99;

	const ResTIMG* restTex1 = mNumberTex[rest / 10]->getTexInfo();
	((J2DPicture*)mScreen->search('1s_1'))->changeTexture(restTex1, 0);
	const ResTIMG* restTex2 = mNumberTex[rest % 10]->getTexInfo();
	((J2DPicture*)mScreen->search('1s_2'))->changeTexture(restTex2, 0);

	if (totalShines > 999)
		totalShines = 999;

	J2DPicture* total1 = (J2DPicture*)mScreen->search('lt_1');
	J2DPicture* total2 = (J2DPicture*)mScreen->search('lt_2');
	J2DPicture* total3 = (J2DPicture*)mScreen->search('lt_3');
	if (totalShines < 100) {
		total1->hide();
		total2->changeTexture(mNumberTex[totalShines / 10]->getTexInfo(), 0);
		total3->changeTexture(mNumberTex[totalShines % 10]->getTexInfo(), 0);
	} else {
		total1->show();
		total1->changeTexture(mNumberTex[totalShines / 100]->getTexInfo(), 0);
		totalShines -= totalShines / 100 * 100;
		total2->changeTexture(mNumberTex[totalShines / 10]->getTexInfo(), 0);
		total3->changeTexture(mNumberTex[totalShines % 10]->getTexInfo(), 0);
	}

	switch (gpApplication.mSaveFile) {
	case 0:
		mScreen->search('ld_a')->show();
		mScreen->search('ld_b')->hide();
		mScreen->search('ld_c')->hide();
		break;
	case 1:
		mScreen->search('ld_a')->hide();
		mScreen->search('ld_b')->show();
		mScreen->search('ld_c')->hide();
		break;
	case 2:
		mScreen->search('ld_a')->hide();
		mScreen->search('ld_b')->hide();
		mScreen->search('ld_c')->show();
		break;
	}

	unk47C = 255.0f
	         * (1.0f - TFlagManager::getInstance()->getFlag(0x40000) / 30 * 0.25f);
	unk478->getPane()->setAlpha(unk47C);
}

void TGuide::resetObjects()
{
	int total = 0;

	for (u32 i = 0; i < 13; ++i) {
		if (i >= 10)
			continue;

		mStageInfo[i].unk0 = 0;

		int shines = 0;
		if (i != 0 && i != 1) {
			for (u32 j = 0; j < 8; ++j) {
				if (SMS_isGetShine(i, j, false))
					++shines;
			}
		}

		int shineCount          = shines < 100 ? shines : 99;
		mStageInfo[i].mShines = shineCount;
		total += shineCount;

		int etcShines = 0;
		if (i != 0 && i != 1) {
			if (SMS_isGetShine(i, 1, true))
				++etcShines;
			if (SMS_isGetShine(i, 2, true))
				++etcShines;
		}

		etcShines                 = etcShines < 10 ? etcShines : 9;
		mStageInfo[i].mEtcShines = etcShines;
		total += etcShines;

		int coins = (u16)TFlagManager::getInstance()->getFlag(0x20005 + i);
		mStageInfo[i].mCoins = coins < 1000 ? coins : 999;

		mStageInfo[i].mEtcShine0 = SMS_isGetShine(i, 0, true);
		if (mStageInfo[i].mEtcShine0)
			total += 1;

		int blueCoins = 0;
		if (i != 0) {
			for (u8 j = 0; j < 50; ++j) {
				if (TFlagManager::getInstance()->getBlueCoinFlag(
				        scNormalStageTable[i], j))
					++blueCoins;
			}
		}

		blueCoins                 = blueCoins < 1000 ? blueCoins : 999;
		mStageInfo[i].mBlueCoins = blueCoins;

		if (TFlagManager::getInstance()->getBool(0x103A5 + i)) {
			unk44C[i]->show();
			mStagePane[i]->show();
		} else {
			unk44C[i]->hide();
			mStagePane[i]->hide();
		}
	}

	mStageInfo[9].unk0 = 1;

	int blueCoins = 0;
	for (u8 j = 0; j < 50; ++j) {
		if (TFlagManager::getInstance()->getBlueCoinFlag(scNormalStageTable[9],
		                                                 j))
			++blueCoins;
	}
	mStageInfo[9].mBlueCoins = blueCoins;

	s16 secret = 0;
	if (TFlagManager::getInstance()->getBool(0x10056))
		secret = 1;
	if (TFlagManager::getInstance()->getBool(0x10058))
		secret += 1;

	mStageInfo[0].mShines = secret;
	total += secret;
	mStageInfo[1].mShines = TFlagManager::getInstance()->getFlag(0x40000) - total;

	changeBotStatus(-1);
	resetScore();
	mCursor[0]->getPane()->show();
	mCursor[1]->getPane()->show();
}

void TGuide::load(JSUMemoryInputStream& stream)
{
	unkC5 = 0;
	JDrama::TNameRef::load(stream);

	JKRMemArchive* archive = setup(gpMarDirector->unkD8);

	mScreen = new J2DSetScreen("guide_1.blo", archive);

	((J2DTextBox*)mScreen->search('a_ic'))->setFont(gpSystemFont);
	((J2DTextBox*)mScreen->search('a_tx'))->setFont(gpSystemFont);
	((J2DTextBox*)mScreen->search('b_ic'))->setFont(gpSystemFont);
	((J2DTextBox*)mScreen->search('b_tx'))->setFont(gpSystemFont);

	mStageNameText = nullptr;
	mStageNameText = (J2DTextBox*)mScreen->search('s_mn');
	SMSMakeTextBuffer(mStageNameText, 26);
	mStageNameText->setFont(gpSystemFont);

	for (int i = 0; i < 10; ++i) {
		char path[0xFF];
		char trash[0x10];
		snprintf(path, 0xFF, "/guide/timg/coin_number_%d.bti", i);
		mNumberTex[i] = new JUTTexture((const ResTIMG*)JKRGetResource(path));
	}

	unkF4 = mScreen->search('ss_i');
	for (int i = 0; i < 2; ++i)
		unkF8[i] = (J2DPicture*)mScreen->search('ss_1' + i);

	unk100 = mScreen->search('sq_i');
	for (int i = 0; i < 2; ++i)
		unk104[i] = mScreen->search('sq_1' + i);

	for (int i = 0; i < 3; ++i)
		unk10C[i] = (J2DPicture*)mScreen->search('sc_1' + i);

	unk118 = mScreen->search('sc_s');
	for (int i = 0; i < 2; ++i)
		unk11C[i] = (J2DPicture*)mScreen->search('sb_1' + i);

	JUTTexture* cursorTex = new JUTTexture(
	    (const ResTIMG*)JKRGetResource("/guide/timg/guide_cursor_2.bti"));
	for (int i = 0; i < 2; ++i) {
		mCursor[i]            = new TExPane(mScreen, 'cu_a' + i);
		J2DPicture* cursorPic = (J2DPicture*)mCursor[i]->getPane();
		cursorPic->insert(cursorTex, cursorPic->mTextureNum, 0.0f);
	}

	for (int i = 0; i < 13; ++i) {
		u32 tag = 0x3030 + i % 10 + i / 10 * 0x100;

		mStagePane[i] = mScreen->search(tag);
		unk1C0[i]     = new TExPane(mScreen, (tag << 16) + '\0\0_0');
		unk218[i]     = unk1C0[i]->getPane()->getBounds();
		unk378[i]     = new TExPane(mScreen, (tag << 16) + '\0\0_1');

		tag = tag << 16;
		((J2DTextBox*)mScreen->search(tag + '\0\0_3'))->setFont(gpSystemFont);
		((J2DTextBox*)mScreen->search(tag + '\0\0_5'))->setFont(gpSystemFont);
	}

	mStagePane[13] = mScreen->search(0x3230);
	unk1C0[13]     = new TExPane(mScreen, 'lwin');
	unk218[13]     = unk1C0[13]->getPane()->getBounds();
	unk378[13]     = new TExPane(mScreen, 'llin');

	for (int i = 0; i < 10; ++i)
		unk44C[i] = mScreen->search('pn00' + i);

	unk430 = mScreen->search('01mi');
	unk434 = mScreen->search('01_9')->getBounds();
	unk474 = JKRGetResource("/common/2d/stagename.bmg");

	unk134 = (J2DPicture*)mScreen->search(0x3130);
	JUTTexture* unk134Tex = new JUTTexture(
	    (const ResTIMG*)JKRGetResource("/guide/timg/guide_draw_sun_2.bti"));
	unk134->insert(unk134Tex, unk134->mTextureNum, 0.0f);

	unk138 = (J2DPicture*)mScreen->search(0x3133);
	JUTTexture* unk138Tex = new JUTTexture(
	    (const ResTIMG*)JKRGetResource("/guide/timg/guide_draw_ship_2.bti"));
	unk138->insert(unk138Tex, unk138->mTextureNum, 0.0f);

	unk13C = (J2DPicture*)mScreen->search(0x3136);
	JUTTexture* unk13CTex = new JUTTexture(
	    (const ResTIMG*)JKRGetResource("/guide/timg/guide_draw_palmtree_2.bti"));
	unk13C->insert(unk13CTex, unk13C->mTextureNum, 0.0f);

	unk140 = (J2DPicture*)mScreen->search(0x3137);
	JUTTexture* unk140Tex = new JUTTexture(
	    (const ResTIMG*)JKRGetResource("/guide/timg/guide_draw_palmtree_1.bti"));
	unk140->insert(unk140Tex, unk140->mTextureNum, 0.0f);

	unk144 = (J2DPicture*)mScreen->search(0x3138);
	JUTTexture* unk144Tex = new JUTTexture(
	    (const ResTIMG*)JKRGetResource("/guide/timg/guide_draw_fish_2.bti"));
	unk144->insert(unk144Tex, unk144->mTextureNum, 0.0f);

	unk148 = (J2DPicture*)mScreen->search(0x3131);
	unk14C = (J2DPicture*)mScreen->search(0x3134);
	unk150 = (J2DPicture*)mScreen->search(0x3135);

	unk154 = (J2DPicture*)mScreen->search(0x3132);
	unk154->setBasePosition(J2DBasePosition_4);
	unk158 = (J2DPicture*)mScreen->search(0x3139);
	unk158->setBasePosition(J2DBasePosition_4);

	void* messages = JKRGetResource("/guide/guidemess.bmg");
	for (int i = 0; i < 13; ++i) {
		u32 key = i / 10 * 0x1000000 + 0x30300000 + i % 10 * 0x10000
		          + '\0\0_0';

		J2DTextBox* title = (J2DTextBox*)mScreen->search(key + 3);
		SMSMakeTextBuffer(title, 30);
		title->setFont(gpSystemFont);
		strncpy(title->getStringPtr(), SMSGetMessageData(messages, i + 13),
		        30);

		J2DTextBox* body = (J2DTextBox*)mScreen->search(key + 5);
		SMSMakeTextBuffer(body, 0x200);
		body->setFont(gpSystemFont);
		strncpy(body->getStringPtr(), SMSGetMessageData(messages, i), 0x200);
	}

	unk478 = new TExPane(mScreen, 'mark');
	unk444 = new TBoundPane(mScreen, 0x3230);

	resetObjects();
	unkC5 = 1;
}

TGuide::TGuide(const char* name)
    : JDrama::TViewObj(name)
    , mState(8)
    , mScreen(nullptr)
    , unkC0(nullptr)
    , unkC4(0)
    , unkC5(0)
    , mAlpha(0xFF)
    , unk164(1)
    , unk480(-1)
{
}
