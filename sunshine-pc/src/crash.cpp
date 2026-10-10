// Crash report for the PC port: on an unhandled exception print the code, the faulting address and a symbolized
// stack (function, file:line from the PDB) to stderr. Keeps early boot failures of the decompiled game debuggable
// without an attached debugger.
#include <windows.h>
#include <dbghelp.h>

#include <cstdio>

#pragma comment(lib, "dbghelp.lib")

static LONG WINAPI crashFilter(EXCEPTION_POINTERS* info)
{
	const EXCEPTION_RECORD* rec = info->ExceptionRecord;
	std::fprintf(stderr, "\n*** unhandled exception 0x%08lx at %p", rec->ExceptionCode, rec->ExceptionAddress);
	if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2) {
		const char* kind = rec->ExceptionInformation[0] == 0 ? "read" : rec->ExceptionInformation[0] == 1 ? "write" : "execute";
		std::fprintf(stderr, " (%s of %p)", kind, (void*)rec->ExceptionInformation[1]);
	}
	std::fprintf(stderr, "\n");

	HANDLE process = GetCurrentProcess();
	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
	SymInitialize(process, nullptr, TRUE);

	CONTEXT ctx = *info->ContextRecord;
	STACKFRAME64 frame = {};
	frame.AddrPC.Offset    = ctx.Rip;
	frame.AddrFrame.Offset = ctx.Rbp;
	frame.AddrStack.Offset = ctx.Rsp;
	frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;

	for (int i = 0; i < 40; i++) {
		if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &ctx, nullptr,
		                 SymFunctionTableAccess64, SymGetModuleBase64, nullptr)
		    || frame.AddrPC.Offset == 0)
			break;
		alignas(SYMBOL_INFO) char buf[sizeof(SYMBOL_INFO) + 256];
		SYMBOL_INFO* sym  = (SYMBOL_INFO*)buf;
		sym->SizeOfStruct = sizeof(SYMBOL_INFO);
		sym->MaxNameLen   = 255;
		DWORD64 disp      = 0;
		IMAGEHLP_LINE64 line = { sizeof(line) };
		DWORD lineDisp       = 0;
		const bool hasSym    = SymFromAddr(process, frame.AddrPC.Offset, &disp, sym);
		const bool hasLine   = SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisp, &line);
		std::fprintf(stderr, "  #%02d %p  %s", i, (void*)frame.AddrPC.Offset, hasSym ? sym->Name : "?");
		if (hasSym)
			std::fprintf(stderr, "+0x%llx", (unsigned long long)disp);
		std::fprintf(stderr, " (rva 0x%llx)",
		             (unsigned long long)(frame.AddrPC.Offset - SymGetModuleBase64(process, frame.AddrPC.Offset)));
		if (hasLine)
			std::fprintf(stderr, "  %s:%lu", line.FileName, line.LineNumber);
		std::fprintf(stderr, "\n");
	}
	std::fflush(stderr);
	return EXCEPTION_EXECUTE_HANDLER;
}

void installCrashHandler() { SetUnhandledExceptionFilter(crashFilter); }
