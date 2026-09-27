#include "tour_cli.hpp"
#include <QCoreApplication>
int main(int aCount, char** aArguments)
{
	QCoreApplication application{ aCount, aArguments };
	QCoreApplication::setOrganizationName(QStringLiteral("BetterPullReview"));
	QCoreApplication::setApplicationName(QStringLiteral("BetterPullReview"));
	return bpr::app::run_tour_cli(application.arguments().mid(1));
}
