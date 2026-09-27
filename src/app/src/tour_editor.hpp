#ifndef BPR_APP_TOUR_EDITOR_HPP
#define BPR_APP_TOUR_EDITOR_HPP
#include "tour_runtime.hpp"
#include <QString>
namespace bpr::app {
	struct tour_editor_candidate
	{
		std::string Text;
		QString     Path, Directory;
	};
	/// Explicit trusted argv, private projection, blocking editor completion; owns no profile lease.
	[[nodiscard]] auto run_tour_editor(std::string_view aOriginal) -> tour_cli_result<tour_editor_candidate>;
	/// Removes only the known projection and an empty containing directory. Editor backups stay retained.
	void cleanup_tour_editor(const tour_editor_candidate& aCandidate);
}
#endif
