// OS layer, part 1: threads, interrupts, mutex/cond, message queues (the decomp's OSThread.c, OSMutex.c, OSMessage.c).
//
// SCHEDULING DESIGN: baton-gated native threads, one strict-priority scheduler
// ---------------------------------------------------------------------------
// Every OSThread is backed by a native Windows thread, but only the thread named by `gCurrent` (the baton) may run
// game code. The run queues, priority inheritance through mutexes, suspend counts, joins and so on are the decomp's
// own algorithms (src/dolphin/os/OSThread.c, OSMutex.c, OSMessage.c); only the context switch is replaced: instead
// of OSSaveContext/OSLoadContext, SelectThread() hands the baton to the next thread's condition variable and parks
// the caller. A thread therefore loses the CPU exactly when it would on the console: when it blocks (OSSleepThread
// and everything built on it), yields, suspends/cancels/exits, or when a higher-priority thread becomes ready.
// The game depends on this ordering (setup thread at priority 17 only runs while the main thread at 16 is blocked;
// the DVD/ARAM/audio workers run to completion before the thread that woke them continues), which is why free-running
// threads (Dusklight's approach) were rejected: they would turn those hand-offs into data races.
//
// OSDisableInterrupts/OSEnableInterrupts/OSRestoreInterrupts keep the console meaning "no interrupt handler runs":
// disabled == the calling native thread holds `gM`. The flag is per native thread (like MSR[EE] is per context),
// so a thread parked inside a disabled region stays disabled when it is resumed. Host threads that emulate interrupt
// sources (AI DMA, VI retrace, DVD completion) must bracket their callbacks with OSDisableInterrupts()/
// OSRestoreInterrupts(); doing so also excludes them from every game thread's disabled region.
// They may call the non-blocking OS API there (OSWakeupThread, OSSendMessage(NOBLOCK), mutex-free code); blocking
// calls (OSSleepThread, OSLockMutex contended, OSReceiveMessage(BLOCK), ...) from a non-game thread are a panic.
//
// DETERMINISM CEILING
//  * Order among game threads is a pure function of the sequence of OS calls and of host events: no time slicing
//    (the console has none either), ties go FIFO within a priority exactly as on the console.
//  * A wake issued by a host thread cannot preempt asynchronously: the running game thread notices it at its next
//    interrupt-enable point (OSRestoreInterrupts/OSEnableInterrupts, hence every mutex, message-queue and heap
//    operation) or at its next blocking call. On the console the preemption is immediate. A game thread running a
//    long computation without any OS call therefore delays a higher-priority thread woken by the host.
//  * If nothing is ready the CPU is idle and the next host wake dispatches directly from the host thread.
//  * Host interrupt callbacks run truly in parallel with game code outside disabled regions (on the console they
//    preempt it); code that relied on "the handler cannot run now" must be inside OSDisableInterrupts.
//  * A game thread that waits on a host event with a host primitive (std::atomic::wait, Sleep) keeps the baton and
//    starves every lower-priority game thread. Waits for host events must use OSSleepThread/OSWakeupThread.
//
// OTHER PORT DECISIONS
//  * The guest stack passed to OSCreateThread is never dereferenced (callers truncate it to 32 bits); native threads
//    get their own 4 MiB stack. stackBase/stackEnd are only recorded because JKRThread reads them.
//  * OSExitThread/OSCancelThread end the native thread without unwinding, like the console discards the context.
//  * The native record of a thread is a pointer kept in the unused 8-byte slot OSThread::context.fpr[0]. Records are
//    leaked on purpose (a few hundred bytes per OSCreateThread), which makes reuse of an OSThread object safe while
//    its previous native thread is still leaving.
//  * The process main thread (the one running static initialization) is the DefaultThread, priority 16.
//    If the game runs elsewhere, call __OSThreadInit() from that thread before any other OS call.
//  * OSMessage slots are pointer sized (see OSReceiveMessage).
#ifndef _WIN32
#error "os_thread.cpp starts and ends native threads with the Win32 CRT; port it before building elsewhere"
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <process.h>

#include <dolphin/base/PPCArch.h>
#include <dolphin/os.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <new>
#include <mutex>

namespace {

constexpr unsigned kNativeStackBytes = 4u << 20;
constexpr s32 kDefaultThreadPriority = 16;

// Native side of one OSThread. Never freed (see header comment).
struct Rec {
	OSThread* os = nullptr;
	void* (*func)(void*) = nullptr;
	void* param = nullptr;
	std::condition_variable cv; // signalled when the baton is handed to this thread or the thread is killed
	bool started = false;       // native thread exists
	bool dead = false;          // exited/cancelled: the native thread must end instead of running
};

std::mutex gM;                     // the "interrupts disabled" lock; also protects everything below
thread_local bool tlsDisabled;     // this native thread holds gM
thread_local Rec* tlsSelf;         // null on host threads (interrupt context)

OSThread sMainThread;
Rec sMainRec;
std::atomic<OSThread*> gCurrent{ &sMainThread }; // null while the CPU is idle
OSThreadQueue sRunQueue[OS_PRIORITY_MAX + 1];
OSThreadQueue sActiveQueue;
u32 sRunQueueBits;   // bit (31 - priority) set when sRunQueue[priority] is not empty
bool sRunQueueHint;  // a reschedule may be due
s32 sReschedule;     // OSDisableScheduler nesting

static_assert(sizeof(Rec*) <= sizeof(f64), "the Rec pointer is stored in OSContext::fpr[0]");

Rec* recOf(OSThread* t)
{
	Rec* r;
	std::memcpy(&r, &t->context.fpr[0], sizeof(r));
	return r;
}

void setRec(OSThread* t, Rec* r) { std::memcpy(&t->context.fpr[0], &r, sizeof(r)); }

// ---- queues: the decomp's ENQUEUE_/DEQUEUE_ macros ----

using Link = OSThreadLink OSThread::*;

void enqueue(OSThreadQueue* q, OSThread* t, Link l)
{
	OSThread* prev = q->tail;
	if (prev)
		(prev->*l).next = t;
	else
		q->head = t;
	(t->*l).prev = prev;
	(t->*l).next = nullptr;
	q->tail = t;
}

void dequeue(OSThreadQueue* q, OSThread* t, Link l)
{
	OSThread* next = (t->*l).next;
	OSThread* prev = (t->*l).prev;
	if (next)
		(next->*l).prev = prev;
	else
		q->tail = prev;
	if (prev)
		(prev->*l).next = next;
	else
		q->head = next;
}

OSThread* dequeueHead(OSThreadQueue* q, Link l)
{
	OSThread* t    = q->head;
	OSThread* next = (t->*l).next;
	if (next)
		(next->*l).prev = nullptr;
	else
		q->tail = nullptr;
	q->head = next;
	return t;
}

// Sorted insert: after every thread of equal or better priority.
void enqueuePrio(OSThreadQueue* q, OSThread* t)
{
	OSThread* next = q->head;
	while (next && next->priority <= t->priority)
		next = next->link.next;
	if (!next) {
		enqueue(q, t, &OSThread::link);
		return;
	}
	OSThread* prev = next->link.prev;
	t->link.next   = next;
	next->link.prev = t;
	t->link.prev   = prev;
	if (prev)
		prev->link.next = t;
	else
		q->head = t;
}

void enqueueMutex(OSMutexQueue* q, OSMutex* m)
{
	OSMutex* prev = q->tail;
	if (prev)
		prev->link.next = m;
	else
		q->head = m;
	m->link.prev = prev;
	m->link.next = nullptr;
	q->tail      = m;
}

void dequeueMutex(OSMutexQueue* q, OSMutex* m)
{
	OSMutex* next = m->link.next;
	OSMutex* prev = m->link.prev;
	if (next)
		next->link.prev = prev;
	else
		q->tail = prev;
	if (prev)
		prev->link.next = next;
	else
		q->head = next;
}

OSMutex* dequeueMutexHead(OSMutexQueue* q)
{
	OSMutex* m    = q->head;
	OSMutex* next = m->link.next;
	if (next)
		next->link.prev = nullptr;
	else
		q->tail = nullptr;
	q->head = next;
	return m;
}

// ---- run queue (OSThread.c SetRun/UnsetRun/priorities) ----

bool isThreadActive(OSThread* t)
{
	if (t->state == 0)
		return false;
	for (OSThread* a = sActiveQueue.head; a; a = a->linkActive.next)
		if (a == t)
			return true;
	return false;
}

void setRun(OSThread* t)
{
	t->queue = &sRunQueue[t->priority];
	enqueue(t->queue, t, &OSThread::link);
	sRunQueueBits |= 1u << (31 - t->priority);
	sRunQueueHint = true;
}

void unsetRun(OSThread* t)
{
	OSThreadQueue* q = t->queue;
	dequeue(q, t, &OSThread::link);
	if (!q->head)
		sRunQueueBits &= ~(1u << (31 - t->priority));
	t->queue = nullptr;
}

// Best priority a thread could have: its base, raised to that of the best waiter on any mutex it owns.
s32 getEffectivePriority(OSThread* t)
{
	s32 priority = t->base;
	for (OSMutex* m = t->queueMutex.head; m; m = m->link.next) {
		OSThread* blocked = m->queue.head;
		if (blocked && blocked->priority < priority)
			priority = blocked->priority;
	}
	return priority;
}

// Returns the mutex owner that has to be re-prioritised next (priority inheritance chain), or null.
OSThread* setEffectivePriority(OSThread* t, s32 priority)
{
	switch (t->state) {
	case OS_THREAD_STATE_READY:
		unsetRun(t);
		t->priority = priority;
		setRun(t);
		break;
	case OS_THREAD_STATE_WAITING:
		dequeue(t->queue, t, &OSThread::link);
		t->priority = priority;
		enqueuePrio(t->queue, t);
		if (t->mutex)
			return t->mutex->thread;
		break;
	case OS_THREAD_STATE_RUNNING:
		sRunQueueHint = true;
		t->priority   = priority;
		break;
	}
	return nullptr;
}

void updatePriority(OSThread* t)
{
	while (t->suspend <= 0) {
		s32 priority = getEffectivePriority(t);
		if (t->priority == priority)
			break;
		t = setEffectivePriority(t, priority);
		if (!t)
			break;
	}
}

void promoteThread(OSThread* t, s32 priority)
{
	while (t->suspend <= 0 && t->priority > priority) {
		t = setEffectivePriority(t, priority);
		if (!t)
			break;
	}
}

void unlockAllMutex(OSThread* t)
{
	while (t->queueMutex.head) {
		OSMutex* m = dequeueMutexHead(&t->queueMutex);
		m->count   = 0;
		m->thread  = nullptr;
		OSWakeupThread(&m->queue);
	}
}

// ---- baton hand-off ----

unsigned __stdcall nativeEntry(void* p);

void startNative(Rec* r)
{
	HANDLE h = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, kNativeStackBytes, nativeEntry, r, 0, nullptr));
	if (!h)
		OSPanic(__FILE__, __LINE__, "cannot create the native thread for OSThread %p\n", static_cast<void*>(r->os));
	CloseHandle(h);
}

// Waits on the thread's condition variable; gM is held on entry and on return.
void waitBaton(Rec* r)
{
	std::unique_lock<std::mutex> lk(gM, std::adopt_lock);
	r->cv.wait(lk);
	lk.release();
}

// gM held, no thread owns the CPU: give it to the best ready thread, if any.
void dispatch()
{
	if (!sRunQueueBits)
		return; // idle until a host thread wakes something
	sRunQueueHint    = false;
	int priority     = __cntlzw(sRunQueueBits);
	OSThreadQueue* q = &sRunQueue[priority];
	OSThread* next   = dequeueHead(q, &OSThread::link);
	if (!q->head)
		sRunQueueBits &= ~(1u << (31 - priority));
	next->queue = nullptr;
	next->state = OS_THREAD_STATE_RUNNING;
	gCurrent.store(next, std::memory_order_release);
	Rec* r = recOf(next);
	if (!r->started) {
		r->started = true;
		startNative(r);
	} else {
		r->cv.notify_one();
	}
}

[[noreturn]] void endNativeThread()
{
	gM.unlock();
	_endthreadex(0);
	__builtin_unreachable();
}

// OSThread.c SelectThread(): pick the next thread to run. Called with gM held.
// Game thread context: may park this native thread until it is selected again, or end it if the thread is dead.
// Interrupt (host thread) context: only hands out an idle CPU; a running game thread keeps it until its next
// interrupt-enable point.
void selectThread(bool yield)
{
	Rec* self      = tlsSelf;
	OSThread* cur  = gCurrent.load(std::memory_order_acquire);
	bool inThread  = self && cur == self->os;
	bool dying     = inThread && self->dead;
	if (sReschedule > 0 && !dying)
		return;
	if (!inThread) {
		if (!cur)
			dispatch();
		return;
	}
	if (cur->state == OS_THREAD_STATE_RUNNING) {
		if (!yield && cur->priority <= __cntlzw(sRunQueueBits))
			return;
		cur->state = OS_THREAD_STATE_READY;
		setRun(cur);
	}
	gCurrent.store(nullptr, std::memory_order_release);
	dispatch();
	if (gCurrent.load(std::memory_order_acquire) == cur)
		return; // picked ourselves again
	while (gCurrent.load(std::memory_order_acquire) != cur && !self->dead)
		waitBaton(self);
	if (self->dead)
		endNativeThread();
}

void reschedule()
{
	if (sRunQueueHint)
		selectThread(false);
}

// Marks the native thread of `t` for termination. A parked thread wakes and ends itself; the running thread
// (cancelled or exiting itself) ends in its next selectThread().
void killNative(OSThread* t)
{
	Rec* r = recOf(t);
	if (!r)
		return;
	r->dead = true;
	if (r->started && r != tlsSelf)
		r->cv.notify_one();
}

// Body of every native game thread: wait for the first baton, run the function, exit like the console's lr trampoline.
void runThread(Rec* r)
{
	tlsSelf = r;
	gM.lock();
	while (gCurrent.load(std::memory_order_acquire) != r->os && !r->dead)
		waitBaton(r);
	if (r->dead)
		endNativeThread();
	tlsDisabled = false; // a new thread starts with interrupts enabled
	gM.unlock();
	void* val = r->func(r->param);
	OSExitThread(static_cast<OSThread*>(val));
}

unsigned __stdcall nativeEntry(void* p)
{
	runThread(static_cast<Rec*>(p));
	return 0;
}

void registerMainThread()
{
	OSThread* t = &sMainThread;
	if (!sMainRec.os) {
		t->state    = OS_THREAD_STATE_RUNNING;
		t->attr     = OS_THREAD_ATTR_DETACH;
		t->priority = t->base = kDefaultThreadPriority;
		t->suspend  = 0;
		t->val      = reinterpret_cast<void*>(static_cast<intptr_t>(-1));
		t->mutex    = nullptr;
		OSInitThreadQueue(&t->queueJoin);
		t->queueMutex.head = t->queueMutex.tail = nullptr;
		ULONG_PTR lo, hi;
		GetCurrentThreadStackLimits(&lo, &hi);
		t->stackBase = reinterpret_cast<u8*>(hi);
		t->stackEnd  = reinterpret_cast<u32*>(lo);
		enqueue(&sActiveQueue, t, &OSThread::linkActive);
		sMainRec.os      = t;
		sMainRec.started = true;
		setRec(t, &sMainRec);
	}
	tlsSelf = &sMainRec;
	gCurrent.store(t, std::memory_order_release);
}

[[maybe_unused]] const bool sMainRegistered = (registerMainThread(), true);

} // namespace

extern "C" {

void __OSThreadInit(void) { registerMainThread(); }

// ---- interrupts ----

BOOL OSDisableInterrupts(void)
{
	if (tlsDisabled)
		return FALSE;
	gM.lock();
	tlsDisabled = true;
	return TRUE;
}

BOOL OSEnableInterrupts(void)
{
	if (!tlsDisabled)
		return TRUE;
	if (tlsSelf)
		reschedule(); // interrupt return: a pending higher-priority wake preempts here
	tlsDisabled = false;
	gM.unlock();
	return FALSE;
}

BOOL OSRestoreInterrupts(BOOL level) { return level ? OSEnableInterrupts() : OSDisableInterrupts(); }

// Synthesized MSR: floating point, machine check, address translation and recoverable-exception bits as on the console.
u32 PPCMfmsr()
{
	return MSR_FP | MSR_ME | MSR_IR | MSR_DR | MSR_RI | (tlsDisabled ? 0 : MSR_EE);
}

// ---- threads ----

void OSInitThreadQueue(OSThreadQueue* queue) { queue->head = queue->tail = nullptr; }

OSThread* OSGetCurrentThread(void) { return gCurrent.load(std::memory_order_acquire); }

BOOL OSIsThreadTerminated(OSThread* thread)
{
	return thread->state == OS_THREAD_STATE_MORIBUND || thread->state == 0;
}

s32 OSDisableScheduler(void)
{
	BOOL enabled = OSDisableInterrupts();
	s32 count    = sReschedule++;
	OSRestoreInterrupts(enabled);
	return count;
}

s32 OSEnableScheduler(void)
{
	BOOL enabled = OSDisableInterrupts();
	s32 count    = sReschedule--;
	OSRestoreInterrupts(enabled);
	return count;
}

void OSYieldThread(void)
{
	BOOL enabled = OSDisableInterrupts();
	selectThread(true);
	OSRestoreInterrupts(enabled);
}

int OSCreateThread(OSThread* thread, void* (*func)(void*), void* param, void* stack, u32 stackSize, s32 priority,
                   u16 attr)
{
	if (priority < OS_PRIORITY_MIN || priority > OS_PRIORITY_MAX)
		return 0;

	std::memset(&thread->context, 0, sizeof(thread->context));
	thread->state    = OS_THREAD_STATE_READY;
	thread->attr     = attr & OS_THREAD_ATTR_DETACH;
	thread->base     = priority;
	thread->priority = priority;
	thread->suspend  = 1;
	thread->val      = reinterpret_cast<void*>(static_cast<intptr_t>(-1));
	thread->mutex    = nullptr;
	thread->queue    = nullptr;
	OSInitThreadQueue(&thread->queueJoin);
	thread->queueMutex.head = thread->queueMutex.tail = nullptr;
	thread->stackBase = static_cast<u8*>(stack);
	thread->stackEnd  = reinterpret_cast<u32*>(reinterpret_cast<uintptr_t>(stack) - stackSize);

	// malloc, not operator new: the port may route global new through the JKR heap, which locks an OSMutex
	Rec* r   = new (std::malloc(sizeof(Rec))) Rec;
	r->os    = thread;
	r->func  = func;
	r->param = param;
	setRec(thread, r);

	BOOL enabled = OSDisableInterrupts();
	enqueue(&sActiveQueue, thread, &OSThread::linkActive);
	OSRestoreInterrupts(enabled);
	return 1;
}

void OSExitThread(OSThread* val)
{
	OSDisableInterrupts(); // never restored: the thread ends below
	OSThread* cur = OSGetCurrentThread();
	sReschedule++; // finish releasing mutexes and waking joiners before the CPU is given up
	if (cur->attr & OS_THREAD_ATTR_DETACH) {
		dequeue(&sActiveQueue, cur, &OSThread::linkActive);
		cur->state = 0;
	} else {
		cur->state = OS_THREAD_STATE_MORIBUND;
		cur->val   = val;
	}
	unlockAllMutex(cur);
	OSWakeupThread(&cur->queueJoin);
	sReschedule--;
	killNative(cur);
	sRunQueueHint = true;
	reschedule(); // never returns: the dead thread hands the baton on and ends
}

void OSCancelThread(OSThread* thread)
{
	BOOL enabled = OSDisableInterrupts();
	switch (thread->state) {
	case OS_THREAD_STATE_READY:
		if (thread->suspend <= 0)
			unsetRun(thread);
		break;
	case OS_THREAD_STATE_RUNNING:
		sRunQueueHint = true;
		break;
	case OS_THREAD_STATE_WAITING:
		dequeue(thread->queue, thread, &OSThread::link);
		thread->queue = nullptr;
		if (thread->suspend <= 0 && thread->mutex)
			updatePriority(thread->mutex->thread);
		break;
	default:
		OSRestoreInterrupts(enabled);
		return;
	}
	if (thread->attr & OS_THREAD_ATTR_DETACH) {
		dequeue(&sActiveQueue, thread, &OSThread::linkActive);
		thread->state = 0;
	} else {
		thread->state = OS_THREAD_STATE_MORIBUND;
	}
	sReschedule++;
	unlockAllMutex(thread);
	OSWakeupThread(&thread->queueJoin);
	sReschedule--;
	killNative(thread);
	reschedule();
	OSRestoreInterrupts(enabled);
}

int OSJoinThread(OSThread* thread, void* val)
{
	BOOL enabled = OSDisableInterrupts();
	if (!(thread->attr & OS_THREAD_ATTR_DETACH) && thread->state != OS_THREAD_STATE_MORIBUND
	    && thread->queueJoin.head == nullptr) {
		OSSleepThread(&thread->queueJoin);
		if (!isThreadActive(thread)) {
			OSRestoreInterrupts(enabled);
			return 0;
		}
	}
	if (thread->state == OS_THREAD_STATE_MORIBUND) {
		if (val) // 32-bit store like the SDK: callers pass u32 as well as void* destinations
			*static_cast<s32*>(val) = static_cast<s32>(reinterpret_cast<intptr_t>(thread->val));
		dequeue(&sActiveQueue, thread, &OSThread::linkActive);
		thread->state = 0;
		OSRestoreInterrupts(enabled);
		return 1;
	}
	OSRestoreInterrupts(enabled);
	return 0;
}

void OSDetachThread(OSThread* thread)
{
	BOOL enabled = OSDisableInterrupts();
	thread->attr |= OS_THREAD_ATTR_DETACH;
	if (thread->state == OS_THREAD_STATE_MORIBUND) {
		dequeue(&sActiveQueue, thread, &OSThread::linkActive);
		thread->state = 0;
	}
	OSWakeupThread(&thread->queueJoin);
	OSRestoreInterrupts(enabled);
}

long OSResumeThread(OSThread* thread)
{
	BOOL enabled = OSDisableInterrupts();
	long suspendCount = thread->suspend--;
	if (thread->suspend < 0) {
		thread->suspend = 0;
	} else if (thread->suspend == 0) {
		switch (thread->state) {
		case OS_THREAD_STATE_READY:
			thread->priority = getEffectivePriority(thread);
			setRun(thread);
			break;
		case OS_THREAD_STATE_WAITING:
			dequeue(thread->queue, thread, &OSThread::link);
			thread->priority = getEffectivePriority(thread);
			enqueuePrio(thread->queue, thread);
			if (thread->mutex)
				updatePriority(thread->mutex->thread);
			break;
		}
		reschedule();
	}
	OSRestoreInterrupts(enabled);
	return suspendCount;
}

s32 OSSuspendThread(OSThread* thread)
{
	BOOL enabled      = OSDisableInterrupts();
	s32 suspendCount = thread->suspend++;
	if (suspendCount == 0) {
		switch (thread->state) {
		case OS_THREAD_STATE_RUNNING:
			sRunQueueHint = true;
			thread->state = OS_THREAD_STATE_READY;
			break;
		case OS_THREAD_STATE_READY:
			unsetRun(thread);
			break;
		case OS_THREAD_STATE_WAITING:
			dequeue(thread->queue, thread, &OSThread::link);
			thread->priority = OS_PRIORITY_MAX + 1; // behind every waiter
			enqueue(thread->queue, thread, &OSThread::link);
			if (thread->mutex)
				updatePriority(thread->mutex->thread);
			break;
		}
		reschedule();
	}
	OSRestoreInterrupts(enabled);
	return suspendCount;
}

void OSSleepThread(OSThreadQueue* queue)
{
	BOOL enabled = OSDisableInterrupts();
	if (!tlsSelf)
		OSPanic(__FILE__, __LINE__, "OSSleepThread(): called from a non-game (interrupt) thread\n");
	OSThread* cur = OSGetCurrentThread();
	cur->state    = OS_THREAD_STATE_WAITING;
	cur->queue    = queue;
	enqueuePrio(queue, cur);
	sRunQueueHint = true;
	reschedule();
	OSRestoreInterrupts(enabled);
}

void OSWakeupThread(OSThreadQueue* queue)
{
	BOOL enabled = OSDisableInterrupts();
	while (queue->head) {
		OSThread* t = dequeueHead(queue, &OSThread::link);
		t->state    = OS_THREAD_STATE_READY;
		if (t->suspend <= 0)
			setRun(t);
	}
	reschedule();
	OSRestoreInterrupts(enabled);
}

long OSGetThreadPriority(OSThread* thread) { return thread->base; }

// ---- mutex / condition variable ----

void OSInitMutex(OSMutex* mutex)
{
	OSInitThreadQueue(&mutex->queue);
	mutex->thread = nullptr;
	mutex->count  = 0;
}

void OSLockMutex(OSMutex* mutex)
{
	BOOL enabled  = OSDisableInterrupts();
	OSThread* cur = OSGetCurrentThread();
	for (;;) {
		OSThread* owner = mutex->thread;
		if (!owner) {
			mutex->thread = cur;
			mutex->count++;
			enqueueMutex(&cur->queueMutex, mutex);
			break;
		} else if (owner == cur) {
			mutex->count++;
			break;
		} else {
			cur->mutex = mutex;
			promoteThread(mutex->thread, cur->priority);
			OSSleepThread(&mutex->queue);
			cur->mutex = nullptr;
		}
	}
	OSRestoreInterrupts(enabled);
}

void OSUnlockMutex(OSMutex* mutex)
{
	BOOL enabled  = OSDisableInterrupts();
	OSThread* cur = OSGetCurrentThread();
	if (mutex->thread == cur && !--mutex->count) {
		dequeueMutex(&cur->queueMutex, mutex);
		mutex->thread = nullptr;
		if (cur->priority < cur->base)
			cur->priority = getEffectivePriority(cur);
		OSWakeupThread(&mutex->queue);
	}
	OSRestoreInterrupts(enabled);
}

BOOL OSTryLockMutex(OSMutex* mutex)
{
	BOOL enabled  = OSDisableInterrupts();
	OSThread* cur = OSGetCurrentThread();
	BOOL locked;
	if (!mutex->thread) {
		mutex->thread = cur;
		mutex->count++;
		enqueueMutex(&cur->queueMutex, mutex);
		locked = TRUE;
	} else if (mutex->thread == cur) {
		mutex->count++;
		locked = TRUE;
	} else {
		locked = FALSE;
	}
	OSRestoreInterrupts(enabled);
	return locked;
}

void OSInitCond(OSCond* cond) { OSInitThreadQueue(&cond->queue); }

void OSWaitCond(OSCond* cond, OSMutex* mutex)
{
	BOOL enabled  = OSDisableInterrupts();
	OSThread* cur = OSGetCurrentThread();
	if (mutex->thread == cur) {
		long count   = mutex->count;
		mutex->count = 0;
		dequeueMutex(&cur->queueMutex, mutex);
		mutex->thread = nullptr;
		if (cur->priority < cur->base)
			cur->priority = getEffectivePriority(cur);
		OSDisableScheduler();
		OSWakeupThread(&mutex->queue);
		OSEnableScheduler();
		OSSleepThread(&cond->queue);
		OSLockMutex(mutex);
		mutex->count = count;
	}
	OSRestoreInterrupts(enabled);
}

void OSSignalCond(OSCond* cond) { OSWakeupThread(&cond->queue); }

// ---- message queues ----
// Slots are pointer sized (OSMessage is void*), not the SDK's 32 bit: messages carry host pointers.
// Callers must pass OSMessage-sized receive variables and arrays (see the report: JASAudioThread, JASDvdThread,
// DrawSyncManager still use 4-byte ones).

void OSInitMessageQueue(OSMessageQueue* mq, void* msgArray, long msgCount)
{
	OSInitThreadQueue(&mq->queueSend);
	OSInitThreadQueue(&mq->queueReceive);
	mq->msgArray   = msgArray;
	mq->msgCount   = msgCount;
	mq->firstIndex = 0;
	mq->usedCount  = 0;
}

int OSSendMessage(OSMessageQueue* mq, void* msg, long flags)
{
	BOOL enabled = OSDisableInterrupts();
	while (mq->msgCount <= mq->usedCount) {
		if (!(flags & OS_MESSAGE_BLOCK)) {
			OSRestoreInterrupts(enabled);
			return 0;
		}
		OSSleepThread(&mq->queueSend);
	}
	long lastIndex = (mq->firstIndex + mq->usedCount) % mq->msgCount;
	static_cast<OSMessage*>(mq->msgArray)[lastIndex] = msg;
	mq->usedCount++;
	OSWakeupThread(&mq->queueReceive);
	OSRestoreInterrupts(enabled);
	return 1;
}

// The decomp has no OSJamMessage body: same as OSSendMessage but the message goes to the front of the queue.
int OSJamMessage(OSMessageQueue* mq, void* msg, long flags)
{
	BOOL enabled = OSDisableInterrupts();
	while (mq->msgCount <= mq->usedCount) {
		if (!(flags & OS_MESSAGE_BLOCK)) {
			OSRestoreInterrupts(enabled);
			return 0;
		}
		OSSleepThread(&mq->queueSend);
	}
	mq->firstIndex = (mq->firstIndex + mq->msgCount - 1) % mq->msgCount;
	static_cast<OSMessage*>(mq->msgArray)[mq->firstIndex] = msg;
	mq->usedCount++;
	OSWakeupThread(&mq->queueReceive);
	OSRestoreInterrupts(enabled);
	return 1;
}

int OSReceiveMessage(OSMessageQueue* mq, void* msg, long flags)
{
	BOOL enabled = OSDisableInterrupts();
	while (mq->usedCount == 0) {
		if (!(flags & OS_MESSAGE_BLOCK)) {
			OSRestoreInterrupts(enabled);
			return 0;
		}
		OSSleepThread(&mq->queueReceive);
	}
	if (msg)
		*static_cast<OSMessage*>(msg) = static_cast<OSMessage*>(mq->msgArray)[mq->firstIndex];
	mq->firstIndex = (mq->firstIndex + 1) % mq->msgCount;
	mq->usedCount--;
	OSWakeupThread(&mq->queueSend);
	OSRestoreInterrupts(enabled);
	return 1;
}

} // extern "C"
