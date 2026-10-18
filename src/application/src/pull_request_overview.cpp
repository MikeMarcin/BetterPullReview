#include "bpr/application/pull_request_overview.hpp"

#include "bpr/core/markdown_navigation.hpp"

#include "bpr/core/diff_model.hpp"
#include "bpr/core/file_progress_mark.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace bpr::application {
	namespace {
		/// \brief [[BPR-1339]]. Whether a row counts as done, as
		/// `core::progress_done` decided for the receipt the row carries: it
		/// was read, or its mark is accepted, which it is only while the
		/// receipt is accepted (the core never builds one accepted and
		/// excluded).
		[[nodiscard]] auto row_done(const overview_file_row& aRow) noexcept -> bool
		{
			return aRow.Read == std::optional{ core::file_read_state::read }
			|| aRow.Mark == std::optional{ core::file_progress_mark::accepted };
		}

		[[nodiscard]] auto operation_of(const github_file_change aChange) noexcept
			-> overview_file_operation
		{
			switch (aChange) {
			case github_file_change::added:
				return overview_file_operation::added;
			case github_file_change::removed:
				return overview_file_operation::removed;
			case github_file_change::renamed:
				return overview_file_operation::renamed;
			case github_file_change::copied:
				return overview_file_operation::copied;
			case github_file_change::type_changed:
				return overview_file_operation::type_changed;
			case github_file_change::unchanged:
				return overview_file_operation::unchanged;
			case github_file_change::uninitialized:
			case github_file_change::modified:
				break;
			}
			return overview_file_operation::modified;
		}

		[[nodiscard]] auto signoff_of(const github_review_state aState) noexcept
			-> overview_signoff
		{
			switch (aState) {
			case github_review_state::approved:
				return overview_signoff::approved;
			case github_review_state::changes_requested:
				return overview_signoff::changes_requested;
			case github_review_state::dismissed:
				return overview_signoff::dismissed;
			case github_review_state::pending:
				return overview_signoff::pending;
			case github_review_state::commented:
				return overview_signoff::commented;
			case github_review_state::uninitialized:
				break;
			}
			return overview_signoff::none;
		}

		/// \brief Splits a path at its last separator.
		[[nodiscard]] auto split_path(const std::string_view aPath)
			-> std::pair<std::string, std::string>
		{
			const auto slash = aPath.rfind('/');
			if (slash == std::string_view::npos) {
				return { std::string{ }, std::string{ aPath } };
			}
			return { std::string{ aPath.substr(0U, slash) },
					 std::string{ aPath.substr(slash + 1U) } };
		}

		[[nodiscard]] auto login_of(
			const std::optional<github_actor_record>& aActor) -> std::string
		{
			return aActor.has_value() ? aActor->Login : std::string{ };
		}

		/// \brief [[BPR-1589]]. My latest submitted review's state; a pending
		/// review is not submitted.
		[[nodiscard]] auto my_latest_review(const github_pull_request_snapshot& aSnapshot, const std::string& aViewer)
			-> std::optional<core::my_review_state>
		{
			auto latest = std::optional<core::my_review_state>{ };
			for (const auto& review : aSnapshot.Reviews) {
				if (!review.Author.has_value() || !core::same_login(review.Author->Login, aViewer)) {
					continue;
				}
				switch (review.State) {
				case github_review_state::commented:
					latest = core::my_review_state::commented;
					break;
				case github_review_state::approved:
					latest = core::my_review_state::approved;
					break;
				case github_review_state::changes_requested:
					latest = core::my_review_state::changes_requested;
					break;
				case github_review_state::dismissed:
					latest = core::my_review_state::dismissed;
					break;
				case github_review_state::pending:
				case github_review_state::uninitialized:
					break;
				}
			}
			return latest;
		}

		/// \brief [[BPR-1589]]. Where someone else first mentioned me: the
		/// description, then any comment or review.
		[[nodiscard]] auto mention_of(const github_pull_request_snapshot& aSnapshot, const std::string& aViewer)
			-> std::optional<core::mention_place>
		{
			const auto byOther = [&aViewer](const std::optional<github_actor_record>& aAuthor) {
				return !aAuthor.has_value() || !core::same_login(aAuthor->Login, aViewer);
			};
			if (byOther(aSnapshot.PullRequest.Author) && core::mentions_login(aSnapshot.PullRequest.Body, aViewer)) {
				return core::mention_place::description;
			}
			const auto inComment = [&](const auto& aComment) { return byOther(aComment.Author) && core::mentions_login(aComment.Body, aViewer); };
			if (std::ranges::any_of(aSnapshot.IssueComments, inComment) || std::ranges::any_of(aSnapshot.Reviews, inComment)) {
				return core::mention_place::comment;
			}
			for (const auto& thread : aSnapshot.ReviewThreads) {
				if (std::ranges::any_of(thread.Comments, inComment)) {
					return core::mention_place::comment;
				}
			}
			for (const auto& group : aSnapshot.ReviewCommentGroups) {
				if (inComment(group.Root) || std::ranges::any_of(group.Replies, inComment)) {
					return core::mention_place::comment;
				}
			}
			return std::nullopt;
		}

		/// \brief [[BPR-1589]]. Decides my roles and the attention list.
		void project_status(const github_pull_request_snapshot& aSnapshot, pull_request_overview_projection& aProjection)
		{
			const auto& viewer = aSnapshot.Viewer.Login;
			auto        facts  = core::review_role_facts{ .Viewer = viewer, .Author = aProjection.AuthorLogin };
			for (const auto& request : aProjection.ReviewRequests) {
				facts.Requests.push_back({ .Name = request.Name, .Team = request.Team, .CodeOwner = request.CodeOwner });
			}
			if (!viewer.empty()) {
				facts.LatestReview = my_latest_review(aSnapshot, viewer);
				facts.Mention      = mention_of(aSnapshot, viewer);
			}
			aProjection.Status.Viewer = viewer;
			aProjection.Status.Roles  = core::decide_review_roles(facts);

			auto        attention = core::status_attention_facts{ .Viewer = viewer, .RepliesToMe = aProjection.SinceLastReview.RepliesToMe };
			const auto& merge     = aSnapshot.Merge;
			attention.Conflicts   = merge.Assessment == github_merge_assessment::conflicts
				&& (merge.Availability == github_observation_availability::available || merge.Stale);
			for (const auto& row : aProjection.CheckRows) {
				if (row.Failed && !std::ranges::contains(attention.FailedChecks, row.Name)) {
					attention.FailedChecks.push_back(row.Name);
				}
			}
			for (const auto& request : aProjection.ReviewRequests) {
				attention.WaitingOn.push_back(request.Name);
			}
			aProjection.Status.Attention = core::decide_status_attention(attention);
		}

		/// \brief A participant list that keeps first-seen order.
		class participant_roster final
		{
		public:
			explicit participant_roster(std::string aViewer)
				: Viewer{ std::move(aViewer) }
			{
			}

			void place(
				const std::string&              aLogin,
				const overview_participant_role aRole,
				const overview_signoff          aSignoff)
			{
				if (aLogin.empty()) {
					return;
				}
				auto* const existing = find(aLogin);
				if (existing == nullptr) {
					Participants.push_back(overview_participant{
						.Login   = aLogin,
						.Role    = aRole,
						.Signoff = aSignoff,
						.Viewer  = aLogin == Viewer,
					});
					return;
				}
				// A stronger role wins; the author stays the author however
				// much they comment, and a reviewer who also commented is a
				// reviewer. A later review replaces an earlier standing.
				if (aRole < existing->Role) {
					existing->Role = aRole;
				}
				if (aSignoff != overview_signoff::commented
					|| existing->Signoff == overview_signoff::none) {
					if (existing->Role != overview_participant_role::author
						|| aSignoff != overview_signoff::commented) {
						existing->Signoff = aSignoff;
					}
				}
			}

			[[nodiscard]] auto take() -> std::vector<overview_participant>
			{
				return std::move(Participants);
			}

		private:
			[[nodiscard]] auto find(const std::string& aLogin) -> overview_participant*
			{
				const auto found = std::ranges::find(
					Participants, aLogin, &overview_participant::Login);
				return found == Participants.end() ? nullptr : &*found;
			}

			std::string                       Viewer;
			std::vector<overview_participant> Participants;
		};

		struct thread_facts
		{
			std::size_t Threads             = 0U;
			std::size_t Unresolved          = 0U;
			std::size_t RepliesToMe         = 0U;
			std::size_t MyUnresolvedThreads = 0U;
		};

		[[nodiscard]] auto same_incarnation(
			const core::epoch_catalog_identity& aLeft,
			const core::epoch_catalog_identity& aRight) noexcept -> bool
		{
			return aLeft.Incarnation == aRight.Incarnation;
		}
		[[nodiscard]] auto availability_note(github_observation_availability aAvailability) -> std::string
		{
			switch (aAvailability) {
			case github_observation_availability::available:   return "observed";
			case github_observation_availability::permission:  return "access denied or resource unavailable";
			case github_observation_availability::unsupported: return "unsupported by the selected host";
			case github_observation_availability::incomplete:  return "incomplete or outside the read limit";
			case github_observation_availability::failed:      return "read failed";
			case github_observation_availability::unobserved:  return "not observed";
			}
			return "not observed";
		}

		[[nodiscard]] auto requested(const github_review_request_record& aRequest) -> overview_review_request
		{
			auto result      = overview_review_request{ };
			result.CodeOwner = aRequest.AsCodeOwner == true;
			if (aRequest.Kind == github_requested_actor_kind::user) {
				result.Name = aRequest.Login;
			} else if (aRequest.Kind == github_requested_actor_kind::team) {
				result.Name        = aRequest.Organization + "/" + aRequest.Login;
				result.DisplayName = aRequest.Name;
				result.Team        = true;
			}
			return result;
		}

		/// \brief Indexes required contexts and observed producers once per accepted snapshot.
		class overview_check_policy_index
		{
		public:
			explicit overview_check_policy_index(const github_pull_request_snapshot& aSnapshot)
			{
				const auto& policy = aSnapshot.BranchPolicy;
				Complete           = policy.RulesAvailability == github_observation_availability::available && policy.ProtectionAvailability == github_observation_availability::available && !policy.RulesStale && !policy.ProtectionStale;
				for (const auto& [rules, stale] : { std::pair{ &policy.Rules, policy.RulesStale }, std::pair{ &policy.Protection, policy.ProtectionStale } }) {
					for (const auto& rule : *rules) {
						for (const auto& check : rule.Checks) {
							auto [entry, inserted] = Required[check.Context].try_emplace(check.AppId, stale);
							if (!inserted) {
								entry->second = entry->second && stale;
							}
						}
					}
				}
				for (const auto& check : aSnapshot.Checks) {
					ObservedNames.insert(check.Name);
					ObservedResults.emplace(check.Name, check.AppId);
				}
			}
			[[nodiscard]] auto label(const github_check_record& aCheck) const -> std::string
			{
				if (const auto named = Required.find(aCheck.Name); named != Required.end()) {
					auto stale = std::optional<bool>{ };
					for (const auto& app : { std::optional<std::uint64_t>{ }, aCheck.AppId }) {
						if (const auto matched = named->second.find(app); matched != named->second.end()) {
							stale = stale ? *stale && matched->second : matched->second;
						}
					}
					if (stale) {
						return *stale ? "Required (stale policy)" : "Required";
					}
					if (!aCheck.AppId) {
						return "Requirement unknown (app identity unavailable)";
					}
				}
				return Complete ? "Optional" : "Requirement unknown";
			}
			[[nodiscard]] auto observed(const github_required_check& aCheck) const -> bool
			{
				return aCheck.AppId ? ObservedResults.contains({ aCheck.Context, aCheck.AppId }) : ObservedNames.contains(aCheck.Context);
			}

		private:
			bool                                                                Complete = false;
			std::map<std::string, std::map<std::optional<std::uint64_t>, bool>> Required;
			std::set<std::string>                                               ObservedNames;
			std::set<std::pair<std::string, std::optional<std::uint64_t>>>      ObservedResults;
		};

		/// \brief [[BPR-1405]]. The assessment as one toned phrase, and the
		/// sentence that qualifies it for the details.
		struct merge_reading
		{
			std::string_view    State;
			overview_merge_tone Tone = overview_merge_tone::unknown;
			std::string_view    Sentence;
		};

		[[nodiscard]] auto read_merge(const github_merge_observation& aMerge) -> merge_reading
		{
			if (aMerge.Availability != github_observation_availability::available && !aMerge.Stale) {
				return { "Merge state unavailable", overview_merge_tone::unknown, "GitHub merge assessment unavailable" };
			}
			switch (aMerge.Assessment) {
			case github_merge_assessment::unknown:   return { "Merge state unknown", overview_merge_tone::unknown, "GitHub is checking merge requirements, or the assessment is unknown" };
			case github_merge_assessment::behind:    return { "Behind base", overview_merge_tone::blocked, "GitHub reports the branch is behind its base" };
			case github_merge_assessment::blocked:   return { "Blocked", overview_merge_tone::blocked, "GitHub reports merging is blocked" };
			case github_merge_assessment::clean:     return { "Mergeable", overview_merge_tone::mergeable, "GitHub reports mergeable with passing status. Your merge permission is not established" };
			case github_merge_assessment::conflicts: return { "Conflicts", overview_merge_tone::blocked, "GitHub reports merge conflicts" };
			case github_merge_assessment::draft:     return { "Draft", overview_merge_tone::unknown, "Merging is blocked while this pull request is a draft" };
			case github_merge_assessment::hooks:     return { "Mergeable", overview_merge_tone::mergeable, "GitHub reports mergeable with passing status; server hooks apply. Your merge permission is not established" };
			case github_merge_assessment::unstable:  return { "Mergeable, checks not passing", overview_merge_tone::caution, "GitHub reports mergeable with non-passing status" };
			}
			return { "Merge state unknown", overview_merge_tone::unknown, "The merge assessment is unknown" };
		}

		void project_merge_requirements(const github_pull_request_snapshot& aSnapshot, pull_request_overview_projection& aProjection, const overview_check_policy_index& aChecks)
		{
			const auto& merge      = aSnapshot.Merge;
			const auto  reading    = read_merge(merge);
			aProjection.MergeState = reading.State;
			aProjection.MergeTone  = reading.Tone;
			auto& conditions       = aProjection.MergeConditions;
			auto& details          = aProjection.RequirementDetails;
			details                = std::string{ reading.Sentence } + (merge.Availability != github_observation_availability::available ? ": " + availability_note(merge.Availability) + "." : ".");
			if (merge.Stale) {
				details += " Last known assessment.";
			}

			const auto& policy  = aSnapshot.BranchPolicy;
			auto        minimum = std::optional<std::uint32_t>{ };
			for (const auto* rules : { &policy.Rules, &policy.Protection }) {
				for (const auto& rule : *rules) {
					if (rule.ApprovalMinimum) {
						minimum = std::max(minimum.value_or(0U), *rule.ApprovalMinimum);
					}
				}
			}
			switch (merge.Review) {
			case github_review_decision::required:
				// A configured minimum, never a count of approvals still missing.
				conditions.push_back(minimum ? "Review required, minimum " + std::to_string(*minimum) + (*minimum == 1U ? " approval" : " approvals") : std::string{ "Review required" });
				break;
			case github_review_decision::changes_requested:
				conditions.emplace_back("Changes requested");
				break;
			case github_review_decision::approved:
				details += "\nGitHub's review assessment is approved.";
				break;
			case github_review_decision::unknown:
				details += "\nReview assessment unavailable.";
				break;
			}

			for (const auto& request : aSnapshot.ReviewRequests) {
				aProjection.ReviewRequests.push_back(requested(request));
			}
			// Code owners first; the provider's order otherwise.
			std::ranges::stable_partition(aProjection.ReviewRequests, &overview_review_request::CodeOwner);
			if (aSnapshot.ReviewRequestsStale) {
				aProjection.ReviewRequestsNote = "Last known; refresh to update";
			} else if (aSnapshot.ReviewRequestsAvailability != github_observation_availability::available) {
				aProjection.ReviewRequestsNote = aProjection.ReviewRequests.empty() ? "Review requests unavailable: " + availability_note(aSnapshot.ReviewRequestsAvailability) : "Code owners and the complete list unavailable: " + availability_note(aSnapshot.ReviewRequestsAvailability);
			}

			details += "\nTarget " + aSnapshot.PullRequest.BaseRef + ", head " + std::string{ aSnapshot.PullRequest.HeadOid.value().substr(0, 12) } + ".";
			details += merge.AutoMerge ? (*merge.AutoMerge ? "\nAuto-merge is enabled; the visible conditions still apply." : "\nAuto-merge is not enabled.") : "\nAuto-merge state unavailable.";
			if (merge.Stale && merge.AutoMerge) {
				details += " Last known auto-merge observation.";
			}
			details += "\nRequested code-owner teams are who is waited on, not proof that each team must approve.";
			auto shownMissing = std::unordered_set<std::string>{ };
			auto missing      = std::vector<std::string>{ };
			for (const auto& [name, availability, stale] : { std::tuple{ "Applicable rules", policy.RulesAvailability, policy.RulesStale }, std::tuple{ "Classic branch protection", policy.ProtectionAvailability, policy.ProtectionStale } }) {
				if (availability != github_observation_availability::available || stale) {
					details += "\n" + std::string{ name } + ": " + availability_note(availability) + (stale ? " (stale)." : ".");
				}
			}
			for (const auto* rules : { &policy.Rules, &policy.Protection }) {
				for (const auto& rule : *rules) {
					details += "\n\n" + rule.SourceKind + ": " + rule.Source;
					if (rule.RulesetId) {
						details += " (ruleset " + std::to_string(rule.RulesetId) + ")";
					}
					if (rule.ApprovalMinimum) {
						details += "\nConfigured approval minimum: " + std::to_string(*rule.ApprovalMinimum) + ". Remaining approvals are not established.";
					}
					if (rule.CodeOwners) {
						details += "\nCode-owner review required.";
					}
					if (rule.StrictChecks) {
						details += "\nChecks must test the latest base. This does not establish whether the branch is currently behind.";
					}
					if (rule.LastPushApproval) {
						details += "\nApproval of the latest push by another person is required.";
					}
					if (rule.DismissStaleReviews) {
						details += "\nNew pushes dismiss stale approvals.";
					}
					if (rule.Type.find("conversation") != std::string::npos) {
						details += "\nConversation resolution required; not evaluated here.";
					}
					if (!rule.ApprovalMinimum && rule.Checks.empty()) {
						details += "\nRequired rule: " + rule.Type + "; not evaluated here.";
					}
					for (const auto& [team, teamMinimum] : rule.RequiredTeams) {
						details += "\nSeparate team-review policy for team ID " + std::to_string(team) + ", minimum " + std::to_string(teamMinimum) + "; matching file patterns are not evaluated here.";
					}
					for (const auto& check : rule.Checks) {
						details += "\nRequired check: " + check.Context + (check.AppId ? " (app " + std::to_string(*check.AppId) + ")" : " (any app)") + ".";
						if (!aChecks.observed(check)) {
							const auto identity = "required:" + check.Context + ":" + (check.AppId ? std::to_string(*check.AppId) : "any");
							if (shownMissing.insert(identity).second) {
								auto row        = overview_check_row{ };
								row.Identity    = identity;
								row.Name        = check.Context;
								row.Kind        = "Required check";
								row.Outcome     = aProjection.ChecksComplete ? "Waiting for check result" : "Result unavailable";
								row.Pending     = true;
								row.Requirement = (rules == &policy.Rules ? policy.RulesStale : policy.ProtectionStale) ? "Required (stale policy)" : "Required";
								row.Description = "No matching result was observed on the accepted head. Test merge evaluation remains unknown.";
								aProjection.CheckRows.push_back(std::move(row));
								missing.push_back(check.Context + (aProjection.ChecksComplete ? " has not reported" : " result unavailable"));
							}
						}
					}
				}
			}
			// [[BPR-1589]]. The Status drawer names each failure once, in its
			// attention list; its details show the other conditions.
			auto& open = aProjection.Status.OpenConditions;
			open       = conditions;
			for (const auto& row : aProjection.CheckRows) {
				if (row.Failed) {
					conditions.push_back(row.Name + " failed" + (row.Requirement == "Optional" ? " (optional)" : ""));
				}
			}
			open.insert(open.end(), missing.begin(), missing.end());
			std::ranges::move(missing, std::back_inserter(conditions));
			if (merge.Stale) {
				conditions.emplace_back("Last known; refresh to update");
				open.emplace_back("Last known; refresh to update");
			}
		}
		void project_checks(const github_pull_request_snapshot& aSnapshot, pull_request_overview_projection& aProjection, const overview_check_policy_index& aPolicy)
		{
			const auto& request = aSnapshot.PullRequest;
			// Checks.
			aProjection.ChecksComplete = aSnapshot.ChecksAvailability == github_collection_availability::available
				&& aSnapshot.StatusesAvailability == github_collection_availability::available;
			aProjection.ChecksNote = "Head commit " + std::string{ request.HeadOid.value().substr(0, 12) } + "…\nTest merge evaluation is not observed.";
			if (!aProjection.ChecksComplete) {
				aProjection.ChecksNote += " Partial results: ";
				if (aSnapshot.ChecksAvailability != github_collection_availability::available) {
					aProjection.ChecksNote += "check runs unavailable. ";
				}
				if (aSnapshot.StatusesAvailability != github_collection_availability::available) {
					aProjection.ChecksNote += "commit statuses unavailable.";
				}
			}
			for (const auto& check : aSnapshot.Checks) {
				auto& checks         = aProjection.Checks;
				auto  row            = overview_check_row{ };
				row.Identity         = "check:" + std::string{ check.Check.value() };
				row.Name             = check.Name;
				row.Producer         = check.Producer;
				row.Requirement      = aPolicy.label(check);
				row.Kind             = check.Source == github_check_source::check_run ? "Check run" : "Commit status";
				row.Description      = check.Description;
				row.DetailsUrl       = check.DetailsUrl;
				auto linkContext     = core::markdown_context{ };
				linkContext.Revision = 1U;
				const auto link      = core::admit_markdown_link({ linkContext, { }, row.DetailsUrl }, { .Context = linkContext, .BodyAvailable = true, .BrowserAvailable = true });
				row.LinkAvailable    = link && link->Kind == core::markdown_destination_kind::absolute_web;
				row.Pending          = check.Status != github_check_status::completed;
				switch (check.Status) {
				case github_check_status::queued:        row.Outcome = "Queued"; break;
				case github_check_status::in_progress:   row.Outcome = "In progress"; break;
				case github_check_status::waiting:       row.Outcome = "Waiting"; break;
				case github_check_status::requested:     row.Outcome = "Requested"; break;
				case github_check_status::pending:       row.Outcome = "Pending"; break;
				case github_check_status::uninitialized: row.Outcome = "Unknown"; break;
				case github_check_status::completed:     break;
				}
				if (!row.Pending) {
					switch (check.Conclusion) {
					case github_check_conclusion::success: row.Outcome = "Passed"; break;
					case github_check_conclusion::failure:
						row.Outcome = check.StatusError ? "Error" : "Failed";
						row.Failed  = true;
						break;
					case github_check_conclusion::timed_out:
						row.Outcome = "Timed out";
						row.Failed  = true;
						break;
					case github_check_conclusion::action_required:
						row.Outcome = "Action required";
						row.Failed  = true;
						break;
					case github_check_conclusion::cancelled:
						row.Outcome = "Cancelled";
						row.Failed  = true;
						break;
					case github_check_conclusion::skipped:       row.Outcome = "Skipped"; break;
					case github_check_conclusion::neutral:       row.Outcome = "Neutral"; break;
					case github_check_conclusion::stale:         row.Outcome = "Stale"; break;
					case github_check_conclusion::none:
					case github_check_conclusion::uninitialized: row.Outcome = "Unknown"; break;
					}
				}
				aProjection.CheckRows.push_back(std::move(row));
				if (check.Status != github_check_status::completed) {
					++checks.Pending;
					continue;
				}
				switch (check.Conclusion) {
				case github_check_conclusion::success:
					++checks.Passed;
					break;
				case github_check_conclusion::failure:
				case github_check_conclusion::timed_out:
				case github_check_conclusion::action_required:
				case github_check_conclusion::cancelled:
					++checks.Failed;
					break;
				case github_check_conclusion::skipped:
					++checks.Skipped;
					break;
				case github_check_conclusion::uninitialized:
				case github_check_conclusion::none:
				case github_check_conclusion::neutral:
				case github_check_conclusion::stale:
					++checks.Other;
					break;
				}
			}
		}
		void sort_check_rows(pull_request_overview_projection& aProjection)
		{
			std::ranges::sort(aProjection.CheckRows, [](const overview_check_row& aLeft, const overview_check_row& aRight) {
				const auto group = [](const overview_check_row& aRow) { return aRow.Failed ? 0 : aRow.Pending ? 1
																											  : 2; };
				return std::pair{ group(aLeft), aLeft.Identity } < std::pair{ group(aRight), aRight.Identity };
			});
		}

	} // namespace

	auto project_overview_history(const std::span<const epoch_row_projection> aUpdates,
								  const std::optional<core::git_commit_oid>&  aReviewed) -> overview_history_update
	{
		auto result = overview_history_update{ .Known = aReviewed.has_value() };
		if (!aReviewed) {
			return result;
		}
		const epoch_row_projection* first = nullptr;
		const auto                  key   = [](const epoch_row_projection& aRow) { return std::pair{ aRow.ObservedAt, aRow.Id }; };
		for (const auto& row : aUpdates) {
			if (row.Head == *aReviewed && (first == nullptr || key(row) < key(*first))) {
				first = &row;
			}
		}
		if (first != nullptr) {
			result.ReviewedAt = first->ObservedAt;
			result.Uploads    = static_cast<std::size_t>(std::ranges::count_if(aUpdates, [&](const epoch_row_projection& aRow) { return key(aRow) >= key(*first); })) - 1U;
		}
		return result;
	}

	auto project_pull_request_overview(
		const github_pull_request_snapshot& aSnapshot,
		const overview_context&             aContext)
		-> pull_request_overview_projection
	{
		auto        projection = pull_request_overview_projection{ };
		const auto& request    = aSnapshot.PullRequest;
		const auto  viewer     = aSnapshot.Viewer.Login;

		projection.Owner               = aSnapshot.Repository.Owner;
		projection.RepositoryName      = aSnapshot.Repository.Name;
		projection.Number              = request.Number;
		projection.Title               = request.Title;
		projection.Body                = request.Body;
		projection.AuthorLogin         = login_of(request.Author);
		projection.BaseRef             = request.BaseRef;
		projection.HeadRef             = request.HeadRef;
		projection.State               = request.State;
		projection.Draft               = request.Draft;
		projection.CommitCount         = request.ReportedCommitCount.value_or(aSnapshot.Commits.size());
		projection.ReturnedCommitCount = aSnapshot.Commits.size();

		// My last review, and what the retained updates say about it.
		const auto lastReviewed = aContext.Private.has_value()
			? aContext.Private->LastReviewedHead
			: std::nullopt;
		auto&      since        = projection.SinceLastReview;
		const auto history      = project_overview_history(aContext.Updates, lastReviewed);
		since.Known             = history.Known;
		since.ReviewedAt        = history.ReviewedAt;
		since.Uploads           = history.Uploads;
		const auto headMoved    = lastReviewed.has_value()
			&& (!aContext.Head.has_value() || *aContext.Head != *lastReviewed);
		auto changedPaths = std::unordered_set<std::string_view>{ };
		if (aContext.ChangedSinceReviewPaths.has_value()) {
			for (const auto& path : *aContext.ChangedSinceReviewPaths) {
				changedPaths.insert(path);
			}
		}
		// A head I already reviewed has nothing changed since; a moved head has
		// a count only once its change set is resolved.
		if (lastReviewed.has_value() && (!headMoved || aContext.ChangedSinceReviewPaths.has_value())) {
			since.FilesChanged = 0U;
		}

		// Threads, per file and mine.
		auto factsByPath = std::unordered_map<std::string_view, thread_facts>{ };
		auto factsFor    = [&factsByPath](const std::string& aPath) -> thread_facts& {
			return factsByPath[std::string_view{ aPath }];
		};
		auto roster = participant_roster{ viewer };
		roster.place(projection.AuthorLogin, overview_participant_role::author, overview_signoff::uploaded);
		if (aSnapshot.ReviewThreadsAvailability == github_collection_availability::available) {
			for (const auto& thread : aSnapshot.ReviewThreads) {
				auto& facts = factsFor(thread.Path);
				++facts.Threads;
				if (!thread.Resolved) {
					++facts.Unresolved;
				}
				const auto rootLogin = thread.Comments.empty()
					? std::string{ }
					: login_of(thread.Comments.front().Author);
				if (!thread.Resolved && !viewer.empty() && rootLogin == viewer) {
					++since.MyUnresolvedThreads;
				}
				// [[BPR-0972]]. A reply finds its parent by binary search over
				// the thread's comments; the first comment with an id answers,
				// as a search from the front did.
				auto byComment = std::vector<std::pair<github_comment_id, std::size_t>>{ };
				byComment.reserve(thread.Comments.size());
				for (std::size_t index = 0U; index < thread.Comments.size(); ++index) {
					byComment.emplace_back(thread.Comments[index].Comment, index);
				}
				std::ranges::sort(byComment);
				for (const auto& comment : thread.Comments) {
					const auto login = login_of(comment.Author);
					roster.place(login, overview_participant_role::observer, overview_signoff::commented);
					if (comment.ReplyTo.has_value() && !viewer.empty() && login != viewer) {
						const auto parent = std::ranges::lower_bound(
							byComment, *comment.ReplyTo, { }, &std::pair<github_comment_id, std::size_t>::first);
						if (parent != byComment.end() && parent->first == *comment.ReplyTo
							&& login_of(thread.Comments[parent->second].Author) == viewer) {
							++since.RepliesToMe;
						}
					}
				}
			}
		} else if (aSnapshot.ReviewCommentsAvailability == github_collection_availability::available) {
			for (const auto& group : aSnapshot.ReviewCommentGroups) {
				auto& facts = factsFor(group.Root.Path);
				++facts.Threads;
				const auto rootLogin = login_of(group.Root.Author);
				roster.place(rootLogin, overview_participant_role::observer, overview_signoff::commented);
				for (const auto& reply : group.Replies) {
					const auto login = login_of(reply.Author);
					roster.place(login, overview_participant_role::observer, overview_signoff::commented);
					if (!viewer.empty() && rootLogin == viewer && login != viewer) {
						++since.RepliesToMe;
					}
				}
			}
		}
		for (const auto& comment : aSnapshot.IssueComments) {
			roster.place(login_of(comment.Author), overview_participant_role::observer, overview_signoff::commented);
		}
		for (const auto& review : aSnapshot.Reviews) {
			const auto login = login_of(review.Author);
			if (login == projection.AuthorLogin) {
				continue;
			}
			roster.place(login, overview_participant_role::reviewer, signoff_of(review.State));
		}
		projection.Participants = roster.take();

		const auto checkPolicy = overview_check_policy_index{ aSnapshot };
		project_checks(aSnapshot, projection, checkPolicy);
		project_merge_requirements(aSnapshot, projection, checkPolicy);
		sort_check_rows(projection);
		project_status(aSnapshot, projection);

		// The files. [[BPR-0972]]. Receipts and notes are ordered by file once,
		// so each file's are a binary search; the first for a file answers,
		// as a search from the front did.
		auto receipts = std::vector<const core::file_receipt*>{ };
		receipts.reserve(aContext.Receipts.size());
		for (const auto& receipt : aContext.Receipts) {
			receipts.push_back(&receipt);
		}
		std::ranges::stable_sort(receipts, { }, [](const core::file_receipt* aReceipt) { return aReceipt->File; });
		auto notes = std::vector<const local_note_record*>{ };
		if (aContext.Private.has_value()) {
			for (const auto& note : aContext.Private->Notes) {
				if (note.Note.File.has_value()) {
					notes.push_back(&note);
				}
			}
			std::ranges::stable_sort(notes, { }, [](const local_note_record* aNote) { return *aNote->Note.File; });
		}
		projection.Files.reserve(aSnapshot.Files.size());
		for (const auto& file : aSnapshot.Files) {
			auto row                       = overview_file_row{ };
			row.Path                       = file.Path;
			row.PreviousPath               = file.PreviousPath;
			std::tie(row.Folder, row.Name) = split_path(file.Path);
			row.Operation                  = operation_of(file.Change);
			row.Additions                  = file.Additions;
			row.Deletions                  = file.Deletions;
			// Unchanged unless the head moved since my last review: then unknown
			// until the change set is resolved, and changed or new by that set.
			if (lastReviewed.has_value() && headMoved && !aContext.ChangedSinceReviewPaths.has_value()) {
				row.Changed = overview_change_since_review::unknown;
			} else if (lastReviewed.has_value() && headMoved && changedPaths.contains(file.Path)) {
				row.Changed = row.Operation == overview_file_operation::added
					? overview_change_since_review::added
					: overview_change_since_review::changed;
				++*since.FilesChanged;
			}
			// [[BPR-1404]]. A file identity joins private observations. Without
			// either population there is nothing to join, so a refresh need not
			// derive every path's identity on the serialized presentation lane.
			if (!receipts.empty() || !notes.empty()) {
				const auto identity = core::file_identity_of(file.Path);
				const auto receipt  = std::ranges::lower_bound(receipts, identity, { }, [](const core::file_receipt* aReceipt) { return aReceipt->File; });
				if (receipt != receipts.end() && (*receipt)->File == identity) {
					row.Read = (*receipt)->Read;
					row.Mark = core::progress_mark(**receipt);
					if (core::progress_done(**receipt)) {
						++projection.Progress.FilesRead;
					}
				}
				const auto note = std::ranges::lower_bound(notes, identity, { }, [](const local_note_record* aNote) { return *aNote->Note.File; });
				if (note != notes.end() && *(*note)->Note.File == identity) {
					row.Note = (*note)->Note.Text;
				}
			}
			const auto facts = factsByPath.find(std::string_view{ file.Path });
			if (facts != factsByPath.end()) {
				row.ThreadCount           = facts->second.Threads;
				row.UnresolvedThreadCount = facts->second.Unresolved;
			}
			projection.Files.push_back(std::move(row));
		}
		std::ranges::stable_sort(projection.Files, [](const overview_file_row& aLeft, const overview_file_row& aRight) {
			return std::pair{ std::string_view{ aLeft.Folder }, std::string_view{ aLeft.Name } }
			< std::pair{ std::string_view{ aRight.Folder }, std::string_view{ aRight.Name } };
		});

		projection.Progress.FilesTotal      = projection.Files.size();
		projection.Progress.PrivateNotes    = aContext.Private.has_value() ? aContext.Private->Notes.size() : 0U;
		projection.Progress.DraftedComments = aContext.DraftedComments;

		// The one action. A first visit starts at the first file; otherwise
		// the review continues at the first file that changed since my last
		// review, else the first file not yet done: neither read nor accepted.
		const auto firstVisit = !lastReviewed.has_value() && aContext.Receipts.empty();
		projection.Entry      = firstVisit ? overview_entry_kind::start_review : overview_entry_kind::continue_review;
		const auto changed    = std::ranges::find_if(projection.Files, [](const overview_file_row& aRow) {
			return aRow.Changed == overview_change_since_review::changed
				|| aRow.Changed == overview_change_since_review::added;
		});
		const auto unread     = std::ranges::find_if(projection.Files, [](const overview_file_row& aRow) { return !row_done(aRow); });
		if (!firstVisit && changed != projection.Files.end()) {
			projection.EntryPath = changed->Path;
		} else if (!firstVisit && unread != projection.Files.end()) {
			projection.EntryPath = unread->Path;
		} else if (!projection.Files.empty()) {
			projection.EntryPath = projection.Files.front().Path;
		}
		return projection;
	}

	void overview_receipt_index::adopt(
		const pull_request_overview_projection& aProjection,
		const bool                              aLastReviewedKnown,
		const bool                              aAnyReceipt)
	{
		// [[BPR-1404]]. Refreshes change observations without necessarily
		// changing paths. Reuse only the exact current ordered path list;
		// receipt, progress and navigation state are rebuilt every time.
		const auto samePaths = std::ranges::equal(aProjection.Files, Paths, { }, &overview_file_row::Path);
		if (!samePaths) {
			Rows.clear();
			Paths.clear();
			Rows.reserve(aProjection.Files.size());
			Paths.reserve(aProjection.Files.size());
		}
		NotDone.clear();
		FirstChanged.reset();
		for (std::size_t row = 0U; row < aProjection.Files.size(); ++row) {
			const auto& file = aProjection.Files[row];
			if (!samePaths) {
				Rows.emplace_back(core::file_identity_of(file.Path), row);
				Paths.push_back(file.Path);
			}
			if (!row_done(file)) {
				NotDone.insert(row);
			}
			if (!FirstChanged.has_value()
				&& (file.Changed == overview_change_since_review::changed
					|| file.Changed == overview_change_since_review::added)) {
				FirstChanged = row;
			}
		}
		// The first row for a file answers for it, as the projection's own
		// search did.
		if (!samePaths) {
			std::ranges::stable_sort(Rows, { }, &std::pair<core::file_identity, std::size_t>::first);
		}
		LastReviewedKnown = aLastReviewedKnown;
		AnyReceipt        = aAnyReceipt;
		Adopted           = true;
	}

	void overview_receipt_index::clear() noexcept
	{
		Paths.clear();
		Rows.clear();
		NotDone.clear();
		FirstChanged.reset();
		Adopted = false;
	}

	auto overview_receipt_index::apply(
		pull_request_overview_projection&         aProjection,
		const std::span<const core::file_receipt> aReceipts)
		-> std::optional<overview_receipt_update>
	{
		if (!Adopted) {
			return std::nullopt;
		}
		for (const auto& receipt : aReceipts) {
			if (!std::ranges::binary_search(Rows, receipt.File, { }, &std::pair<core::file_identity, std::size_t>::first)) {
				return std::nullopt;
			}
		}
		auto update = overview_receipt_update{ };
		for (const auto& receipt : aReceipts) {
			// Every row of the file takes the receipt, as the projection gives
			// each row its file's receipt.
			AnyReceipt = true;
			for (const auto& [file, index] : std::ranges::equal_range(Rows, receipt.File, { }, &std::pair<core::file_identity, std::size_t>::first)) {
				// [[BPR-1339]]. An Accept or an Exclude moves the mark and not
				// the read state, so both are compared.
				auto&      row     = aProjection.Files[index];
				const auto mark    = core::progress_mark(receipt);
				const auto wasDone = row_done(row);
				const auto isDone  = core::progress_done(receipt);
				if (row.Read == std::optional{ receipt.Read } && row.Mark == std::optional{ mark }) {
					continue;
				}
				row.Read = receipt.Read;
				row.Mark = mark;
				if (isDone && !wasDone) {
					++aProjection.Progress.FilesRead;
					NotDone.erase(index);
				} else if (wasDone && !isDone) {
					--aProjection.Progress.FilesRead;
					NotDone.insert(index);
				}
				update.Rows.push_back(row);
			}
		}
		// The one action, decided as the projection decides it.
		const auto firstVisit = !LastReviewedKnown && !AnyReceipt;
		aProjection.Entry     = firstVisit ? overview_entry_kind::start_review : overview_entry_kind::continue_review;
		if (!firstVisit && FirstChanged.has_value()) {
			aProjection.EntryPath = aProjection.Files[*FirstChanged].Path;
		} else if (!firstVisit && !NotDone.empty()) {
			aProjection.EntryPath = aProjection.Files[*NotDone.begin()].Path;
		} else if (!aProjection.Files.empty()) {
			aProjection.EntryPath = aProjection.Files.front().Path;
		} else {
			aProjection.EntryPath.reset();
		}
		update.Progress  = aProjection.Progress;
		update.Entry     = aProjection.Entry;
		update.EntryPath = aProjection.EntryPath;
		return update;
	}

	void epoch_row_ledger::apply(const epoch_catalog_projection& aProjection)
	{
		if (aProjection.Replace) {
			Catalog = aProjection.Catalog;
			Rows.clear();
		} else if (aProjection.Catalog.has_value()) {
			if (Catalog.has_value() && !same_incarnation(*Catalog, *aProjection.Catalog)) {
				Rows.clear();
			}
			Catalog = aProjection.Catalog;
		}
		const auto remove = [this](const core::review_epoch_id& aId) {
			std::erase_if(Rows, [&aId](const epoch_row_projection& aRow) { return aRow.Id == aId; });
		};
		const auto insertOrMove = [this, &remove](const std::size_t aIndex, epoch_row_projection aRow) {
			remove(aRow.Id);
			const auto target = std::min(aIndex, Rows.size());
			Rows.insert(Rows.begin() + static_cast<std::ptrdiff_t>(target), std::move(aRow));
		};
		for (const auto& change : aProjection.Changes) {
			switch (change.Kind) {
			case epoch_row_change_kind::insert:
				if (change.Row.has_value() && change.Row->Id == change.Id) {
					insertOrMove(change.Index, *change.Row);
				}
				break;
			case epoch_row_change_kind::remove:
				remove(change.Id);
				break;
			case epoch_row_change_kind::update:
				if (change.Row.has_value() && change.Row->Id == change.Id) {
					const auto existing = std::ranges::find(Rows, change.Id, &epoch_row_projection::Id);
					if (existing != Rows.end()) {
						*existing = *change.Row;
					}
				}
				break;
			}
		}
		while (Rows.size() > core::maximum_epoch_history_page_size) {
			Rows.pop_back();
		}
		if (aProjection.HistoryPage.has_value()
			&& aProjection.HistoryPage->Phase == core::epoch_history_page_phase::loaded
			&& aProjection.HistoryPage->Rows.size() <= core::maximum_epoch_history_page_size
			&& (!Catalog.has_value() || same_incarnation(*Catalog, aProjection.HistoryPage->Catalog))) {
			Catalog          = aProjection.HistoryPage->Catalog;
			const auto& page = aProjection.HistoryPage->Rows;
			std::erase_if(Rows, [&page](const epoch_row_projection& aRow) {
				return std::ranges::find(page, aRow.Id, &epoch_row_projection::Id) == page.end();
			});
			for (std::size_t index = 0U; index < page.size(); ++index) {
				insertOrMove(index, page[index]);
			}
		}
	}

	void epoch_row_ledger::clear()
	{
		Catalog.reset();
		Rows.clear();
	}

	auto epoch_row_ledger::rows() const noexcept -> const std::vector<epoch_row_projection>&
	{
		return Rows;
	}
} // namespace bpr::application
