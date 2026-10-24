#include "cycle_cli.hpp"
#include "installed_runtime.hpp"
#include "tour_cli.hpp"
#include <QCoreApplication>
#include <QtLogging>

#include <cstdio>

#if defined(_WIN32)
	#include <fcntl.h>
	#include <io.h>
#endif

namespace {
	QtMessageHandler PreviousHandler = nullptr;

	/// \brief Drops Qt's notice that it switched a non-UTF-8 LC_CTYPE to
	/// UTF-8; every other message goes to the handler it replaced.
	///
	/// \details [[BPR-1266]]. Constructing QCoreApplication applies the
	/// environment's locale and, on POSIX, switches a non-UTF-8 LC_CTYPE to a
	/// UTF-8 one itself, announcing it with a warning. In a C locale that
	/// notice reached standard error on every successful command, against the
	/// CLI's contract that success writes nothing there. The switch has
	/// already happened, so the notice carries nothing a user must act on.
	/// Qt's warning that it failed to switch is still passed on.
	void drop_utf8_switch_notice(const QtMsgType aType, const QMessageLogContext& aContext, const QString& aMessage)
	{
		if (aType == QtWarningMsg && aMessage.startsWith(QStringLiteral("Detected locale \""))
			&& aMessage.contains(QStringLiteral("Qt depends on a UTF-8 locale, and has switched to"))) {
			return;
		}
		if (PreviousHandler != nullptr) {
			PreviousHandler(aType, aContext, aMessage);
		}
	}
}

int main(int aCount, char** aArguments)
{
#if defined(_WIN32)
	// [[BPR-1167]]. The CLI's output and input are exact bytes: a JSON document
	// ends in one LF, an export prints byte for byte what it saves, and text
	// read from standard input is stored as given. Windows opens the standard
	// streams in text mode, which wrote every LF as CR LF and read CR LF as LF.
	// A stream that is not attached has no descriptor and is left alone.
	for (auto* const stream : { stdin, stdout, stderr }) {
		if (const auto descriptor = _fileno(stream); descriptor >= 0) {
			static_cast<void>(_setmode(descriptor, _O_BINARY));
		}
	}
#endif
	if (!bpr::app::restrict_installed_runtime(aCount, aArguments)) {
		std::fputs("INSTALLED_RUNTIME_REFUSED\n", stderr);
		return 1;
	}
	// Only while QCoreApplication applies the locale; restored right after.
	PreviousHandler = qInstallMessageHandler(drop_utf8_switch_notice);
	QCoreApplication application{ aCount, aArguments };
	qInstallMessageHandler(PreviousHandler);
	QCoreApplication::setOrganizationName(QStringLiteral("BetterPullReview"));
	QCoreApplication::setApplicationName(QStringLiteral("BetterPullReview"));
	const auto arguments = QCoreApplication::arguments().mid(1);
	// [[BPR-1185]]. `bpr` alone names both families. It printed the tour
	// family's help, so `bpr cycle` could not be found from the terminal.
	if (arguments.isEmpty() || arguments == QStringList{ QStringLiteral("help") } || arguments == QStringList{ QStringLiteral("--help") }) {
		std::fputs("bpr - BetterPullReview from a terminal\n"
				   "bpr tour   review tours of a pull request (bpr tour capabilities)\n"
				   "bpr cycle  local Review Cycles of a folder or file (bpr cycle capabilities)\n",
				   stdout);
		return 0;
	}
	// One executable, two command families on the same non-GUI composition.
	if (arguments.front() == QStringLiteral("cycle")) {
		return bpr::app::run_cycle_cli(arguments.mid(1));
	}
	return bpr::app::run_tour_cli(arguments);
}
