#ifndef JASHOSTFMT_HPP
#define JASHOSTFMT_HPP

#include <dolphin/types.h>

// Host handling of the big-endian JAudio disc formats (BNK, WSYS, BARC, AAF, sequences). See the format notes in
// sunshine-pc/docs/platform-notes.md.
//
// DiscU16/DiscS16/DiscU32/DiscS32/DiscF32 are the member types of structs that overlay disc bytes and are only read.
// On the console they are the plain scalar types (identical code); on the host they are BE<T> (hostendian.h), which
// keeps the disc layout and converts on every read, so the buffer is never modified and parsing it twice is safe.

#ifdef TARGET_PC

#include <hostendian.h>

typedef BE<u16> DiscU16;
typedef BE<s16> DiscS16;
typedef BE<u32> DiscU32;
typedef BE<s32> DiscS32;
typedef BE<f32> DiscF32;

namespace JASystem {
namespace HostFmt {

	// Oscillator envelope tables (TOscillator::Osc_::mAdsTable/mRelTable) are s16 triplets (mode, time, value) ended by
	// the first triplet whose mode is > 0xA (BNKParser::getOscTableEndPtr, TOscillator::calc). The oscillator reads them
	// as host s16, the disc and the sequence data store them big-endian. Every real mode is below 0x100, so in a mode
	// field exactly one byte is non-zero and the terminator triplet tells which byte order the table is in: this makes
	// the in-place conversion idempotent without a marker (a sequence table is converted each time a track sets it up).

	// Number of s16 in the table, terminator triplet included (works for both byte orders).
	static inline u32 oscTableLength(const s16* table)
	{
		const u8* p = (const u8*)table;
		u32 n       = 0;
		for (;; p += 6, n += 3)
			if ((u32)(p[0] | p[1]) > 0xA)
				return n + 3;
	}

	// Converts a big-endian table to host order in place; a table that is already in host order is left alone.
	static inline s16* oscTableToHost(s16* table)
	{
		u32 n = oscTableLength(table);
		if (((const u8*)table)[(n - 3) * 2] == 0)
			for (u32 i = 0; i < n; ++i)
				swapInPlace16(&table[i]);
		return table;
	}

} // namespace HostFmt
} // namespace JASystem

#else

typedef u16 DiscU16;
typedef s16 DiscS16;
typedef u32 DiscU32;
typedef s32 DiscS32;
typedef f32 DiscF32;

#endif // TARGET_PC

#endif // JASHOSTFMT_HPP
