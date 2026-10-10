// Headless check of the EventWatcher SPCB host bindings on real disc scripts (sms/src/System/EventWatcher.cpp,
// TARGET_PC handle table in spcinterp.cpp). For every .sb in the scene archive it runs the real binary init (which
// binds the watcher's natives) and then, for every script name listed in the table, prints whether the symbol is
// bound, the 64-bit callback address its handle resolves to, and the result of one call with zero-valued arguments.
// Usage: spc_bind_test <scene.szs> <table.txt>   table lines: "<script name> <argc>" (tools\spc_bind_test.py writes it).
// Output lines: IMAGE, UNBOUND, BOUND, CALL, SB (see spc_bind_test.py for the checks applied to them).
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#define protected public
#define private public
#include <JSystem/JKernel/JKRDecomp.hpp>
#include <JSystem/JKernel/JKRMemArchive.hpp>
#include <Strategic/spcinterp.hpp>
#include <System/EventWatcher.hpp>
#undef private
#undef protected

static void collect(JKRMemArchive* a, u32 dirIdx, const std::string& prefix, std::vector<std::string>& out)
{
	JKRArchive::SDIDirEntry* dir = a->mDirectories + dirIdx;
	for (u32 i = 0; i < dir->mNum; i++) {
		JKRArchive::SDIFileEntry* f = a->mFileEntries + dir->mFirstIdx + i;
		std::string name            = a->mStrTable + (f->mFlagsAndNameOffset & 0xFFFFFF);
		if (f->mFlagsAndNameOffset >> 24 & 2) {
			if (name != "." && name != "..")
				collect(a, f->mDataOffset, prefix + "/" + name, out);
		} else {
			out.push_back(prefix + "/" + name);
		}
	}
}

struct TableEntry {
	std::string name;
	u32 argc;
};

static uintptr_t sImageBase, sImageEnd;

// Filter for the last fault: records where it happened and the address it touched (AV only).
static void* sFaultRip;
static ULONG_PTR sFaultAddr;

static int recordFault(EXCEPTION_POINTERS* p)
{
	sFaultRip = p->ExceptionRecord->ExceptionAddress;
	sFaultAddr = p->ExceptionRecord->NumberParameters > 1 ? p->ExceptionRecord->ExceptionInformation[1] : 0;
	return EXCEPTION_EXECUTE_HANDLER;
}

// Runs one native call. Kept free of C++ objects so the SEH frame is legal.
static DWORD guardedDispatch(TSpcTypedInterp<TEventWatcher>* interp, u32 sym_index, u32 argc)
{
	__try {
		interp->dispatchBuiltin(sym_index, argc);
		return 0;
	} __except (recordFault(GetExceptionInformation())) {
		return GetExceptionCode();
	}
}

static void testSb(const std::string& path, void* res, const std::vector<TableEntry>& table)
{
	TSpcTypedBinary<TEventWatcher>* bin = new TSpcTypedBinary<TEventWatcher>(res);
	bin->init();
	TSpcHeader* h = bin->getHeader();
	std::printf("SB %s %u\n", path.c_str(), h->mSymbolNum);

	std::map<std::string, u32> index;
	for (u32 i = 0; i < h->mSymbolNum; ++i)
		index.emplace(bin->getSymbolName(bin->getSymbol(i)), i);

	for (const TableEntry& e : table) {
		auto it = index.find(e.name);
		if (it == index.end())
			continue;
		TSpcSymbol* sym = bin->getSymbol(it->second);
		if (sym->mNativeCall == 0) {
			std::printf("UNBOUND %s %s %u\n", path.c_str(), e.name.c_str(), it->second);
			continue;
		}
		uintptr_t fn = TSpcBinary::getNativeCall(sym->mNativeCall);
		std::printf("BOUND %s %s %u %u %llx %d\n", path.c_str(), e.name.c_str(), it->second, sym->mNativeCall,
		            (unsigned long long)fn, fn >= sImageBase && fn < sImageEnd ? 1 : 0);

		TSpcTypedInterp<TEventWatcher>* interp = new TSpcTypedInterp<TEventWatcher>(bin, nullptr, 0x20, 0x20, 0x20, 0x20);
		for (u32 k = 0; k < e.argc; ++k)
			interp->push((int)0);
		DWORD code = guardedDispatch(interp, it->second, e.argc);
		if (code == 0)
			std::printf("CALL %s %s ok\n", path.c_str(), e.name.c_str());
		else
			std::printf("CALL %s %s fault 0x%08lx rip=%p addr=%p\n", path.c_str(), e.name.c_str(), (unsigned long)code, sFaultRip, (void*)sFaultAddr);
	}
}

int main(int argc, char** argv)
{
	if (argc < 3)
		return 2;
	std::setvbuf(stdout, nullptr, _IONBF, 0);

	auto base = (const u8*)GetModuleHandleA(nullptr);
	auto nt   = (const IMAGE_NT_HEADERS*)(base + ((const IMAGE_DOS_HEADER*)base)->e_lfanew);
	sImageBase = (uintptr_t)base;
	sImageEnd  = sImageBase + nt->OptionalHeader.SizeOfImage;
	std::printf("IMAGE %llx %llx\n", (unsigned long long)sImageBase, (unsigned long long)sImageEnd);

	std::vector<TableEntry> table;
	{
		FILE* f = std::fopen(argv[2], "r");
		if (!f)
			return 2;
		char name[128];
		unsigned n;
		while (std::fscanf(f, "%127s %u", name, &n) == 2)
			table.push_back({name, n});
		std::fclose(f);
	}

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
		JKRDecomp::decodeSZS(in.data(), out.data(), n, 0);
		out.resize(n);
	}
	JKRMemArchive* a = new JKRMemArchive(out.data(), (u32)out.size(), MBF_0);
	std::vector<std::string> files;
	collect(a, 0, "", files);
	for (auto& path : files) {
		if (path.size() >= 3 && path.compare(path.size() - 3, 3, ".sb") == 0)
			testSb(path, a->getResource(path.c_str()), table);
	}
	return 0;
}
