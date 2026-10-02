#ifndef BPR_APP_CYCLE_CLI_HPP
#define BPR_APP_CYCLE_CLI_HPP
#include <QStringList>
namespace bpr::app {
	/// \brief Runs one bounded `bpr cycle` command (BPR-1167): local Review
	/// Cycles from a terminal, on the desktop's own cycle services, with no
	/// window, account, provider, agent or network. `aArguments` follow `cycle`.
	[[nodiscard]] auto run_cycle_cli(const QStringList& aArguments) -> int;
}
#endif
