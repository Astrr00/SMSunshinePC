// Global operator new/delete for the PC port.
//
// The decomp allocates game objects two ways: `new T` (the console routes this to the current JKR heap) and
// `new (heap, align) T` (explicit JKR heap, defined in JSystem/JKernel/JKRHeap.cpp). It frees both with plain `delete`,
// which on the console always returns the block to the JKR heap that owns it.
//
// On the host, Aurora, SDL and Dawn also call global new/delete before and while the game runs, so plain `new` must
// keep using the CRT heap. `delete` therefore has to tell the two kinds of memory apart: a pointer inside the emulated
// MEM1 window (the OS arena the JKR heaps are carved from) belongs to a JKR heap, anything else belongs to the CRT.
// Without this, `delete` on a JKR-placement-allocated object hands a MEM1 address to free() and corrupts the CRT heap.
//
// Plain `new` stays on the CRT; converting those sites to JKR heaps is only needed if a game feature depends on
// JKR heap statistics or limits, not for correctness.
//
// Plain `new` returns zeroed memory. Some game object (not identified) reads memory its
// constructor never wrote: with CRT heap garbage a bianco/0 start showed a flat-colour screen in 8 of 14 runs, with any
// uniform fill (0x00/0xAA/0xFF/0x7F) in 0 of 24 (ROADMAP update 2026-10-13, wedge).
#include <JSystem/JKernel/JKRHeap.hpp>
#include <dolphin/os.h>
#include <sunshine_config.h>

#include <cstdint>
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <mutex>

static bool isJkrMemory(void* p)
{
	// MEM1Start is null before OSInit, which makes every pointer fall outside the window.
	const uintptr_t base = (uintptr_t)OSPhysicalToCached(0);
	return base != 0 && (uintptr_t)p - base < kSunshineMem1Size;
}

// Debug mode (SUNSHINE_CHECK_HEAP=1): every `new` block gets a 32-byte header and a 16-byte canary footer, frees are
// validated (unknown pointer, double free, overflow past the end) with a symbolized stack, and freed blocks are
// poisoned and quarantined so a late write through a dangling pointer shows up at the next check. For hunting CRT
// heap corruption (STATUS_HEAP_CORRUPTION shows up far from the culprit).
void sunshineReportAndAbort(const char* message); // crash.cpp

namespace {

constexpr uint64_t kLiveMagic = 0x4C49564544425F31ull;  // "LIVEDB_1"
constexpr uint64_t kFreedMagic = 0x46524545445F5F31ull; // "FREED__1"
constexpr size_t kHeader = 32, kFooter = 16, kQuarantine = 8192;

struct Header {
	uint64_t magic;
	uint64_t size;
	void* callers[2];
};

bool checkHeap()
{
	static const bool on = std::getenv("SUNSHINE_CHECK_HEAP") != nullptr;
	return on;
}

// SUNSHINE_CHECK_HEAP=2: page-heap style. Each block ends flush with a PAGE_NOACCESS page, so an overflow faults at the
// overrunning instruction; freed blocks are made PAGE_NOACCESS (kept reserved, released after a long quarantine), so a
// use after free faults too.
bool pageHeap()
{
	static const bool on = [] {
		const char* v = std::getenv("SUNSHINE_CHECK_HEAP");
		return v && v[0] == '2';
	}();
	return on;
}

struct PageBlock {
	void* base;
	size_t total;
};

void* pageAlloc(size_t size)
{
	constexpr size_t kPage = 4096;
	const size_t payload   = (size + 15) & ~(size_t)15;
	const size_t pages     = (payload + sizeof(PageBlock) + kPage - 1) / kPage;
	const size_t total     = (pages + 1) * kPage; // + guard page
	uint8_t* base = (uint8_t*)VirtualAlloc(nullptr, total, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!base)
		return nullptr;
	DWORD old;
	VirtualProtect(base + pages * kPage, kPage, PAGE_NOACCESS, &old);
	uint8_t* user = base + pages * kPage - payload;
	PageBlock* blk = (PageBlock*)(user - sizeof(PageBlock));
	blk->base  = base;
	blk->total = total;
	return user;
}

std::mutex quarantineMutex; // ponytail: one lock for both debug rings; debug-only path, contention does not matter
constexpr size_t kPageQuarantine = 20000;
PageBlock pageQuarantine[kPageQuarantine];
size_t pageQuarantineNext = 0;

void pageFree(void* user)
{
	PageBlock* blk = (PageBlock*)((uint8_t*)user - sizeof(PageBlock));
	PageBlock mine = *blk;
	DWORD old;
	VirtualProtect(mine.base, mine.total, PAGE_NOACCESS, &old);
	std::lock_guard lock{quarantineMutex};
	PageBlock& slot = pageQuarantine[pageQuarantineNext++ % kPageQuarantine];
	if (slot.base)
		VirtualFree(slot.base, 0, MEM_RELEASE);
	slot = mine;
}

void* quarantine[kQuarantine];
size_t quarantineNext = 0;

void verifyFooter(Header* h, const char* what)
{
	const uint8_t* footer = (const uint8_t*)(h + 1) + 0; // placeholder to keep types honest below
	(void)footer;
	const uint8_t* user = (const uint8_t*)h + kHeader;
	for (size_t i = 0; i < kFooter; i++) {
		if (user[h->size + i] != 0xFD) {
			char msg[160];
			std::snprintf(msg, sizeof(msg), "heap check: %s: block %p size %llu overflowed (footer byte %zu = %02x)", what,
			              (const void*)user, (unsigned long long)h->size, i, user[h->size + i]);
			sunshineReportAndAbort(msg);
		}
	}
}

// Diagnosis (SUNSHINE_DEV_GARBAGE_NEW=lo-hi, bytes): plain `new` blocks of that size come from malloc unzeroed (stale CRT contents), to
// bisect which object class reads uninitialised memory (docs/platform-notes.md, "port zero fill").
bool garbageRange(size_t size)
{
	static size_t lo = 1, hi = 0;
	static const bool on = [] {
		const char* e = std::getenv("SUNSHINE_DEV_GARBAGE_NEW");
		return e && std::sscanf(e, "%zu-%zu", &lo, &hi) == 2;
	}();
	return on && size >= lo && size <= hi;
}

} // namespace

void* operator new(size_t size)
{
	for (;;) {
		if (pageHeap()) {
			if (void* p = pageAlloc(size ? size : 1))
				return p;
		} else if (checkHeap()) {
			uint8_t* raw = (uint8_t*)std::malloc(kHeader + size + kFooter);
			if (raw) {
				Header* h = (Header*)raw;
				h->magic  = kLiveMagic;
				h->size   = size;
				std::memset(raw + kHeader, 0, size); // same zeroed contents as the plain path
				std::memset(raw + kHeader + size, 0xFD, kFooter);
				return raw + kHeader;
			}
		} else if (garbageRange(size)) {
			if (void* p = std::malloc(size ? size : 1))
				return p; // CRT garbage, as before the zero fill
		} else if (void* p = std::calloc(1, size ? size : 1)) {
			return p;
		}
		std::new_handler handler = std::get_new_handler();
		if (!handler)
			std::abort(); // ponytail: exceptions are disabled in the port; OOM is fatal, as an uncaught bad_alloc would be
		handler();
	}
}

void* operator new[](size_t size) { return operator new(size); }

void operator delete(void* p) noexcept
{
	if (p == nullptr)
		return;
	if (isJkrMemory(p)) {
		JKRHeap::free(p, nullptr);
	} else if (pageHeap()) {
		pageFree(p);
	} else if (checkHeap()) {
		Header* h = (Header*)((uint8_t*)p - kHeader);
		if (h->magic == kFreedMagic)
			sunshineReportAndAbort("heap check: double delete");
		if (h->magic != kLiveMagic)
			sunshineReportAndAbort("heap check: delete of a pointer that did not come from operator new");
		verifyFooter(h, "delete");
		h->magic = kFreedMagic;
		std::memset((uint8_t*)p, 0xDD, (size_t)h->size);
		// Quarantine: re-verify the oldest freed block (poison intact => no write after free) before really freeing it.
		std::lock_guard lock{quarantineMutex}; // operator delete runs on every thread; the ring must not race
		void*& slot = quarantine[quarantineNext++ % kQuarantine];
		if (slot) {
			Header* old = (Header*)slot;
			const uint8_t* user = (const uint8_t*)old + kHeader;
			for (size_t i = 0; i < old->size; i++) {
				if (user[i] != 0xDD) {
					char msg[160];
					std::snprintf(msg, sizeof(msg), "heap check: write after free, block %p size %llu offset %zu", (const void*)user,
					              (unsigned long long)old->size, i);
					sunshineReportAndAbort(msg);
				}
			}
			std::free(old);
		}
		slot = h;
	} else {
		std::free(p);
	}
}

void operator delete[](void* p) noexcept { operator delete(p); }

// Sized deallocation (C++14): the size is not needed, forward to the plain forms.
void operator delete(void* p, size_t) noexcept { operator delete(p); }
void operator delete[](void* p, size_t) noexcept { operator delete(p); }
