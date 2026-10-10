// Crash report for the PC port: on a fatal exception print the code, the faulting address and a symbolized stack
// (function+offset, file:line from the PDB) to stderr and to crash.log next to the exe. Keeps early boot failures of
// the decompiled game debuggable without an attached debugger.
//
// Design notes:
// - A vectored handler registered as FIRST sees the exception before frame handlers and before any
//   UnhandledExceptionFilter that a later-initialized library (Tracy, SDL, Dawn) may have installed over ours.
// - Output uses WriteFile and stack buffers only: after heap corruption the CRT's stdio locks and the heap are not safe.
// - Only fatal codes are reported; C++ exceptions and benign first-chance exceptions pass through untouched.
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cstring>

#pragma comment(lib, "dbghelp.lib")

namespace {

LONG g_reported = 0;
HANDLE g_log = INVALID_HANDLE_VALUE;

void emit(const char* s)
{
	DWORD n = 0;
	const DWORD len = (DWORD)std::strlen(s);
	WriteFile(GetStdHandle(STD_ERROR_HANDLE), s, len, &n, nullptr);
	if (g_log != INVALID_HANDLE_VALUE)
		WriteFile(g_log, s, len, &n, nullptr);
}

bool isFatal(DWORD code)
{
	switch (code) {
	case EXCEPTION_ACCESS_VIOLATION:
	case EXCEPTION_ILLEGAL_INSTRUCTION:
	case EXCEPTION_PRIV_INSTRUCTION:
	case EXCEPTION_IN_PAGE_ERROR:
	case EXCEPTION_STACK_OVERFLOW:
	case EXCEPTION_BREAKPOINT:
	case EXCEPTION_INT_DIVIDE_BY_ZERO: // MWCC computes x/0 without trapping; the x86 divide does
	case 0xC0000374: // STATUS_HEAP_CORRUPTION
	case 0xC0000409: // STATUS_STACK_BUFFER_OVERRUN / fast fail
		return true;
	}
	return false;
}

void printPc(HANDLE process, int i, DWORD64 pc)
{
	char line[600];
	alignas(SYMBOL_INFO) char buf[sizeof(SYMBOL_INFO) + 256];
	SYMBOL_INFO* sym   = (SYMBOL_INFO*)buf;
	sym->SizeOfStruct  = sizeof(SYMBOL_INFO);
	sym->MaxNameLen    = 255;
	DWORD64 disp       = 0;
	IMAGEHLP_LINE64 ln = { sizeof(ln) };
	DWORD lineDisp     = 0;
	const bool hasSym  = SymFromAddr(process, pc, &disp, sym);
	const bool hasLine = SymGetLineFromAddr64(process, pc, &lineDisp, &ln);
	const DWORD64 mod  = SymGetModuleBase64(process, pc);
	int m = _snprintf_s(line, sizeof(line), _TRUNCATE, "  #%02d %p  %s+0x%llx (rva 0x%llx)", i, (void*)pc,
	                    hasSym ? sym->Name : "?", (unsigned long long)disp, (unsigned long long)(pc - mod));
	if (hasLine && m > 0)
		_snprintf_s(line + m, sizeof(line) - m, _TRUNCATE, "  %s:%lu", ln.FileName, ln.LineNumber);
	emit(line);
	emit("\n");
}

void symInit()
{
	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
	SymInitialize(GetCurrentProcess(), nullptr, TRUE);
}

void printStack(HANDLE thread, CONTEXT ctx)
{
	HANDLE process = GetCurrentProcess();
	symInit();

	STACKFRAME64 frame = {};
	frame.AddrPC.Offset    = ctx.Rip;
	frame.AddrFrame.Offset = ctx.Rbp;
	frame.AddrStack.Offset = ctx.Rsp;
	frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;

	for (int i = 0; i < 40; i++) {
		if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &ctx, nullptr, SymFunctionTableAccess64,
		                 SymGetModuleBase64, nullptr)
		    || frame.AddrPC.Offset == 0)
			break;
		printPc(process, i, frame.AddrPC.Offset);
	}
}

// Diagnostic: SUNSHINE_HANG_DUMP=<seconds> prints the stack of every thread once after that delay, to see where a
// hung boot is waiting. Pass 1 unwinds with Rtl* only (no allocation: a suspended thread may hold the heap lock),
// pass 2 symbolizes after every thread is running again.
void dumpAllThreads()
{
	constexpr int kMaxThreads = 64, kDepth = 40;
	static DWORD ids[kMaxThreads];
	static DWORD64 pcs[kMaxThreads][kDepth];
	static int depth[kMaxThreads];
	int count = 0;

	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (snap == INVALID_HANDLE_VALUE)
		return;
	THREADENTRY32 te = { sizeof(te) };
	HANDLE held[kMaxThreads];
	for (BOOL ok = Thread32First(snap, &te); ok && count < kMaxThreads; ok = Thread32Next(snap, &te)) {
		if (te.th32OwnerProcessID != GetCurrentProcessId() || te.th32ThreadID == GetCurrentThreadId())
			continue;
		HANDLE t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID);
		if (!t)
			continue;
		SuspendThread(t);
		held[count] = t;
		ids[count]  = te.th32ThreadID;
		CONTEXT c      = {};
		c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
		int n          = 0;
		if (GetThreadContext(t, &c)) {
			while (n < kDepth && c.Rip) {
				pcs[count][n++] = c.Rip;
				DWORD64 base    = 0;
				PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(c.Rip, &base, nullptr);
				if (!fn) { // leaf: return address is at the top of the stack
					c.Rip = *(DWORD64*)c.Rsp;
					c.Rsp += 8;
				} else {
					PVOID handlerData = nullptr;
					DWORD64 frameBase = 0;
					RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, c.Rip, fn, &c, &handlerData, &frameBase, nullptr);
				}
			}
		}
		depth[count++] = n;
	}
	for (int k = 0; k < count; k++) {
		ResumeThread(held[k]);
		CloseHandle(held[k]);
	}
	CloseHandle(snap);

	symInit();
	for (int k = 0; k < count; k++) {
		char head[64];
		_snprintf_s(head, sizeof(head), _TRUNCATE, "\n--- thread %lu ---\n", ids[k]);
		emit(head);
		for (int i = 0; i < depth[k]; i++)
			printPc(GetCurrentProcess(), i, pcs[k][i]);
	}
}

DWORD WINAPI hangDumpThread(LPVOID p)
{
	Sleep((DWORD)(uintptr_t)p * 1000);
	dumpAllThreads();
	return 0;
}

// Watchdog (always on once sunshineWatchdogStart() ran): the main loop calls sunshineHeartbeat() every presented frame.
// If no heartbeat arrives for the limit (SUNSHINE_WATCHDOG_SECONDS, default 120, 0 = off) every thread's stack is written
// to crash.log and the process is terminated, so a deadlocked worker or a stalled boot can never leave a silent zombie
// process burning CPU. SUNSHINE_DEV_MAX_SECONDS=<n> additionally ends dev runs after n seconds of wall time.
volatile LONG64 g_heartbeat = 0;

DWORD WINAPI watchdogThread(LPVOID)
{
	const char* wd   = std::getenv("SUNSHINE_WATCHDOG_SECONDS");
	const int limit  = wd ? std::atoi(wd) : 120;
	const char* mx   = std::getenv("SUNSHINE_DEV_MAX_SECONDS");
	const int maxSec = mx ? std::atoi(mx) : 0;
	const ULONGLONG start = GetTickCount64();
	LONG64 last           = -1;
	ULONGLONG lastChange  = start;
	for (;;) {
		Sleep(1000);
		const ULONGLONG now = GetTickCount64();
		if (maxSec > 0 && now - start >= (ULONGLONG)maxSec * 1000) {
			emit("\n*** SUNSHINE_DEV_MAX_SECONDS reached: ending the process\n");
			TerminateProcess(GetCurrentProcess(), 0xDEAD0002);
		}
		const LONG64 hb = g_heartbeat;
		if (hb != last) {
			last       = hb;
			lastChange = now;
			continue;
		}
		if (limit > 0 && now - lastChange >= (ULONGLONG)limit * 1000) {
			emit("\n*** WATCHDOG: no frame presented for too long; thread stacks follow, then the process ends\n");
			dumpAllThreads();
			emit("\n*** WATCHDOG: terminating\n");
			TerminateProcess(GetCurrentProcess(), 0xDEAD0001);
		}
	}
}

void heartbeatImpl()
{
	InterlockedIncrement64(&g_heartbeat);
}

void watchdogStartImpl()
{
	static LONG started = 0;
	if (InterlockedExchange(&started, 1) == 0)
		CloseHandle(CreateThread(nullptr, 1 << 20, watchdogThread, nullptr, 0, nullptr));
}

void report(EXCEPTION_POINTERS* info)
{
	if (InterlockedExchange(&g_reported, 1) != 0)
		return; // another thread is already reporting; do not interleave

	char line[600];
	const EXCEPTION_RECORD* rec = info->ExceptionRecord;
	int n = _snprintf_s(line, sizeof(line), _TRUNCATE, "\n*** fatal exception 0x%08lx at %p thread %lu",
	                    rec->ExceptionCode, rec->ExceptionAddress, GetCurrentThreadId());
	if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2) {
		const char* kind = rec->ExceptionInformation[0] == 0 ? "read" : rec->ExceptionInformation[0] == 1 ? "write" : "execute";
		_snprintf_s(line + n, sizeof(line) - n, _TRUNCATE, " (%s of %p)", kind, (void*)rec->ExceptionInformation[1]);
	}
	emit(line);
	emit("\n");

	printStack(GetCurrentThread(), *info->ContextRecord);

	// Heap corruption / fast-fail exceptions are raised from inside ntdll with a context StackWalk64 cannot unwind;
	// the handler runs on the faulting thread's stack, so a plain back trace from here still shows the callers.
	if (rec->ExceptionCode == 0xC0000374 || rec->ExceptionCode == 0xC0000409) {
		void* frames[48];
		const USHORT count = RtlCaptureStackBackTrace(0, 48, frames, nullptr);
		emit("  -- back trace from the handler --\n");
		HANDLE process = GetCurrentProcess();
		symInit();
		for (USHORT i = 0; i < count; i++)
			printPc(process, i, (DWORD64)frames[i]);
	}
}

DWORD WINAPI reportThread(LPVOID p)
{
	report((EXCEPTION_POINTERS*)p);
	return 0;
}

LONG CALLBACK vectoredHandler(EXCEPTION_POINTERS* info)
{
	const DWORD code = info->ExceptionRecord->ExceptionCode;
	if (!isFatal(code))
		return EXCEPTION_CONTINUE_SEARCH;
	if (code == EXCEPTION_STACK_OVERFLOW) {
		// The faulting thread has no stack left for dbghelp: report from a fresh thread using the saved context.
		if (HANDLE t = CreateThread(nullptr, 1 << 20, reportThread, info, 0, nullptr)) {
			WaitForSingleObject(t, 15000);
			CloseHandle(t);
		}
	} else {
		report(info);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI unhandledFilter(EXCEPTION_POINTERS* info)
{
	report(info);
	return EXCEPTION_EXECUTE_HANDLER;
}

// Diagnostic: hardware write watchpoint on a 4-byte address, armed on every thread that exists at call time.
// Every hit prints the value now stored and a symbolized stack. Used to find the writer of a corrupted field.
volatile const unsigned* g_watchAddr = nullptr;
LONG g_watchHits                     = 0;

LONG CALLBACK watchHandler(EXCEPTION_POINTERS* info)
{
	if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || !g_watchAddr || !(info->ContextRecord->Dr6 & 1))
		return EXCEPTION_CONTINUE_SEARCH;
	info->ContextRecord->Dr6 = 0;
	if (InterlockedIncrement(&g_watchHits) > 40)
		return EXCEPTION_CONTINUE_EXECUTION;
	char line[100];
	_snprintf_s(line, sizeof(line), _TRUNCATE, "\n*** watch hit #%ld: [%p] = 0x%08x, thread %lu\n", g_watchHits,
	            (const void*)g_watchAddr, *g_watchAddr, GetCurrentThreadId());
	emit(line);
	LONG saved = InterlockedExchange(&g_reported, 0);
	report(info);
	InterlockedExchange(&g_reported, saved);
	return EXCEPTION_CONTINUE_EXECUTION;
}

DWORD WINAPI armThreads(LPVOID)
{
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (snap == INVALID_HANDLE_VALUE)
		return 1;
	THREADENTRY32 te = { sizeof(te) };
	for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
		if (te.th32OwnerProcessID != GetCurrentProcessId() || te.th32ThreadID == GetCurrentThreadId())
			continue;
		if (HANDLE t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, te.th32ThreadID)) {
			SuspendThread(t);
			CONTEXT c        = {};
			c.ContextFlags   = CONTEXT_DEBUG_REGISTERS;
			c.Dr0            = (DWORD64)g_watchAddr;
			c.Dr7            = 1 | (1 << 16) | (3 << 18); // L0, write, 4 bytes
			SetThreadContext(t, &c);
			ResumeThread(t);
			CloseHandle(t);
		}
	}
	CloseHandle(snap);
	return 0;
}

} // namespace

void sunshineHeartbeat() { heartbeatImpl(); }
void sunshineWatchdogStart() { watchdogStartImpl(); }

void installCrashHandler()
{
#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer))
	// ASan commits its shadow memory on demand through first-chance access violations and reports real errors itself;
	// reporting those faults as fatal (and symbolizing inside the fault) deadlocks the process.
	return;
#endif
	g_log = CreateFileA("crash.log", GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	AddVectoredExceptionHandler(1, vectoredHandler);
	if (const char* hang = std::getenv("SUNSHINE_HANG_DUMP"))
		CloseHandle(CreateThread(nullptr, 1 << 20, hangDumpThread, (LPVOID)(uintptr_t)std::atoi(hang), 0, nullptr));
	SetUnhandledExceptionFilter(unhandledFilter);
}

// Prints message and a symbolized stack of the calling thread, then terminates (used by the SUNSHINE_CHECK_HEAP allocator).
void sunshineReportAndAbort(const char* message)
{
	emit("\n*** ");
	emit(message);
	emit("\n");
	CONTEXT ctx = {};
	RtlCaptureContext(&ctx);
	printStack(GetCurrentThread(), ctx);
	std::abort();
}

// Arms a hardware write watchpoint on the 4 bytes at addr for all current threads (see watchHandler).
void sunshineWatchWrite4(const void* addr)
{
	static bool registered = false;
	if (!registered) {
		AddVectoredExceptionHandler(1, watchHandler);
		registered = true;
	}
	g_watchAddr = (const unsigned*)addr;
	if (HANDLE t = CreateThread(nullptr, 1 << 16, armThreads, nullptr, 0, nullptr)) {
		WaitForSingleObject(t, 5000);
		CloseHandle(t);
	}
}
