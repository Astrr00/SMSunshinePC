#include <JSystem/JAudio/JASystem/JASRegisterParam.hpp>
#ifdef TARGET_PC
#include <stdio.h>
#endif

namespace JASystem {

#ifdef TARGET_PC
void (*TRegisterParam::sAccessHook)(u8, bool, bool) = nullptr;
u32 TRegisterParam::sBadAccessCount                = 0;

static void reportBadRegister(u8 id, bool write)
{
	if (TRegisterParam::sBadAccessCount++ == 0)
		fprintf(stderr,
		        "JASystem: sequence %s register 0x%02x, which is not a register (on the console this corrupts the "
		        "track); %s. Reported once.\n",
		        write ? "writes" : "reads", id, write ? "write dropped" : "reads 0");
}

u16 TRegisterParam::getReg(u8 id) const
{
	u16 value = 0;
	bool ok   = true;
	if (id < 6)
		value = unk0[id];
	else if (id < 8)
		value = id == 6 ? unkC : unkE;
	else if (id < 13)
		value = mPanPower[id - 8];
	else if (id == 13)
		value = unk1A;
	else if (id < 16)
		value = id == 14 ? (u16)((u32)unk1C >> 16) : (u16)unk1C;
	else if (id < 24)
		value = (id & 1) ? (u16)unk20[(id - 16) >> 1] : (u16)((u32)unk20[(id - 16) >> 1] >> 16);
	else if (id < 32)
		value = mSpare[id - 24];
	else
		ok = false;
	if (sAccessHook)
		sAccessHook(id, false, ok);
	if (!ok)
		reportBadRegister(id, false);
	return value;
}

void TRegisterParam::setReg(u8 id, u16 value)
{
	bool ok = true;
	if (id < 6)
		unk0[id] = value;
	else if (id == 6)
		unkC = value;
	else if (id == 7)
		unkE = value;
	else if (id < 13)
		mPanPower[id - 8] = value;
	else if (id == 13)
		unk1A = value;
	else if (id < 16)
		unk1C = id == 14 ? (int)((u32)value << 16 | ((u32)unk1C & 0xFFFF)) : (int)(((u32)unk1C & 0xFFFF0000) | value);
	else if (id < 24) {
		u32 w = (u32)unk20[(id - 16) >> 1];
		w     = (id & 1) ? (w & 0xFFFF0000) | value : (w & 0xFFFF) | (u32)value << 16;
		unk20[(id - 16) >> 1] = (int)w;
	} else if (id < 32)
		mSpare[id - 24] = value;
	else
		ok = false;
	if (sAccessHook)
		sAccessHook(id, true, ok);
	if (!ok)
		reportBadRegister(id, true);
}
#endif

TRegisterParam::TRegisterParam()
{
#ifdef TARGET_PC
	unk1C = 0;
	for (int i = 0; i < 8; ++i)
		mSpare[i] = 0;
#endif
	unk0[0]      = 0;
	unk0[1]      = 0;
	unk0[2]      = 0;
	unk0[3]      = 0;
	unk0[4]      = 0;
	unk0[5]      = 0;
	unkC         = 0;
	unkE         = 0;
	unk1A        = 0;
	mPanPower[0] = 0;
	mPanPower[1] = 0;
	mPanPower[2] = 0;
	mPanPower[3] = 0;
	mPanPower[4] = 0;

	for (int i = 0; i < 4; ++i)
		unk20[i] = 0;
}

void TRegisterParam::init()
{
	unk0[0]      = 0;
	unk0[1]      = 0;
	unk0[2]      = 0;
	unk0[3]      = 0;
	unk0[4]      = 0;
	unk0[5]      = 0;
	unkC         = 0xf0;
	unkE         = 0x0c;
	unk1A        = 0x40;
	mPanPower[0] = 0;
	mPanPower[1] = 1;
	mPanPower[2] = 1;
	mPanPower[3] = 0x7fff;
	mPanPower[4] = 0x4000;

	for (int i = 0; i < 4; ++i)
		unk20[i] = 0;
}

void TRegisterParam::inherit(const TRegisterParam& other)
{
	unk0[0] = 0;
	unk0[1] = 0;
	unk0[2] = 0;
	unk0[3] = 0;
	unk0[4] = 0;
	unk0[5] = 0;
	unkC    = other.unkC;
	unkE    = other.unkE;
	unk1A   = other.unk1A;

	for (int i = 0; i < 5; ++i)
		mPanPower[i] = other.mPanPower[i];

	for (int i = 0; i < 4; ++i)
		unk20[i] = 0;
}

void TRegisterParam::setBankNumber(u8 bankNum) { }

u8 TRegisterParam::getBankNumber() const { return unkC >> 8; }

u8 TRegisterParam::getProgramNumber() const { return unkC; }

} // namespace JASystem
