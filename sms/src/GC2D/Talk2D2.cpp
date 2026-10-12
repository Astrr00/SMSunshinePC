#include <GC2D/Talk2D2.hpp>
#include <Camera/Camera.hpp>
#include <GC2D/BoundPane.hpp>
#include <GC2D/GCConsole2.hpp>
#include <GC2D/MessageLoader.hpp>
#include <GC2D/MessageUtil.hpp>
#include <JSystem/J2D/J2DOrthoGraph.hpp>
#include <JSystem/J2D/J2DPicture.hpp>
#include <JSystem/J2D/J2DScreen.hpp>
#include <JSystem/J2D/J2DTextBox.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JGeometry/JGUtil.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <JSystem/JSupport/JSUMemoryOutputStream.hpp>
#include <JSystem/JUtility/JUTPoint.hpp>
#include <JSystem/JUtility/JUTRect.hpp>
#include <JSystem/JUtility/JUTResFont.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/ReinitGX.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MoveBG/MapObjHide.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <NPC/NpcBase.hpp>
#include <System/Application.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <System/MarioGamePad.hpp>
#include <dolphin/gx.h>
#include <M3DUtil/InfectiousStrings.hpp>
#include <math.h>
#include <stdio.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// Sound played when a message starts, indexed by the sound id byte of its BMG entry
static const u32 scTalkSoundList[135] = {
	0x8850, 0x8851, 0x8852, 0x8853, 0x8854, 0x8855,
	0x8856, 0x8857, 0x8858, 0x8859, 0x885A, 0x885B,
	0x885C, 0x885D, 0x885E, 0x885F, 0x8860, 0x8861,
	0x8862, 0x8863, 0x8866, 0x8867, 0x8868, 0x8869,
	0x886A, 0x886B, 0x886C, 0x886D, 0x886E, 0x886F,
	0x8870, 0x8871, 0x8872, 0x8873, 0x8874, 0x8875,
	0x8876, 0x8877, 0x8878, 0x8879, 0x887A, 0x887B,
	0x887C, 0x887D, 0x887E, 0x887F, 0x8880, 0x8881,
	0x8882, 0x8883, 0x8884, 0x8885, 0x8886, 0x8887,
	0x8888, 0x8889, 0x888A, 0x888B, 0x888C, 0x888D,
	0x888E, 0x888F, 0x8890, 0x8891, 0x8892, 0x8893,
	0x8894, 0x8895, 0x8896, 0xFFFFFFFF, 0x8899, 0x889A,
	0x889B, 0x889C, 0x889D, 0x889E, 0x889F, 0x88A0,
	0x88A1, 0x88A2, 0x88A3, 0x88C0, 0x88C1, 0x88C2,
	0x88C3, 0x88C4, 0x88C5, 0x88C6, 0x88C7, 0x88C8,
	0x88C9, 0x88CA, 0x88CB, 0x88CC, 0x88CD, 0x88CE,
	0x88CF, 0x88E5, 0x88E6, 0x88E7, 0x88E8, 0x88E9,
	0x88EA, 0x88EB, 0x88D0, 0x88EC, 0x88ED, 0x88D1,
	0x88EE, 0x88D2, 0x88EF, 0x88D3, 0x88D4, 0x88D5,
	0x88D6, 0x88D7, 0x88D8, 0x88D9, 0x88DA, 0x88DB,
	0x88DC, 0x88DD, 0x88DE, 0x88DF, 0x88E0, 0x88E1,
	0x88E2, 0x88E3, 0x88E4, 0x4849, 0x80010025, 0x483D,
	0x88A6, 0x8864, 0x8865,
};

u32 TTalk2D2::cColorTable[6] = {
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFB48CFF, 0x6EE6FFFF, 0xFFFF00FF, 0xAAFF50FF,
};

// fabricated
static inline void startTalkSE(u32 id)
{
	if (gpMSound->gateCheck(id))
		MSoundSESystem::MSoundSE::startSoundSystemSE(id, 0, nullptr, 0);
}

// Point of the quadratic curve p0 -> p1 -> p2 the characters of a line are set on
static inline void makeLine(f32* outX, f32* outY, f32 t, const JUTPoint& p0,
                            const JUTPoint& p1, const JUTPoint& p2)
{
	f32 u  = 1.0f - t;
	f32 b0 = u * u;
	f32 b1 = 2.0f * t * u;
	f32 b2 = t * t;
	*outX  = p0.x * b0 + p1.x * b1 + p2.x * b2;
	*outY  = p0.y * b0 + p1.y * b1 + p2.y * b2;
}

// Same curve; makeBoxLine's asm (0x80214A5C..) sums p2*b2 last, unfused: p2*b2 + (p0*b0 + p1*b1)
static inline void makeLineBox(f32* outX, f32* outY, f32 t, const JUTPoint& p0,
                               const JUTPoint& p1, const JUTPoint& p2)
{
	f32 u  = 1.0f - t;
	f32 b0 = u * u;
	f32 b1 = 2.0f * t * u;
	f32 b2 = t * t;
	f32 x2 = p2.x * b2;
	f32 y2 = p2.y * b2;
	*outX  = x2 + (p0.x * b0 + p1.x * b1);
	*outY  = y2 + (p0.y * b0 + p1.y * b1);
}

TTalk2D2::TTalk2D2(const char* name)
    : JDrama::TViewObj(name)
    , mIsBoard(false)
    , mScreen(nullptr)
    , mWindowPane(nullptr)
    , mCharAdvance(0.0f)
    , mSelection(-1)
    , unk220(26)
    , unk222(0)
    , mTalkMode(0)
    , mGamePad(nullptr)
    , unk250(1)
    , unk251(0)
    , unk252(0)
    , unk254(nullptr)
    , unk264(3)
    , unk26A(0)
    , unk26B(0)
    , unk26C(0)
    , unk26D(0)
    , unk270(1)
    , unk274(0)
    , unk278(0)
    , unk27C(0xFFFFFFFF)
    , unk280(0)
    , unk2DC(0)
    , unk330(0)
    , unk332(0)
    , unk334(0)
    , unk338(0.04f)
    , unk33C(1.0f)
    , unk340(100)
{
	gpTalk2D = this;
	for (int i = 0; i < 90; ++i)
		unk9C[i] = nullptr;
	for (int i = 0; i < 3; ++i) {
		unk224[i] = 0;
		unk6C[i]  = nullptr;
		unk78[i]  = nullptr;
		unk228[i] = 0;
	}
}

TTalk2D2* gpTalk2D;

void TTalk2D2::load(JSUMemoryInputStream& stream)
{
	JDrama::TViewObj::load(stream);

	JKRArchive* arch = (JKRArchive*)JKRFileLoader::getVolume("game_6");
	mScreen          = new J2DSetScreen("message_2.blo", arch);

	for (int i = 0; i < 3; ++i) {
		unk30[i] = mScreen->search('me_1' + i);
		unk3C[i] = (J2DPicture*)mScreen->search('bac1' + i);
		unk48[i] = mScreen->search('\0f_1' + i * 3);
		unk54[i] = mScreen->search('\0f_2' + i * 3);
		unk60[i] = mScreen->search('\0f_3' + i * 3);
		unk6C[i] = mScreen->search('cu_1' + i);
		unk78[i] = mScreen->search('cc_1' + i);
		unk84[i] = mScreen->search('cs_1' + i);
	}

	unk244 = new JUTTexture(
	    (ResTIMG*)JKRFileLoader::getGlbResource("/game_6/timg/message_back_1.bti"));
	mWindowPane = mScreen->search('me_0');

	unk258 = new TMessageLoader;
	unk258->loadMessageData("/scene/map/message.bmg");
	unk25C = new TMessageLoader;
	unk25C->loadMessageData("/common/2d/sys_message.bmg");

	unk204 = mScreen->search('me_4');
	unk208 = (J2DTextBox*)mScreen->search('slct');
	unk208->setFont(gpSystemFont);
	for (int i = 0; i < 2; ++i) {
		unk20C[i] = mScreen->search('sc_1' + i);
		unk218[i] = new char[0x11];
	}

	mBoardScreen = new J2DSetScreen("message_board_1.blo", arch);
	mBoardBound  = new TBoundPane(mBoardScreen, 'mb_0');
	mBoardText   = (J2DTextBox*)mBoardScreen->search('text');
	SMSMakeTextBuffer(mBoardText, 0x200);
	mBoardText->setFont(gpSystemFont);
	unk1C = mBoardScreen->search('cu_1');
	unk20 = mBoardScreen->search('cc_1');
	unk24 = mBoardScreen->search('cs_1');
}

void TTalk2D2::loadAfter()
{
	JDrama::TViewObj::loadAfter();

	JUTPoint p0(unk48[1]->getBounds().x1, unk48[1]->getBounds().y1);
	JUTPoint p1(0, unk54[1]->getBounds().y1 - unk220);
	JUTPoint p2(unk60[1]->getBounds().x2 - 10, unk60[1]->getBounds().y1);

	f32 length = 0.0f;
	f32 prevX;
	f32 prevY;
	makeLine(&prevX, &prevY, 0.0f, p0, p1, p2);
	for (f32 t = 0.01f; t <= 1.0f; t += 0.01f) {
		f32 x;
		f32 y;
		makeLine(&x, &y, t, p0, p1, p2);
		f32 dx = x - prevX;
		f32 dy = y - prevY;
		length += JGeometry::TUtil<f32>::sqrt(dx * dx + dy * dy);
		prevX = x;
		prevY = y;
	}
	mCharAdvance = 1.0f / length;

	for (int i = 0; i < 3; ++i) {
		unk48[i]->hide();
		unk54[i]->hide();
		unk60[i]->hide();
	}

	for (int i = 0; i < 90; ++i) {
		unk9C[i] = new J2DTextBox(0, JUTRect(0, 0, 20, 20),
		                          gpSystemFont->getResFont(), "あ", HBIND_LEFT,
		                          VBIND_CENTER);
		unk9C[i]->setBlackWhite(JUtility::TColor(0xffffff00),
		                        JUtility::TColor(0xffffffff));
		unk9C[i]->hide();
	}

	unk234[0] = 1.0f;
	unk6C[0]->hide();
	unk234[1] = 2.0f;
	unk6C[1]->hide();
	unk234[2] = 3.0f;
	unk6C[2]->hide();

	unk244->mWrapT = 1;
	unk244->mWrapS = 0;

	mWindowPane->move(0x189, 0x73);
	mWindowPane->mRotation = -18.0f;
	unk330                 = mWindowPane->mBounds.x1;
	unk332                 = mWindowPane->mBounds.y1;
	unk334                 = mWindowPane->mRotation;
	unk204->hide();

	char* buffer = new char[0x5e];
	for (int i = 0; i < 0x5d; ++i)
		buffer[i] = ' ';
	buffer[0x5d] = 0;
	unk208->setString(buffer);

	TMessageLoader* sysMessages = unk25C;
	void* sysData               = sysMessages->unk4;
	setupTextBox(sysData, sysMessages->getMessageEntry(3));
	unk260 = unk25C;

	const char* names[10] = {
		"空港沈みモンテ", "モンテ1", "", "", "", "", "", "", "", "",
	};
	int messageIDs[10] = { 0x26, 0x23, 0, 0, 0, 0, 0, 0, 0, 0 };
	for (int i = 0; i < 10; ++i) {
		unk2E0[i].mNpc = (TBaseNPC*)JDrama::TNameRefGen::search(names[i]);
		unk2E0[i].mMessageID = messageIDs[i];
	}
}

void TTalk2D2::setMessageID(u32 messageID, u32 param)
{
	char trash[0x28]; // unused stack slot (present in the retail frame)
	TBaseNPC* npc = gpMarDirector->unkA0;
	if (npc->checkActionFlag(TBaseNPC::NPC_ACTION_HAPPY)) {
		if (npc->isMonte()) {
			if (npc->isNormalMonteW() || npc->isSpecialMonteW()) {
				if (npc->isChild())
					unk264 = 0x2c;
				else
					unk264 = 0x27;
			} else {
				if (npc->isChild())
					unk264 = 0x29;
				else
					unk264 = 0x23;
			}
		} else if (npc->isMare()) {
			if (npc->isNormalMareW() || npc->isSpecialMareW()) {
				if (npc->isChild())
					unk264 = 0x2d;
				else
					unk264 = 0x28;
			} else {
				if (npc->isChild())
					unk264 = 0x2a;
				else
					unk264 = 0x24;
			}
		} else {
			if (npc->mActorType == 0x4000016)
				unk264 = 0x25;
			else if (npc->mActorType == 0x4000010)
				unk264 = 0x2b;
		}

		for (int i = 0; i < 10; ++i) {
			if (npc == unk2E0[i].mNpc) {
				unk264 = unk2E0[i].mMessageID;
				break;
			}
		}
	} else {
		unk264 = messageID;
	}

	if (npc->mActorType == 0x400001d)
		mIsBoard = true;
	else
		mIsBoard = false;

	unk270     = param;
	unk278     = 0;
	mSelection = -1;
	unk26A     = 0;
	unk27C.set(0xFFFFFFFF);
	unk280 = 0;
	unk26C = 0;
	unk26D = 0;
	unk254 = nullptr;
	mBoardState = 0;
	if (TFlagManager::smInstance->getFlag(0xa0001) == 0x100)
		unk340 = 0x20;
	else
		unk340 = 0x40;

	TMessageLoader* loader;
	if ((unk264 & 0xffff0000) == 0)
		loader = unk25C;
	else
		loader = unk258;

	if (loader->unk4) {
		JMSMesgEntry* entry = loader->getMessageEntry((u16)unk264);
		if (!entry) {
			unk264 = 4;
			loader = unk25C;
			entry  = loader->getMessageEntry((u16)unk264);
		}
		setupTextBox(loader->unk4, entry);
	} else {
		unk264 = 3;
		loader = unk25C;
		void* data = loader->unk4;
		JMSMesgEntry* entry = loader->getMessageEntry((u16)unk264);
		setupTextBox(data, entry);
	}
	unk260 = loader;
	unk2DC = 0;

	if (unk254) {
		int soundID = scTalkSoundList[(u8)unk254->unk8[0]];
		if (soundID != -1 && gpMSound->gateCheck(soundID)) {
			if (soundID & 0x80000000)
				MSBgm::startBGM(soundID);
			else
				MSoundSESystem::MSoundSE::startSoundSystemSE(soundID, 0,
				                                             nullptr, 0);
		}
	}

	if (mTalkMode == 1) {
		mTalkMode           = 3;
		mWindowPane->mAlpha = 0xff;
	}
	unk252 = 1;
}

void TTalk2D2::forceCloseTalk()
{
	char trash[0x18]; // unused stack slot (present in the retail frame)
	gpCamera->makeMtxForPrevTalk();
	if (mIsBoard)
		startTalkSE(0x4851);
	else
		gpMSound->talkModeOut();

	gpMarDirector->mConsole->startAppearTelop(false);
	if (mTalkMode == 1)
		mTalkMode = 0;
	else
		mTalkMode = 6;
}

void TTalk2D2::openTalkWindow(TBaseNPC* npc)
{
	char trash[0x60]; // unused stack slot (present in the retail frame)
	if (npc)
		gpCamera->makeMtxForTalk(npc);

	if (mIsBoard) {
		mBoardBound->setPanePosition(60, JUTPoint(0, -800), JUTPoint(0, 80),
		                             JUTPoint(0, 80));
		mBoardBound->update();
		unk1C->mAlpha = 0;
		mTalkMode     = 4;
	} else {
		mTalkMode = 2;
	}

	mWindowPane->mAlpha = 0xff;

	switch (gpCamera->mMode) {
	case CAMERA_MODE_TALK_B:
		unk330 = 0xa0;
		unk332 = 0x87;
		unk334 = 0x14;
		break;
	case CAMERA_MODE_TALK_A:
	default:
		unk330 = 0x184;
		unk332 = 0x73;
		unk334 = -0x12;
		break;
	}

	mWindowPane->move(unk330, unk332);
	mWindowPane->mRotation = unk334;
	unk250                 = 1;

	gpMarDirector->mConsole->startDisappearTelop();
	TGCConsole2* console = gpMarDirector->mConsole;
	console->startDisappearBalloon(console->unk3E0, true);
	gpMarDirector->mConsole->startDisappearMario();

	if (unk264 == 0x19) {
		startTalkSE(0x4848);
	} else if (mIsBoard) {
		startTalkSE(0x4819);
		gpMSound->talkModeIn(false);
	} else {
		gpMSound->talkModeIn(true);
	}

	SMSRumbleMgr->startPause();
}

void TTalk2D2::makeBoxLine(s8 line, char* text)
{
	int col       = 0;
	int textIndex = 0;

	JUTPoint p0(unk48[line]->getBounds().x1, unk48[line]->getBounds().y1);
	JUTPoint p1(0, unk54[line]->getBounds().y1 - unk220);
	JUTPoint p2(unk60[line]->getBounds().x2 - 10, unk60[line]->getBounds().y1);

	f32 t = 0.0f;
	f32 prevX;
	f32 prevY;
	{
		f32 u  = 1.0f - t;
		f32 b0 = u * u;
		f32 b1 = 2.0f * t * u;
		f32 b2 = t * t;
		prevX  = p2.x * b2 + (p0.x * b0 + p1.x * b1);
		prevY  = p2.y * b2 + (p0.y * b0 + p1.y * b1);
	}

	do {
		int idx = col + line * 30;
		if (text) {
			char* box = unk9C[idx]->getStringPtr();
			box[0]    = text[textIndex];
			if ((u8)text[textIndex] >= 0x80) {
				box[1] = text[textIndex + 1];
				textIndex += 2;
			} else {
				box[1] = 0;
				textIndex += 1;
			}
		}

		s8 ch    = unk9C[idx]->getStringPtr()[0];
		u16 code = ch;
		if (code >= 0x80)
			code = ch;
		if (code == 0)
			break;

		JUTFont::TWidth width;
		gpSystemFont->getWidthEntry(code, &width);
		if (TFlagManager::smInstance->getFlag(0xa0001) == 0x100)
			t = mCharAdvance * (f32)width.field_0x1 + t;
		else
			t = mCharAdvance * (0.7f * (f32)width.field_0x1 + 4.0f) + t;

		f32 x;
		f32 y;
		makeLineBox(&x, &y, t, p0, p1, p2);

		f32 angle = fabsf(atan2f(y - prevY, x - prevX));
		if (x > 0.0f)
			angle = angle * -1.0f;

		f32 sumX = prevX + x;
		f32 sumY = prevY + y;
		f32 hx   = 0.5f * sumX;
		f32 hy   = 0.5f * sumY;
		f32 c    = cosf(angle);
		f32 s    = sinf(angle);
		f32 cx   = -0.5f * sumX;
		f32 cy   = -0.5f * sumY;
		f32 posX = (hx + (prevX * c + prevY * -s)) + cx;
		f32 posY = (hy + (prevX * s + prevY * c)) + cy;

		int xi = (int)(posX + (posX > 0.0f ? 0.5f : -0.5f));
		int yi = (int)(posY + (posY > 0.0f ? 0.5f : -0.5f));
		unk9C[idx]->move((s16)xi, -37 - (s16)yi);
		unk9C[idx]->setBasePosition(J2DBasePosition_4);
		unk9C[idx]->mRotation = (180.0f * angle) / 3.1415927f;
		unk30[line]->mPaneTree.appendChild(&unk9C[idx]->mPaneTree);

		if (t > 1.1f) {
			if (unk228[line] > col)
				unk228[line] = col;
			break;
		}

		++col;
		makeLineBox(&prevX, &prevY, t, p0, p1, p2);
	} while (col < 30);
}

bool TTalk2D2::openBoardWindow()
{
	bool result = false;
	switch (mBoardState) {
	case 0:
		if (mBoardBound->update()) {
			mBoardBound->setPanePosition(25, JUTPoint(0, 80), JUTPoint(0, 80),
			                             JUTPoint(0, 0));
			++mBoardState;
		}
		break;

	case 1:
		if (mBoardBound->update()) {
			unk1C->setAlpha(0);
			result = true;
			unk1C->show();
			++mBoardState;
		}
		break;
	}
	return result;
}

bool TTalk2D2::openNormalWindow()
{
	bool result = false;
	if (unk2DE > 2 && mGamePad->checkMeaning(TMarioGamePad::MEANING_SELECT_A)) {
		unk26C = 1;
		if (TFlagManager::smInstance->getFlag(0xa0001) == 0x100)
			unk340 = 0x5a;
		else
			unk340 = 0x80;
	}

	bool allMoved = true;
	for (int i = 0; i <= unk274; ++i) {
		if (unk234[i] > -0.109f) {
			unk234[i] = unk234[i] - unk338;
			allMoved  = false;
		}

		if (unk234[i] < 1.0f && i != 0) {
			if (!unk9C[unk228[i - 1] + (i - 1) * 30]->isVisible())
				unk234[i] = 1.0f;
		}

		if (unk224[i] == 0 && unk234[i] < unk33C) {
			startTalkSE(0x4827);
			unk224[i] = 1;
			unk9C[i * 30]->show();
		}
	}

	if (allMoved) {
		for (int i = 0; i <= unk274; ++i)
			unk3C[i]->show();
		result = true;
	}

	for (int i = 0; i < 3; ++i) {
		if (unk224[i] != 0) {
			int base = unk224[i] + i * 30;
			while (unk224[i] <= unk228[i]) {
				J2DTextBox*& box = unk9C[base];
				if (box->isVisible()) {
					s16 alpha   = box->mAlpha + unk340;
					s16 clamped = alpha;
					if (clamped > 0xff)
						clamped = 0xff;
					box->mAlpha = clamped;
					unk2DE      = base;
					if (alpha < 0xff)
						break;
					++unk224[i];
					++base;
				} else {
					if (unk2DC <= 0) {
						if (unk26C)
							unk2DC = 0;
						else
							unk2DC = unk281[base];
						box->show();
						box->mAlpha = 0;
					} else {
						unk2DC = unk2DC - 1;
					}
					break;
				}
			}
		}
	}

	return result;
}

void TTalk2D2::moveBoardWindow()
{
	int alpha = unk1C->mAlpha;
	if (alpha < 0xff) {
		alpha += 4;
		if (alpha > 0xff) {
			unk24->mAlpha = 0;
			alpha         = 0xff;
			unk20->mAlpha = 0;
			unk26B        = 1;
		}
		unk1C->mAlpha = alpha;
		return;
	}

	J2DPane* arrow;
	if (unk26A)
		arrow = unk24;
	else
		arrow = unk20;

	int blink = arrow->mAlpha;
	if (unk26B) {
		blink += 4;
		if (blink > 0xff) {
			unk26B = 0;
			blink  = 0xff;
		}
	} else {
		blink -= 4;
		if (blink < 0) {
			unk26B = 1;
			blink  = 0;
		}
	}
	arrow->mAlpha = blink;
}

void TTalk2D2::checkBoardControler()
{
	char trash[0x38]; // unused stack slot (present in the retail frame)
	if (unk26A) {
		if (unk26D || mGamePad->checkFrameMeaning(TMarioGamePad::MEANING_SELECT_A)
		    || mGamePad->checkFrameMeaning(TMarioGamePad::MEANING_SELECT_B)) {
			startTalkSE(0x481c);
			mBoardBound->setPanePosition(60, JUTPoint(0, 0), JUTPoint(0, 0),
			                             JUTPoint(0, -600));
			if (unk270 & 1) {
				if (unk264 == 0x19) {
					startTalkSE(0x4851);
				} else if (mIsBoard) {
					startTalkSE(0x481a);
					gpMSound->talkModeOut();
				} else {
					gpMSound->talkModeOut();
				}
				gpCamera->makeMtxForPrevTalk();
				gpMarDirector->mConsole->startAppearTelop(false);
				SMSRumbleMgr->finishPause();
				unk252 = 0;
			}
			mTalkMode = 6;
		}
	} else if (mGamePad->checkFrameMeaning(TMarioGamePad::MEANING_SELECT_A
	                                       | TMarioGamePad::MEANING_SELECT_B)) {
		startTalkSE(0x481c);
		mTalkMode = 7;
	}
}

void TTalk2D2::moveTalkWindow()
{
	for (int line = 0; line < 3; line++) {
		int idx = unk224[line] + line * 30;
		if (unk224[line] != 0 && unk224[line] <= unk228[line]) {
			if (unk9C[idx]->isVisible()) {
				s16 alpha = unk9C[idx]->getAlpha();
				alpha += unk340;
				unk9C[idx]->setAlpha(alpha > 255 ? 255 : alpha);
				if (alpha >= 255)
					unk224[line]++;
			} else if (unk2DC <= 0) {
				unk2DC = unk281[idx];
				unk9C[idx]->show();
				unk9C[idx]->setAlpha(0);
			} else {
				unk2DC--;
			}
		}
	}

	if (unk224[unk274] < 30 && unk224[unk274] <= unk228[unk274])
		return;

	J2DPane* pane;
	J2DPane* fade;
	if (mSelection == -1) {
		pane = unk6C[unk274];
		if (unk26A != 0) {
			fade = unk84[unk274];
			unk78[unk274]->hide();
			unk84[unk274]->show();
		} else {
			fade = unk78[unk274];
			unk78[unk274]->show();
			unk84[unk274]->hide();
		}
	} else {
		pane = unk204;
		fade = unk20C[mSelection];
	}

	if (pane->isVisible()) {
		s16 alpha = pane->getAlpha();
		if (alpha < 255) {
			alpha += 0x10;
			if (alpha >= 255)
				alpha = 255;
			pane->setAlpha(alpha);
		}

		s16 fadeAlpha = fade->getAlpha();
		if (unk26B != 0) {
			fadeAlpha += 2;
			if (fadeAlpha > 255) {
				unk26B    = 0;
				fadeAlpha = 255;
			}
		} else {
			fadeAlpha -= 4;
			if (fadeAlpha < 60) {
				unk26B    = 1;
				fadeAlpha = 60;
			}
		}
		fade->setAlpha(fadeAlpha);

		u8 color = fadeAlpha;
		if (mSelection == 1) {
			snprintf(unk208->getStringPtr(), 0x5E,
			         "\033CC[ffffff60]\033GC[ffffff60]%s\033CC[ffffff%02x]\033GC[ffffff%02x]\n%s",
			         unk218[0], color, color, unk218[1]);
		} else if (mSelection == 0) {
			snprintf(unk208->getStringPtr(), 0x5E,
			         "\033CC[ffffff%02x]\033GC[ffffff%02x]%s\n\033CC[ffffff60]\033GC[ffffff60]%s",
			         color, color, unk218[0], unk218[1]);
		}
	} else {
		pane->show();
		pane->setAlpha(0);
		fade->setAlpha(255);
	}
}

void TTalk2D2::checkControler()
{
	char trash[0x60]; // unused stack slot (present in the retail frame)
	if (unk6C[unk274]->isVisible()) {
		if (unk26A) {
			if (unk26D || mGamePad->checkFrameMeaning(TMarioGamePad::MEANING_SELECT_A)
			    || mGamePad->checkFrameMeaning(TMarioGamePad::MEANING_SELECT_B)) {
				startTalkSE(0x481c);
				if (unk270 & 1) {
					if (unk264 == 0x19) {
						startTalkSE(0x4851);
					} else if (mIsBoard) {
						startTalkSE(0x481a);
						gpMSound->talkModeOut();
					} else {
						gpMSound->talkModeOut();
					}
					gpCamera->makeMtxForPrevTalk();
					gpMarDirector->mConsole->startAppearTelop(false);
					SMSRumbleMgr->finishPause();
					unk252 = 0;
				}
				mTalkMode = 6;
			}
		} else if (mGamePad->checkFrameMeaning(
		               TMarioGamePad::MEANING_SELECT_A
		               | TMarioGamePad::MEANING_SELECT_B)) {
			startTalkSE(0x481c);
			mTalkMode = 7;
		}
	} else if (unk204->isVisible()) {
		if (mGamePad->checkFrameMeaning(TMarioGamePad::MEANING_SELECT_UP)
		    && mSelection == 1) {
			startTalkSE(0x481e);
			mSelection        = 0;
			unk20C[1]->mAlpha = 0xfe;
			unk20C[1]->hide();
			unk20C[0]->show();
		} else if (mGamePad->checkFrameMeaning(
		               TMarioGamePad::MEANING_SELECT_DOWN)
		           && mSelection == 0) {
			startTalkSE(0x481e);
			mSelection        = 1;
			unk20C[0]->mAlpha = 0xfe;
			unk20C[0]->hide();
			unk20C[1]->show();
		} else if (unk26A) {
			if (mGamePad->checkFrameMeaning(TMarioGamePad::MEANING_SELECT_A
			                                | TMarioGamePad::MEANING_SELECT_B)) {
				if (unk270 & 1)
					gpCamera->makeMtxForPrevTalk();
				startTalkSE(0x481c);
				mTalkMode = 6;
			}
		} else if (mGamePad->checkFrameMeaning(
		               TMarioGamePad::MEANING_SELECT_A
		               | TMarioGamePad::MEANING_SELECT_B)) {
			startTalkSE(0x481c);
			mTalkMode = 7;
		}
	}
}

bool TTalk2D2::closeNormalWindow()
{
	s16 alpha = mWindowPane->getAlpha();
	alpha -= 0x10;
	bool result = false;
	if (alpha < 0) {
		alpha = 0;
		for (int i = 0; i < 3; ++i) {
			unk234[i] = i + 1.0f;
			unk3C[i]->hide();
			unk224[i] = 0;
			unk6C[i]->hide();
		}
		for (int i = 0; i < 90; ++i) {
			if (unk9C[i] != nullptr)
				unk9C[i]->hide();
		}
		if (unk204->isVisible())
			unk204->hide();
		result = true;
	}
	mWindowPane->setAlpha(alpha);
	if (unk204->isVisible())
		unk204->setAlpha(alpha);
	return result;
}

bool TTalk2D2::eraseNormalWindow()
{
	s16 alpha = mWindowPane->getAlpha();
	alpha -= 0x10;
	bool result = false;
	if (alpha < 0) {
		alpha = 0xFF;
		for (int i = 0; i < 3; ++i) {
			unk234[i] = i + 1.0f;
			unk3C[i]->hide();
			unk224[i] = 0;
			unk6C[i]->hide();
		}
		for (int i = 0; i < 90; ++i) {
			if (unk9C[i] != nullptr)
				unk9C[i]->hide();
		}
		setupTextBox(unk260->getMessageData(),
		             unk260->getMessageEntry(unk264 & 0xFFFF));
		unk26C = 0;
		if (TFlagManager::smInstance->getFlag(0xa0001) == 0x100)
			unk340 = 0x20;
		else
			unk340 = 0x40;
		unk27C.set(0xFFFFFFFF);
		unk2DE = 0;
		unk2DC = 0;
		result = true;
	}
	mWindowPane->setAlpha(alpha);
	return result;
}

bool TTalk2D2::eraseBoardWindow()
{
	s16 alpha = mBoardText->getAlpha();
	alpha -= 4;
	bool result = false;
	if (alpha < 0) {
		alpha = 0;
		setupTextBox(unk260->getMessageData(),
		             unk260->getMessageEntry(unk264 & 0xFFFF));
		unk27C.set(0xFFFFFFFF);
		unk2DE = 0;
		unk2DC = 0;
		result = true;
	}
	mBoardText->setAlpha(alpha);
	return result;
}

void TTalk2D2::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_MOVE) {
		switch (gpMarDirector->unk124) {
		case 2:
			switch (mTalkMode) {
			case 2: {
				CPolarSubCamera* camera = gpCamera;
				bool isTalkCamera       = false;
				if (camera->isTalkCameraSpecifyMode(camera->mMode)
				    && !camera->isNowInbetween())
					isTalkCamera = true;

				if (isTalkCamera ? true : false) {
					unk251    = 20;
					mTalkMode = 3;
				}
				break;
			}

			case 4: {
				bool opened = mIsBoard ? openBoardWindow() : openNormalWindow();
				if (opened)
					mTalkMode = 5;
				break;
			}

			case 5:
				if (mIsBoard) {
					moveBoardWindow();
					checkBoardControler();
				} else {
					moveTalkWindow();
					checkControler();
				}
				break;

			case 6: {
				bool closed;
				if (mIsBoard) {
					closed = false;
					if (mBoardBound->update())
						closed = true;
				} else {
					closed = closeNormalWindow();
				}

				if (closed) {
					if (unk270 & 1)
						mTalkMode = 0;
					else
						mTalkMode = 1;
				}
				break;
			}

			case 7: {
				bool erased = mIsBoard ? eraseBoardWindow() : eraseNormalWindow();
				if (erased) {
					if (mIsBoard)
						mTalkMode = 8;
					else
						mTalkMode = 4;
				}
				break;
			}

			case 8: {
				bool done = false;
				u16 alpha = mBoardText->mAlpha + 4;
				if (alpha > 0xff) {
					alpha = 0xff;
					done  = true;
				}
				mBoardText->mAlpha = alpha;
				if (done)
					mTalkMode = 5;
				break;
			}
			}
			break;
		}
	}

	if (cue & CUE_CALC_ANIM) {
		switch (gpMarDirector->unk124) {
		case 2:
			switch (mTalkMode) {
			case 2:
				break;

			case 3:
				--unk251;
				if (unk251 < 0) {
					unk2DE    = 0;
					mTalkMode = 4;
				}
				break;

			case 7: {
				s16 alpha = mWindowPane->mAlpha;
				alpha -= 16;
				if (alpha < 0) {
					unk234[0] = 1.0f;
					unk3C[0]->hide();
					unk224[0] = 0;
					unk6C[0]->hide();
					unk234[1] = 2.0f;
					unk3C[1]->hide();
					unk224[1] = 0;
					unk6C[1]->hide();
					unk234[2] = 3.0f;
					unk3C[2]->hide();
					unk224[2] = 0;
					unk6C[2]->hide();
					alpha = 0xff;
					for (int i = 0; i < 90; ++i) {
						if (unk9C[i])
							unk9C[i]->hide();
					}

					void* data          = unk260->unk4;
					JMSMesgEntry* entry = unk260->getMessageEntry((u16)unk264);
					setupTextBox(data, entry);
					unk26C = 0;
					if (TFlagManager::smInstance->getFlag(0xa0001) == 0x100)
						unk340 = 0x20;
					else
						unk340 = 0x40;
					unk27C.set(0xFFFFFFFF);
					unk27C.set(0xFFFFFFFF);
					unk2DE    = 0;
					unk2DC    = 0;
					mTalkMode = 4;
				}
				mWindowPane->mAlpha = alpha;
				break;
			}
			}
			break;
		}
	}

	if (cue & CUE_DRAW) {
		switch (gpMarDirector->unk124) {
		case 2:
			ReInitializeGX();
			SMS_DrawInit();
			J2DOrthoGraph ortho(graphics->getViewport());
			ortho.setup2D();

			if (unk250) {
				unk3C[0]->show();
				unk3C[1]->show();
				unk3C[2]->show();
				J2DPane* root = mScreen->search('ROOT');
				root->mAlpha  = 0;
				mScreen->draw(0, 0, &ortho);
				root->mAlpha = 0xff;
				ortho.setup2D();
				unk250 = 0;
				unk3C[0]->hide();
				unk3C[1]->hide();
				unk3C[2]->hide();
			}

			switch (mTalkMode) {
			case 4:
				for (s8 i = 0; i <= unk274; ++i)
					openWindow(i, unk234[i]);
			case 5:
			case 6:
			case 7:
				ortho.setup2D();
				if (mIsBoard) {
					mBoardScreen->draw(0, 0, &ortho);
				} else {
					mWindowPane->move(unk330, unk332);
					mWindowPane->mRotation = unk334;
					mScreen->draw(0, 0, &ortho);
				}
				break;
			}
			break;
		}
	}
}

void TTalk2D2::setupBoardTextBox(const void* data, JMSMesgEntry* entry)
{
	JSUMemoryInputStream in((const u8*)data + entry->unk0 + unk278, 0x400);
	JSUMemoryOutputStream out(mBoardText->getStringPtr(), 0x200);
	unk254 = entry;

	int lines = 0;
	while (lines < 6) {
		u8 c = in.readU8();
		switch (c) {
		case '\n': {
			u8 newline = '\n';
			out.write(&newline, 1);
			++lines;
			break;
		}

		case 0:
			unk26A = 1;
			lines  = 6;
			break;

		case 0x1a:
			break;

		default: {
			in.skip(-1);
			u8 first = in.readU8();
			u8 firstOut = first;
			out.write(&firstOut, 1);
			if (first >= 0x80) {
				u8 second = in.readU8();
				out.write(&second, 1);
			}
			break;
		}
		}
	}

	if (unk26A == 0) {
		s8 next;
		in.peek(&next, 1);
		if (next == 0) {
			unk26A = 1;
			in.skip(1);
			u8 terminator = 0;
			out.write(&terminator, 1);
		}
	}

	unk278 += in.getPosition();
}

void TTalk2D2::setupTextBox(const void* data, JMSMesgEntry* entry)
{
	if (mIsBoard) {
		setupBoardTextBox(data, entry);
		return;
	}

	u32 off = entry->unk0 + unk278;
	JSUMemoryInputStream stream((const u8*)data + off, 0x400);
	unk254  = entry;
	int col  = 0;
	int line = 0;
	unk274   = 0;
	unk2DE   = 0;

	while (line < 3) {
		char* text = unk9C[col + line * 30]->getStringPtr();
		s8 c = stream.readS8();
		switch (c) {
		case '\n':
			unk228[line] = col == 0 ? 0 : col - 1;
			col = 0;
			makeBoxLine(line, nullptr);
			++line;
			break;

		case 0:
			if (col != 0) {
				unk228[line] = col - 1;
				col          = 0;
				makeBoxLine(line, nullptr);
			}
			line   = 3;
			unk26A = 1;
			break;

		case 0x1a:
			setTagParam(stream, *unk9C[col + line * 30], &col, &line);
			break;

		default: {
			if (unk274 != line)
				unk274 = line;
			stream.skip(-1);
			text[0] = stream.readU8();
			if ((u8)text[0] >= 0x80) {
				text[1] = stream.readU8();
			} else {
				text[1] = 0;
			}

			int idx         = col + line * 30;
			J2DTextBox* box = unk9C[idx];
			box->mCharColor  = unk27C.get();
			box->mGradColor  = unk27C.get();
			unk9C[idx]->setBlackWhite(
			    JUtility::TColor((u32)unk27C & 0xffffff00),
			    JUtility::TColor((u32)unk27C));
			unk281[unk2DE] = unk280;
			unk2DE         = idx;
			++col;
			break;
		}
		}
	}

	if (unk26A == 0) {
		s8 next;
		stream.peek(&next, 1);
		if (next == 0)
			unk26A = 1;
	}

	if (mSelection != -1) {
		if (mSelection == 0) {
			snprintf(unk208->getStringPtr(), 0x5e,
			         "%s\n\033CC[7f7f7f]\033GC[7f7f7f]%s", unk218[0],
			         unk218[1]);
		} else {
			snprintf(unk208->getStringPtr(), 0x5e,
			         "\033CC[7f7f7f]\033GC[7f7f7f]%s\033CC[ffffff]\033GC[ffffff]\n%s",
			         unk218[0], unk218[1]);
		}
	}

	unk278 += stream.mPosition;
}

void TTalk2D2::setTagParam(JSUMemoryInputStream& stream, J2DTextBox& textBox,
                           int* col, int* line)
{
	u8 size   = stream.readU8();
	u8 group  = stream.readU8();
	u16 type  = stream.readU16();

	switch (group) {
	case 0:
		switch (type) {
		case 0:
			unk280 = stream.readU8();
			break;
		case 1:
			unk26D = 1;
			break;
		default:
			stream.skip(size - 5);
			break;
		}
		break;

	case 1:
		switch (type) {
		case 0: {
			if (mSelection == -1) {
				mSelection = 0;
				unk20C[1]->mAlpha = 0xfe;
				unk20C[1]->hide();
				unk20C[0]->show();
			}
			int len = size - 4 < 0x11 ? size - 4 : 0x11;
			snprintf(unk218[0], len, "%s", (char*)stream.getCurrent());
			stream.skip(size - 5);
			break;
		}
		case 1: {
			if (mSelection == -1) {
				mSelection = 1;
				unk20C[0]->mAlpha = 0xfe;
				unk20C[0]->hide();
				unk20C[1]->show();
			}
			int len = size - 4 < 0x11 ? size - 4 : 0x11;
			snprintf(unk218[1], len, "%s", (char*)stream.getCurrent());
			stream.skip(size - 5);
			break;
		}
		default:
			stream.skip(size - 5);
			break;
		}
		break;

	case 2:
		switch (type) {
		case 0:
		case 1:
		case 6: {
			int value;
			if (type == 0)
				value = TFlagManager::smInstance->getFlag(0x20003);
			else if (type == 1)
				value = TFlagManager::smInstance->getFlag(0x20002);
			else if (type == 6)
				value = TFlagManager::smInstance->getFlag(0x20014);

			if (value > 599999)
				value = 599999;
			if (value < 0)
				value = 0;

			u16 minutes = (value - value % 100) / 6000;
			int rest    = value - minutes * 6000;
			u16 seconds = (int)(0.01 * rest);
			u16 centis  = rest - seconds * 100;

			snprintf(unk9C[*col + *line * 30]->getStringPtr(), 2, "%d",
			         minutes / 10);
			snprintf(unk9C[*col + *line * 30 + 1]->getStringPtr(), 2, "%d",
			         minutes % 10);
			snprintf(unk9C[*col + *line * 30 + 2]->getStringPtr(), 2, ":");
			snprintf(unk9C[*col + *line * 30 + 3]->getStringPtr(), 2, "%d",
			         seconds / 10);
			snprintf(unk9C[*col + *line * 30 + 4]->getStringPtr(), 2, "%d",
			         seconds % 10);
			snprintf(unk9C[*col + *line * 30 + 5]->getStringPtr(), 2, ":");
			snprintf(unk9C[*col + *line * 30 + 6]->getStringPtr(), 2, "%d",
			         centis / 10);
			snprintf(unk9C[*col + *line * 30 + 7]->getStringPtr(), 2, "%d",
			         centis % 10);

			for (int i = 0; i < 8; ++i) {
				J2DTextBox* box = unk9C[i + *line * 30 + *col];
				box->mCharColor = unk27C.get();
				box->mGradColor = unk27C.get();
				unk9C[i + *line * 30 + *col]->setBlackWhite(
				    JUtility::TColor((u32)unk27C & 0xffffff00),
				    JUtility::TColor((u32)unk27C));
				unk281[i + *col + *line * 30] = unk280;
			}
			*col += 8;
			break;
		}

		case 2: {
			int count = (int)((f32)(TFlagManager::smInstance->getFlag(0x20004)
			                        + 99)
			                  * 0.01f);
			if (count < 10) {
				snprintf(unk9C[*col + *line * 30]->getStringPtr(), 2, "%d",
				         count);
				*col += 1;
			} else {
				snprintf(unk9C[*col + *line * 30]->getStringPtr(), 2, "%d",
				         count / 10);
				snprintf(unk9C[*col + *line * 30 + 1]->getStringPtr(), 2, "%d",
				         count % 10);
				*col += 2;
			}
			break;
		}

		case 3: {
			int total = TFlagManager::smInstance->getFlag(0x40001);
			int found = 0;
			for (int i = 0x46; i < 0x56; ++i) {
				if (TFlagManager::smInstance->getFlag(0x10000 + i))
					++found;
			}
			for (int i = 0x6c; i <= 0x73; ++i) {
				if (TFlagManager::smInstance->getFlag(0x10000 + i))
					++found;
			}
			total -= found * 10;
			if (total < 100) {
				snprintf(unk9C[*col + *line * 30]->getStringPtr(), 2, "%d",
				         total / 10);
				*col += 1;
			} else {
				snprintf(unk9C[*col + *line * 30]->getStringPtr(), 2, "%d",
				         total / 100);
				total -= total / 100 * 100;
				snprintf(unk9C[*col + *line * 30 + 1]->getStringPtr(), 2, "%d",
				         total / 10);
				*col += 2;
			}
			break;
		}

		case 4: {
			TFruitBasketEvent* basket = nullptr;
			int remaining;
			int fruit;
			switch (stream.readU8()) {
			case 0:
				basket = (TFruitBasketEvent*)JDrama::TNameRefGen::search(
				    "フルーツかごＡ");
				remaining = 3;
				fruit     = 0;
				break;
			case 1:
				basket = (TFruitBasketEvent*)JDrama::TNameRefGen::search(
				    "フルーツかごＢ");
				remaining = 3;
				fruit     = 4;
				break;
			case 2:
				basket = (TFruitBasketEvent*)JDrama::TNameRefGen::search(
				    "フルーツかごＣ");
				remaining = 3;
				fruit     = 3;
				break;
			case 3:
				basket = (TFruitBasketEvent*)JDrama::TNameRefGen::search(
				    "フルーツかごＤ");
				fruit     = 1;
				remaining = 3;
				break;
			}

			if (basket) {
				remaining -= basket->getFruitNum(fruit);
				if (remaining < 0 || remaining > 9)
					remaining = 0;
				snprintf(unk9C[*col + *line * 30]->getStringPtr(), 2, "%d",
				         remaining);
				snprintf(unk9C[*col + *line * 30 + 1]->getStringPtr(), 2, " ");
				*col += 2;
			}
			break;
		}
		}
		break;

	case 0xFF:
		switch (type) {
		case 0:
			unk27C.set(cColorTable[stream.readU8()]);
			break;
		}
		break;

	default:
		stream.skip(size - 5);
		break;
	}
}

void TTalk2D2::openWindow(s8 idx, f32 offset)
{
	Mtx mtxA;
	Mtx mtxB;
	f32 x = mWindowPane->getGlobalBounds().x1 + 5;
	f32 y = mWindowPane->getGlobalBounds().y1 + 5;

	MTXTrans(mtxA, -x, -y, 0.0f);
	MTXRotRad(mtxB, 'Z', DEG_TO_RAD(-mWindowPane->getRotation()));
	MTXConcat(mtxB, mtxA, mtxA);
	MTXTrans(mtxB, x, y, 0.0f);
	MTXConcat(mtxB, mtxA, mtxA);
	GXLoadPosMtxImm(mtxA, GX_PNMTX0);

	GXSetCullMode(GX_CULL_BACK);
	GXSetNumTexGens(2);
	GXSetNumTevStages(2);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S8, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_TRUE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL,
	              GX_DF_NONE, GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL,
	              GX_DF_NONE, GX_AF_NONE);
	GXSetChanAmbColor(GX_COLOR0A0, (GXColor) { 0xff, 0xff, 0xff, 0xff });

	JUTTexture* tex = unk3C[idx]->getTexture(0);
	tex->load(GX_TEXMAP1);
	unk244->load(GX_TEXMAP0);

	GXSetTevColor(GX_TEVREG0, JUtility::TColor(0x0000ff00));
	GXSetTevColor(GX_TEVREG1, JUtility::TColor(0x0000ffa0));
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C0, GX_CC_C1, GX_CC_TEXC, GX_CC_ZERO);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_A0, GX_CA_A1, GX_CA_TEXA, GX_CA_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	MTXTrans(mtxA, offset, 0.0f, 0.0f);
	GXLoadTexMtxImm(mtxA, GX_TEXMTX0, GX_MTX2x4);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0,
	                  GX_FALSE, GX_PTIDENTITY);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);

	GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_CPREV, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_APREV, GX_CA_TEXA,
	                GX_CA_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	MTXIdentity(mtxA);
	GXLoadTexMtxImm(mtxA, GX_TEXMTX1, GX_MTX2x4);
	GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX1,
	                  GX_FALSE, GX_PTIDENTITY);
	GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR_NULL);

	JUTRect rect(unk3C[idx]->getGlobalBounds());

	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition2f32(rect.x1, rect.y1);
	GXTexCoord2s8(0, 0);
	GXPosition2f32(rect.x2, rect.y1);
	GXTexCoord2s8(1, 0);
	GXPosition2f32(rect.x2, rect.y2);
	GXTexCoord2s8(1, 1);
	GXPosition2f32(rect.x1, rect.y2);
	GXTexCoord2s8(0, 1);
	GXEnd();
}
