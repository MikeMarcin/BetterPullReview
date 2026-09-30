#include "bpr/application/review_comment_placement.hpp"
#include "github_projection_mapping.hpp"

#include <algorithm>
#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto source_matches(const github_pull_request_snapshot& aSnapshot,
										  const review_permalink_source&      aSource) -> bool
		{
			if (!aSource.Endpoint || !aSource.LineCount) {
				return false;
			}
			const auto& authority = aSource.Endpoint->request().WorkbenchAuthority;
			const auto& target    = aSource.Target;
			const auto  path      = aSource.Endpoint->path().generic_string();
			return aSnapshot.Profile == authority.Profile && aSnapshot.Host == authority.Host
				&& aSnapshot.Account == authority.Account
				&& aSnapshot.Repository.Repository == authority.Repository
				&& aSnapshot.PullRequest.PullRequest == authority.PullRequest
				&& aSnapshot.PullRequest.Number == target.Number
				&& aSnapshot.Repository.Owner == target.Repository.Owner
				&& aSnapshot.Repository.Name == target.Repository.Name
				&& aSnapshot.PullRequest.HeadOid.value() == core::canonical_hex(target.Semantic.ExpectedHead).view()
				&& std::ranges::any_of(aSnapshot.Files, [&path](const auto& aFile) { return aFile.Path == path; });
		}

		[[nodiscard]] auto root_comment(const github_review_thread_record& aThread)
			-> const github_review_comment_record*
		{
			const github_review_comment_record* root = nullptr;
			for (const auto& comment : aThread.Comments) {
				if (!comment.ReplyTo) {
					if (root || comment.Deleted) {
						return nullptr;
					}
					root = &comment;
				}
			}
			return root;
		}
	}

	void review_comment_placement::clear()
	{
		Source.reset();
		Roots.clear();
	}

	auto review_comment_placement::project(const github_pull_request_snapshot&    aSnapshot,
										   std::optional<review_permalink_source> aSource) -> github_pull_request_projection
	{
		auto projection = project_github_pull_request(aSnapshot);
		if (!Decode || !aSource || !source_matches(aSnapshot, *aSource)) {
			clear();
			return projection;
		}
		if (Source != aSource) {
			Roots.clear();
		}
		Source                = std::move(aSource);
		auto       nextRoots  = std::map<std::string, cached_root>{ };
		auto       placements = std::map<std::string, std::pair<const parsed_review_permalink*, bool>>{ };
		const auto path       = Source->Endpoint->path().generic_string();
		for (const auto& thread : aSnapshot.ReviewThreads) {
			if (thread.Subject != github_review_thread_subject::file || thread.Path != path) {
				continue;
			}
			const auto* root = root_comment(thread);
			if (!root || root->Path != path || !root->Observation.RawBody) {
				continue;
			}
			const auto& body   = *root->Observation.RawBody;
			const auto  found  = Roots.find(std::string{ thread.Thread.value() });
			auto        cached = found != Roots.end() && found->second.Identity == root->Comment.value() && found->second.Body == body
				? found->second
				: cached_root{ std::string{ root->Comment.value() }, body, Decode(*Source, body) };
			auto&       kept   = nextRoots.emplace(thread.Thread.value(), std::move(cached)).first->second;
			if (!kept.Parsed) {
				continue;
			}
			placements.emplace("review-thread:" + std::string{ thread.Thread.value() }, std::pair{ &*kept.Parsed, false });
			for (const auto& comment : thread.Comments) {
				placements.emplace("review-comment:" + std::string{ comment.Comment.value() }, std::pair{ &*kept.Parsed, comment.Comment == root->Comment });
			}
		}
		for (auto& node : projection.Conversations) {
			const auto found = placements.find(node.Id);
			if (found == placements.end()) {
				continue;
			}
			const auto& parsed      = *found->second.first;
			node.SemanticAnchor     = parsed.Anchor;
			node.Line               = parsed.Anchor.Line + 1U;
			node.DiffSide           = github_conversation_diff_side::right;
			node.AnchorAvailability = github_conversation_anchor_availability::current;
			if (found->second.second) {
				node.ReviewerProse = parsed.Prose;
			}
		}
		Roots = std::move(nextRoots);
		return projection;
	}
}
