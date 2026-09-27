#ifndef BPR_APP_TOUR_OBSERVATION_HPP
#define BPR_APP_TOUR_OBSERVATION_HPP
#include "bpr/application/review_tour_import.hpp"
#include "review_comment_permalink.hpp"
namespace bpr::app {
	/// \brief Both clients reconstruct ordinary published links with the same scope.
	[[nodiscard]] inline auto observe_published_tours(const core::review_tour& aSeed, const application::github_pull_request_snapshot& aSnapshot, std::string_view aHostname) -> std::vector<core::tour_import_candidate>
	{
		const auto scope = adapters::github::detail::tour_permalink_scope{ std::string{ aHostname }, { aSnapshot.Repository.Owner, aSnapshot.Repository.Name } };
		return application::observe_review_tours(aSeed, aSnapshot, aHostname, [scope, aSeed](std::string_view aBody) {
			auto source = adapters::github::detail::parse_tour_permalink(scope, aBody);
			if (source && source->Commit == aSeed.Base) {
				source->Side = core::diff_side::left;
			}
			return source;
		});
	}
}
#endif
