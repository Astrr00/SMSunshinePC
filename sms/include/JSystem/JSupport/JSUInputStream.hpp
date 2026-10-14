#ifndef JSU_INPUT_STREAM_H
#define JSU_INPUT_STREAM_H

#include <types.h>
#include <JSystem/JSupport/JSUIosBase.hpp>
#ifdef TARGET_PC
#include <hostendian.h>
#endif

class JSUInputStream : public JSUIosBase {
public:
	virtual ~JSUInputStream();
	virtual int getAvailable() const = 0;
	virtual int skip(s32 amount);
	virtual int readData(void* buf, s32 size) = 0;

	u32 read(void* buf, s32 size);
	char* read(char* buf);
	char* readString();
	char* readString(char* buf, u16 len);

	u32 read(s8& p) { return read(&p, sizeof(s8)); }
	u32 read(u8& p) { return read(&p, sizeof(u8)); }
	u32 read(bool& p) { return read(&p, sizeof(bool)); }
#ifdef TARGET_PC
	// Byte order policy (TARGET_PC): every TYPED multi-byte scalar read (read(T&), readS16/U16/S32/U32/U64/F32,
	// read16b/read32b, operator>>, and the u16 length prefix of the string readers) converts from the big-endian
	// disc order to host order. Raw reads (read(void*, n), readData, peek, skip) return the bytes untouched; a caller
	// that reads a multi-byte scalar or an array of them raw must convert it itself.
	// The preset is swapped into disc order before the read and back after it: retail 0x8000E590/0x8000F098: failed
	// read keeps buffer, so a short or failed read leaves the caller's preset (or its untouched low bytes) intact.
	// The readXX helpers below zero their local first, so that pre-swap never reads an uninitialised value.
	u32 read(s16& p)
	{
		swapInPlace16(&p);
		u32 r = read(&p, sizeof(s16));
		swapInPlace16(&p);
		return r;
	}
	u32 read(u16& p)
	{
		swapInPlace16(&p);
		u32 r = read(&p, sizeof(u16));
		swapInPlace16(&p);
		return r;
	}
	u32 read(s32& p)
	{
		swapInPlace32(&p);
		u32 r = read(&p, sizeof(s32));
		swapInPlace32(&p);
		return r;
	}
	u32 read(u32& p)
	{
		swapInPlace32(&p);
		u32 r = read(&p, sizeof(u32));
		swapInPlace32(&p);
		return r;
	}
	u32 read(s64& p)
	{
		p = (s64)HOST_BSWAP64((u64)p);
		u32 r = read(&p, sizeof(s64));
		p     = (s64)HOST_BSWAP64((u64)p);
		return r;
	}
	u32 read(u64& p)
	{
		p = HOST_BSWAP64(p);
		u32 r = read(&p, sizeof(u64));
		p     = HOST_BSWAP64(p);
		return r;
	}
	u32 read(f32& p)
	{
		swapInPlace32(&p);
		u32 r = read(&p, sizeof(f32));
		swapInPlace32(&p);
		return r;
	}
#else
	u32 read(s16& p) { return read(&p, sizeof(s16)); }
	u32 read(u16& p) { return read(&p, sizeof(u16)); }
	u32 read(s32& p) { return read(&p, sizeof(s32)); }
	u32 read(u32& p) { return read(&p, sizeof(u32)); }
	u32 read(s64& p) { return read(&p, sizeof(s64)); }
	u32 read(u64& p) { return read(&p, sizeof(u64)); }
#endif

	u8 read8b()
	{
		u8 b;
		read(&b, sizeof(u8));
		return b;
	}

	u16 read16b()
	{
		u16 s;
#ifdef TARGET_PC
		s = 0;
		read(s);
#else
		read(&s, sizeof(u16));
#endif
		return s;
	}

	u32 read32b()
	{
		u32 i;
#ifdef TARGET_PC
		i = 0;
		read(i);
#else
		read(&i, sizeof(u32));
#endif
		return i;
	}

	/* @fabricated */
	s8 readS8()
	{
		s8 b;
		read(&b, sizeof(s8));
		return b;
	}

	u8 readU8()
	{
		u8 b;
		read(&b, sizeof(u8));
		return b;
	}

	s16 readS16()
	{
		s16 s;
#ifdef TARGET_PC
		s = 0;
		read(s);
#else
		read(&s, sizeof(s16));
#endif
		return s;
	}

	u16 readU16()
	{
		u16 s;
#ifdef TARGET_PC
		s = 0;
		read(s);
#else
		read(&s, sizeof(u16));
#endif
		return s;
	}

	s32 readS32()
	{
		s32 i;
#ifdef TARGET_PC
		i = 0;
		read(i);
#else
		read(&i, sizeof(s32));
#endif
		return i;
	}

	u32 readU32()
	{
		u32 i;
#ifdef TARGET_PC
		i = 0;
		read(i);
#else
		read(&i, sizeof(u32));
#endif
		return i;
	}

	u64 readU64()
	{
		u64 i;
#ifdef TARGET_PC
		i = 0;
		read(i);
#else
		read(&i, sizeof(u64));
#endif
		return i;
	}

	/* @fabricated */
	f32 readF32()
	{
		f32 f;
#ifdef TARGET_PC
		f = 0;
		read(f);
#else
		read(&f, sizeof(f32));
#endif
		return f;
	}

	JSUInputStream& operator>>(bool& p)
	{
		read(&p, sizeof(bool));
		return *this;
	}

	JSUInputStream& operator>>(s8& p)
	{
		read(&p, sizeof(s8));
		return *this;
	}

	JSUInputStream& operator>>(u8& p)
	{
		read(&p, sizeof(u8));
		return *this;
	}

	JSUInputStream& operator>>(s16& p)
	{
#ifdef TARGET_PC
		read(p);
#else
		read(&p, sizeof(s16));
#endif
		return *this;
	}

	JSUInputStream& operator>>(u16& p)
	{
#ifdef TARGET_PC
		read(p);
#else
		read(&p, sizeof(u16));
#endif
		return *this;
	}

	JSUInputStream& operator>>(s32& p)
	{
#ifdef TARGET_PC
		read(p);
#else
		read(&p, sizeof(s32));
#endif
		return *this;
	}

	JSUInputStream& operator>>(u32& p)
	{
#ifdef TARGET_PC
		read(p);
#else
		read(&p, sizeof(u32));
#endif
		return *this;
	}

	JSUInputStream& operator>>(f32& p)
	{
#ifdef TARGET_PC
		read(p);
#else
		read(&p, sizeof(f32));
#endif
		return *this;
	}
};

#endif
