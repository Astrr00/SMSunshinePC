// Developer CPU sampler (SUNSHINE_DEV_SAMPLE=<delay s>[,<length s>], default length 20): after the delay, suspends each
// thread of the process every millisecond, walks its stack and counts functions; at the end prints, per thread with at
// least 5% of the samples, the top self and inclusive functions. For finding hot spots on hosts without a profiler.
// Diagnosis only: the sampled threads run slower while it samples.
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "winmm.lib")

namespace {

struct ThreadStats {
	HANDLE handle = nullptr;
	std::string name;
	int samples = 0;
	std::map<DWORD64, int> self, incl;
};

std::string threadName(HANDLE h)
{
	PWSTR w = nullptr;
	std::string out;
	if (SUCCEEDED(GetThreadDescription(h, &w)) && w) {
		for (PWSTR p = w; *p; ++p)
			out += (char)(*p < 128 ? *p : '?');
		LocalFree(w);
	}
	return out;
}

std::string symbol(DWORD64 pc)
{
	alignas(SYMBOL_INFO) char buf[sizeof(SYMBOL_INFO) + 256];
	SYMBOL_INFO* sym  = reinterpret_cast<SYMBOL_INFO*>(buf);
	sym->SizeOfStruct = sizeof(SYMBOL_INFO);
	sym->MaxNameLen   = 255;
	DWORD64 disp      = 0;
	if (SymFromAddr(GetCurrentProcess(), pc, &disp, sym))
		return sym->Name;
	char tmp[32];
	std::snprintf(tmp, sizeof(tmp), "0x%llx", (unsigned long long)pc);
	return tmp;
}

void run(double delay, double length)
{
	Sleep((DWORD)(delay * 1000));
	const DWORD self = GetCurrentThreadId();
	std::map<DWORD, ThreadStats> threads;
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	THREADENTRY32 te{ sizeof(te) };
	for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te))
		if (te.th32OwnerProcessID == GetCurrentProcessId() && te.th32ThreadID != self)
			if (HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_LIMITED_INFORMATION, FALSE,
			                          te.th32ThreadID)) {
				threads[te.th32ThreadID].handle = h;
				threads[te.th32ThreadID].name   = threadName(h);
			}
	CloseHandle(snap);
	SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
	SymInitialize(GetCurrentProcess(), nullptr, TRUE);
	std::fprintf(stderr, "[sampler] sampling %zu threads for %.0f s\n", threads.size(), length);
	const ULONGLONG end = GetTickCount64() + (ULONGLONG)(length * 1000);
	timeBeginPeriod(1);
	while (GetTickCount64() < end) {
		for (auto& [tid, t] : threads) {
			if (SuspendThread(t.handle) == (DWORD)-1)
				continue;
			CONTEXT ctx{};
			ctx.ContextFlags = CONTEXT_FULL;
			if (GetThreadContext(t.handle, &ctx)) {
				// Skip threads parked in the kernel: their leaf is a wait syscall stub.
				STACKFRAME64 f{};
				f.AddrPC    = { ctx.Rip, 0, AddrModeFlat };
				f.AddrFrame = { ctx.Rbp, 0, AddrModeFlat };
				f.AddrStack = { ctx.Rsp, 0, AddrModeFlat };
				std::set<DWORD64> seen;
				bool leaf = true;
				for (int depth = 0; depth < 24; ++depth) {
					if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(), t.handle, &f, &ctx, nullptr,
					                 SymFunctionTableAccess64, SymGetModuleBase64, nullptr)
					    || f.AddrPC.Offset == 0)
						break;
					DWORD64 disp = 0;
					alignas(SYMBOL_INFO) char buf[sizeof(SYMBOL_INFO)];
					SYMBOL_INFO* sym  = reinterpret_cast<SYMBOL_INFO*>(buf);
					sym->SizeOfStruct = sizeof(SYMBOL_INFO);
					sym->MaxNameLen   = 0;
					const DWORD64 key = SymFromAddr(GetCurrentProcess(), f.AddrPC.Offset, &disp, sym) ? sym->Address
					                                                                                    : f.AddrPC.Offset;
					if (leaf)
						++t.self[key];
					leaf = false;
					if (seen.insert(key).second)
						++t.incl[key];
				}
				++t.samples;
			}
			ResumeThread(t.handle);
		}
		Sleep(1);
	}
	timeEndPeriod(1);
	int total = 0;
	for (auto& [tid, t] : threads)
		total += t.samples;
	for (auto& [tid, t] : threads) {
		// Rank threads by samples whose leaf is not a wait: approximated by excluding the top self entry when it is a
		// kernel stub (Nt*/Zw*).
		int busy = t.samples;
		for (auto& [pc, n] : t.self) {
			const std::string s = symbol(pc);
			if (s.rfind("Nt", 0) == 0 || s.rfind("Zw", 0) == 0)
				busy -= n;
		}
		if (t.samples == 0 || busy * 20 < t.samples)
			continue;
		std::fprintf(stderr, "[sampler] thread %lu '%s': %d samples, %d busy\n", tid, t.name.c_str(), t.samples, busy);
		for (auto* m : { &t.self, &t.incl }) {
			std::vector<std::pair<int, DWORD64>> v;
			for (auto& [pc, n] : *m)
				v.push_back({ n, pc });
			std::sort(v.rbegin(), v.rend());
			for (size_t i = 0; i < v.size() && i < 25; ++i)
				std::fprintf(stderr, "[sampler]   %s %5.1f%% %s\n", m == &t.self ? "self" : "incl",
				             100.0 * v[i].first / t.samples, symbol(v[i].second).c_str());
		}
	}
	std::fflush(stderr);
}

} // namespace

void sunshineStartSampler()
{
	const char* e = std::getenv("SUNSHINE_DEV_SAMPLE");
	if (!e)
		return;
	double delay = 30, length = 20;
	std::sscanf(e, "%lf,%lf", &delay, &length);
	std::thread([=] { run(delay, length); }).detach();
}
