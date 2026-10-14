// Host-conversion checks for game code built with TARGET_PC (tools/port_tests.cmd compiles this file with the sms include
// path and links the real JSUInputStream/JSUMemoryStream): typed stream reads keep the preset on a short read,
// float -> u32 equals the console's __cvt_fp2unsigned.
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
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
}
