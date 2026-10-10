#ifdef TARGET_PC
#include <hostendian.h>
#endif
#include <GC2D/MessageLoader.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>

TMessageLoader::TMessageLoader()
    : unk0(0)
    , unk4(0)
{
}

TMessageLoader::TMessageLoader(const char* param_1)
    : unk0(0)
    , unk4(0)
{
	char trash[8];
	u8* res = (u8*)JKRGetResource(param_1);
	if (res) {
		u32 a;
		u32 b;
		readHeader(&a, &b, res);
		unk4 = parseBlock(a, b, res + 0x20);
		if (unk4)
			(void)unk4;
	}
}

u32 TMessageLoader::loadMessageData(const char* param_1)
{
	u8* res = (u8*)JKRGetResource(param_1);
	if (!res)
		return -1;

#ifdef TARGET_PC
	// BMG files are only ever read through these accessors (never swapped in place), so repeated loads of the same
	// resource are safe: the header, tags and block sizes are big-endian, the message text is plain bytes.
	unk4 = parseBlock((int)rdbe32(res + 0x8) * 32, (int)rdbe32(res + 0xC),
	                  res + 0x20);
#else
	unk4 = parseBlock(*(int*)(res + 0x8) * 32, *(int*)(res + 0xC), res + 0x20);
#endif
	if (!unk4)
		return -1;

	return unk2;
}

void TMessageLoader::readHeader(u32* a, u32* b, void* header)
{
	u32* casted = (u32*)header;

#ifdef TARGET_PC
	*a = rdbe32(casted + 2) * 32;
	*b = rdbe32(casted + 3);
#else
	*a = *(casted + 2) * 32;
	*b = *(casted + 3);
#endif
}

void* TMessageLoader::parseBlock(u32 param_1, u32 param_2, void* param_3)
{
	JSUMemoryInputStream local_5c(param_3, param_1);

	void* result;

	for (int i = 0; i < param_2; ++i) {
		int local_74;
		local_5c.read(&local_74, 4);
#ifdef TARGET_PC
		local_74 = (int)be32((u32)local_74); // raw read of a big-endian block tag
#endif

		int local_70;

		switch (local_74) {
		case 'INF1': {
			local_70 = readInfoBlock(local_5c.getCurrent());
			local_5c.skip(4);
			break;
		}

		case 'DAT1':
			local_5c.read(&local_70, 4);
#ifdef TARGET_PC
			local_70 = (int)be32((u32)local_70); // raw read of a big-endian block size
#endif
			result = local_5c.getCurrent();
			break;

		case 'STR1':
			local_70 = 0;
			break;

		default:
			local_70 = 0;
			break;
		}

		local_5c.skip(local_70 - 8);
	}

	return result;
}

TMessageLoader::EntryInfo* TMessageLoader::getMessageEntry(u32 param_1)
{
	EntryInfo* result;
	if (u16(param_1) >= unk0)
		result = nullptr;
	else
		result = &unk8[param_1];

	return result;
}

int TMessageLoader::readInfoBlock(void* data)
{
#ifdef TARGET_PC
	int length = (int)rdbe32(data);
#else
	int length = *(int*)data;
#endif
	data       = (u8*)data + 4;
	JSUMemoryInputStream local_38(data, length - 8);
	local_38.read(&unk0, 2);
#ifdef TARGET_PC
	unk0 = be16(unk0); // raw read of the big-endian entry count
#endif
	local_38.readU16();
	unk2 = local_38.readU16();
	local_38.skip(2);

#ifdef TARGET_PC
	for (int i = 0; i < unk0; ++i) {
		local_38.read(&unk8[i], sizeof(EntryInfo));
		// raw read of a 12 byte disc record: offset into the DAT1 text, two s16 attributes, four bytes
		unk8[i].unk0 = be32(unk8[i].unk0);
		unk8[i].unk4 = (s16)be16((u16)unk8[i].unk4);
		unk8[i].unk6 = (s16)be16((u16)unk8[i].unk6);
	}
#else
	for (int i = 0; i < unk0; ++i)
		local_38.read(&unk8[i], sizeof(EntryInfo));
#endif

	return length;
}
