#ifndef BPR_TESTS_SUPPORT_WINDOWS_CRASH_TRACE_HPP
#define BPR_TESTS_SUPPORT_WINDOWS_CRASH_TRACE_HPP

// TEMPORARY DIAGNOSTIC (BPR-2215): prints the faulting stack of an unhandled
// structured exception on Windows, so a crash in teardown names its frame.

#if defined(_WIN32)
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>

	#include <dbghelp.h>

	#include <cstdio>

	#pragma comment(lib, "dbghelp.lib")

namespace bpr::tests::crash_trace {
	inline auto WINAPI handler(EXCEPTION_POINTERS* aPointers) -> LONG
	{
		std::fprintf(stderr, "CRASH code=0x%08lx address=%p\n", aPointers->ExceptionRecord->ExceptionCode, aPointers->ExceptionRecord->ExceptionAddress);
		const auto process = GetCurrentProcess();
		SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
		SymInitialize(process, nullptr, TRUE);
		auto context = *aPointers->ContextRecord;
		auto frame   = STACKFRAME64{ };
		frame.AddrPC.Offset    = context.Rip;
		frame.AddrPC.Mode      = AddrModeFlat;
		frame.AddrFrame.Offset = context.Rbp;
		frame.AddrFrame.Mode   = AddrModeFlat;
		frame.AddrStack.Offset = context.Rsp;
		frame.AddrStack.Mode   = AddrModeFlat;
		for (int depth = 0; depth < 48; ++depth) {
			if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)
				|| frame.AddrPC.Offset == 0) {
				break;
			}
			alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 512]{ };
			auto* const symbol   = reinterpret_cast<SYMBOL_INFO*>(buffer);
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			symbol->MaxNameLen   = 511;
			DWORD64 displacement = 0;
			auto    line         = IMAGEHLP_LINE64{ };
			line.SizeOfStruct    = sizeof(line);
			DWORD lineDisplacement = 0;
			const auto named       = SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol) != FALSE;
			const auto lined       = SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &line) != FALSE;
			std::fprintf(stderr, "CRASH frame %d %s %s:%lu\n", depth, named ? symbol->Name : "?", lined ? line.FileName : "?", lined ? line.LineNumber : 0UL);
		}
		std::fflush(stderr);
		return EXCEPTION_EXECUTE_HANDLER;
	}

	inline void install() { SetUnhandledExceptionFilter(&handler); }
}
#else
namespace bpr::tests::crash_trace {
	inline void install() { }
}
#endif

#endif // BPR_TESTS_SUPPORT_WINDOWS_CRASH_TRACE_HPP
