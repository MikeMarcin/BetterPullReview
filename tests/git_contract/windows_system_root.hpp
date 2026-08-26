#ifndef BPR_TESTS_GIT_CONTRACT_WINDOWS_SYSTEM_ROOT_HPP
#define BPR_TESTS_GIT_CONTRACT_WINDOWS_SYSTEM_ROOT_HPP

#include <filesystem>
#include <optional>

#if defined(_WIN32)
	#include <array>

	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
#endif

namespace bpr::tests::git_contract {
	/// \brief The Windows system root the contracts hand the installed-Git
	/// adapter.
	///
	/// Under `Q_OS_WIN` the adapter refuses to construct without one, because
	/// the child's `SystemRoot` is the only environment entry Windows itself
	/// needs. This is the same source the application uses
	/// (`windows_system_root` in `github_application.cpp`): the directory
	/// Windows reports for itself, and nothing on the other platforms
	/// (BPR-0437).
	[[nodiscard]] inline auto windows_system_root()
		-> std::optional<std::filesystem::path>
	{
#if defined(_WIN32)
		auto       value  = std::array<wchar_t, MAX_PATH + 1U>{ };
		const auto length = GetWindowsDirectoryW(
			value.data(), static_cast<UINT>(value.size()));
		if (length == 0U || length >= value.size()) {
			return std::nullopt;
		}
		// The API spells it `C:\WINDOWS`; the directory is `C:\Windows`, and
		// the adapter accepts only the canonical spelling.
		std::error_code error;
		auto            canonical = std::filesystem::canonical(
			std::filesystem::path{ value.data() }, error);
		if (error) {
			return std::nullopt;
		}
		return canonical;
#else
		return std::nullopt;
#endif
	}
} // namespace bpr::tests::git_contract

#endif // BPR_TESTS_GIT_CONTRACT_WINDOWS_SYSTEM_ROOT_HPP
