#include <JSystem/JParticle/JPAExtraShape.hpp>
#include <JSystem/JParticle/JPAMath.hpp>

JPAExtraShape::JPAExtraShape(const u8* data)
{
	mAlphaInTiming  = JPAConvertFixToFloat(JPA_S16(data + 0x14));
	mAlphaOutTiming = JPAConvertFixToFloat(JPA_S16(data + 0x16));
	mAlphaInValue   = JPAConvertFixToFloat(JPA_S16(data + 0x18));
	mAlphaBaseValue = JPAConvertFixToFloat(JPA_S16(data + 0x1A));
	unk18           = JPAConvertFixToFloat(JPA_S16(data + 0x1C));

	unk78 = data[0x1E];
	unk79 = data[0x1F];

	mAlphaWaveParam1 = JPAConvertFixToFloat(JPA_S16(data + 0x20));
	mAlphaWaveParam2 = JPAConvertFixToFloat(JPA_S16(data + 0x22));
	mAlphaWaveParam3 = JPAConvertFixToFloat(JPA_S16(data + 0x24));
	mAlphaWaveRandom = JPAConvertFixToFloat(JPA_S16(data + 0x26));

	mAlphaIncreaseRate
	    = mAlphaInTiming != 0.0f
	          ? (mAlphaBaseValue - mAlphaInValue) / mAlphaInTiming
	          : 1.0f;
	mAlphaDecreaseRate
	    = mAlphaOutTiming != 1.0f
	          ? (unk18 - mAlphaBaseValue) / (1.0f - mAlphaOutTiming)
	          : 1.0f;

	mRandomScale    = JPAConvertFixToFloat(JPA_S16(data + 0x34));
	mScaleInTiming  = JPAConvertFixToFloat(JPA_S16(data + 0x36));
	mScaleOutTiming = JPAConvertFixToFloat(JPA_S16(data + 0x38));
	mScaleInValueY  = JPAConvertFixToFloat(JPA_S16(data + 0x3A)) * 10.0f;
	unk44           = JPAConvertFixToFloat(JPA_S16(data + 0x3E)) * 10.0f;

	unk7A          = data[0x40];
	unk7B          = data[0x41];
	mAnmCycleY     = JPA_S16(data + 0x42);
	mScaleInValueX = JPAConvertFixToFloat(JPA_S16(data + 0x44)) * 10.0f;
	unk4C          = JPAConvertFixToFloat(JPA_S16(data + 0x48)) * 10.0f;
	unk7C          = data[0x4A];
	unk7D          = data[0x4B];
	mAnmCycleX     = JPA_S16(data + 0x4C);
	unk7E          = data[0x4E];

	if (mScaleInTiming != 0.0f) {
		mIncreaseRateX = (1.0f - mScaleInValueX) / mScaleInTiming;
		mIncreaseRateY = (1.0f - mScaleInValueY) / mScaleInTiming;
	} else {
		mIncreaseRateY = 1.0f;
		mIncreaseRateX = 1.0f;
	}

	if (mScaleOutTiming != 1.0f) {
		mDecreaseRateX = (unk4C - 1.0f) / (1.0f - mScaleOutTiming);
		mDecreaseRateY = (unk44 - 1.0f) / (1.0f - mScaleOutTiming);
	} else {
		mDecreaseRateY = 1.0f;
		mDecreaseRateX = 1.0f;
	}

	mRotateAngle       = JPAConvertFixToFloat(JPA_S16(data + 0x5A));
	mRotateSpeed       = JPAConvertFixToFloat(JPA_S16(data + 0x5C));
	mRotateRandomSpeed = JPAConvertFixToFloat(JPA_S16(data + 0x5E));
	mRotateRandomAngle = JPAConvertFixToFloat(JPA_S16(data + 0x60));
	mRotateDirection   = JPAConvertFixToFloat(JPA_S16(data + 0x62));

	unk7F = data[0x64];
}
