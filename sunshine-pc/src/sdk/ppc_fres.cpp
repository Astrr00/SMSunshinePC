// Gekko `fres` (reciprocal estimate, single precision) as seen by the game through __fres().
//
// The result is bit-exact with the PowerPC 750CL: a 32-entry base/delta table indexed by the top five mantissa bits,
// linearly interpolated with the next ten mantissa bits. The table values and the special-case handling (zero,
// denormal, infinity, NaN, denormal results) are taken from Andrew Church's real-hardware-validated test routine
// "calc_fres" in https://achurch.org/cpu-tests/ppc750cl.s ("No copyright is claimed on this file"). FPSCR side effects
// (FI/XX/OX/UX/ZX) are not modelled: the game never reads them after __fres.
#include <cstdint>
#include <cstring>

namespace {

struct BaseDelta {
	uint16_t base;
	uint16_t delta;
};

constexpr BaseDelta kFresTable[32] = {
	{ 0x3FFC, 0x3E1 }, { 0x3C1C, 0x3A7 }, { 0x3875, 0x371 }, { 0x3504, 0x340 }, { 0x31C4, 0x313 }, { 0x2EB1, 0x2EA },
	{ 0x2BC8, 0x2C4 }, { 0x2904, 0x2A0 }, { 0x2664, 0x27F }, { 0x23E5, 0x261 }, { 0x2184, 0x245 }, { 0x1F40, 0x22A },
	{ 0x1D16, 0x212 }, { 0x1B04, 0x1FB }, { 0x190A, 0x1E5 }, { 0x1725, 0x1D1 }, { 0x1554, 0x1BE }, { 0x1396, 0x1AC },
	{ 0x11EB, 0x19B }, { 0x104F, 0x18B }, { 0x0EC4, 0x17C }, { 0x0D48, 0x16E }, { 0x0BD7, 0x15B }, { 0x0A7C, 0x15B },
	{ 0x0922, 0x143 }, { 0x07DF, 0x143 }, { 0x069C, 0x12D }, { 0x056F, 0x12D }, { 0x0442, 0x11A }, { 0x0328, 0x11A },
	{ 0x020E, 0x108 }, { 0x0106, 0x106 },
};

} // namespace

extern "C" float __fres(float input)
{
	uint32_t bits;
	std::memcpy(&bits, &input, sizeof(bits));

	const uint32_t sign = bits & 0x80000000u;
	int32_t exponent    = (int32_t)((bits >> 23) & 0xFF);
	uint32_t mantissa   = bits & 0x007FFFFFu;

	uint32_t result;
	if (exponent == 255) {
		// infinity -> signed zero, NaN -> quieted NaN
		result = mantissa == 0 ? sign : (bits | 0x00400000u);
	} else {
		if (exponent == 0) {
			// Zero and denormals. A mantissa below 0x200000 overflows the reciprocal.
			if (mantissa < 0x00200000u) {
				result = mantissa == 0 ? (sign | 0x7F800000u) : (sign | 0x7F7FFFFFu);
				float out;
				std::memcpy(&out, &result, sizeof(out));
				return out;
			}
			// Normalize: one or two left shifts bring the leading one to bit 23.
			mantissa <<= 1;
			if (!(mantissa & 0x00800000u)) {
				mantissa <<= 1;
				exponent -= 1;
			}
			mantissa &= 0x007FFFFFu;
		}

		exponent = 253 - exponent;
		const BaseDelta& e   = kFresTable[mantissa >> 18];
		uint32_t value       = ((uint32_t)e.base << 10) - (uint32_t)e.delta * ((mantissa >> 8) & 0x3FFu);
		bool sticky          = (value & 1u) != 0; // FI bit; only matters for the denormal rounding below
		value >>= 1;

		if (exponent <= 0) {
			// Denormal result: shift right once or twice, ORing the dropped bits into the sticky flag (no rounding).
			const bool twice = exponent < 0;
			exponent         = 0;
			value |= 0x00800000u;
			sticky |= (value & 1u) != 0;
			value >>= 1;
			if (twice) {
				sticky |= (value & 1u) != 0;
				value >>= 1;
			}
		}
		(void)sticky;
		result = sign | ((uint32_t)exponent << 23) | value;
	}

	float out;
	std::memcpy(&out, &result, sizeof(out));
	return out;
}
