#ifndef GC2D_GUIDE_HPP
#define GC2D_GUIDE_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JUtility/JUTRect.hpp>

class JKRMemArchive;
class J2DPane;
class J2DPicture;
class J2DScreen;
class J2DTextBox;
class JUTTexture;
class TBoundPane;
class TExPane;
class TMarioGamePad;

/**
 * @brief The guide screen: a map with one pane per stage. The player moves a
 * cursor over the panes to see that stage's shine, coin and blue coin counts,
 * and can open an info pane for it.
 */
class TGuide : public JDrama::TViewObj {
public:
	/// Per-stage progress shown on the guide screen.
	struct TStageInfo {
		/// Nonzero for the stage that only shows its blue coin count.
		/* 0x0 */ u8 unk0;
		/* 0x1 */ u8 mShines;
		/// Shines of the extra (index 1 and 2) shine table of the stage.
		/* 0x2 */ u8 mEtcShines;
		/* 0x4 */ u16 mCoins;
		/// Shine of index 0 of the extra shine table of the stage.
		/* 0x6 */ u8 mEtcShine0;
		/* 0x7 */ u8 mBlueCoins;
	};

	TGuide(const char* name = "<Guide>");

	virtual void load(JSUMemoryInputStream& stream);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void resetObjects();
	void resetScore();
	JKRMemArchive* setup(JKRMemArchive*);
	void startMoveCursor();
	void linkSelect();
	void changePattern(J2DPicture*, s16, u32);
	void mirrorPattern(J2DPicture*, s16, u32);
	void rotatePattern(J2DPicture*, s16, u32, s16);
	void shinePattern(TBoundPane*, s16, u32);
	int checkPoint(int, int);
	void changeBotStatus(int);
	void placeMario();
	void appearGuidePane(int);

public:
	/* 0x10 */ int mState;
	/* 0x14 */ TStageInfo mStageInfo[13];
	/* 0x7C */ char unk7C[0xBC - 0x7C];
	/* 0xBC */ J2DScreen* mScreen;
	/* 0xC0 */ TMarioGamePad* unkC0;
	/* 0xC4 */ u8 unkC4;
	/* 0xC5 */ u8 unkC5;
	/* 0xC8 */ JUTTexture* mNumberTex[10];
	/* 0xF0 */ u16 mFrame;
	/* 0xF4 */ J2DPane* unkF4;
	/* 0xF8 */ J2DPicture* unkF8[2];
	/* 0x100 */ J2DPane* unk100;
	/* 0x104 */ J2DPane* unk104[2];
	/* 0x10C */ J2DPicture* unk10C[3];
	/* 0x118 */ J2DPane* unk118;
	/* 0x11C */ J2DPicture* unk11C[2];
	/* 0x124 */ J2DTextBox* mStageNameText;
	/// The cursor and its shadow.
	/* 0x128 */ TExPane* mCursor[2];
	/* 0x130 */ u32 unk130;
	/* 0x134 */ J2DPicture* unk134;
	/* 0x138 */ J2DPicture* unk138;
	/* 0x13C */ J2DPicture* unk13C;
	/* 0x140 */ J2DPicture* unk140;
	/* 0x144 */ J2DPicture* unk144;
	/* 0x148 */ J2DPicture* unk148;
	/* 0x14C */ J2DPicture* unk14C;
	/* 0x150 */ J2DPicture* unk150;
	/* 0x154 */ J2DPicture* unk154;
	/* 0x158 */ J2DPicture* unk158;
	/// Direction of the pulse of mAlpha.
	/* 0x15C */ bool unk15C;
	/// Pulsing alpha of the stage panes.
	/* 0x160 */ int mAlpha;
	/// Direction of the pulse of the hovered pane's alpha.
	/* 0x164 */ u8 unk164;
	/// Only the first 14 entries of the arrays below are used.
	/* 0x168 */ J2DPane* mStagePane[22];
	/// Info pane shown when a stage is chosen, and its resting bounds.
	/* 0x1C0 */ TExPane* unk1C0[22];
	/* 0x218 */ JUTRect unk218[22];
	/* 0x378 */ TExPane* unk378[22];
	/// Mario's icon, one per stage.
	/* 0x3D0 */ J2DPane* mMarioIcon[10];
	/* 0x3F8 */ char unk3F8[0x424 - 0x3F8];
	/// The info panes that are currently open (copies of unk1C0 / unk378).
	/* 0x424 */ TExPane* unk424;
	/* 0x428 */ TExPane* unk428;
	/// The stage whose info is shown.
	/* 0x42C */ s16 mStage;
	/// Mario's marker on the map and the bounds of the map.
	/* 0x430 */ J2DPane* unk430;
	/* 0x434 */ JUTRect unk434;
	/* 0x444 */ TBoundPane* unk444;
	/* 0x448 */ J2DPane* unk448;
	/// Highlight panes of the stages.
	/* 0x44C */ J2DPane* unk44C[10];
	/// Stage name message data.
	/* 0x474 */ void* unk474;
	/// The "mark" pane and its alpha.
	/* 0x478 */ TExPane* unk478;
	/* 0x47C */ u8 unk47C;
	/// The stage that was hovered last frame, or -1.
	/* 0x480 */ int unk480;
	/* 0x484 */ char unk484[0x48C - 0x484];
	/* 0x48C */ JUTRect unk48C;
	/* 0x49C */ char unk49C[0x6F8 - 0x49C];
};

#endif
