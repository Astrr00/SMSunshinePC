// Host test of the endian-aware JSU stream layer on real JDrama object streams.
// Walks a scene.bin/tables.bin with the same calls TNameRef::getType/load and TNameRefPtrListT::load use
// (readU32, readU16, readString, readS32 on JSUMemoryInputStream) and checks every node: nameKey == calcKeyCode(name),
// node sizes nest exactly, group child counts are consistent. Exit code 0 = all files OK.
// Build (from sunshine-pc, see tools/chk_stream.cmd):  clang-cl ... chk_stream.cpp JSUInputStream.cpp JSUMemoryStream.cpp
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <stdio.h>
#include <stdlib.h>

static unsigned short calcKey(const char* n)
{
	unsigned r = 0;
	while (*n)
		r = r * 3 + (unsigned char)*n++;
	return (unsigned short)r;
}

static int nodes, groups, bad;

// Returns true if the node at the stream position parses, consuming exactly its size.
static bool walk(JSUMemoryInputStream& stream, int depth)
{
	JSUMemoryInputStream rem;
	u32 size = stream.readU32();
	if (size < 4 || (s32)size - 4 > stream.getLength() - stream.getPosition())
		return false;
	rem.setBuffer((u8*)stream.mBuffer + stream.mPosition, size - 4);
	stream.skip(size - 4);

	rem.readU16();
	char* type = rem.readString();
	u16 key    = rem.readU16();
	char* name = rem.readString();
	if (!rem.isGood() || key != calcKey(name)) {
		bad++;
		return false;
	}
	++nodes;
	// try as group: s32 count + count children that exactly consume the rest
	int save = rem.getPosition();
	if (rem.getLength() - save >= 4) {
		s32 cnt = rem.readS32();
		if (cnt >= 0 && cnt < 100000) {
			bool ok = true;
			int n0 = nodes;
			for (s32 i = 0; i < cnt && ok; ++i)
				ok = walk(rem, depth + 1);
			if (ok && rem.getPosition() == rem.getLength()) {
				++groups;
				return true;
			}
			nodes = n0; // not a group after all, leaf data follows the header
		}
	}
	return true;
}

int main(int argc, char** argv)
{
	for (int i = 1; i < argc; ++i) {
		FILE* f = fopen(argv[i], "rb");
		if (!f) { printf("cannot open %s\n", argv[i]); return 2; }
		fseek(f, 0, SEEK_END);
		long n = ftell(f);
		fseek(f, 0, SEEK_SET);
		unsigned char* buf = (unsigned char*)malloc(n);
		fread(buf, 1, n, f);
		fclose(f);
		nodes = groups = bad = 0;
		JSUMemoryInputStream stream(buf, (s32)n);
		bool ok = walk(stream, 0) && stream.getPosition() == (int)n && bad == 0;
		printf("%s: %s nodes=%d groups=%d bad=%d\n", argv[i], ok ? "OK" : "FAIL", nodes, groups, bad);
		if (!ok) return 1;
	}
	return 0;
}
