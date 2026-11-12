#ifndef BPR_APP_LOCAL_DRAFT_CLI_HPP
#define BPR_APP_LOCAL_DRAFT_CLI_HPP
#include <QStringList>
namespace bpr::app {
	/// \brief Runs one `bpr draft` command (RFC-BPR-0108 AC05): an authoring
	/// harness hands a worktree, and optional context, to the same local
	/// draft Prepare Pull Request… opens. While the desktop owns the profile
	/// the request goes to it over the same-user command channel and the
	/// desktop shows the draft; otherwise it runs here on the profile's store.
	/// Nothing is published. `aArguments` follow `draft`.
	[[nodiscard]] auto run_draft_cli(const QStringList& aArguments) -> int;
}
#endif
