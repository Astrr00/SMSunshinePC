#include <MarioUtil/PacketUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DShape.hpp>
#include <JSystem/J3D/J3DGraphBase/Blocks/J3DPEBlocks.hpp>

static void FifoSetChanMatColor(GXChannelID chan, GXColor color)
{
	// clang-format off
	u32 reg =
		(u32)color.r << 24 |
		(u32)color.g << 16 |
		(u32)color.b << 8 |
		color.a;
	// clang-format on
	GXWGFifo.u8  = GX_LOAD_XF_REG;
	GXWGFifo.u16 = 0;
	GXWGFifo.u16 = 0x100C + (chan & 1);
	GXWGFifo.u32 = reg;
}

static void FifoSetTevColorS10(GXTevRegID id, GXColorS10 color)
{
	// clang-format off
	u32 ra =
		(u32)(color.a & 0x7FF) << 12 |
		(color.r & 0x7FF) |
		(u32)(0xE0 + id * 2) << 24;
	u32 bg =
		(u32)(color.g & 0x7FF) << 12 |
		(color.b & 0x7FF) |
		(u32)(0xE1 + id * 2) << 24;
	// clang-format on
	// retail: RA once, then BG three times (asm 0x800D1200)
	GXWGFifo.u8  = GX_LOAD_BP_REG;
	GXWGFifo.u32 = ra;
	GXWGFifo.u8  = GX_LOAD_BP_REG;
	GXWGFifo.u32 = bg;
	GXWGFifo.u8  = GX_LOAD_BP_REG;
	GXWGFifo.u32 = bg;
	GXWGFifo.u8  = GX_LOAD_BP_REG;
	GXWGFifo.u32 = bg;
}

static void FifoSetTevKColor(GXTevKColorID id, GXColor color)
{
	// clang-format off
	u32 regRA =
		(u32)(0xE0 + id * 2) << 24 |
		0x8 << 20 |
		(u32)color.a << 12 |
		color.r;
	u32 regBG =
		(u32)(0xE1 + id * 2) << 24 |
		0x8 << 20 |
		(u32)color.g << 12 |
		color.b;
	// clang-format on

	GXWGFifo.u8  = GX_LOAD_BP_REG;
	GXWGFifo.u32 = regRA;
	GXWGFifo.u8  = GX_LOAD_BP_REG;
	GXWGFifo.u32 = regBG;
}

static void FifoSetFogRangeAdj(u8 enable, u16 center, GXFogAdjTable* table)
{
	if (enable) {
		for (int i = 0; i < 10; i += 2) {
			// clang-format off
			u32 reg =
				(0xE9 + (i / 2)) << 24 |
				table->r[i + 1] << 12 |
				table->r[i];
			// clang-format on
			GXWGFifo.u8  = GX_LOAD_BP_REG;
			GXWGFifo.u32 = reg;
		}
	}
	// clang-format off
	u32 reg =
		0xE8 << 24 |
		(center + 342) |
		enable << 10;
	// clang-format on
	GXWGFifo.u8  = GX_LOAD_BP_REG;
	GXWGFifo.u32 = reg;
}

static void FifoSetFog(GXFogType type, float startz, float endz, float nearz,
                       float farz, GXColor color)
{
	float A;
	float B;
	float B_mant;
	float C;
	float A_f;
	u32 b_expn;
	u32 b_m;
	u32 a_hex;
	u32 c_hex;

	if ((farz == nearz) || (endz == startz)) {
		A = 0.0f;
		B = 0.5f;
		C = 0.0f;
	} else {
		A = (farz * nearz) / ((farz - nearz) * (endz - startz));
		B = (farz / (farz - nearz));
		C = (startz / (endz - startz));
	}

	B_mant = B;
	b_expn = 1;
	while (B_mant > 1.0) {
		B_mant *= 0.5f;
		b_expn++;
	}
	while (B_mant > 0.0f && B_mant < 0.5) {
		B_mant *= 2.0f;
		b_expn--;
	}

	A_f = A / (1 << b_expn);
	b_m = (u32)(8388638.0f * B_mant);

	a_hex = *(u32*)&A_f;
	c_hex = *(u32*)&C;

	GXWGFifo.u8  = GX_LOAD_BP_REG;
	GXWGFifo.u32 = BP_FOG_UNK0(a_hex >> 12, 0xee);
	GXWGFifo.u8  = GX_LOAD_BP_REG;
	GXWGFifo.u32 = BP_FOG_UNK1(b_m, 0xef);
	GXWGFifo.u8  = GX_LOAD_BP_REG;
	GXWGFifo.u32 = BP_FOG_UNK2(b_expn, 0xf0);
	GXWGFifo.u8  = GX_LOAD_BP_REG;
	GXWGFifo.u32 = BP_FOG_UNK3(c_hex >> 12, 0, type, 0xf1);
	u32 colorReg = BP_FOG_COLOR(color.r, color.g, color.b, 0xf2);
	GXWGFifo.u8  = GX_LOAD_BP_REG;
	GXWGFifo.u32 = colorReg;
}

static void SetFogBase(const J3DFogInfo* fog)
{
	FifoSetFog(GXFogType(fog->mType), fog->mStartZ, fog->mEndZ, fog->mNearZ,
	           fog->mFarZ, fog->mColor);
	FifoSetFogRangeAdj(fog->mAdjEnable, fog->mCenter,
	                   (GXFogAdjTable*)fog->mFogAdjTable);
}

static void ShapePacketCallBackFunc(J3DCallBackPacket*, int);

static J3DShapePacket* InitPacket_Sub(J3DModel* model, u16 mat_idx)
{
	J3DMaterial* mat = model->getModelData()->getMaterialNodePointer(mat_idx);
	return model->getShapePacket(mat->getShape()->getIndex());
}

// fabricated
struct PacketUserData_MatColor {
	u32 unk0;
	GXChannelID unk4;
	const GXColor* unk8;
};

void SMS_InitPacket_MatColor(J3DModel* param_1, u16 param_2,
                             GXChannelID param_3, const GXColor* param_4)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_MatColor* userData = new PacketUserData_MatColor;

	userData->unk0 = 0;
	userData->unk4 = param_3;
	userData->unk8 = param_4;

	packet->setUserArea(PTR_U32(userData));
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_OneTevColor {
	u32 unk0;
	GXTevRegID unk4;
	const GXColorS10* unk8;
};

void SMS_InitPacket_OneTevColor(J3DModel* param_1, u16 param_2,
                                GXTevRegID param_3, const GXColorS10* param_4)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_OneTevColor* userData = new PacketUserData_OneTevColor;

	userData->unk0 = 1;
	userData->unk4 = param_3;
	userData->unk8 = param_4;

	packet->setUserArea(PTR_U32(userData));
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_TwoTevColor {
	u32 unk0;
	GXTevRegID unk4;
	GXTevRegID unk8;
	const GXColorS10* unkC;
	const GXColorS10* unk10;
};

void SMS_InitPacket_TwoTevColor(J3DModel* param_1, u16 param_2,
                                GXTevRegID param_3, const GXColorS10* param_4,
                                GXTevRegID param_5, const GXColorS10* param_6)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_TwoTevColor* userData = new PacketUserData_TwoTevColor;

	userData->unk0  = 2;
	userData->unk4  = param_3;
	userData->unkC  = param_4;
	userData->unk8  = param_5;
	userData->unk10 = param_6;

	packet->setUserArea(PTR_U32(userData));
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_ThreeTevColor {
	u32 unk0;
	GXTevRegID unk4;
	GXTevRegID unk8;
	GXTevRegID unkC;
	const GXColorS10* unk10;
	const GXColorS10* unk14;
	const GXColorS10* unk18;
};

void SMS_InitPacket_ThreeTevColor(J3DModel* param_1, u16 param_2,
                                  GXTevRegID param_3, const GXColorS10* param_4,
                                  GXTevRegID param_5, const GXColorS10* param_6,
                                  GXTevRegID param_7, const GXColorS10* param_8)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_ThreeTevColor* userData = new PacketUserData_ThreeTevColor;

	userData->unk0  = 3;
	userData->unk4  = param_3;
	userData->unk10 = param_4;
	userData->unk8  = param_5;
	userData->unk14 = param_6;
	userData->unkC  = param_7;
	userData->unk18 = param_8;

	packet->setUserArea(PTR_U32(userData));
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_CallDL {
	u32 unk0;
	void* unk4;
	u32 unk8;
};

// fabricated
struct PacketUserData_Fog {
	u32 unk0;
	J3DFog* unk4;
};

void SMS_InitPacket_Fog(J3DModel* param_1, u16 param_2)
{
	char trash[40];
	J3DMaterial* mat = param_1->getModelData()->getMaterialNodePointer(param_2);
	J3DPEBlock* peblock     = mat->getPEBlock();
	J3DShapePacket* packet = param_1->getShapePacket(mat->getShape()->getIndex());
	J3DFog* fog             = peblock->getFog();

	PacketUserData_Fog* userData = new PacketUserData_Fog;
	userData->unk0               = 5;
	userData->unk4               = fog;

	packet->setUserArea(PTR_U32(userData));
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_OneTevKColor {
	u32 unk0;
	GXTevKColorID unk4;
	const GXColor* unk8;
};

void SMS_InitPacket_OneTevKColor(J3DModel* param_1, u16 param_2,
                                 GXTevKColorID param_3, const GXColor* param_4)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_OneTevKColor* userData = new PacketUserData_OneTevKColor;

	userData->unk0 = 6;
	userData->unk4 = param_3;
	userData->unk8 = param_4;

	packet->setUserArea(PTR_U32(userData));
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_TwoTevKColor {
	u32 unk0;
	GXTevKColorID unk4;
	GXTevKColorID unk8;
	const GXColor* unkC;
	const GXColor* unk10;
};

void SMS_InitPacket_TwoTevKColor(J3DModel* param_1, u16 param_2,
                                 GXTevKColorID param_3, const GXColor* param_4,
                                 GXTevKColorID param_5, const GXColor* param_6)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_TwoTevKColor* userData = new PacketUserData_TwoTevKColor;

	userData->unk0  = 7;
	userData->unk4  = param_3;
	userData->unkC  = param_4;
	userData->unk8  = param_5;
	userData->unk10 = param_6;

	packet->setUserArea(PTR_U32(userData));
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_OneTevKColorAndFog {
	u32 unk0;
	u32 unk4;
	GXTevKColorID unk8;
	const GXColor* unkC;
	u32 unk10;
	J3DFog* unk14;
};

void SMS_InitPacket_OneTevKColorAndFog(J3DModel* param_1, u16 param_2,
                                       GXTevKColorID param_3,
                                       const GXColor* param_4)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_OneTevKColorAndFog* userData
	    = new PacketUserData_OneTevKColorAndFog;

	userData->unk0 = 8;
	userData->unk4 = 6;
	userData->unk8 = param_3;

	if (param_4 != nullptr) {
		userData->unkC = param_4;
	} else {
		userData->unkC = &param_1->getModelData()
		                      ->getMaterialNodePointer(param_2)
		                      ->getTevBlock()
		                      ->getTevKColor(param_3)
		                      ->color;
	}

	J3DFog* fog = param_1->getModelData()
	                  ->getMaterialNodePointer(param_2)
	                  ->getPEBlock()
	                  ->getFog();

	userData->unk10 = 5;
	userData->unk14 = fog;

	packet->setUserArea(PTR_U32(userData));
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_OneTevColorAndOneTevKColor {
	u32 unk0;
	GXTevRegID unk4;
	const GXColorS10* unk8;
	const GXColor* unkC;
};

void SMS_InitPacket_OneTevColorAndOneTevKColor(J3DModel* param_1, u16 param_2,
                                               GXTevRegID param_3,
                                               const GXColorS10* param_4,
                                               const GXColor* param_5)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_OneTevColorAndOneTevKColor* userData
	    = new PacketUserData_OneTevColorAndOneTevKColor;

	userData->unk0 = 9;
	userData->unk4 = param_3;
	userData->unk8 = param_4;
	userData->unkC = param_5;

	packet->setUserArea(PTR_U32(userData));
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_TwoTevColorAndOneTevKColor {
	u32 unk0;
	GXTevRegID unk4;
	GXTevRegID unk8;
	const GXColorS10* unkC;
	const GXColorS10* unk10;
	const GXColor* unk14;
};

void SMS_InitPacket_TwoTevColorAndOneTevKColor(J3DModel* param_1, u16 param_2,
                                               GXTevRegID param_3,
                                               const GXColorS10* param_4,
                                               GXTevRegID param_5,
                                               const GXColorS10* param_6,
                                               const GXColor* param_7)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_TwoTevColorAndOneTevKColor* userData
	    = new PacketUserData_TwoTevColorAndOneTevKColor;

	userData->unk0  = 10;
	userData->unk4  = param_3;
	userData->unkC  = param_4;
	userData->unk8  = param_5;
	userData->unk10 = param_6;
	userData->unk14 = param_7;

	packet->setUserArea(PTR_U32(userData));
	packet->setCallback(&ShapePacketCallBackFunc);
}

#ifdef TARGET_PC
// Retail writes these states to GXWGFifo, which PC lacks. Aurora's GX setters emit the same state.
static void SetPacketFog(const J3DFogInfo* fog)
{
	GXSetFog(GXFogType(fog->mType), fog->mStartZ, fog->mEndZ, fog->mNearZ,
	         fog->mFarZ, fog->mColor);
	GXSetFogRangeAdj(fog->mAdjEnable, fog->mCenter,
	                 (GXFogAdjTable*)fog->mFogAdjTable);
}

static void ShapePacketCallBackFunc(J3DCallBackPacket* packet, int param)
{
	void* area = (void*)packet->getUserArea();
	u32 type   = *(u32*)area;

	if (param != 0) {
		// asm 0x800D1894: fog packets switch fog off after the shape draws.
		if (param == 1 && (type == 5 || type == 8)) {
			GXColor off = { 0, 0, 0, 0 };
			GXSetFog(GX_FOG_NONE, 0.0f, 0.0f, 0.0f, 0.0f, off);
		}
		return;
	}

	// asm 0x800D1164 jump table "@2227". Type 4 (CallDL) has no writer in this port.
	switch (type) {
	case 0: {
		PacketUserData_MatColor* d = (PacketUserData_MatColor*)area;
		GXSetChanMatColor(d->unk4, *d->unk8);
		break;
	}
	case 1: {
		PacketUserData_OneTevColor* d = (PacketUserData_OneTevColor*)area;
		GXSetTevColorS10(d->unk4, *d->unk8);
		break;
	}
	case 2: {
		PacketUserData_TwoTevColor* d = (PacketUserData_TwoTevColor*)area;
		GXSetTevColorS10(d->unk4, *d->unkC);
		GXSetTevColorS10(d->unk8, *d->unk10);
		break;
	}
	case 3: {
		PacketUserData_ThreeTevColor* d = (PacketUserData_ThreeTevColor*)area;
		GXSetTevColorS10(d->unk4, *d->unk10);
		GXSetTevColorS10(d->unk8, *d->unk14);
		GXSetTevColorS10(d->unkC, *d->unk18);
		break;
	}
	case 5: {
		PacketUserData_Fog* d = (PacketUserData_Fog*)area;
		SetPacketFog(d->unk4);
		break;
	}
	case 6: {
		PacketUserData_OneTevKColor* d = (PacketUserData_OneTevKColor*)area;
		GXSetTevKColor(d->unk4, *d->unk8);
		break;
	}
	case 7: {
		PacketUserData_TwoTevKColor* d = (PacketUserData_TwoTevKColor*)area;
		GXSetTevKColor(d->unk4, *d->unkC);
		GXSetTevKColor(d->unk8, *d->unk10);
		break;
	}
	case 8: {
		PacketUserData_OneTevKColorAndFog* d = (PacketUserData_OneTevKColorAndFog*)area;
		GXSetTevKColor(d->unk8, *d->unkC);
		SetPacketFog(d->unk14);
		break;
	}
	case 9: {
		PacketUserData_OneTevColorAndOneTevKColor* d = (PacketUserData_OneTevColorAndOneTevKColor*)area;
		GXSetTevColorS10(d->unk4, *d->unk8);
		GXSetTevKColor(GX_KCOLOR0, *d->unkC); // asm hardwires K0 (0xE080)
		break;
	}
	case 10: {
		PacketUserData_TwoTevColorAndOneTevKColor* d = (PacketUserData_TwoTevColorAndOneTevKColor*)area;
		GXSetTevColorS10(d->unk4, *d->unkC);
		GXSetTevColorS10(d->unk8, *d->unk10);
		GXSetTevKColor(GX_KCOLOR0, *d->unk14); // asm hardwires K0 (0xE080)
		break;
	}
	}
}
#else
static void ShapePacketCallBackFunc(J3DCallBackPacket* packet, int param)
{
	void* area = (void*)packet->getUserArea();
	char trash[0x90]; // unused stack slot (present in the retail frame)

	if (param == 0) {
		// asm 0x800D1164 jump table "@2227"
		switch (*(u32*)area) {
		case 0: {
			PacketUserData_MatColor* d = (PacketUserData_MatColor*)area;
			FifoSetChanMatColor(d->unk4, *d->unk8);
			break;
		}
		case 1: {
			PacketUserData_OneTevColor* d = (PacketUserData_OneTevColor*)area;
			FifoSetTevColorS10(d->unk4, *d->unk8);
			break;
		}
		case 2: {
			PacketUserData_TwoTevColor* d = (PacketUserData_TwoTevColor*)area;
			FifoSetTevColorS10(d->unk4, *d->unkC);
			FifoSetTevColorS10(d->unk8, *d->unk10);
			break;
		}
		case 3: {
			PacketUserData_ThreeTevColor* d = (PacketUserData_ThreeTevColor*)area;
			FifoSetTevColorS10(d->unk4, *d->unk10);
			FifoSetTevColorS10(d->unk8, *d->unk14);
			FifoSetTevColorS10(d->unkC, *d->unk18);
			break;
		}
		case 4: {
			PacketUserData_CallDL* d = (PacketUserData_CallDL*)area;
			GXCallDisplayList(d->unk4, d->unk8);
			break;
		}
		case 5: {
			PacketUserData_Fog* d = (PacketUserData_Fog*)area;
			SetFogBase(d->unk4);
			break;
		}
		case 6: {
			PacketUserData_OneTevKColor* d = (PacketUserData_OneTevKColor*)area;
			FifoSetTevKColor(d->unk4, *d->unk8);
			break;
		}
		case 7: {
			PacketUserData_TwoTevKColor* d = (PacketUserData_TwoTevKColor*)area;
			FifoSetTevKColor(d->unk4, *d->unkC);
			FifoSetTevKColor(d->unk8, *d->unk10);
			break;
		}
		case 8: {
			PacketUserData_OneTevKColorAndFog* d = (PacketUserData_OneTevKColorAndFog*)area;
			FifoSetTevKColor(d->unk8, *d->unkC);
			SetFogBase(d->unk14);
			break;
		}
		case 9: {
			PacketUserData_OneTevColorAndOneTevKColor* d = (PacketUserData_OneTevColorAndOneTevKColor*)area;
			FifoSetTevColorS10(d->unk4, *d->unk8);
			FifoSetTevKColor(GX_KCOLOR0, *d->unkC);
			break;
		}
		case 10: {
			PacketUserData_TwoTevColorAndOneTevKColor* d = (PacketUserData_TwoTevColorAndOneTevKColor*)area;
			FifoSetTevColorS10(d->unk4, *d->unkC);
			FifoSetTevColorS10(d->unk8, *d->unk10);
			FifoSetTevKColor(GX_KCOLOR0, *d->unk14);
			break;
		}
		}
	} else if (param == 1) {
		switch (*(s32*)area) {
		case 5:
		case 8: {
			// asm 0x800D1894: fog packets switch fog off after the shape draws.
			static GXColor sFogOffColor = { 0, 0, 0, 0 };
			FifoSetFog(GX_FOG_NONE, 0.0f, 0.0f, 0.0f, 0.0f, sFogOffColor);
			break;
		}
		}
	}
}
#endif

void SMS_HideAllShapePacket(J3DModel* model)
{
	u16 mats = model->getModelData()->getMaterialNum();
	for (u16 i = 0; i < mats; ++i)
		model->getShapePacket(i)->hide();
}

void SMS_ShowAllShapePacket(J3DModel* model)
{
	u16 mats = model->getModelData()->getMaterialNum();
	for (u16 i = 0; i < mats; ++i)
		model->getShapePacket(i)->show();
}
