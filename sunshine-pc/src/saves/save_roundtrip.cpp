// SUNSHINE_DEV_CARD_TEST: drives the decomp's TCardManager save and load path with the same calls as saveBookmark()
// (GC2D/CardSave.cpp) and the title-screen read (System/Application.cpp), so the .gci written here is the real one.
#include "saves.hpp"

#include <System/Application.hpp>
#include <System/CardManager.hpp>
#include <System/FlagManager.hpp>
#include <aurora/aurora.h>
#include <dolphin/card.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

// Int flags the title flow seeds. The shine count 0x40000 is not independent state: the game bumps it per collected
// shine (FlagManager.cpp:349) and correctFlag() recomputes it from the shine bits on load, so the test sets real bits.
constexpr u32 kFlags[]   = { 0x20002, 0x20003, 0x20014 };
constexpr s32 kValues[]  = { 1234, 5678, 9012 };
constexpr size_t kCount  = sizeof(kFlags) / sizeof(kFlags[0]);
constexpr u32 kShineBase = 0x10000;
constexpr s32 kShines    = 42;

// Polls the card worker until it leaves BUSY. Returns the last status (BUSY only on timeout).
s32 waitForCard()
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	s32 status          = gpCardManager->getLastStatus();
	while (status == CARD_RESULT_BUSY && std::chrono::steady_clock::now() < deadline) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		status = gpCardManager->getLastStatus();
	}
	return status;
}

} // namespace

int savesRoundTripTest()
{
	TFlagManager* flags = TFlagManager::getInstance();
	for (size_t i = 0; i < kCount; ++i)
		flags->setFlag(kFlags[i], kValues[i]);
	for (s32 i = 0; i < kShines; ++i)
		flags->setFlag(kShineBase + i, 1);
	flags->setFlag(0x40000, kShines);

	// Title-screen read (CardLoad.cpp). writeBlock_ refuses (-0x80) until block 0 has been read once.
	// On a fresh card the read returns NOFILE; the game then calls createFile() and reads again.
	gpCardManager->readBlock(0);
	s32 status = waitForCard();
	if (status == CARD_RESULT_NOFILE) {
		std::printf("[card-test] no save yet, creating file\n");
		gpCardManager->createFile();
		status = waitForCard();
		if (status != CARD_RESULT_READY) {
			std::printf("[card-test] FAIL: create status %d\n", status);
			return 1;
		}
		gpCardManager->readBlock(0);
		status = waitForCard();
	}
	if (status != CARD_RESULT_READY) {
		std::printf("[card-test] FAIL: pre-write read status %d\n", status);
		return 1;
	}

	// saveBookmark(0): fill the sector through the write stream, then write block 0.
	JSUMemoryOutputStream out;
	gpCardManager->getWriteStream(&out);
	flags->save(out);
	gpCardManager->writeBlock(0);
	if (s32 status = waitForCard(); status != CARD_RESULT_READY) {
		std::printf("[card-test] FAIL: write status %d\n", status);
		return 1;
	}

	// Clobber the flags so a read that does nothing cannot pass.
	for (size_t i = 0; i < kCount; ++i)
		flags->setFlag(kFlags[i], 0);
	for (s32 i = 0; i < kShines; ++i)
		flags->setFlag(kShineBase + i, 0);
	flags->setFlag(0x40000, 0);

	// Title-screen read: read block 0, then load from the read stream.
	gpCardManager->readBlock(0);
	if (s32 status = waitForCard(); status != CARD_RESULT_READY) {
		std::printf("[card-test] FAIL: read status %d\n", status);
		return 1;
	}
	JSUMemoryInputStream in;
	gpCardManager->getReadStream(&in);
	flags->load(in);

	int failures = 0;
	for (size_t i = 0; i < kCount; ++i) {
		const s32 got = flags->getFlag(kFlags[i]);
		const bool ok = got == kValues[i];
		std::printf("[card-test] flag 0x%X wrote %d read %d %s\n", kFlags[i], kValues[i], got, ok ? "ok" : "MISMATCH");
		failures += ok ? 0 : 1;
	}
	// Shine bits come back from mCardBools; the count is recomputed from them by correctFlag() on load.
	const bool bitsOk = flags->getFlag(kShineBase + kShines - 1) != 0 && flags->getFlag(kShineBase + kShines) == 0;
	const s32 shineCount = flags->getFlag(0x40000);
	const bool countOk = shineCount == kShines;
	std::printf("[card-test] shine bits 0x%X..0x%X %s, count 0x40000 wrote %d read %d %s\n", kShineBase,
	            kShineBase + kShines - 1, bitsOk ? "ok" : "MISMATCH", kShines, shineCount, countOk ? "ok" : "MISMATCH");
	failures += (bitsOk && countOk) ? 0 : 1;
	std::printf("[card-test] %s\n", failures == 0 ? "PASS" : "FAIL");
	return failures == 0 ? 0 : 1;
}

// Called from TApplication::proc() (sms/src/System/Application.cpp) right after initialize_bootAfter(), which is the
// first point where gpCardManager exists. Runs the round trip once, shuts Aurora down, and exits with its result.
void sunshineBootAfterHook()
{
	static bool done = false;
	if (done || std::getenv("SUNSHINE_DEV_CARD_TEST") == nullptr)
		return;
	done         = true;
	const int rc = savesRoundTripTest();
	std::exit(rc);
}
