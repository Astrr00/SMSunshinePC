#ifndef GC2D_TALK_2D_2_HPP
#define GC2D_TALK_2D_2_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JUtility/JUTColor.hpp>

class JUTPoint;
class JUTTexture;
class J2DPane;
class J2DPicture;
class J2DScreen;
class J2DTextBox;
struct JMSMesgEntry;
class TBaseNPC;
class TBoundPane;
class TMarioGamePad;
class TMessageLoader;

class TTalk2D2;

extern TTalk2D2* gpTalk2D;

/// 2D conversation window: normal NPC speech boxes and the signboard variant
class TTalk2D2 : public JDrama::TViewObj {
public:
	TTalk2D2(const char* name = "<TTalk2D2>");

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void setMessageID(u32, u32);
	void forceCloseTalk();
	void openTalkWindow(TBaseNPC*);
	void makeBoxLine(s8, char*);
	bool openBoardWindow();
	bool openNormalWindow();
	void moveBoardWindow();
	void checkBoardControler();
	void moveTalkWindow();
	void checkControler();
	bool closeNormalWindow();
	bool eraseNormalWindow();
	bool eraseBoardWindow();
	void setupBoardTextBox(const void*, JMSMesgEntry*);
	void setupTextBox(const void*, JMSMesgEntry*);
	void setTagParam(JSUMemoryInputStream&, J2DTextBox&, int*, int*);
	void openWindow(s8, f32);

	static u32 cColorTable[6];

	int getTalkMode() const { return mTalkMode; }
	s8 getSelectedValue() const { return mSelection; }

public:
	/// An NPC and the message it says by default
	struct TNpcMessage {
		/* 0x0 */ TBaseNPC* mNpc;
		/* 0x4 */ u32 mMessageID;
	};

	// Signboard variant
	/* 0x10 */ J2DScreen* mBoardScreen;
	/* 0x14 */ TBoundPane* mBoardBound;
	/* 0x18 */ J2DTextBox* mBoardText;
	/* 0x1C */ J2DPane* unk1C;
	/* 0x20 */ J2DPane* unk20;
	/* 0x24 */ J2DPane* unk24;
	/* 0x28 */ bool mIsBoard;
	/* 0x29 */ u8 mBoardState;

	// Normal conversation window
	/* 0x2C */ J2DScreen* mScreen;
	/* 0x30 */ J2DPane* unk30[3];
	/* 0x3C */ J2DPicture* unk3C[3];
	/* 0x48 */ J2DPane* unk48[3];
	/* 0x54 */ J2DPane* unk54[3];
	/* 0x60 */ J2DPane* unk60[3];
	/* 0x6C */ J2DPane* unk6C[3];
	/* 0x78 */ J2DPane* unk78[3];
	/* 0x84 */ J2DPane* unk84[3];
	/* 0x90 */ J2DPane* mWindowPane;
	/* 0x94 */ f32 mCharAdvance;
	/* 0x98 */ char unk98[0x4];
	/* 0x9C */ J2DTextBox* unk9C[3 * 30];
	/* 0x204 */ J2DPane* unk204;
	/* 0x208 */ J2DTextBox* unk208;
	/* 0x20C */ J2DPane* unk20C[2];
	/* 0x214 */ s8 mSelection; // the line the player selected in a choice window
	/* 0x218 */ char* unk218[2];
	/* 0x220 */ s16 unk220;
	/* 0x222 */ s16 unk222;
	/* 0x224 */ u8 unk224[3];
	/* 0x228 */ int unk228[3];
	/* 0x234 */ f32 unk234[3];
	/* 0x240 */ char unk240[0x4];
	/* 0x244 */ JUTTexture* unk244;
	/* 0x248 */ u32 mTalkMode;
	/* 0x24C */ TMarioGamePad* mGamePad;
	/* 0x250 */ u8 unk250;
	/* 0x251 */ s8 unk251;
	/* 0x252 */ u8 unk252;
	/* 0x254 */ JMSMesgEntry* unk254;
	/* 0x258 */ TMessageLoader* unk258;
	/* 0x25C */ TMessageLoader* unk25C;
	/* 0x260 */ TMessageLoader* unk260;
	/* 0x264 */ u32 unk264;
	/* 0x268 */ char unk268[0x2];
	/* 0x26A */ u8 unk26A;
	/* 0x26B */ u8 unk26B;
	/* 0x26C */ u8 unk26C;
	/* 0x26D */ u8 unk26D;
	/* 0x270 */ int unk270;
	/* 0x274 */ int unk274;
	/* 0x278 */ int unk278;
	/* 0x27C */ JUtility::TColor unk27C;
	/* 0x280 */ u8 unk280;
	/* 0x281 */ u8 unk281[3 * 30];
	/* 0x2DC */ s16 unk2DC;
	/* 0x2DE */ u16 unk2DE;
	/* 0x2E0 */ TNpcMessage unk2E0[10];
	/* 0x330 */ s16 unk330;
	/* 0x332 */ s16 unk332;
	/* 0x334 */ s16 unk334;
	/* 0x338 */ f32 unk338;
	/* 0x33C */ f32 unk33C;
	/* 0x340 */ s16 unk340;
};

#endif
