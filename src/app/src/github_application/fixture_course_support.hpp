#ifndef BPR_APP_GITHUB_APPLICATION_FIXTURE_COURSE_SUPPORT_HPP
#define BPR_APP_GITHUB_APPLICATION_FIXTURE_COURSE_SUPPORT_HPP
#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/qt/workbench_window.hpp"
	#include "github_application_fixture_access.hpp"
	#include <QModelIndex>
	#include <QString>
	#include <QTextStream>
	#include <array>
	#include <chrono>
	#include <cstddef>
	#include <optional>
	#include <string_view>
#endif

// Shared steps of the fixture courses: a course phase and a drawer row by path.
namespace bpr::app {
#if defined(BPR_BUILD_TESTING)
	/// \brief [[BPR-0860]], [[BPR-0864]], [[BPR-1037]]. How long one phase
	/// of a fixture probe may wait for an effect it does not control.
	///
	/// \details A phase's bounded wait names its outstanding condition.
	/// Five seconds remains the default; a course may supply its own finite
	/// functional watchdog without changing the outer run budget. The bound
	/// uses time rather than ticks because the event loop stretches under
	/// load. The call floor keeps one long tick from reading as a stall
	/// ([[BPR-0863]]).
	inline constexpr auto fixture_probe_wait_cap = std::chrono::milliseconds{ 5'000 };

	inline constexpr std::size_t fixture_probe_wait_floor = 16U;

	/// \brief [[BPR-0864]], [[BPR-1037]]. Counts one call of a probe phase
	/// that is still waiting.
	///
	/// \details A refusal on one tick means "not yet" (`src/app/AGENTS.md`),
	/// so a phase keeps waiting until its bound. Then this prints one line,
	/// `TERM:waited_ms=... wait_cap_ms=... waits=... <facts> <modality facts>`.
	/// `aFacts` writes the inputs to the condition the phase waits on. The
	/// line ends with the modality facts every course's failure ends with
	/// ([[BPR-0863]]), which include the host's outstanding requests.
	///
	/// \returns Whether the phase may keep waiting. `false` means it
	/// stalled, and the caller fails the probe.
	template <typename TFacts>
	[[nodiscard]] auto keep_waiting(
		const github_application&       aApplication,
		detail::fixture_probe_wait&     aWait,
		const char* const               aTerm,
		const TFacts&                   aFacts,
		const std::chrono::milliseconds aWaitCap = fixture_probe_wait_cap) -> bool
	{
		const auto now = std::chrono::steady_clock::now();
		if (!aWait.Since.has_value()) {
			aWait.Since = now;
		}
		++aWait.Calls;
		const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(now - *aWait.Since);
		if (waited < aWaitCap || aWait.Calls < fixture_probe_wait_floor) {
			return true;
		}
		auto stream = QTextStream{ stdout };
		stream << aTerm << ":waited_ms=" << static_cast<qlonglong>(waited.count())
			   << " wait_cap_ms=" << static_cast<qlonglong>(aWaitCap.count())
			   << " waits=" << static_cast<qulonglong>(aWait.Calls) << ' ';
		aFacts(stream);
		stream << ' ' << detail::github_application_fixture_access::course_modality_facts(aApplication) << '\n';
		stream.flush();
		return false;
	}

	/// \brief [[BPR-0864]], [[BPR-1037]]. Moves a probe to another phase.
	///
	/// \details The only thing that restarts a phase's wait, so a wait
	/// cannot restart itself on a tick ([[BPR-0859]]).
	template <typename TPhase>
	void enter_phase(TPhase& aPhase, detail::fixture_probe_wait& aWait, const TPhase aNext) noexcept
	{
		aPhase = aNext;
		aWait  = { };
	}

	[[nodiscard]] auto drawer_row(
		const qt::workbench_window& aWindow,
		const std::string_view      aPath) -> QModelIndex;
#endif
}
#endif
