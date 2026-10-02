#include "cycle_cli.hpp"
#include "tour_cli.hpp"
#include <QCoreApplication>
int main(int aCount, char** aArguments)
{
	QCoreApplication application{ aCount, aArguments };
	QCoreApplication::setOrganizationName(QStringLiteral("BetterPullReview"));
	QCoreApplication::setApplicationName(QStringLiteral("BetterPullReview"));
	const auto arguments = QCoreApplication::arguments().mid(1);
	// One executable, two command families on the same non-GUI composition.
	if (!arguments.isEmpty() && arguments.front() == QStringLiteral("cycle")) {
		return bpr::app::run_cycle_cli(arguments.mid(1));
	}
	return bpr::app::run_tour_cli(arguments);
}
