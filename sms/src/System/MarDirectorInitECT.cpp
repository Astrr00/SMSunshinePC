#include <System/MarDirector.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/PerformList.hpp>
#include <System/RenderModeObj.hpp>
#include <System/Resolution.hpp>
#include <Map/MapMirror.hpp>
#include <Map/PollutionManager.hpp>
#include <Map/BathWaterManager.hpp>
#include <Camera/LensGlow.hpp>
#include <Camera/LensFlare.hpp>
#include <JSystem/JDrama/JDRViewport.hpp>
#include <JSystem/JDrama/JDRCamera.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JDrama/JDREfbCtrl.hpp>

// rogue includes
#include <M3DUtil/InfectiousStrings.hpp>

#ifdef TARGET_PC
namespace {
// TARGET_PC: each pollution layer pass renders into an offscreen framebuffer of exactly the map's w x h (Aurora's
// logical EFB is only 448 lines and is scaled), so the R8 copy is 1:1 and GXSetCopyReadback can write it back into
// the CPU map (mPollutionMap) the way the GameCube copy writes main RAM. The offscreen target starts cleared to 0,
// the state the copy's clear (colour 0) leaves the EFB in on hardware.
class TGraffitoEfbTex : public JDrama::TEfbCtrlTex {
public:
	TGraffitoEfbTex(const char* name)
	    : JDrama::TEfbCtrlTex(name)
	{
	}

	virtual void perform(u32 cue, JDrama::TGraphics* graphics)
	{
		if (cue & CUE_DRAW_INIT)
			GXCreateFrameBuffer(mSize.mWidth, mSize.mHeight);
		JDrama::TEfbCtrlTex::perform(cue, graphics);
		if (cue & CUE_DRAW)
			GXRestoreFrameBuffer();
	}
};
} // namespace
#endif

void TMarDirector::initECTGft(
    TPerformList* param_1, TPerformList* param_2,
    JDrama::TViewObjPtrListT<JDrama::TViewObj>* perf_event_group,
    JDrama::TViewObjPtrListT<JDrama::TViewObj>* scene)
{
	char trash[0x38];
#ifdef TARGET_PC
	GXSetCopyReadback(nullptr, 0, 0); // stage change: drop the previous stage's map registrations
#endif
	if (gpPollution->getJointModelNum() == 0) {
		TBathWaterManager* bathtubWater = static_cast<TBathWaterManager*>(
		    JDrama::TNameRefGen::search("バスタブの水"));
		if (bathtubWater)
			param_2->push_back(bathtubWater->getPreprocessor(), CUE_DRAW);

		return;
	}

	JDrama::TViewObjPtrListT<JDrama::TViewObj>* graffitiGroup
	    = static_cast<JDrama::TViewObjPtrListT<JDrama::TViewObj>*>(
	        JDrama::TNameRefGen::search("落書きグループ"));
	JDrama::TViewObj* drawInit = static_cast<JDrama::TViewObj*>(
	    JDrama::TNameRefGen::search("SMS Draw Init"));

	JDrama::TEfbCtrlTex* graffitiEfbTex
	    = new JDrama::TEfbCtrlTex("graffito check");
	scene->insert(graffitiEfbTex);

	JDrama::TRect rect;
	rect.set(0, 0, 0x200, 0x200);
	graffitiEfbTex->setSrcRect(rect);
	param_1->push_back(graffitiEfbTex, CUE_DRAW_INIT);

	param_1->push_back(new JDrama::TViewport(rect, "graffito"), CUE_DRAW);
	param_1->push_back(
	    new JDrama::TOrthoProj(-1.0f, 1.0f, 0.0f, 0.0f, 512.0f, 512.0f),
	    CUE_SET_PROJECTION);
	param_1->push_back(drawInit, CUE_DRAW);
	param_1->push_back(graffitiGroup, CUE_UNK1000000);
	param_1->push_back(graffitiEfbTex, CUE_DRAW);

	for (int i = 0; i < gpPollution->getJointModelNum(); ++i) {
#ifdef TARGET_PC
		JDrama::TEfbCtrlTex* efbTex = new TGraffitoEfbTex("graffito");
#else
		JDrama::TEfbCtrlTex* efbTex = new JDrama::TEfbCtrlTex("graffito");
#endif
		scene->insert(efbTex);

		const ResTIMG* img = gpPollution->getLayer(i)->getPollutionImage();

		efbTex->setImgPtr((u8*)img + img->imageDataOffset); // asm 0x800EF4F0: img + offset, not &img
#ifdef TARGET_PC
		// On GameCube this copy writes the map in main RAM, which isPolluted/cleaned read on the next tick; Aurora
		// keeps copies on the GPU, so register the map for a readback of its exact w x h R8 copy.
		GXSetCopyReadback((u8*)img + img->imageDataOffset, img->width, img->height);
#endif
		JDrama::TSize size(img->width, img->height);
		efbTex->setDstSize(size);
		efbTex->setTexFmt(GX_CTF_R8);
		JDrama::TRect rect;
		rect.set(0, 0, size.mWidth, size.mHeight);
		efbTex->setSrcRect(rect);

		param_2->push_back(efbTex, CUE_DRAW_INIT);
		param_2->push_back(new JDrama::TViewport(rect, "graffito"), CUE_DRAW);
		param_2->push_back(new JDrama::TOrthoProj(-1.0f, 1.0f, 0.0f, 0.0f,
		                                          img->width, img->height),
		                   CUE_SET_PROJECTION);
		param_2->push_back(drawInit, CUE_DRAW); // asm 0x800EF638: retail pushes r29 (param_2)
		param_2->push_back(graffitiGroup, (i << CUE_OFFSET_POLLUTION_LAYER) // asm 0x800EF65C: r29
		                                      | CUE_SEMITRANSPARENT_PRIO_2
		                                      | CUE_DRAW);
		param_2->push_back(efbTex, CUE_DRAW); // asm 0x800EF66C: r29
	}
}

JDrama::TViewObj* TMarDirector::initECTMir(
    TPerformList* param_1,
    JDrama::TViewObjPtrListT<JDrama::TViewObj, JDrama::TViewObj>* param_2)
{
	JDrama::TEfbCtrlTex* mirrorTex = static_cast<JDrama::TEfbCtrlTex*>(
	    JDrama::TNameRefGen::search("鏡描画ステージ"));

	mirrorTex->unk20.set(0x228);
	mirrorTex->mVFilter = SMSVFilter_flicker;

	TMirrorCamera* mirrorCam
	    = static_cast<TMirrorCamera*>(JDrama::TNameRefGen::search("鏡カメラ"));

	GXTexObj& obj = mirrorCam->unk60;
	char trash[0x18];
	mirrorTex->setTexAttb(obj);
	mirrorTex->setSrcRect(
	    JDrama::TRect(0, 0, GXGetTexObjWidth(&obj), GXGetTexObjHeight(&obj)));

	return mirrorTex;
}

extern void marker();

void TMarDirector::initECDisp(
    TPerformList* param_1,
    JDrama::TViewObjPtrListT<JDrama::TViewObj, JDrama::TViewObj>* param_2, // asm: r4 never read
    JDrama::TViewObjPtrListT<JDrama::TViewObj, JDrama::TViewObj>* param_3)
{
	JDrama::TEfbCtrlDisp* stageDisp = new JDrama::TEfbCtrlDisp("stageDisp");
	stageDisp->JDrama::TEfbCtrl::setSrcRect(JDrama::TRect(
	    0, 0, (u16)SMSGetGameRenderWidth(), (u16)SMSGetGameRenderHeight()));
	param_3->insert(stageDisp);

	JDrama::TViewObj* composite3
	    = static_cast<JDrama::TViewObj*>(JDrama::TNameRefGen::search("合成3"));
	JDrama::TViewObj* specularSheen = static_cast<JDrama::TViewObj*>(
	    JDrama::TNameRefGen::search("スペキュラシーン"));

	TLensGlow* lensGlow       = nullptr;
	TLensFlare* lensFlare     = nullptr;
	JDrama::TOrthoProj* ortho = nullptr;

	JDrama::TViewObj* sunModel = static_cast<JDrama::TViewObj*>(
	    JDrama::TNameRefGen::search("太陽モデル"));

	if (sunModel) {
		lensGlow = new TLensGlow(false, "太陽遮蔽物グロー"); // asm 800EE5C8: li r4, 0 (sun volume)
		param_3->insert(lensGlow);
		lensFlare = new TLensFlare("レンズフレア");
		param_3->insert(lensFlare);
	} else {
		sunModel = static_cast<JDrama::TViewObj*>(
		    JDrama::TNameRefGen::search("夕日モデル"));
		if (sunModel) {
			lensGlow = new TLensGlow(true, "太陽遮蔽物グロー");
			param_3->insert(lensGlow);
			lensFlare = new TLensFlare("レンズフレア");
			param_3->insert(lensFlare);
		}
	}

	if (specularSheen || lensFlare || lensGlow) {
		f32 w = (u16)SMSGetGameRenderWidth() / 2;
		f32 h = (u16)SMSGetGameRenderHeight() / 2;
		ortho = new JDrama::TOrthoProj(-1.0f, 1.0f, -w, h, w, -h);
	}

	// asm 0x800EE920-0x800EE9FC: +0x38 = Width (f30), +0x3C = Height (f31)
	JDrama::TOrthoProj* ortho2 = new JDrama::TOrthoProj(
	    10.0f, 300000.0f, 0.0f, 0.0f, (u16)SMSGetGameRenderWidth(),
	    (u16)SMSGetGameRenderHeight());

	param_1->push_back(stageDisp, CUE_DRAW_INIT);
	param_1->push_back(
	    new JDrama::TViewport(SMSGetRederRect_Game(), "Screen 2D"), CUE_DRAW);
	param_1->push_back(ortho2, CUE_SET_PROJECTION);
	param_1->push_back(composite3, CUE_DRAW);

	JDrama::TViewObj* setViewMtx = static_cast<JDrama::TViewObj*>(
	    JDrama::TNameRefGen::search("J3D System Set View Mtx"));
	JDrama::TViewObj* drawInit = static_cast<JDrama::TViewObj*>(
	    JDrama::TNameRefGen::search("SMS Draw Init"));
	JDrama::TDrawBufObj* drawBufLensFlare = static_cast<JDrama::TDrawBufObj*>(
	    JDrama::TNameRefGen::search("DrawBuf LensFlare"));
	JDrama::TCamera* camera1 = static_cast<JDrama::TCamera*>(
	    JDrama::TNameRefGen::search("camera 1"));

	if (specularSheen || lensFlare || lensGlow) {
		param_1->push_back(ortho, CUE_SET_PROJECTION);
		param_1->push_back(setViewMtx, CUE_CALC_VIEW);
		param_1->push_back(drawBufLensFlare,
		                   CUE_SET_DRAW_BUFFER | CUE_DRAW_INIT);
		if (specularSheen)
			param_1->push_back(specularSheen, CUE_CALC_VIEW | CUE_ENTRY);
		if (lensGlow)
			param_1->push_back(lensGlow, CUE_CALC_VIEW | CUE_ENTRY);
		param_1->push_back(drawInit, CUE_DRAW);
		param_1->push_back(drawBufLensFlare, CUE_DRAW);
		if (lensFlare) {
			param_1->push_back(camera1, CUE_SET_PROJECTION);
			param_1->push_back(setViewMtx, CUE_CALC_VIEW);
			param_1->push_back(drawBufLensFlare, CUE_DRAW_INIT);
			param_1->push_back(lensFlare, CUE_CALC_VIEW | CUE_ENTRY);
			param_1->push_back(drawInit, CUE_DRAW);
			param_1->push_back(drawBufLensFlare, CUE_DRAW);
		}
	}

	JDrama::TCamera* drawBufChrOpa = static_cast<JDrama::TCamera*>(
	    JDrama::TNameRefGen::search("DrawBuf ChrOpa"));
	JDrama::TCamera* drawBufChrXlu = static_cast<JDrama::TCamera*>(
	    JDrama::TNameRefGen::search("DrawBuf ChrXlu"));

	param_1->push_back(camera1, CUE_SET_PROJECTION);
	param_1->push_back(setViewMtx, CUE_CALC_VIEW);
	param_1->push_back(drawBufChrOpa, CUE_SET_DRAW_BUFFER | CUE_DRAW_INIT);
	param_1->push_back(drawBufChrXlu, CUE_SET_DRAW_BUFFER | CUE_DRAW_INIT);
	param_1->push_back("会話カーソル", CUE_CALC_VIEW | CUE_ENTRY);
	param_1->push_back("ターゲット矢印", CUE_CALC_VIEW | CUE_ENTRY);
	param_1->push_back(drawInit, CUE_DRAW);
	param_1->push_back(drawBufChrOpa, CUE_DRAW);
	param_1->push_back(drawBufChrXlu, CUE_DRAW);

	// asm 0x800EED20-0x800EEE08: +0x38 = Width (f31), +0x3C = Height (f30)
	JDrama::TOrthoProj* ortho3 = new JDrama::TOrthoProj(
	    -1.0f, 1.0f, 0.0f, 0.0f, (u16)SMSGetGameRenderWidth(),
	    (u16)SMSGetGameRenderHeight());
	param_1->push_back(ortho3, CUE_SET_PROJECTION);

	JDrama::TViewObj* group2D2 = static_cast<JDrama::TViewObj*>(
	    JDrama::TNameRefGen::search("Group 2D 2"));
	param_1->push_back(group2D2, CUE_DRAW);

#ifdef TARGET_PC
	// Widescreen: 1 unit stays 16/15 px as at 4:3 and the 600-unit layout is centred in the wide viewport
	// (left edge -bx px, right edge W - bx px); at 4:3 (bx 0, W 640) this is exactly 0 / 600.
	const f32 uiOffsetX = SMSGetUIOffsetX();
	JDrama::TOrthoProj* ortho4 = new JDrama::TOrthoProj(
	    -500.0f, 500.0f, 0.0f - uiOffsetX * 0.9375f, 16.0f,
	    (SMSGetGameRenderWidth() - uiOffsetX) * 0.9375f, 464.0f);
#else
	JDrama::TOrthoProj* ortho4
	    = new JDrama::TOrthoProj(-500.0f, 500.0f, 0.0f, 16.0f, 600.0f, 464.0f);
#endif
	param_1->push_back(ortho4, CUE_SET_PROJECTION);

	JDrama::TViewObj* group2D = static_cast<JDrama::TViewObj*>(
	    JDrama::TNameRefGen::search("Group 2D"));
	param_1->push_back(group2D, CUE_DRAW);

#ifdef TARGET_PC
	// Widescreen: the guide map keeps the pre-HudFix fixed ortho (stretched over the full width, as before HudFix)
	JDrama::TOrthoProj* guideOrtho
	    = new JDrama::TOrthoProj(-500.0f, 500.0f, 0.0f, 16.0f, 600.0f, 464.0f);
	param_1->push_back(guideOrtho, CUE_SET_PROJECTION);
#else
	param_1->push_back(ortho4, CUE_SET_PROJECTION);
#endif
	JDrama::TViewObj* guide
	    = static_cast<JDrama::TViewObj*>(JDrama::TNameRefGen::search("Guide"));
	param_1->push_back(guide, CUE_DRAW);
	param_1->push_back(stageDisp, CUE_DRAW);
}

extern JPAEmitterManager* gpEmitterManager4D2;

void TMarDirector::setupPerformList_console()
{
	char trash[0x10];
	JDrama::TViewObjPtrListT<JDrama::TViewObj>* list
	    = static_cast<JDrama::TViewObjPtrListT<JDrama::TViewObj>*>(
	        JDrama::TNameRefGen::search("Group 2D"));

	list->insert(new TEmitterViewObj(gpEmitterManager4D2));

	unk30->push_back(list, CUE_MOVE | CUE_CALC_ANIM);
	unk30->push_back("Group 2D 2", CUE_MOVE | CUE_CALC_ANIM);
	unk30->push_back("Guide", CUE_MOVE | CUE_CALC_ANIM);
}
