// The Debug CRT answers abort(), a failed assertion, or a report with a modal
// box on the desktop and waits for a person. An automated test that fails
// that way stalls the whole run on exactly the attended interaction the
// boundary exists to rule out; the first Windows Qt-graph run put dozens of
// "abort() has been called" boxes in front of the owner (BPR-0434). The
// error mode a child inherits from the launcher covers the loader and crash
// boxes; this covers the CRT's own, which is an in-process setting and has
// to be made in every test executable. bpr_add_executable links this unit
// into every non-application executable under MSVC, so a failure is an exit
// code and a line on stderr, which is what CTest can act on.
//
// On every other toolchain this unit is empty.
#if defined(_MSC_VER)
	#include <cstdlib>
	#if defined(_DEBUG)
		#include <crtdbg.h>
	#endif

namespace {
	struct quiet_crt_reports final
	{
		quiet_crt_reports() noexcept
		{
			static_cast<void>(::_set_abort_behavior(0U, _WRITE_ABORT_MSG | _CALL_REPORTFAULT));
	#if defined(_DEBUG)
			route_to_stderr(_CRT_WARN);
			route_to_stderr(_CRT_ERROR);
			route_to_stderr(_CRT_ASSERT);
	#endif
		}

	#if defined(_DEBUG)
		static void route_to_stderr(const int aReport) noexcept
		{
			static_cast<void>(::_CrtSetReportMode(aReport, _CRTDBG_MODE_FILE));
			static_cast<void>(::_CrtSetReportFile(aReport, _CRTDBG_FILE_STDERR));
		}
	#endif
	};

	const quiet_crt_reports quiet_crt_reports_instance{ };
} // namespace
#endif
