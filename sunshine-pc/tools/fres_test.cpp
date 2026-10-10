// Spot checks for __fres against values that follow directly from the table and the documented bounds (1/4096).
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
extern "C" float __fres(float);
static uint32_t u(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
int main()
{
	int bad = 0;
	auto expect = [&](const char* n, float in, uint32_t want) {
		uint32_t got = u(__fres(in));
		if (got != want) { std::printf("FAIL %s: got %08x want %08x\n", n, got, want); ++bad; }
	};
	expect("1.0", 1.0f, 0x3F7FF800);       // 0.99975586
	expect("+0", 0.0f, 0x7F800000);
	expect("-0", -0.0f, 0xFF800000);
	expect("+inf", INFINITY, 0x00000000);
	expect("-inf", -INFINITY, 0x80000000);
	expect("tiny denorm (overflow)", 1e-45f, 0x7F7FFFFF);
	// accuracy bound: |est - 1/x| <= 1/4096 relative, over many normal inputs
	float worst = 0;
	for (uint32_t b = 0x00800000; b < 0x7F000000; b += 0x2371) {
		float x; std::memcpy(&x, &b, 4);
		double r = 1.0 / (double)x, e = __fres(x);
		if (r > 3.0e38 || r < 1.2e-38) continue;
		worst = std::fmax(worst, (float)std::fabs((e - r) / r));
	}
	std::printf("worst relative error %g (bound %g)\n", worst, 1.0 / 4096);
	if (worst > 1.0 / 4096) ++bad;
	std::printf(bad ? "FRES TEST FAILED\n" : "fres test ok\n");
	return bad != 0;
}
