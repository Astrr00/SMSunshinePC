#ifndef HOSTENDIAN_H
#define HOSTENDIAN_H

// Host byte order helpers for TARGET_PC. The console is big-endian and the disc formats are stored big-endian; the
// host (x86-64) is little-endian. Format code swaps a record in place once, where the raw bytes first become
// available (see the format strategy in the port docs), and then uses the structs as usual.
// be16/be32/bef32 are involutions: they convert big-endian to host and host to big-endian alike.
// Everything here exists only under TARGET_PC; the console build never includes this API.

#ifdef TARGET_PC

#include <types.h>
#include <string.h>

// Byte swaps: clang and gcc spell them __builtin_bswap*, MSVC cl has _byteswap_*. clang-cl defines _MSC_VER as well,
// so it takes the builtin branch.
#if defined(_MSC_VER) && !defined(__clang__)
#include <stdlib.h>
#define HOST_BSWAP16(v) _byteswap_ushort(v)
#define HOST_BSWAP32(v) _byteswap_ulong(v)
#define HOST_BSWAP64(v) _byteswap_uint64(v)
#else
#define HOST_BSWAP16(v) __builtin_bswap16(v)
#define HOST_BSWAP32(v) __builtin_bswap32(v)
#define HOST_BSWAP64(v) __builtin_bswap64(v)
#endif

static inline u16 be16(u16 v) { return (u16)((u16)(v >> 8) | (u16)(v << 8)); }

static inline u32 be32(u32 v) { return HOST_BSWAP32(v); }

static inline f32 bef32(f32 v)
{
	u32 u;
	memcpy(&u, &v, sizeof(u));
	u = HOST_BSWAP32(u);
	memcpy(&v, &u, sizeof(v));
	return v;
}

// In-place variants for a field whose address is known, regardless of alignment.
static inline void swapInPlace16(void* p)
{
	u16 v;
	memcpy(&v, p, sizeof(v));
	v = be16(v);
	memcpy(p, &v, sizeof(v));
}

static inline void swapInPlace32(void* p)
{
	u32 v;
	memcpy(&v, p, sizeof(v));
	v = be32(v);
	memcpy(p, &v, sizeof(v));
}

// Read a big-endian value from a byte stream without assuming alignment.
static inline u16 rdbe16(const void* p)
{
	u16 v;
	memcpy(&v, p, sizeof(v));
	return be16(v);
}

static inline u32 rdbe32(const void* p)
{
	u32 v;
	memcpy(&v, p, sizeof(v));
	return be32(v);
}

#ifdef __cplusplus
// A scalar of an on-disc structure that stays big-endian in memory. Overlay structs that are only ever read (parsers
// that copy the data out, never write back) use BE<T> members instead of swapping the buffer in place: the struct keeps
// the disc layout and size, each read converts, and re-reading the same bytes is always safe.
// T is a 1/2/4-byte arithmetic type (u8, s16, u32, f32, ...); the value is stored in the byte order of the disc.
template <typename T> struct BE {
	T raw;

	operator T() const
	{
		T v;
		unsigned char* d       = reinterpret_cast<unsigned char*>(&v);
		const unsigned char* s = reinterpret_cast<const unsigned char*>(&raw);
		for (unsigned i = 0; i < sizeof(T); ++i)
			d[i] = s[sizeof(T) - 1 - i];
		return v;
	}
};
#endif // __cplusplus

#endif // TARGET_PC

#endif // HOSTENDIAN_H
