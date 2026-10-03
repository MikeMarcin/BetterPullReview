#include "cycle_cli.hpp"
#include "tour_cli.hpp"
#include <QCoreApplication>
#include <QtLogging>

#if defined(_WIN32)
	#include <fcntl.h>
	#include <io.h>

	#include <cstdio>
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
	// Only while QCoreApplication applies the locale; restored right after.
	PreviousHandler = qInstallMessageHandler(drop_utf8_switch_notice);
	QCoreApplication application{ aCount, aArguments };
	qInstallMessageHandler(PreviousHandler);
	QCoreApplication::setOrganizationName(QStringLiteral("BetterPullReview"));
	QCoreApplication::setApplicationName(QStringLiteral("BetterPullReview"));
	const auto arguments = QCoreApplication::arguments().mid(1);
	// One executable, two command families on the same non-GUI composition.
	if (!arguments.isEmpty() && arguments.front() == QStringLiteral("cycle")) {
		return bpr::app::run_cycle_cli(arguments.mid(1));
	}
	return bpr::app::run_tour_cli(arguments);
}
