// Native check of the RARC + Yaz0/Yay0 host path (JKRDecomp, JKRMemArchive, JKRArchive finders).
// Usage: rarc_test <archive.szs|.szp|.arc>   (driven by tools\verify-rarc.py; build with rarc_test.cmd)
// Prints the decoded size/CRC, then one line per directory and per file of the mounted archive:
// "D <path>" and "F <path> <size> <crc32>" (a .bti file carries its header swapped by JKRArchive::hostSwapFetchedFile). verify-rarc.py compares all of it with an independent Python decoder/parser.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

// The test walks the archive's tables directly (like the finders do), so open up the class.
#define protected public
#define private public
#include <JSystem/JKernel/JKRDecomp.hpp>
#include <JSystem/JKernel/JKRMemArchive.hpp>
#undef private
#undef protected

extern "C" int ripperDvd(u8*, u8*, u32);
extern "C" int ripperAram(u8*, u8*, u32);
extern "C" int ripperDvdAram(u8*, u8*, u32);

static u32 crc32(const u8* p, size_t n)
{
	u32 c = 0xFFFFFFFF;
	for (size_t i = 0; i < n; i++) {
		c ^= p[i];
		for (int k = 0; k < 8; k++)
			c = (c >> 1) ^ (0xEDB88320 & (0 - (c & 1)));
	}
	return ~c;
}

static void walk(JKRMemArchive* a, u32 dirIdx, const std::string& prefix)
{
	JKRArchive::SDIDirEntry* dir = a->mDirectories + dirIdx;
	for (u32 i = 0; i < dir->mNum; i++) {
		JKRArchive::SDIFileEntry* f = a->mFileEntries + dir->mFirstIdx + i;
		std::string name            = a->mStrTable + (f->mFlagsAndNameOffset & 0xFFFFFF);
		std::string path            = prefix + "/" + name;
		if (f->mFlagsAndNameOffset >> 24 & 2) {
			if (name == "." || name == "..")
				continue;
			std::printf("D %s\n", path.c_str());
			walk(a, f->mDataOffset, path);
		} else {
			void* data = a->getResource(path.c_str()); // exercises findFsResource, hashing, CArcName
			s32 size   = data ? a->getResSize(data) : -1;
			u32 crc    = data ? crc32((u8*)data, size) : 0;
			// fetching again (cached pointer) and reading into a caller buffer must not swap a .bti header twice
			void* again = a->getResource(path.c_str());
			std::vector<u8> buf(data ? (((size_t)size + 0x3F) & ~(size_t)0x1F) : 0x20, 0);
			if (data)
				a->readResource(buf.data(), (u32)buf.size(), path.c_str());
			bool same = again == data && data && crc32((u8*)again, size) == crc && crc32(buf.data(), size) == crc;
			std::printf("F %s %d %08x%s\n", path.c_str(), size, crc, same ? "" : " MISMATCH");
		}
	}
}

int main(int argc, char** argv)
{
	if (argc < 2)
		return 2;
	std::vector<u8> in;
	{
		FILE* f = std::fopen(argv[1], "rb");
		if (!f)
			return 2;
		u8 buf[65536];
		size_t n;
		while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
			in.insert(in.end(), buf, buf + n);
		std::fclose(f);
	}

	std::vector<u8> out;
	JKRCompression c = JKRDecomp::checkCompressed(in.data());
	if (c == JKR_COMPRESSION_NONE) {
		out = in;
	} else {
		u32 n = JKRDecompExpandSize(in.data());
		out.assign(n + 32, 0);
		if (c == JKR_COMPRESSION_YAZ0)
			JKRDecomp::decodeSZS(in.data(), out.data(), n, 0);
		else
			JKRDecomp::decodeSZP(in.data(), out.data(), n, 0);
		out.resize(n);
		std::printf("decoded %u %08x\n", n, crc32(out.data(), n));
		if (c == JKR_COMPRESSION_YAZ0) { // the three file-static stream decoders must produce the same bytes
			int (*ripper[3])(u8*, u8*, u32) = { ripperDvd, ripperAram, ripperDvdAram };
			for (auto fn : ripper) {
				std::vector<u8> o(n + 32, 0);
				fn(in.data(), o.data(), n);
				std::printf("ripper %08x\n", crc32(o.data(), n));
			}
		}
	}

	JKRMemArchive* a = new JKRMemArchive(out.data(), (u32)out.size(), MBF_0);
	std::printf("archive nodes %u files %u volume %s\n", a->mArcInfoBlock->num_nodes, a->mArcInfoBlock->num_file_entries,
	            a->mVolumeName);
	walk(a, 0, "");

	// Mounting the same (already swapped) buffer again must not swap twice.
	JKRMemArchive* b = new JKRMemArchive(out.data(), (u32)out.size(), MBF_0);
	std::printf("remount files %u first %s\n", b->mArcInfoBlock->num_file_entries,
	            b->mStrTable + (b->mFileEntries[0].mFlagsAndNameOffset & 0xFFFFFF));
	return 0;
}
