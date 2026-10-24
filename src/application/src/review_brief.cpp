#include "bpr/application/review_brief.hpp"

#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto status_of(const github_file_change aChange) noexcept -> core::brief_file_status
		{
			switch (aChange) {
			case github_file_change::added:
				return core::brief_file_status::added;
			case github_file_change::modified:
				return core::brief_file_status::modified;
			case github_file_change::removed:
				return core::brief_file_status::removed;
			case github_file_change::renamed:
				return core::brief_file_status::renamed;
			case github_file_change::copied:
				return core::brief_file_status::copied;
			case github_file_change::type_changed:
				return core::brief_file_status::type_changed;
			case github_file_change::uninitialized:
			case github_file_change::unchanged:
				break;
			}
			return core::brief_file_status::other;
		}

		void add_comment(core::brief_thread& aThread, const github_review_comment_record& aComment)
		{
			if (aComment.Deleted) {
				return;
			}
			aThread.Comments.push_back({ .Author = aComment.Author ? aComment.Author->Login : std::string{ }, .Body = aComment.Body });
		}
	} // namespace

	auto read_review_brief(const github_pull_request_snapshot& aSnapshot, std::optional<core::brief_review_skill> aSkill) -> core::review_brief_input
	{
		auto input = core::review_brief_input{
			.Title       = aSnapshot.PullRequest.Title,
			.Description = aSnapshot.PullRequest.Body,
			.BaseBranch  = aSnapshot.PullRequest.BaseRef,
			.Skill       = std::move(aSkill),
		};
		input.Files.reserve(aSnapshot.Files.size());
		for (const auto& file : aSnapshot.Files) {
			input.Files.push_back({ .Path = file.Path, .PreviousPath = file.PreviousPath, .Status = status_of(file.Change) });
		}
		if (aSnapshot.ReviewThreadsAvailability == github_collection_availability::available) {
			for (const auto& thread : aSnapshot.ReviewThreads) {
				auto brief = core::brief_thread{
					.Path     = thread.Path,
					.Line     = thread.Line ? thread.Line : thread.OriginalLine,
					.Resolved = thread.Resolved,
					.Outdated = thread.Outdated,
				};
				for (const auto& comment : thread.Comments) {
					add_comment(brief, comment);
				}
				if (!brief.Comments.empty()) {
					input.Threads.push_back(std::move(brief));
				}
			}
		} else if (aSnapshot.ReviewCommentsAvailability == github_collection_availability::available) {
			for (const auto& group : aSnapshot.ReviewCommentGroups) {
				auto brief = core::brief_thread{
					.Path     = group.Root.Path,
					.Line     = group.Root.Line ? group.Root.Line : group.Root.OriginalLine,
					.Outdated = group.Root.Outdated,
				};
				add_comment(brief, group.Root);
				for (const auto& reply : group.Replies) {
					add_comment(brief, reply);
				}
				if (!brief.Comments.empty()) {
					input.Threads.push_back(std::move(brief));
				}
			}
		}
		return input;
	}
} // namespace bpr::application
