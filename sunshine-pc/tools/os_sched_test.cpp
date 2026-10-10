// Self-check for src/sdk/os_thread.cpp + os_misc.cpp (strict-priority baton scheduler). Standalone: no Aurora, no game.
// Build and run (from sunshine-pc, LLVM_BIN set as for syntax.cmd):
//   tools\os_sched_test.cmd
// Exits 0 and prints "os_sched_test: ok" when every check passes; aborts with the failing line otherwise.
#include <dolphin/os.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

// os_misc.cpp needs the time base; Aurora provides it in the real build.
extern "C" OSTime OSGetTime(void) { return 0; }

#define SECTION(n) std::fprintf(stderr, "os_sched_test: section %d\n", n)

#define CHECK(c)                                                                                                       \
	do {                                                                                                               \
		if (!(c)) {                                                                                                    \
			std::fprintf(stderr, "os_sched_test: FAILED line %d: %s\nlog: %s\n", __LINE__, #c, gLog.c_str());          \
			std::abort();                                                                                              \
		}                                                                                                              \
	} while (0)

// Only the baton owner appends, so the log doubles as a check that two game threads never run at once.
static std::string gLog;
static void note(const char* s) { gLog += s; }

static OSThread tA, tB, tC;
static u8 stack[0x1000];

static void* hiPrio(void*)
{
	note("A1 ");
	OSYieldThread(); // nothing else at priority 5: keeps running
	note("A2 ");
	return reinterpret_cast<void*>(7);
}

static void* loPrio(void*)
{
	note("B ");
	return nullptr;
}

// receives messages sent by main; higher priority than main, so it must run inside every OSSendMessage
static OSMessageQueue gMq;
static OSMessage gMsgs[4];
static void* receiver(void*)
{
	for (;;) {
		OSMessage m;
		OSReceiveMessage(&gMq, &m, OS_MESSAGE_BLOCK);
		if (m == reinterpret_cast<OSMessage>(3))
			return nullptr;
		note(m == reinterpret_cast<OSMessage>(1) ? "r1 " : "r2 ");
	}
}

// two equal-priority threads ping-pong through OSYieldThread
static void* yielder(void* name)
{
	for (int i = 0; i < 3; i++) {
		note(static_cast<const char*>(name));
		OSYieldThread();
	}
	return nullptr;
}

static OSMutex gMutex;
static OSCond gCond;
static bool gReady;
static void* condWaiter(void*)
{
	OSLockMutex(&gMutex);
	while (!gReady)
		OSWaitCond(&gCond, &gMutex);
	note("cw ");
	OSUnlockMutex(&gMutex);
	return nullptr;
}

static OSThreadQueue gSleepQ;
static void* sleeper(void*)
{
	BOOL e = OSDisableInterrupts();
	note("s0 ");
	OSSleepThread(&gSleepQ); // woken by a host thread while every game thread is blocked (idle CPU)
	note("s1 ");
	OSRestoreInterrupts(e);
	return nullptr;
}

static void* forever(void*)
{
	OSThreadQueue q;
	OSInitThreadQueue(&q);
	BOOL e = OSDisableInterrupts();
	OSSleepThread(&q);
	OSRestoreInterrupts(e);
	note("forever-ran ");
	return nullptr;
}

// ping-pong over two one-slot queues: every message forces two thread switches
static OSMessageQueue gPing, gPong;
static OSMessage gPingSlot, gPongSlot;
constexpr int kRounds = 20000;
static void* pinger(void*)
{
	for (int i = 0; i < kRounds; i++) {
		OSSendMessage(&gPing, reinterpret_cast<OSMessage>(static_cast<intptr_t>(i)), OS_MESSAGE_BLOCK);
		OSMessage m;
		OSReceiveMessage(&gPong, &m, OS_MESSAGE_BLOCK);
		if (reinterpret_cast<intptr_t>(m) != i + 1)
			std::abort();
	}
	return nullptr;
}
static void* ponger(void*)
{
	for (;;) {
		OSMessage m;
		OSReceiveMessage(&gPing, &m, OS_MESSAGE_BLOCK);
		OSSendMessage(&gPong, reinterpret_cast<OSMessage>(reinterpret_cast<intptr_t>(m) + 1), OS_MESSAGE_BLOCK);
	}
}

static void exitDeep(int depth)
{
	if (depth == 0) {
		note("exit ");
		OSExitThread(reinterpret_cast<OSThread*>(5));
	}
	exitDeep(depth - 1);
	note("after-exit "); // must never run
}
static void* exiter(void*)
{
	exitDeep(3);
	return nullptr;
}

int main()
{
	// a deadlock in the scheduler must fail the check, not hang it
	std::thread([] {
		std::this_thread::sleep_for(std::chrono::seconds(60));
		std::fprintf(stderr, "os_sched_test: TIMEOUT (deadlock)\nlog: %s\n", gLog.c_str());
		std::abort();
	}).detach();
	CHECK(OSGetCurrentThread() != nullptr && OSGetThreadPriority(OSGetCurrentThread()) == 16);

	// 1. priority order: A (5) preempts main (16) on resume, B (20) only runs once main blocks in OSJoinThread
	SECTION(1);
	OSCreateThread(&tA, hiPrio, nullptr, stack + sizeof(stack), sizeof(stack), 5, 0);
	OSCreateThread(&tB, loPrio, nullptr, stack + sizeof(stack), sizeof(stack), 20, 0);
	OSResumeThread(&tB);
	note("m1 ");
	OSResumeThread(&tA);
	note("m2 ");
	s32 val = 0;
	CHECK(OSJoinThread(&tA, &val) == 1 && val == 7); // A is already terminated
	note("m3 ");
	CHECK(OSJoinThread(&tB, nullptr) == 1);          // blocks main, B runs
	CHECK(gLog == "m1 A1 A2 m2 m3 B ");
	CHECK(OSIsThreadTerminated(&tA) && OSIsThreadTerminated(&tB));

	// 2. message queue: the higher-priority receiver runs inside each send
	SECTION(2);
	gLog.clear();
	OSInitMessageQueue(&gMq, gMsgs, 4);
	OSCreateThread(&tA, receiver, nullptr, stack + sizeof(stack), sizeof(stack), 8, 0);
	OSResumeThread(&tA);
	note("w ");
	OSSendMessage(&gMq, reinterpret_cast<OSMessage>(1), OS_MESSAGE_NOBLOCK);
	note("m ");
	OSSendMessage(&gMq, reinterpret_cast<OSMessage>(2), OS_MESSAGE_NOBLOCK);
	OSSendMessage(&gMq, reinterpret_cast<OSMessage>(3), OS_MESSAGE_NOBLOCK);
	CHECK(OSJoinThread(&tA, nullptr) == 1);
	CHECK(gLog == "w r1 m r2 ");
	// jam goes to the front
	OSSendMessage(&gMq, reinterpret_cast<OSMessage>(1), OS_MESSAGE_NOBLOCK);
	OSJamMessage(&gMq, reinterpret_cast<OSMessage>(2), OS_MESSAGE_NOBLOCK);
	OSMessage m;
	CHECK(OSReceiveMessage(&gMq, &m, OS_MESSAGE_NOBLOCK) == 1 && m == reinterpret_cast<OSMessage>(2));
	CHECK(OSReceiveMessage(&gMq, &m, OS_MESSAGE_NOBLOCK) == 1 && m == reinterpret_cast<OSMessage>(1));
	CHECK(OSReceiveMessage(&gMq, &m, OS_MESSAGE_NOBLOCK) == 0);

	// 3. equal priority round robin through yield
	SECTION(3);
	gLog.clear();
	OSCreateThread(&tA, yielder, const_cast<char*>("a "), stack + sizeof(stack), sizeof(stack), 10, 0);
	OSCreateThread(&tB, yielder, const_cast<char*>("b "), stack + sizeof(stack), sizeof(stack), 10, 0);
	OSDisableScheduler(); // make both ready before either runs, as when a higher-priority thread wakes two workers
	OSResumeThread(&tA);
	OSResumeThread(&tB);
	OSEnableScheduler();
	OSJoinThread(&tA, nullptr);
	OSJoinThread(&tB, nullptr);
	CHECK(gLog == "a b a b a b ");

	// 4. condition variable + mutex
	SECTION(4);
	gLog.clear();
	OSInitMutex(&gMutex);
	OSInitCond(&gCond);
	OSCreateThread(&tA, condWaiter, nullptr, stack + sizeof(stack), sizeof(stack), 10, 0);
	OSResumeThread(&tA); // runs, waits on the cond
	CHECK(OSTryLockMutex(&gMutex) == TRUE); // the waiter released it
	gReady = true;
	OSSignalCond(&gCond);
	note("sig ");
	OSUnlockMutex(&gMutex); // waiter (10) takes the mutex now
	OSJoinThread(&tA, nullptr);
	CHECK(gLog == "sig cw ");

	// 5. host thread wakes a sleeper while the CPU is idle, and a message queue from a host thread
	SECTION(5);
	gLog.clear();
	OSInitThreadQueue(&gSleepQ);
	OSCreateThread(&tA, sleeper, nullptr, stack + sizeof(stack), sizeof(stack), 10, 0);
	OSResumeThread(&tA); // sleeps with interrupts disabled (restored when woken)
	std::thread host([] {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		BOOL e = OSDisableInterrupts();
		OSWakeupThread(&gSleepQ); // idle: dispatches the sleeper from this thread
		OSRestoreInterrupts(e);
	});
	CHECK(OSJoinThread(&tA, nullptr) == 1); // main blocks => nothing ready => idle until the host wakes A
	host.join();
	CHECK(gLog == "s0 s1 ");

	// 6. host wake of a higher-priority thread preempts the running main thread at its next interrupt-enable point
	SECTION(6);
	gLog.clear();
	OSInitThreadQueue(&gSleepQ);
	OSCreateThread(&tA, sleeper, nullptr, stack + sizeof(stack), sizeof(stack), 3, 0);
	OSResumeThread(&tA);
	std::atomic<bool> woken{ false };
	std::thread host2([&] {
		BOOL e = OSDisableInterrupts();
		OSWakeupThread(&gSleepQ); // main is running: only marks the reschedule
		OSRestoreInterrupts(e);
		woken = true;
	});
	while (!woken) // main spins; each OSRestoreInterrupts is a preemption point
		std::this_thread::yield();
	host2.join();
	BOOL e = OSDisableInterrupts();
	OSRestoreInterrupts(e);
	CHECK(gLog == "s0 s1 ");
	OSJoinThread(&tA, nullptr);

	// 7. cancelling a parked thread ends it without ever running it again
	SECTION(7);
	gLog.clear();
	OSCreateThread(&tC, forever, nullptr, stack + sizeof(stack), sizeof(stack), 10, 0);
	OSResumeThread(&tC); // sleeps forever
	OSCancelThread(&tC);
	CHECK(OSIsThreadTerminated(&tC));
	OSJoinThread(&tC, nullptr);
	std::this_thread::sleep_for(std::chrono::milliseconds(50)); // would show "forever-ran" if the native thread resumed
	CHECK(gLog.empty());

	// 8. stopwatch + modes, interrupt flag semantics
	SECTION(8);
	CHECK(OSDisableInterrupts() == TRUE);
	CHECK(OSDisableInterrupts() == FALSE);
	CHECK(OSRestoreInterrupts(FALSE) == FALSE);
	CHECK(OSRestoreInterrupts(TRUE) == FALSE);
	CHECK(OSEnableInterrupts() == TRUE);
	OSSetSoundMode(0);
	CHECK(OSGetSoundMode() == 0);

	// 9. stress: ping-pong between two worker threads while a host thread hammers the interrupt lock and wakes
	SECTION(9);
	OSInitMessageQueue(&gPing, &gPingSlot, 1);
	OSInitMessageQueue(&gPong, &gPongSlot, 1);
	OSCreateThread(&tA, pinger, nullptr, stack + sizeof(stack), sizeof(stack), 8, 0);
	OSCreateThread(&tB, ponger, nullptr, stack + sizeof(stack), sizeof(stack), 9, 0);
	std::atomic<bool> stop{ false };
	OSThreadQueue noise;
	OSInitThreadQueue(&noise);
	std::thread host3([&] {
		while (!stop) {
			BOOL en = OSDisableInterrupts();
			OSWakeupThread(&noise);
			OSRestoreInterrupts(en);
		}
	});
	OSResumeThread(&tB);
	OSResumeThread(&tA);
	CHECK(OSJoinThread(&tA, nullptr) == 1);
	stop = true;
	host3.join();
	OSCancelThread(&tB); // ponger is blocked in OSReceiveMessage
	CHECK(OSJoinThread(&tB, nullptr) == 1);

	// 10. explicit OSExitThread deep in a call chain ends the thread there; the exit value is delivered
	SECTION(10);
	gLog.clear();
	OSCreateThread(&tA, exiter, nullptr, stack + sizeof(stack), sizeof(stack), 10, 0);
	OSResumeThread(&tA);
	val = 0;
	CHECK(OSJoinThread(&tA, &val) == 1 && val == 5);
	CHECK(gLog == "exit ");

	// 11. one OSThread object reused for many short-lived threads, detached and joinable alternately
	SECTION(11);
	for (int i = 0; i < 300; i++) {
		bool detach = i & 1;
		OSCreateThread(&tA, loPrio, nullptr, stack + sizeof(stack), sizeof(stack), 10, detach ? OS_THREAD_ATTR_DETACH : 0);
		OSResumeThread(&tA); // priority 10 beats main: the thread has already run and exited when this returns
		CHECK(OSIsThreadTerminated(&tA));
		if (!detach)
			CHECK(OSJoinThread(&tA, nullptr) == 1);
	}

	std::printf("os_sched_test: ok\n");
	return 0;
}
