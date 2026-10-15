// Host-conversion checks for game code built with TARGET_PC (tools/port_tests.cmd compiles this file with the sms include
// path and links the real JSUInputStream/JSUMemoryStream): typed stream reads keep the preset on a short read,
// float -> u32 equals the console's __cvt_fp2unsigned, float -> s16 angles keep the low 16 bits.
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <JSystem/JGeometry/JGRotation3.hpp>
#include <hostfloat.h>
#include <cmath>
#include <cstring>

namespace {

f32 readF32(const void* bytes, s32 n, f32 preset)
{
	JSUMemoryInputStream s(bytes, n);
	f32 f = preset;
	s.read(f);
	return f;
}

template <typename T> T readPreset(T preset)
{
	static const u8 none[1] = { 0 };
	JSUMemoryInputStream s(none, 0);
	T v = preset;
	s.read(v);
	return v;
}

// The (s16)(s32) cast used by the gatekeeper.cpp spawn angle, copied here: this tests the cast, not gatekeeper.cpp
// itself. noinline so the conversion is not constant-folded.
__declspec(noinline) u16 gateAngle(int i) { return (u16)(s16)(s32)(182.04445f * (36.0f * (f32)i)); }

} // namespace

void smsHostChecks(void (*check)(bool, const char*))
{
	static const u8 none[1]    = { 0 };
	static const u8 half[2]    = { 0x41, 0x20 };
	static const u8 five[4]    = { 0x40, 0xA0, 0x00, 0x00 };
	check(readF32(none, 0, 5.0f) == 5.0f, "jsu: failed f32 read keeps the preset (was 5.7e-41)");
	check(readF32(half, 2, 5.0f) == 10.0f, "jsu: 2-byte read into a 5.0 preset gives 10.0 (retail high half)");
	check(readF32(five, 4, 1.0f) == 5.0f, "jsu: full f32 read 40 A0 00 00 gives 5.0");
	check(readPreset<s16>(0x1234) == 0x1234, "jsu: failed s16 read keeps the preset");
	check(readPreset<u32>(0x12345678u) == 0x12345678u, "jsu: failed u32 read keeps the preset");
	check(readPreset<u64>(0x0102030405060708ull) == 0x0102030405060708ull, "jsu: failed u64 read keeps the preset");

	check(SMSCvtFp2Unsigned(-22.5) == 0, "fp2unsigned: -22.5 -> 0");
	check(SMSCvtFp2Unsigned(-0.5) == 0, "fp2unsigned: -0.5 -> 0");
	check(SMSCvtFp2Unsigned(3.9) == 3, "fp2unsigned: 3.9 -> 3");
	check(SMSCvtFp2Unsigned(2147483648.0) == 0x80000000u, "fp2unsigned: 2^31 -> 0x80000000");
	check(SMSCvtFp2Unsigned(4294967296.0) == 0xFFFFFFFFu, "fp2unsigned: 2^32 -> 0xFFFFFFFF");
	check(SMSCvtFp2Unsigned(std::nan("")) == 0xFFFFFFFFu, "fp2unsigned: NaN -> 0xFFFFFFFF");

	static const u16 want[10] = { 0, 6553, 13107, 19660, 26214, 0x8000, 0x9999, 0xB333, 0xCCCC, 0xE666 };
	bool ok = true;
	for (int k = 0; k < 10; k++)
		ok = ok && gateAngle(k) == want[k];
	check(ok, "angle: (s16)(s32) cast of the 10 gatekeeper 36-degree angles keeps the low 16 bits");

	JGeometry::TRotation3<JGeometry::TMatrix34<JGeometry::SMatrix34C<f32>>> basis;
	basis.setLookDir(JGeometry::TVec3<f32>(0.0f, -12000.0f, 0.0f),
	                 JGeometry::TVec3<f32>(0.0f, 0.0f, -1.0f));
	check(std::fabs(basis.ref(0, 0) - 1.0f) < 0.000001f
	          && std::fabs(basis.ref(1, 2) + 1.0f) < 0.000001f
	          && std::fabs(basis.ref(2, 1) - 1.0f) < 0.000001f,
	      "look direction: downward bath view preserves retail orientation, not reflected cap winding");
}
