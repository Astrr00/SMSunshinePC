#include <GC2D/ConsoleStr.hpp>
#include <GC2D/BoundPane.hpp>
#include <GC2D/ExPane.hpp>
#include <GC2D/MessageUtil.hpp>
#include <System/Application.hpp>
#include <System/MarDirector.hpp>
#include <System/StageUtil.hpp>
#include <System/FlagManager.hpp>
#include <JSystem/J2D/J2DPicture.hpp>
#include <JSystem/J2D/J2DTextBox.hpp>
#include <JSystem/J2D/J2DScreen.hpp>
#include <JSystem/J2D/J2DOrthoGraph.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JParticle/JPAEmitterManager.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/JUtility/JUTResFont.hpp>
#include <dolphin/gx/GXCull.h>
#include <stdio.h>

JUTPoint TConsoleStr::cShineGetRight1(150, -50);
JUTPoint TConsoleStr::cShineGetLeft1(-21, 7);
JUTPoint TConsoleStr::cShineGetRight2(0, 0);
JUTPoint TConsoleStr::cShineGetLeft2(-21, 7);
JUTPoint TConsoleStr::cShineGetRight3(0, 0);
JUTPoint TConsoleStr::cShineGetLeft3(-200, 65);

TConsoleStr::TConsoleStr(const char* name)
    : JDrama::TViewObj(name)
    , unk10(nullptr)
    , unk14(nullptr)
    , unk20(0xB4)
    , unk24(0)
    , unk2A8(0)
    , unk2B8(0)
    , unk2BC(7)
{
}

f32 TConsoleStr::getWipeCloseTime() { return 30.0f / SMSGetVSyncTimesPerSec(); }

void TConsoleStr::load(JSUMemoryInputStream& stream)
{
	JKRArchive* arch = SMSSwitch2DArchive("guide", gArBkConsole);
	JDrama::TViewObj::load(stream);
	unk10 = new J2DSetScreen("big_tx_1.blo", arch);
	unk10->setCullBack(GX_CULL_BACK);
	unk14 = new J2DSetScreen("scenario_demo_1.blo", arch);
	unk14->setCullBack(GX_CULL_BACK);

	for (int i = 0; i < 3; ++i) {
		unk28[i] = new TBoundPane(unk10, 'go00' + i);
		unk28[i]->unk0->hide();
	}

	for (int i = 0; i < 9; ++i) {
		unk244[i] = new TBoundPane(unk10, 'sg00' + i);
	}

	for (int i = 0; i < 5; ++i) {
		unk268[i] = new TBoundPane(unk10, 'ms00' + i);
	}

	for (int i = 0; i < 5; ++i) {
		unk27C[i] = new TExPane(unk10, 're00' + i);
	}

	unk2A0[0] = (J2DTextBox*)unk14->search('\0map');
	SMSMakeTextBuffer(unk2A0[0], 0x80);
	unk2A0[1] = (J2DTextBox*)unk14->search('stry');
	SMSMakeTextBuffer(unk2A0[1], 0x80);

	for (int i = 0; i < 2; ++i) {
		unk290[i] = new TExPane(unk14, 'msk1' + i);
		unk2A0[i]->setFont(gpSystemFont);
	}

	unk298 = new TExPane(unk14, 'wp_l');
	unk29C = new TExPane(unk14, 'wp_r');

	u32 uVar1     = SMS_getShineStage(gpMarDirector->mMap);
	u32 uVar9     = TFlagManager::getInstance()->getFlag(0x40003);
	void* pvVar10 = JKRGetResource("/common/2d/stagename.bmg");
	unk2A0[0]->setString(SMSGetMessageData(pvVar10, uVar1));

	if (gpMarDirector->mMap != 15) {
		void* pvVar10 = JKRGetResource("/common/2d/scenarioname.bmg");

		s16 uVar2 = SMS_getShineID(uVar1, uVar9, false);

		const void* puVar15;
		if (pvVar10 == nullptr || uVar2 == -1)
			puVar15 = "";
		else
			puVar15 = SMSGetMessageData(pvVar10, SMS_getNormalStage(uVar2));

		snprintf(unk2A0[1]->getStringPtr(), 0x80, "%s", puVar15);
	}
}

void TConsoleStr::loadAfter()
{
	char trash[8];
	JDrama::TViewObj::loadAfter();
	unk2AC[0] = unk2AC[1] = unk2AC[2] = 0;
}

void TConsoleStr::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_MOVE) {
		if (gpMarDirector->mState != 5) {
			bool bVar6 = false;

			switch (unk2B8) {
			case 1:
				bVar6 = processGo(unk18);
				break;

			case 2:
				if (unk18 == unk20) {
					unk244[0]->setPanePosition(0x28, cShineGetRight1,
					                           cShineGetLeft1, cShineGetLeft1);
					unk244[0]->getPane()->show();
				} else if (unk18 > unk20) {
					bVar6 = processShineGet((unk18 - unk20) * 2.0f);
				}
				break;

			case 3:
				bVar6 = processMiss(unk18 * 2.0f);
				break;

			case 4:
				bVar6 = processScenario(unk18);
				break;

			case 5:
				bVar6 = processReady(unk18 * 2.0f);
				if (unk18 >= 60.0f) {
					if (unk18 == 60.0f) {
						unk28[0]->getPane()->hide();
						unk28[1]->getPane()->hide();
						unk28[2]->getPane()->hide();
						unk28[0]->getPane()->setAlpha(0xff);
						unk28[1]->getPane()->setAlpha(0xff);
						unk28[2]->getPane()->setAlpha(0xff);
					}
					bVar6 &= processGo(unk18 - 60.0f);
				}
				break;
			}

			if (unk2BC == 2) {
				bool uVar13 = true;

				for (int i = 0; i < 2; ++i) {
					uVar13 &= unk290[i]->update();
					unk2A0[i]->setAlpha(0xff
					                    - unk290[i]->getPane()->getAlpha());
				}

				if (uVar13) {
					unk2BC = 3;
					unk18  = 0.0f;
				}
			} else if (unk2BC == 8) {
				bool uVar13 = true;

				for (int i = 0; i < 2; ++i)
					uVar13 &= unk290[i]->update();

				if (uVar13) {
					unk2BC = 3;
					unk18  = 0.0f;
				}
			} else if (unk2BC == 5) {
				bool uVar13 = true;
				for (int i = 0; i < 2; ++i)
					uVar13 &= unk290[i]->update();
				if (uVar13) {
					unk2BC = 6;
					bVar6  = true;
					unk298->getPane()->hide();
					unk29C->getPane()->hide();
				}
			} else if (unk2BC == 3) {
				unk18 += 1.0f;
				if (unk18 > 60.0f) {
					unk18  = 0.0f;
					unk2BC = 4;
				}
			} else if (unk2BC == 1) {
				if (unk2A8 != 0)
					startCloseWipe(false);
			}

			if (bVar6)
				unk2B8 = 0;

			unk18 += 0.5f;
		}

		if (!unk2A9 && gpMarDirector->mState != 4) {
			for (int i = 0; i < 3; ++i) {
				if (unk2AC[i])
					unk2AC[i]->stopCreateParticle();
			}
			unk2A9 = true;
		}

		if (unk2A9 && gpMarDirector->mState == 4) {
			for (int i = 0; i < 3; ++i) {
				if (unk2AC[i])
					unk2AC[i]->playCreateParticle();
			}
			unk2A9 = false;
		}
	}

	if ((cue & CUE_DRAW) && unk2B8 != 0) {
		const JDrama::TRect& rect = graphics->getScissor();

		J2DOrthoGraph local_1a0(graphics->getViewport());
		local_1a0.setup2D();

		if (unk2B8 == 1 && unk18 > 60.0f) {
			for (int i = 0; i < 3; ++i) {
				TBoundPane* pane = unk28[i];

				int local_b0[3] = { 4, 10, 20 };

				u32 uVar13       = pane->getPane()->getAlpha();
				JUTRect local_a0 = pane->getPane()->getBounds();
				u8 trailAlpha = uVar13;

				for (int j = 0; j < 3; ++j) {
					int iVar9 = local_b0[j];
					if (unk34[22 * i + iVar9].x != 0) {
						trailAlpha = (u8)(0.7f * trailAlpha);
						unk28[i]->getPane()->setAlpha(trailAlpha);
						unk28[i]->getPane()->resize(
						    local_a0.getWidth() - iVar9 * 3,
						    local_a0.getHeight() - iVar9 * 3);

						JUTRect local_90 = unk28[i]->getPane()->getBounds();
						((J2DPicture*)unk28[i]->getPane())
						    ->draw(unk34[22 * i + iVar9].x,
						           unk34[22 * i + iVar9].y, local_90.getWidth(),
						           local_90.getHeight(), false, false, false);
					}
				}

				pane->getPane()->setAlpha(uVar13);
				pane->getPane()->resize(local_a0.getWidth(),
				                        local_a0.getHeight());
			}
		}

		local_1a0.setup2D();
		if (unk2B8 == 4) {
			unk14->draw(0, 0, &local_1a0);
		} else {
			unk10->draw(0, 0, &local_1a0);
		}

		local_1a0.setup2D();
		graphics->setScissor(rect);
	}
}

void TConsoleStr::startAppearReady()
{
	if (unk2B8 == 1)
		return;

	unk2B8 = 1;
	unk18  = 0.0f;
	unk2A9 = 0;
	unk28[0]->unk0->hide();
	unk28[1]->unk0->hide();
	unk28[2]->unk0->hide();
	unk28[0]->unk0->mAlpha = 0xff;
	unk28[1]->unk0->mAlpha = 0xff;
	unk28[2]->unk0->mAlpha = 0xff;
}

void TConsoleStr::startAppearGo()
{
	if (unk2B8 == 1)
		return;

	unk2B8 = 1;
	unk18  = 0.0f;
	unk2A9 = 0;
	unk28[0]->getPane()->hide();
	unk28[1]->getPane()->hide();
	unk28[2]->getPane()->hide();
	unk28[0]->getPane()->setAlpha(0xff);
	unk28[1]->getPane()->setAlpha(0xff);
	unk28[2]->getPane()->setAlpha(0xff);
}

void TConsoleStr::startAppearShineGet()
{
	if (unk2B8 != 0)
		return;

	unk2B8 = 2;
	unk18  = 0.0f;
	unk24  = 0;
}

void TConsoleStr::startAppearMiss()
{
	if (unk2B8 == 3)
		return;

	if (unk2B8 == 1) {
		unk28[0]->getPane()->hide();
		unk28[1]->getPane()->hide();
		unk28[2]->getPane()->hide();
	}

	unk2B8 = 3;
	unk18  = 0.0f;

	for (int i = 0; i < 5; ++i) {
		unk268[i]->getPane()->hide();
		unk268[i]->getPane()->setAlpha(0);
	}

	unk268[0]->setPanePosition(0x28, JUTPoint(0, -270), JUTPoint(0, -270),
	                           JUTPoint(0, 30));

	unk268[0]->getPane()->show();
}

void TConsoleStr::startAppearScenario()
{
	if (unk2B8 != 0)
		return;

	unk2B8 = 4;
	unk2BC = 0;
	unk18  = 0.0f;
	unk1C  = -200;

	for (int i = 0; i < 2; ++i) {
		unk2A0[i]->setAlpha(0);
		unk290[i]->getPane()->show();
	}

	unk290[0]->setPaneOffset(0x1E, 0.0f, 0.0f, 0.0f,
	                         -(unk290[0]->getInitialBounds().y2 + 1));
	unk290[1]->setPaneOffset(0x1E, 0.0f, 0.0f, 0.0f,
	                         465 - unk290[1]->getInitialBounds().y1);
}

#pragma dont_inline on
bool TConsoleStr::processReady(int param_1)
{
	bool result = false;

	for (int i = 0; i < 5; ++i) {
		if (param_1 == i * 10) {
			JUTRect local_d8 = unk27C[i]->getPane()->getBounds();
			int width = local_d8.getWidth();
			int height = local_d8.getHeight();
			unk27C[i]->setCenteredSize(0x1E, width, height, width + 80, height + 80);
			unk27C[i]->getPane()->show();
			unk27C[i]->getPane()->setAlpha(0);
		} else if (param_1 < i * 10 + 30) {
			unk27C[i]->update();
			u16 alpha = unk27C[i]->getPane()->getAlpha();
			alpha += 9;
			if (alpha > 0xff)
				alpha = 0xff;
			unk27C[i]->getPane()->setAlpha(alpha);
		} else if (param_1 >= i * 10 + 130) {
			if (param_1 == i * 10 + 130) {
				JUTRect local_e8 = unk27C[i]->getPane()->getBounds();
				int width = local_e8.getWidth();
				int height = local_e8.getHeight();
				unk27C[i]->setCenteredSize(0x1E, width - 20, height - 20, width, height);
			} else if (param_1 < i * 10 + 160) {
				unk27C[i]->update();
				s16 alpha = unk27C[i]->getPane()->getAlpha();
				alpha -= 9;
				if (alpha < 0)
					alpha = 0;
				unk27C[i]->getPane()->setAlpha(alpha);
			} else if (i == 4) {
				result = true;
			}
		}
	}

	return result;
}

extern JPAEmitterManager* gpEmitterManager4D2;

bool TConsoleStr::processGo(float param_1)
{
	bool result = false;
	if (param_1 < 90.0f) {
		for (int i = 0; i < 3; ++i) {
			if (param_1 == (float)(i * 10)) {
				unk28[i]->setPanePosition(0x28, JUTPoint(0, 60), JUTPoint(0, -40), JUTPoint(0, -40));
				unk28[i]->getPane()->show();
			}
		}
		for (int i = 0; i < 3; ++i) {
			JUTRect local = unk28[i]->getPane()->getBounds();
			if (unk28[i]->update() && !(unk28[i]->unk14.x1 == 0 && unk28[i]->unk14.y1 == 0)) {
				unk28[i]->setPanePosition(0x1E, JUTPoint(0, -40), JUTPoint(0, -40), JUTPoint(0, 0));
			}
		}
	} else if (param_1 >= 95.0f) {
		if (param_1 == 95.0f) {
			unk28[0]->setPanePosition(0x50, JUTPoint(0, 0), JUTPoint(-170, -180), JUTPoint(-340, -360));
			unk28[1]->setPanePosition(0x50, JUTPoint(0, 0), JUTPoint(0, -220), JUTPoint(0, -440));
			unk28[2]->setPanePosition(0x50, JUTPoint(0, 0), JUTPoint(160, -180), JUTPoint(320, -360));
			for (int i = 0; i < 3; ++i) {
				for (int j = 0; j < 16; ++j) {
					JUTRect r = unk28[i]->getPane()->getGlobalBounds();
					unk34[22 * i + j] = JUTPoint(r.x1, r.y1);
				}
				JUTRect bounds = unk28[i]->getPane()->getBounds();
				float w = (float)bounds.getWidth();
				float h = (float)bounds.getHeight();
				JGeometry::TVec3<f32> pos((float)bounds.x1 + 0.5f * w, (float)bounds.y1 + 0.5f * h, 0.0f);
				gpEmitterManager4D2->createEmitter(pos, 0x1FD, nullptr, nullptr);
				unk2AC[i] = gpEmitterManager4D2->unkC8[0][0];
			}
		} else if (param_1 < 175.0f) {
			bool even = ((int)param_1 % 2) == 0;
			for (int i = 0; i < 3; ++i) {
				J2DPane* pane = unk28[i]->getPane();
				int alpha = pane->getAlpha() - 4;
				if (alpha < 0) {
					alpha = 0;
				}
				JUTRect r = pane->getGlobalBounds();
				pane->setAlpha(alpha);
				float w = (float)r.getWidth();
				float h = (float)r.getHeight();
				pane->resize(r.getWidth() + 2, r.getHeight() + 2);
				unk2AC[i]->setGlobalTranslation((float)r.x1 + 0.5f * w, (float)r.y1 + 0.5f * h, 0.0f);
				unk28[i]->update();
				if (even) {
					for (int k = 14; k >= 0; --k) {
						unk34[22 * i + k + 1] = unk34[22 * i + k];
					}
					unk34[22 * i] = JUTPoint(r.x1, r.y1);
				}
			}
		} else if (param_1 == 175.0f) {
			for (int i = 0; i < 3; ++i) {
				JUTRect bounds = unk28[i]->getPane()->getBounds();
				unk28[i]->getPane()->resize(bounds.getWidth() - 80, bounds.getHeight() - 80);
			}
		} else {
			unk28[0]->getPane()->hide();
			if (unk2AC[0]) {
				unk2AC[0]->setStatus(1);
			}
			unk28[1]->getPane()->hide();
			if (unk2AC[1]) {
				unk2AC[1]->setStatus(1);
			}
			unk28[2]->getPane()->hide();
			if (unk2AC[2]) {
				unk2AC[2]->setStatus(1);
			}
			result = true;
		}
	}
	return result;
}

bool TConsoleStr::processShineGet(int param_1)
{
	bool result = true;

	for (int i = 0; i < 9; ++i) {
		if (param_1 == 6 * i) {
			unk244[i]->getPane()->show();
			unk244[i]->setPanePosition(0x28, cShineGetRight1, cShineGetLeft1,
			                           cShineGetLeft1);
			JUTRect local_74 = unk244[i]->getPane()->getBounds();
			JGeometry::TVec3<f32> local_80(
			    local_74.x1 + local_74.getWidth() * 0.5f,
			    local_74.y1 + local_74.getHeight() * 0.5f, 0.0f);
			gpEmitterManager4D2->createEmitter(local_80, 0x1FE, nullptr,
			                                   nullptr);
		}

		if (param_1 == i * 6 + 40) {
			unk244[i]->setPanePosition(0x14, cShineGetLeft2, cShineGetLeft2,
			                           cShineGetRight2);
		}

		if (param_1 == i * 6 + 60) {
			JUTRect local_94 = unk244[i]->getPane()->getBounds();
			JGeometry::TVec3<f32> local_80(
			    local_94.x1 + local_94.getWidth() * 0.5f,
			    local_94.y1 + local_94.getHeight() * 0.5f, 0.0f);
			gpEmitterManager4D2->createEmitter(local_80, 0x1FF, nullptr,
			                                   nullptr);
		}

		if (param_1 == i * 6 + 200) {
			unk244[i]->setPanePosition(0x28, cShineGetRight3, cShineGetRight3,
			                           cShineGetLeft3);
		}

		if (param_1 < i * 6 + 40) {
			u16 alpha = unk244[i]->getPane()->getAlpha() + 7;
			if (alpha > 0xff)
				alpha = 0xff;
			unk244[i]->getPane()->setAlpha(alpha);
		}

		if (param_1 > i * 6 + 200) {
			s16 alpha = unk244[i]->getPane()->getAlpha() - 7;
			if (alpha < 0)
				alpha = 0;
			unk244[i]->getPane()->setAlpha(alpha);
		}

		if (unk244[i]->update()) {
			if (param_1 > i * 6 + 280) {
				unk244[i]->getPane()->hide();
				result &= true;
			} else {
				result = 0;
			}
		} else {
			result = 0;
		}
	}

	return result;
}

bool TConsoleStr::processMiss(int param_1)
{
	bool result = true;

	for (int i = 0; i < 5; ++i) {
		if (param_1 == i * 10) {
			unk268[i]->getPane()->show();
			unk268[i]->setPanePosition(0x3C, JUTPoint(0, -270),
			                           JUTPoint(0, -270), JUTPoint(0, 30));
		}

		if (param_1 == i * 10 + 1) {
			JUTRect local_9c = unk268[i]->getPane()->getBounds();
			JGeometry::TVec3<f32> local_a8(
			    local_9c.x1 + local_9c.getWidth() * 0.5f,
			    local_9c.y1 + local_9c.getHeight() * 0.5f, 0.0f);
			gpEmitterManager4D2->createEmitter(local_a8, 0x1F9, nullptr,
			                                   nullptr);
		}

		if (param_1 == i * 10 + 60) {
			unk268[i]->getPane()->mRotation = 0.0f;
			unk268[i]->setPanePosition(0x28, JUTPoint(0, 30), JUTPoint(0, -80),
			                           JUTPoint(0, -80));
		}

		if (param_1 == i * 10 + 100) {
			unk268[i]->setPanePosition(0x28, JUTPoint(0, -80), JUTPoint(0, -80),
			                           JUTPoint(0, 0));
		}

		if (param_1 == i * 10 + 300) {
			unk268[i]->setPanePosition(0x1E, JUTPoint(0, 0), JUTPoint(0, 0),
			                           JUTPoint(0, 150));
		}

		if (param_1 < i * 10 + 60) {
			u16 alpha = unk268[i]->getPane()->getAlpha();
			alpha += 12;
			if (alpha > 0xff)
				alpha = 0xff;
			unk268[i]->getPane()->setAlpha(alpha);
		}

		if (unk268[i]->update()) {
			if (param_1 > i * 10 + 360) {
				unk268[i]->getPane()->hide();
				result &= true;
			} else {
				result = false;
			}
		} else {
			if (param_1 < i * 10 + 60) {
				unk268[i]->getPane()->mRotation = (i * 10 - param_1) * 6;
			}

			result = false;
		}
	}

	return result;
}
#pragma dont_inline off

bool TConsoleStr::processScenario(int)
{
	if (unk2BC > 0 && unk2BC != 7)
		return 0;

	bool uVar2 = true;
	uVar2 &= unk290[0]->update();
	uVar2 &= unk290[1]->update();

	unk1C += 2;

	if (unk1C > 355) {
		unk1C = 355;
		uVar2 &= true;
	} else {
		uVar2 = false;
	}

	int alpha = unk1C > 100 ? unk1C - 100 : 0;

	unk2A0[0]->setAlpha(alpha);
	unk2A0[1]->setAlpha(alpha);
	if (uVar2)
		unk2BC = 1;

	return false;
}

void TConsoleStr::startCloseWipe(bool param_1)
{
	if (param_1) {
		unk290[0]->getPane()->show();
		unk290[1]->getPane()->show();
		unk2A0[0]->hide();
		unk2A0[1]->hide();

		JUTRect bounds0 = unk290[0]->getPane()->getBounds();
		unk290[0]->setPaneSize(0x2D, bounds0.getWidth(), 224, bounds0.getWidth(),
		                       0);
		unk290[0]->setPaneAlpha(0x2D, 255, 0);

		s32 y1      = unk290[1]->getPane()->getBounds().y1;
		s32 width   = unk290[1]->getPane()->getBounds().getWidth();
		s32 offsetY = 465 - unk290[1]->getInitialBounds().y1;
		unk290[1]->setPaneOffset(0x2D, 0, 224 - y1, 0, offsetY);
		unk290[1]->setPaneSize(0x2D, width, offsetY + 224, width, 0);
		unk290[1]->setPaneAlpha(0x2D, 255, 0);

		unk2BC = 8;
		unk2B8 = 4;
		unk2A8 = 1;
		return;
	}
	if (unk2BC != 1) {
		unk2A8 = 1;
		return;
	}
	unk2BC = 2;

	JUTRect bounds0 = unk290[0]->getPane()->getBounds();
	unk290[0]->setPaneSize(0x2D, bounds0.getWidth(), 224, bounds0.getWidth(),
	                       bounds0.getHeight());
	unk290[0]->setPaneAlpha(0x2D, 255, unk290[0]->getPane()->getAlpha());

	s32 y1     = unk290[1]->getPane()->getBounds().y1;
	s32 width  = unk290[1]->getPane()->getBounds().getWidth();
	s32 height = unk290[1]->getPane()->getBounds().getHeight();
	unk290[1]->setPaneOffset(0x2D, 0, 224 - y1, 0, 0);
	unk290[1]->setPaneSize(0x2D, width, 240, width, height);
	unk290[1]->setPaneAlpha(0x2D, 255, unk290[1]->getPane()->getAlpha());
}

void TConsoleStr::startOpenWipe()
{
	unk2A8 = 0;
	unk2BC = 5;
	unk290[0]->getPane()->hide();
	unk298->getPane()->show();
	unk290[1]->getPane()->hide();
	unk29C->getPane()->show();

	// TODO: TExPane::setPaneAlpha is wrong

	JUTRect local_3c = unk298->getPane()->getBounds();
	unk298->setPaneOffset(0x1E, -local_3c.getWidth(), 0, 0, 0);
	unk298->setPaneAlpha(30, 100, 255);

	local_3c = unk29C->getPane()->getBounds();
	unk29C->setPaneOffset(0x1E, local_3c.getWidth(), 0, 0, 0);
	unk29C->setPaneAlpha(30, 100, 255);
}
