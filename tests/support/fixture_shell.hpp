#ifndef BPR_TESTS_SUPPORT_FIXTURE_SHELL_HPP
#define BPR_TESTS_SUPPORT_FIXTURE_SHELL_HPP

/// \file
/// \brief The POSIX shell a course runs a `tests/fixtures` script with.
///
/// \details [[BPR-2215]]. Two local-draft courses made their worktree with a
/// `.sh` fixture through `/bin/sh`, which Windows does not have, so both
/// failed before any step there. On Windows the registration names Git for
/// Windows' own `sh.exe` in `BPR_TEST_POSIX_SH`; it puts that Git first on
/// the script's `PATH`. Paths go to the script with forward slashes, which
/// both that shell and the native Git it runs read.

#include <QProcess>
#include <QString>
#include <QtGlobal>

#include <filesystem>
#include <optional>
#include <string>

namespace bpr::tests {
	[[nodiscard]] inline auto fixture_shell() -> QString
	{
		const auto named = qEnvironmentVariable("BPR_TEST_POSIX_SH");
		return named.isEmpty() ? QStringLiteral("/bin/sh") : named;
	}

	/// \brief Runs `aScript` with `aArgument`; the script's standard error
	/// when it fails, or nothing when it succeeds.
	[[nodiscard]] inline auto run_fixture_script(const std::filesystem::path& aScript, const std::filesystem::path& aArgument) -> std::optional<std::string>
	{
		auto process = QProcess{ };
		process.start(fixture_shell(), { QString::fromStdString(aScript.generic_string()), QString::fromStdString(aArgument.generic_string()) });
		if (!process.waitForFinished(120'000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
			return "shell " + fixture_shell().toStdString() + " exit " + std::to_string(process.exitCode()) + ": " + process.readAllStandardError().toStdString()
				+ process.errorString().toStdString();
		}
		return std::nullopt;
	}
}

#endif // BPR_TESTS_SUPPORT_FIXTURE_SHELL_HPP
