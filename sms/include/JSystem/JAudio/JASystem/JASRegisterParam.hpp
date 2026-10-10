#ifndef JASREGISTERPARAM_HPP
#define JASREGISTERPARAM_HPP

#include <dolphin/types.h>

namespace JASystem {

class TRegisterParam {
public:
	TRegisterParam();

	void init();
	void inherit(const TRegisterParam& other);
	void setBankNumber(u8 bankNum);
	u8 getBankNumber() const;
	u8 getProgramNumber() const;

	void setPanPower(int i, u16 power) { mPanPower[i] = power; }
	u16 getPanPower(int i) const { return mPanPower[i]; }

	u16 getPanPowerBank() const { return mPanPower[0]; }
	u16 getPanPowerExt() const { return mPanPower[1]; }
	u16 getPanPowerOsc() const { return mPanPower[2]; }
	u16 getPanPowerParent() const { return mPanPower[3]; }
	u16 getPanPowerTrack() const { return mPanPower[4]; }

	u16 getFlag() const { return unk0[3]; }
	void setFlag(u16 flag) { unk0[3] = flag; }

	int getAddress(int i) const { return unk20[i]; }
	void setAddress(int i, u32 value) { unk20[i] = value; }

public:
	/* 0x00 */ u16 unk0[6];
	/* 0x0C */ u16 unkC;
	/* 0x0E */ u16 unkE;
	/* 0x10 */ u16 mPanPower[5];
	/* 0x1A */ u16 unk1A;
	/* 0x1C */ int unk1C;
	/* 0x20 */ int unk20[4];

#ifdef TARGET_PC
	// The sequence register file. The console addresses it as `u16 regs[id]` from the start of this struct
	// (lhz 0x280 + 2 * id of the track, TTrack::readRegDirect), 0x00-0x17 inside this struct (big-endian halves of
	// unk1C/unk20), 0x18-0x1F in the 16 bytes behind it (TTrack::unk2B0), and an id above 0x1F runs into mParent/mChildren
	// of the track. On the host the same ids map explicitly: 0-5 unk0, 6 unkC, 7 unkE, 8-12 mPanPower, 13 unk1A, 14/15 the
	// halves of unk1C, 16-23 the halves of unk20[] (high half = even id, as on the big-endian console), 24-31 mSpare.
	// An id above 0x1F that is not a special register is not a register on the console either (it corrupts the track):
	// it is reported once on stderr, a read gives 0 and a write is dropped. sAccessHook (test builds) sees every access.
	/* host */ u16 mSpare[8];
	u16 getReg(u8 id) const;
	void setReg(u8 id, u16 value);
	static void (*sAccessHook)(u8 id, bool write, bool inRange);
	static u32 sBadAccessCount;
#endif
};

} // namespace JASystem

#endif // JASREGISTERPARAM_HPP
