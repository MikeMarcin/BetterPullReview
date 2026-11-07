#include "github_application/window_composition.hpp"

#include "bpr/application/github_host_authority.hpp"
#include "bpr/application/window_review_restoration_host.hpp"
#include "bpr/core/generated_files.hpp"
#include "bpr/core/off_hunk_fact.hpp"
#include "bpr/core/pull_request_suggestion.hpp"

#include <QByteArrayView>

#include <algorithm>
#include <variant>

namespace bpr::app {
	/// \brief Decides and publishes the overview of the accepted pull request.
	///
	/// \details [[BPR-0647]]. Joined here because no single host holds all
	/// of it: the GitHub host's accepted snapshot, the ledger's receipts,
	/// notes and last-reviewed head, and the epoch host's update rows and
	/// accepted head. The set of files changed since my last review is not
	/// yet resolved by any host, so it is left unknown rather than guessed.
	// [[BPR-0656]]. The identities the ledger keys a conversation's read
	// state by: the same durable half the defects use for the thread, a
	// half of the comment's projection identity for each post, and a
	// half of the body's digest as the edit version, so an edited post
	// is a different version without the provider naming a time.
	auto github_application::implementation::comment_projection_id(
		const application::github_review_comment_record& aComment) -> std::string
	{
		return "review-comment:" + std::string{ aComment.Comment.value() };
	}

	auto github_application::implementation::comment_projection_id(
		const application::github_issue_comment_record& aComment) -> std::string
	{
		return "issue-comment:" + std::string{ aComment.Comment.value() };
	}

	auto github_application::implementation::comment_projection_id(
		const application::github_review_record& aReview) -> std::string
	{
		return "review:" + std::string{ aReview.Review.value() };
	}

	/// \brief The comparison the filter applies to, by its two commits.
	/// \details [[BPR-1707]]. While a refresh resolves the same comparison
	/// again, the core keeps the accepted one in force, so the filtered set
	/// does not lapse and come back and a gesture made meanwhile applies.
	auto github_application::implementation::commit_filter_comparison() -> std::optional<core::filter_comparison>
	{
		if (EpochHost == nullptr) {
			return std::nullopt;
		}
		const auto& state     = EpochHost->state();
		const auto  resolving = state.PendingComparison.has_value();
		if (state.AcceptedComparison.has_value() && !resolving) {
			SettledFilterRequest = state.RequestedSelection;
		}
		// The accepted comparison was resolved for the settled request.
		const auto& request  = resolving && SettledFilterRequest ? *SettledFilterRequest : state.RequestedSelection;
		const auto* named    = std::get_if<core::named_comparison_selection>(&request);
		auto        accepted = state.AcceptedComparison.has_value()
			? std::optional{ core::filter_comparison{
				  .WholePullRequest = named != nullptr && named->Alias == core::named_comparison::whole_pr,
				  .Left             = state.AcceptedComparison->Left.Commit,
				  .Right            = state.AcceptedComparison->Right.Commit,
			  } }
			: std::nullopt;
		return core::filter_comparison_in_force({ .Accepted    = std::move(accepted),
												  .Resolving   = resolving,
												  .SameRequest = SettledFilterRequest == std::optional{ state.RequestedSelection } });
	}

	/// \brief Adopts an accepted review into the filter before the
	/// workbench observes its files.
	void github_application::implementation::adopt_commit_filter(const application::accepted_pull_request_handoff&               aHandoff,
																 const std::optional<application::github_pull_request_snapshot>& aSnapshot)
	{
		const auto review = std::tuple{ aHandoff.Host, aHandoff.Account, aHandoff.Repository, aHandoff.PullRequest };
		if (CommitFilterReview != std::optional{ review }) {
			CommitFilter.clear();
			CommitFilterWanted   = false;
			CommitFilterReview   = review;
			SettledFilterRequest = std::nullopt;
			// RFC-BPR-0089 M2. The review's marks, when the ledger has them.
			adopt_generated_marks();
		}
		if (aSnapshot.has_value()) {
			CommitFilter.adopt(*aSnapshot);
		}
		CommitFilterDestination = std::tuple{ aHandoff.Locator, aHandoff.Number, aHandoff.Account };
		// [[BPR-1688]]. A restored window brings back its filters with the
		// review: a hidden commit the pull request no longer has is named
		// as after a force push.
		if (RestoringFilters && filter_destination(RestoringFilters->first)) {
			static_cast<void>(CommitFilter.restore(RestoringFilters->second.Commits));
			static_cast<void>(CommitFilter.restore_files(RestoringFilters->second.Files));
			RestoringFilters.reset();
		}
		CommitFilter.adopt_comparison(commit_filter_comparison());
		const auto files = CommitFilter.files();
		static_cast<void>(WorkbenchReview.filter(files ? std::optional{ files->Files } : std::nullopt));
		publish_commit_filter();
		retain_filters();
	}

	auto github_application::implementation::filter_destination(const core::window_review_destination& aDestination) const -> bool
	{
		return CommitFilterDestination == std::optional{ std::tuple{ aDestination.Repository, aDestination.Number, aDestination.Account } };
	}

	/// \brief [[BPR-1688]]. Keeps the window's commit and file filters with
	/// its accepted destination, as its tabs are kept. What a hidden commit
	/// changes is not kept: the chain's trees record it again.
	void github_application::implementation::retain_filters()
	{
		if (Closing || ReadingRestoreUnavailable || !ReadingDestination || !Profile->ReviewRestoration || !filter_destination(*ReadingDestination)) {
			return;
		}
		if (RestoringFilters) {
			if (PendingRestoration || filter_destination(RestoringFilters->first)) {
				return;
			}
			RestoringFilters.reset();
		}
		if (PendingRestoration) {
			return;
		}
		auto filters = core::window_review_filters{ .Commits = CommitFilter.state(), .Files = CommitFilter.file_filter() };
		for (auto& hidden : filters.Commits.Hidden) {
			hidden.Changes.reset();
		}
		static_cast<void>(Profile->ReviewRestoration->accepted_filters(DraftWindow, *ReadingDestination, filters));
	}

	void github_application::implementation::publish_commit_filter()
	{
		if (Closing || Window == nullptr) {
			return;
		}
		++CommitFilterPublications;
		Window->publish(CommitFilter.projection());
		present_split_strip();
	}

	/// \brief Asks for the chain's tree diffs once the tab or a filter
	/// needs them. A refusal (the head's objects are still arriving) is
	/// asked again on the next epoch update.
	void github_application::implementation::request_commit_filter_trees()
	{
		if (Closing || EpochHost == nullptr || !LastAcceptedHandoff || (!CommitFilterWanted && CommitFilter.state().Hidden.empty())) {
			return;
		}
		auto steps = CommitFilter.touches_steps();
		if (!steps || !CommitFilter.head()) {
			return;
		}
		auto request = application::commit_filter_touches_request{
			.WorkbenchAuthority = workbench_authority(*LastAcceptedHandoff),
			.Head               = *CommitFilter.head(),
			.Steps              = std::move(*steps),
		};
		if (EpochHost->resolve_commit_filter_touches(std::move(request), [this](const application::commit_filter_touches_observation& aObservation) {
				if (Closing) {
					return;
				}
				CommitFilter.adopt_touches(aObservation);
				refresh_commit_filter();
			})) {
			CommitFilter.begin_touches();
		}
	}

	/// \brief [[BPR-1692]]. Asks for the head's `.gitattributes` once the
	/// review's paths are known, and again when the chain's tree diffs name
	/// more directories. A refusal (the head's objects are still arriving)
	/// is asked again on the next epoch update.
	void github_application::implementation::request_generated_attributes()
	{
		if (Closing || EpochHost == nullptr || !LastAcceptedHandoff) {
			return;
		}
		auto directories = CommitFilter.generated_attributes_wanted();
		if (!directories || !CommitFilter.head()) {
			return;
		}
		auto request = application::generated_attributes_request{
			.WorkbenchAuthority = workbench_authority(*LastAcceptedHandoff),
			.Head               = *CommitFilter.head(),
			.Directories        = *directories,
		};
		if (!CommitFilter.begin_generated_attributes(std::move(*directories))) {
			return;
		}
		if (!EpochHost->resolve_generated_attributes(std::move(request), [this](const application::generated_attributes_observation& aObservation) {
				if (Closing) {
					return;
				}
				CommitFilter.adopt_generated_attributes(aObservation);
				refresh_commit_filter();
			})) {
			CommitFilter.abandon_generated_attributes();
		}
	}

	/// \brief Re-decides the filtered file set and shows it everywhere it
	/// applies: the Commits tab and chips, the Files drawer and walk, and
	/// the Overview.
	/// \brief [[BPR-1802]]. The accepted pull request permalinks are read
	/// for: its host, owner and repository under this window's account and
	/// head; none before a handoff.
	auto github_application::implementation::permalink_snippet_scope() const -> std::optional<application::markdown_blob_snippet_scope>
	{
		if (Closing || !LastAcceptedHandoff) {
			return std::nullopt;
		}
		const auto& handoff = *LastAcceptedHandoff;
		auto        scope   = application::markdown_blob_snippet_scope{ };
		scope.Host          = std::string{ handoff.Hostname.value() };
		scope.Owner         = handoff.Locator.Owner;
		scope.Repository    = handoff.Locator.Name;
		scope.Identity      = std::to_string(handoff.ApplicationSession.value()) + ':' + std::to_string(handoff.Profile.value()) + ':' + std::to_string(handoff.Account.value()) + ':'
			+ std::to_string(handoff.Repository.value()) + ':' + std::to_string(handoff.PullRequest.value()) + ':' + std::string{ core::canonical_hex(handoff.Head).view() };
		return scope;
	}

	void github_application::implementation::refresh_commit_filter()
	{
		if (Closing || Window == nullptr) {
			return;
		}
		// [[BPR-1802]]. The review's objects may now hold a permalink's commit.
		Window->request_markdown_permalinks();
		CommitFilter.adopt_comparison(commit_filter_comparison());
		request_commit_filter_trees();
		request_generated_attributes();
		const auto files   = CommitFilter.files();
		const auto changed = WorkbenchReview.filter(files ? std::optional{ files->Files } : std::nullopt);
		publish_commit_filter();
		retain_filters();
		// [[BPR-1688]]. Find in files searches the filtered set: a new set
		// is a new scope.
		if (PrFind) {
			PrFind->observe_scope();
		}
		if (!changed) {
			return;
		}
		cancel_preparation();
		if (WorkbenchHost != nullptr && LastAcceptedHandoff) {
			WorkbenchHost->observe(workbench_authority(*LastAcceptedHandoff));
			// RFC-BPR-0089 M4. Observing the new file set starts a new
			// workbench generation; the publication host is bound to one, as
			// after a refresh, or every comment under a filter is refused.
			compose_review_mutation(*LastAcceptedHandoff);
		}
		publish_overview();
	}

	void github_application::implementation::apply_commit_filter_gesture(const application::commit_filter_gesture& aGesture)
	{
		if (Closing) {
			return;
		}
		static_cast<void>(CommitFilter.apply(aGesture));
		refresh_commit_filter();
	}

	/// \brief RFC-BPR-0089 M3. The reviewer chose a rematch offer after a
	/// force push; the core applies only an offer it makes now.
	void github_application::implementation::apply_commit_rematch(const application::commit_filter_rematch_gesture& aGesture)
	{
		if (Closing) {
			return;
		}
		static_cast<void>(CommitFilter.rematch(aGesture));
		refresh_commit_filter();
	}

	/// \brief RFC-BPR-0089 M3 (owner, Q2). The strip over a split file's
	/// diff: each per-commit diff with its own receipt, the one on screen,
	/// and the hidden commits named between them. None for any other diff.
	void github_application::implementation::present_split_strip()
	{
		if (Closing || Window == nullptr) {
			return;
		}
		const auto& publication = SelectedDiffPublication;
		if (!publication || !publication->Request.File.SplitByFilter || !publication->Request.Filter) {
			Window->present_split_diff(std::nullopt);
			return;
		}
		auto        parts = CommitFilter.split_parts(publication->Request.File.PresentationPath);
		const auto  files = CommitFilter.files();
		const auto* file  = [&]() -> const application::commit_filter_file* {
			if (!files) {
				return nullptr;
			}
			for (const auto* list : { &files->Files, &files->SetAside }) {
				const auto found = std::ranges::find(*list, publication->Request.File.PresentationPath, [](const application::commit_filter_file& aFile) -> const std::string& { return aFile.Record.Path; });
				if (found != list->end()) {
					return &*found;
				}
			}
			return nullptr;
		}();
		if (!parts || file == nullptr || file->Segments.size() != parts->Parts.size()) {
			Window->present_split_diff(std::nullopt);
			return;
		}
		const auto identity = core::file_identity_of(file->Record.Path);
		{
			const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
			for (std::size_t index = 0U; index < parts->Parts.size(); ++index) {
				auto&      part = parts->Parts[index];
				const auto ends = file->Segments[index].Endpoints;
				part.Selected   = publication->Request.Filter == std::optional{ ends };
				if (ReviewLedgerAvailable && ReviewLedgerHost != nullptr) {
					if (const auto held = ReviewLedgerHost->cached_receipt(core::filtered_receipt_file(identity, ends)); held.has_value() && held->has_value()) {
						part.Read = (*held)->Read;
					}
				}
			}
		}
		Window->present_split_diff(std::move(parts));
	}

	/// \brief RFC-BPR-0089 M4. The merge base GitHub counts `LEFT` lines in:
	/// the one it named for the snapshot, or the recorded base when it named
	/// none ([[BPR-1560]]).
	auto github_application::implementation::filtered_merge_base() const -> std::optional<core::git_commit_oid>
	{
		if (!WindowRead || !WindowRead->accepted_snapshot()) {
			return std::nullopt;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		return core::parse_git_commit_oid(snapshot.MergeBaseAvailability == application::github_merge_base_availability::available && snapshot.MergeBaseOid
											  ? snapshot.MergeBaseOid->value()
											  : snapshot.PullRequest.BaseOid.value());
	}

	/// \brief RFC-BPR-0089 M4. Whether the diff on screen is commit filtered.
	auto github_application::implementation::selected_diff_filtered() const noexcept -> bool
	{
		return SelectedDiffPublication.has_value() && SelectedDiffPublication->Key.Filter.has_value();
	}

	/// \brief RFC-BPR-0089 M4. What the epoch host reads for comments on the
	/// filtered diff on screen: the file at its commits, the head and the
	/// merge base, by the paths the review names there.
	auto github_application::implementation::filtered_comment_lines_request() const -> std::optional<application::filtered_comment_lines_request>
	{
		if (!selected_diff_filtered() || !WindowRead || !WindowRead->accepted_snapshot()) {
			return std::nullopt;
		}
		const auto& snapshot    = *WindowRead->accepted_snapshot();
		const auto& publication = *SelectedDiffPublication;
		const auto  head        = core::parse_git_commit_oid(snapshot.PullRequest.HeadOid.value());
		const auto  mergeBase   = filtered_merge_base();
		if (!head || !mergeBase) {
			return std::nullopt;
		}
		const auto text = [](const std::filesystem::path& aPath) {
			const auto bytes = aPath.generic_u8string();
			return std::string{ bytes.begin(), bytes.end() };
		};
		const auto right    = text(publication.RightPath.value_or(publication.LeftPath.value_or(publication.Request.File.CurrentPath)));
		const auto left     = publication.LeftPath ? text(*publication.LeftPath) : right;
		const auto headPath = text(publication.Request.File.CurrentPath);
		// The merge base names the file as GitHub's whole diff does.
		const auto whole    = std::ranges::find(snapshot.Files, headPath, &application::github_file_record::Path);
		const auto basePath = whole != snapshot.Files.end() && whole->PreviousPath ? *whole->PreviousPath : headPath;
		return application::filtered_comment_lines_request{
			.WorkbenchAuthority = publication.Request.WorkbenchAuthority,
			.Head               = *head,
			.MergeBase          = *mergeBase,
			.Key                = publication.Key,
			.LeftPath           = left,
			.RightPath          = right,
			.HeadPath           = headPath,
			.BasePath           = basePath,
		};
	}

	/// \brief RFC-BPR-0089 M4. The facts the core places a comment on the
	/// filtered diff on screen from: the accepted chain and, once read, the
	/// lines. A read that failed leaves every correspondence absent.
	auto github_application::implementation::filtered_comment_source() const -> std::optional<core::filtered_comment_source>
	{
		const auto  request = filtered_comment_lines_request();
		const auto* graph   = CommitFilter.graph();
		if (!request || graph == nullptr) {
			return std::nullopt;
		}
		auto source = core::filtered_comment_source{
			.Head      = request->Head,
			.MergeBase = request->MergeBase,
			.Diff      = *request->Key.Filter,
			.Chain     = graph->Chain,
			.Fork      = graph->Fork,
			.Lines     = std::nullopt,
		};
		if (FilteredCommentLines && FilteredCommentLines->Request == *request) {
			source.Lines = FilteredCommentLines->Status == application::filtered_comment_lines_status::resolved ? FilteredCommentLines->Lines
																												: core::filtered_comment_lines{ };
		}
		return source;
	}

	/// \brief RFC-BPR-0089 M4. Asks once per filtered diff for the lines a
	/// placement needs; a diff whose sides are GitHub's own needs none. A
	/// refusal is an answer: nothing can be placed off GitHub's diff.
	/// \brief [[BPR-1735]]. The accepted head and the merge base GitHub
	/// names for it; none when GitHub named no merge base, when nothing is
	/// inferred about its diff.
	auto github_application::implementation::whole_diff_commits() const -> std::optional<std::pair<core::git_commit_oid, core::git_commit_oid>>
	{
		if (WholeDiffPlacementDisabled || !WindowRead || !WindowRead->accepted_snapshot()) {
			return std::nullopt;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		if (snapshot.MergeBaseAvailability != application::github_merge_base_availability::available || !snapshot.MergeBaseOid) {
			return std::nullopt;
		}
		const auto head = core::parse_git_commit_oid(snapshot.PullRequest.HeadOid.value());
		const auto base = core::parse_git_commit_oid(snapshot.MergeBaseOid->value());
		if (!head || !base) {
			return std::nullopt;
		}
		return std::pair{ *head, *base };
	}

	/// \brief [[BPR-1735]]. The facts the core places a comment on GitHub's
	/// own diff of the file at `aHeadPath` from, its lines once read.
	auto github_application::implementation::whole_diff_source(const std::string& aHeadPath) const -> std::optional<core::whole_diff_comment_source>
	{
		const auto commits = whole_diff_commits();
		if (!commits) {
			return std::nullopt;
		}
		auto       source = core::whole_diff_comment_source{ .Head = commits->first, .MergeBase = commits->second, .BaseToHead = std::nullopt };
		const auto found  = GitHubDiffLines.find(aHeadPath);
		if (found != GitHubDiffLines.end() && found->second.Head == commits->first && found->second.MergeBase == commits->second && found->second.Answer
			&& found->second.Answer->Status == application::github_diff_lines_status::resolved) {
			source.BaseToHead = found->second.Answer->BaseToHead;
		}
		return source;
	}

	/// \brief [[BPR-1735]]. GitHub's own diff of the file shown, once read:
	/// none while it is being read, an empty answer when nothing is known
	/// (no merge base named, a filtered diff, or the read failed). The
	/// fixture courses that write line comments choose lines by it.
	auto github_application::implementation::shown_github_diff_lines() -> std::optional<std::optional<core::line_correspondence>>
	{
		if (!whole_diff_commits() || !SelectedDiffPublication || SelectedDiffPublication->Key.Filter) {
			return std::optional<core::line_correspondence>{ };
		}
		const auto bytes = SelectedDiffPublication->Request.File.CurrentPath.generic_u8string();
		const auto path  = std::string{ bytes.begin(), bytes.end() };
		request_github_diff_lines({ path });
		const auto found = GitHubDiffLines.find(path);
		if (found != GitHubDiffLines.end() && !found->second.Answer) {
			return std::nullopt;
		}
		auto       known  = std::optional<core::line_correspondence>{ };
		const auto source = whole_diff_source(path);
		if (source) {
			known = source->BaseToHead;
		}
		return known;
	}

	/// \brief [[BPR-1735]]. Asks once per file, head and merge base for
	/// GitHub's own diff of each path. A refusal is an answer: nothing is
	/// inferred for that file. A cancelled read is asked again later.
	void github_application::implementation::request_github_diff_lines(const std::vector<std::string>& aHeadPaths)
	{
		const auto commits = whole_diff_commits();
		if (Closing || EpochHost == nullptr || !LastAcceptedHandoff || !commits) {
			return;
		}
		// The accepted comparison names the pull request's diff; no file
		// need be shown for it to be read.
		const auto authority = workbench_authority(*LastAcceptedHandoff);
		const auto context   = EpochHost->context(authority);
		if (!context) {
			return;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		for (const auto& path : aHeadPaths) {
			if (const auto found = GitHubDiffLines.find(path); found != GitHubDiffLines.end() && found->second.Head == commits->first && found->second.MergeBase == commits->second) {
				continue;
			}
			const auto whole    = std::ranges::find(snapshot.Files, path, &application::github_file_record::Path);
			const auto basePath = whole != snapshot.Files.end() && whole->PreviousPath ? *whole->PreviousPath : path;
			auto       request  = application::github_diff_lines_request{
				.WorkbenchAuthority = authority,
				.Head               = commits->first,
				.MergeBase          = commits->second,
				.Key                = core::file_comparison_key{ .Authority   = context->Authority,
																 .Repository  = context->Authority.Repository,
																 .PullRequest = context->Authority.PullRequest,
																 .LeftAnchor  = context->Comparison.Left.Identity,
																 .RightAnchor = context->Comparison.Right.Identity,
																 .File        = core::file_identity_of(path),
																 .Options     = context->OptionsDigest,
																 .Algorithm   = context->Algorithm },
				.HeadPath           = path,
				.BasePath           = basePath,
			};
			GitHubDiffLines.insert_or_assign(path, github_diff_lines_entry{ .Head = commits->first, .MergeBase = commits->second, .Answer = std::nullopt });
			if (!EpochHost->resolve_github_diff_lines(request, [this, path](const application::github_diff_lines_observation& aObservation) {
					const auto found = GitHubDiffLines.find(path);
					if (Closing || found == GitHubDiffLines.end() || found->second.Head != aObservation.Request.Head || found->second.MergeBase != aObservation.Request.MergeBase) {
						return;
					}
					if (aObservation.Status == application::github_diff_lines_status::cancelled || aObservation.Status == application::github_diff_lines_status::stale) {
						GitHubDiffLines.erase(found);
						return;
					}
					found->second.Answer = aObservation;
					present_filtered_comment();
					present_review_comment_holds();
				})) {
				GitHubDiffLines.insert_or_assign(path, github_diff_lines_entry{ .Head = commits->first, .MergeBase = commits->second, .Answer = application::github_diff_lines_observation{ .Request = request, .Status = application::github_diff_lines_status::unauthorized, .BaseToHead = std::nullopt } });
			}
		}
	}

	/// \brief [[BPR-1735]]. The selected line of the Whole PR diff placed by
	/// the core, said only when it is not the line itself.
	auto github_application::implementation::whole_diff_presentation() -> std::optional<qt::filtered_comment_presentation>
	{
		if (!SelectedDiffPublication || SelectedDiffPublication->Key.Filter || !SelectedSemanticSelection
			|| SelectedSemanticSelection->PreferredLine.Comparison != core::comparison_digest(SelectedDiffPublication->Key)) {
			return std::nullopt;
		}
		const auto& line  = SelectedSemanticSelection->PreferredLine;
		const auto  bytes = SelectedDiffPublication->Request.File.CurrentPath.generic_u8string();
		const auto  path  = std::string{ bytes.begin(), bytes.end() };
		request_github_diff_lines({ path });
		// A base line is GitHub's only when its document is the merge base's.
		if (line.Side == core::diff_side::left && !(ReviewSourceMaterial && ReviewSourceMaterial->BaseNumberedAsGitHub)) {
			return std::nullopt;
		}
		const auto source = whole_diff_source(path);
		const auto start  = ReviewDraft && ReviewDraft->Anchor == line ? ReviewDraft->StartLine : std::nullopt;
		const auto target = source ? core::place_whole_diff_comment(*source, line.Side, line.Line, start) : std::nullopt;
		if (!target || target->Placement == core::filtered_comment_placement::head) {
			return std::nullopt;
		}
		// [[BPR-1736]]. Where GitHub accepted a comment outside the diff,
		// such a line is native and carries no mark; otherwise the notice
		// says what was learned.
		const auto learned = presented_off_hunk_fact(false);
		if (core::route_github_comment(core::place_on_github_diff(source->BaseToHead, line.Side, line.Line, start), learned) == core::github_comment_route::native_off_hunk) {
			return std::nullopt;
		}
		const auto hex = core::canonical_hex(target->Commit);
		return qt::filtered_comment_presentation{ .Selection = line, .Target = *target, .Commit = QString::fromUtf8(hex.view().substr(0U, 7U)), .Learned = learned };
	}

	/// \brief [[BPR-1735]]. Holds every comment pending in the review that
	/// the core places outside GitHub's diff: Finish your review marks it,
	/// and the review is not submitted while one is held.
	void github_application::implementation::present_review_comment_holds()
	{
		if (Closing || Window == nullptr) {
			return;
		}
		auto paths = std::vector<std::string>{ };
		for (const auto& comment : PendingReviewComments) {
			if (std::ranges::find(paths, comment.Path) == paths.end()) {
				paths.push_back(comment.Path);
			}
		}
		request_github_diff_lines(paths);
		auto held = std::map<core::review_draft_id, qt::workbench_window::unplaceable_review_comment>{ };
		auto ids  = std::vector<core::review_draft_id>{ };
		// [[BPR-1736]]. A review never probes: a line outside the diff rides
		// it only once GitHub accepted such a comment (decision D3).
		const auto learned = PendingReviewComments.empty() ? core::off_hunk_creation_capability::unknown : presented_off_hunk_fact(true);
		for (const auto& comment : PendingReviewComments) {
			const auto source = whole_diff_source(comment.Path);
			if (!source || !core::holds_review_comment(core::place_on_github_diff(source->BaseToHead, comment.Side, comment.Line, comment.StartLine), learned)) {
				continue;
			}
			const auto target = core::place_whole_diff_comment(*source, comment.Side, comment.Line, comment.StartLine);
			if (!target) {
				continue;
			}
			const auto hex = core::canonical_hex(target->Placement == core::filtered_comment_placement::file_comment ? target->Commit : source->Head);
			held.emplace(comment.Draft, qt::workbench_window::unplaceable_review_comment{ .Target = *target, .StartLine = comment.StartLine, .Commit = QString::fromUtf8(hex.view().substr(0U, 7U)) });
			ids.push_back(comment.Draft);
		}
		if (ReviewSubmission) {
			ReviewSubmission->hold_comments(std::move(ids));
		}
		Window->present_unplaceable_review_comments(std::move(held));
	}

	void github_application::implementation::request_filtered_comment_lines()
	{
		if (Closing || EpochHost == nullptr) {
			return;
		}
		const auto source  = filtered_comment_source();
		auto       request = filtered_comment_lines_request();
		if (!source || !request || !core::filtered_comment_needs_lines(*source) || FilteredCommentLinesAsked == request) {
			return;
		}
		FilteredCommentLinesAsked = request;
		FilteredCommentLines.reset();
		if (!EpochHost->resolve_filtered_comment_lines(*request, [this](const application::filtered_comment_lines_observation& aObservation) {
				if (Closing || FilteredCommentLinesAsked != std::optional{ aObservation.Request }) {
					return;
				}
				FilteredCommentLines = aObservation;
				present_filtered_comment();
			})) {
			FilteredCommentLines = application::filtered_comment_lines_observation{ .Request = *request, .Status = application::filtered_comment_lines_status::unauthorized, .Lines = { } };
		}
	}

	/// \brief [[BPR-1733]]. The file shown's path at its right commit.
	auto github_application::implementation::shown_right_path() const -> std::string
	{
		const auto& publication = *SelectedDiffPublication;
		const auto  bytes       = publication.RightPath.value_or(publication.LeftPath.value_or(publication.Request.File.CurrentPath)).generic_u8string();
		return std::string{ bytes.begin(), bytes.end() };
	}

	/// \brief [[BPR-1733]]. Every kept suggestion on an earlier head of the
	/// file shown that this window read that head's comparison of: on the
	/// same path, or the path the file had before the pull request moved it.
	auto github_application::implementation::moved_candidates() const -> std::vector<moved_candidate>
	{
		auto candidates = std::vector<moved_candidate>{ };
		if (!SelectedDiffPublication || SelectedDiffPublication->Key.Filter || !SharedDrafts || !WindowRead || !WindowRead->accepted_snapshot()) {
			return candidates;
		}
		const auto& publication = *SelectedDiffPublication;
		const auto  head        = core::parse_git_commit_oid(WindowRead->accepted_snapshot()->PullRequest.HeadOid.value());
		if (!head || publication.Request.Diff.Comparison.Right.Commit != *head) {
			return candidates;
		}
		const auto  digest   = core::comparison_digest(publication.Key);
		const auto  path     = shown_right_path();
		const auto& files    = WindowRead->accepted_snapshot()->Files;
		const auto  whole    = std::ranges::find(files, path, &application::github_file_record::Path);
		const auto  previous = whole != files.end() && whole->PreviousPath ? *whole->PreviousPath : path;
		// The drafts the store keeps on other heads, and any the shared
		// owner still holds on another comparison.
		auto kept = std::vector<core::revisioned_review_draft>{ };
		for (const auto& earlier : EarlierSuggestions) {
			kept.push_back(earlier.Draft);
		}
		for (auto& saved : SharedDrafts->recovery_snapshot()) {
			if (std::ranges::none_of(kept, [&saved](const core::revisioned_review_draft& aKept) { return aKept.Draft.Id == saved.Draft.Id; })) {
				kept.push_back(std::move(saved));
			}
		}
		for (auto& saved : kept) {
			const auto& draft = saved.Draft;
			if (draft.Anchor.Comparison == digest || !core::decide_suggestion_head_move(draft, std::nullopt).has_value()) {
				continue;
			}
			const auto earlier = std::ranges::find(ShownComparisons, draft.Anchor.Comparison, &shown_comparison::Digest);
			if (earlier == ShownComparisons.end() || earlier->Head == *head || (earlier->Path != path && earlier->Path != previous)) {
				continue;
			}
			candidates.push_back(moved_candidate{
				.Saved   = std::move(saved),
				.Request = application::head_move_lines_request{
					.WorkbenchAuthority = publication.Request.WorkbenchAuthority,
					.Head               = *head,
					.EarlierHead        = earlier->Head,
					.Key                = publication.Key,
					.EarlierPath        = earlier->Path,
					.HeadPath           = path,
				},
			});
		}
		return candidates;
	}

	/// \brief [[BPR-1733]]. Reads the kept suggestions the private store
	/// holds on other heads, then asks for their lines (`ask_head_move_lines`).
	void github_application::implementation::observe_head_moves()
	{
		if (Closing) {
			return;
		}
		// The store's suggestions on other heads are read again each time,
		// so one carried or deleted elsewhere is not offered.
		if (PrivateReview && PrivateDraftsReady) {
			PrivateReview->earlier_suggestions([this, owner = SharedDrafts](std::vector<private_review_workflow::earlier_suggestion> aEarlier) {
				if (Closing || owner != SharedDrafts) {
					return;
				}
				EarlierSuggestions = std::move(aEarlier);
				ask_head_move_lines();
			});
		}
		ask_head_move_lines();
	}

	/// \brief [[BPR-1733]]. Remembers the head and path the comparison
	/// shown was read at, asks once for each candidate's lines, and
	/// presents what each offers.
	void github_application::implementation::ask_head_move_lines()
	{
		if (Closing) {
			return;
		}
		if (SelectedDiffPublication && !SelectedDiffPublication->Key.Filter) {
			const auto digest = core::comparison_digest(SelectedDiffPublication->Key);
			std::erase_if(ShownComparisons, [&digest](const shown_comparison& aShown) { return aShown.Digest == digest; });
			ShownComparisons.push_back(shown_comparison{ .Digest = digest, .Head = SelectedDiffPublication->Request.Diff.Comparison.Right.Commit, .Path = shown_right_path() });
			if (ShownComparisons.size() > maximum_shown_comparisons) {
				ShownComparisons.erase(ShownComparisons.begin());
			}
		}
		for (const auto& candidate : moved_candidates()) {
			if (EpochHost == nullptr || std::ranges::find(HeadMoveLinesAsked, candidate.Request) != HeadMoveLinesAsked.end()) {
				continue;
			}
			if (HeadMoveLinesAsked.size() >= maximum_head_move_answers) {
				HeadMoveLinesAsked.clear();
				HeadMoveLines.clear();
			}
			HeadMoveLinesAsked.push_back(candidate.Request);
			const auto asked = EpochHost->resolve_head_move_lines(candidate.Request, [this](const application::head_move_lines_observation& aObservation) {
				if (Closing || std::ranges::find(HeadMoveLinesAsked, aObservation.Request) == HeadMoveLinesAsked.end()) {
					return;
				}
				HeadMoveLines.push_back(aObservation);
				present_moved_suggestions();
			});
			// A refusal is an answer: nothing is carried, and the change can be copied.
			if (!asked) {
				HeadMoveLines.push_back(application::head_move_lines_observation{ .Request = candidate.Request, .Status = application::head_move_lines_status::unauthorized, .Lines = std::nullopt });
			}
		}
		present_moved_suggestions();
	}

	/// \brief [[BPR-1733]]. Tells the window what each kept suggestion from
	/// an earlier head of the file shown offers here, as the core decides it
	/// from the host's answer; one still being read waits for it.
	void github_application::implementation::present_moved_suggestions()
	{
		if (Closing || Window == nullptr) {
			return;
		}
		auto moved = std::vector<qt::moved_suggestion_card>{ };
		for (const auto& candidate : moved_candidates()) {
			const auto answer = std::ranges::find(HeadMoveLines, candidate.Request, &application::head_move_lines_observation::Request);
			if (answer == HeadMoveLines.end()) {
				continue;
			}
			const auto lines    = answer->Status == application::head_move_lines_status::resolved ? answer->Lines : std::nullopt;
			const auto decision = core::decide_suggestion_head_move(candidate.Saved.Draft, lines);
			if (!decision) {
				continue;
			}
			const auto hex = core::canonical_hex(candidate.Request.EarlierHead);
			moved.push_back(qt::moved_suggestion_card{
				.Draft       = candidate.Saved.Draft,
				.Revision    = candidate.Saved.Revision,
				.Comparison  = core::comparison_digest(candidate.Request.Key),
				.EarlierHead = QString::fromStdString(std::string{ hex.view().substr(0U, 7U) }),
				.Move        = *decision,
			});
		}
		Window->present_moved_suggestions(std::move(moved));
	}

	/// \brief [[BPR-1733]]. The draft a carried suggestion came from goes,
	/// at the revision its note showed, once the carried one is saved.
	/// Nothing is published.
	void github_application::implementation::discard_carried_suggestion(const core::review_draft_id aDraft, const std::uint64_t aRevision)
	{
		if (Closing || !SharedDrafts) {
			return;
		}
		// Kept by the store on another head: deleted there.
		if (PrivateReview && std::ranges::any_of(EarlierSuggestions, [aDraft](const private_review_workflow::earlier_suggestion& aEarlier) { return aEarlier.Draft.Draft.Id == aDraft; })) {
			PrivateReview->discard_earlier_suggestion(aDraft, aRevision, [this, owner = SharedDrafts](bool) {
				if (!Closing && owner == SharedDrafts) {
					observe_head_moves();
				}
			});
			return;
		}
		const auto read = SharedDrafts->read(DraftConsumer, aDraft);
		if (!read.Accepted || read.Accepted->Revision != aRevision || !core::decide_suggestion_head_move(read.Accepted->Draft, std::nullopt).has_value()) {
			return;
		}
		auto command             = core::revisioned_review_draft_command{ };
		command.Command          = core::review_draft_command{ .Id = aDraft, .Kind = core::review_draft_command_kind::discard };
		command.ExpectedRevision = aRevision;
		command.Scope            = read.Accepted->Scope;
		change_review_drafts({ std::move(command) }, [](bool) { });
	}

	/// \brief RFC-BPR-0089 M4 (owner decision Q1). Tells the window where a
	/// comment on the selected line of the filtered diff on screen goes, so
	/// the editor says so before Publish.
	void github_application::implementation::present_filtered_comment()
	{
		if (Closing || Window == nullptr) {
			return;
		}
		if (!selected_diff_filtered()) {
			Window->present_filtered_comment(false, std::nullopt);
			// [[BPR-1735]]. The Whole PR diff's own placement.
			Window->present_whole_diff_comment(whole_diff_presentation());
			return;
		}
		Window->present_whole_diff_comment(std::nullopt);
		auto placement = std::optional<qt::filtered_comment_presentation>{ };
		if (SelectedSemanticSelection && SelectedSemanticSelection->PreferredLine.Comparison == core::comparison_digest(SelectedDiffPublication->Key)) {
			if (const auto source = filtered_comment_source()) {
				const auto& line   = SelectedSemanticSelection->PreferredLine;
				const auto  target = core::place_filtered_comment(*source, line.Side, line.Line, std::nullopt);
				// [[BPR-1706]]. A file comment's permalink names its own
				// side's commit, which on the left is not the diff's right.
				const auto hex = core::canonical_hex(target.Placement == core::filtered_comment_placement::file_comment ? target.Commit : source->Diff.Right);
				placement      = qt::filtered_comment_presentation{
					.Selection = line,
					.Target    = target,
					.Commit    = QString::fromUtf8(hex.view().substr(0U, 7U)),
				};
			}
		}
		Window->present_filtered_comment(true, std::move(placement));
	}

	/// \brief RFC-BPR-0089 M4. On a commit-filtered diff, each of its
	/// file's line threads is marked where the core places it on that diff,
	/// and drawn current there; every other line thread of the file stays in
	/// the conversation drawer, its anchor on no side of this diff.
	void github_application::implementation::place_filtered_threads(application::github_pull_request_projection& aProjection) const
	{
		if (!selected_diff_filtered() || !WindowRead || !WindowRead->accepted_snapshot()) {
			return;
		}
		const auto mergeBase = filtered_merge_base();
		if (!mergeBase) {
			return;
		}
		const auto& endpoints = *SelectedDiffPublication->Key.Filter;
		const auto& file      = SelectedDiffPublication->Request.File.File;
		auto        placed    = std::map<std::string, std::optional<std::uint32_t>>{ };
		for (const auto& thread : WindowRead->accepted_snapshot()->ReviewThreads) {
			if (thread.Subject != application::github_review_thread_subject::line || core::file_identity_of(thread.Path) != file
				|| thread.DiffSide == application::github_diff_side::uninitialized) {
				continue;
			}
			const auto root   = std::ranges::find_if(thread.Comments, [](const application::github_review_comment_record& aComment) { return !aComment.ReplyTo.has_value(); });
			const auto anchor = core::filtered_thread_anchor{
				.Side           = thread.DiffSide == application::github_diff_side::left ? core::diff_side::left : core::diff_side::right,
				.Line           = thread.Line,
				.OriginalLine   = thread.OriginalLine,
				.Commit         = root != thread.Comments.end() ? core::parse_git_commit_oid(root->Observation.Commit) : std::nullopt,
				.OriginalCommit = root != thread.Comments.end() ? core::parse_git_commit_oid(root->Observation.OriginalCommit) : std::nullopt,
			};
			const auto line = core::filtered_thread_line(anchor, endpoints, *mergeBase);
			placed.emplace("review-thread:" + std::string{ thread.Thread.value() }, line);
			for (const auto& comment : thread.Comments) {
				placed.emplace("review-comment:" + std::string{ comment.Comment.value() }, line);
			}
		}
		for (auto& node : aProjection.Conversations) {
			const auto found = placed.find(node.Id);
			if (found == placed.end()) {
				continue;
			}
			if (!found->second) {
				node.AnchorAvailability = application::github_conversation_anchor_availability::selected_comparison_mismatch;
				continue;
			}
			const auto line = *found->second + 1U;
			if (node.Line != std::optional{ line }) {
				node.StartLine.reset();
			}
			node.Line               = line;
			node.AnchorAvailability = application::github_conversation_anchor_availability::current;
			node.Outdated           = false;
		}
	}

	/// \brief RFC-BPR-0089 M2. "Hide files these commits changed".
	void github_application::implementation::apply_file_filter_gesture(const std::vector<std::string>& aCommits)
	{
		if (Closing) {
			return;
		}
		static_cast<void>(CommitFilter.hide_files_changed_by(aCommits));
		refresh_commit_filter();
	}

	/// \brief RFC-BPR-0089 M2. A generated-file gesture: the core decides
	/// the file's next mark, the ledger keeps it, and the review follows
	/// once it is durable. A refused write changes nothing.
	void github_application::implementation::apply_generated_gesture(const application::generated_file_gesture& aGesture)
	{
		if (Closing || ReviewLedgerHost == nullptr) {
			return;
		}
		const auto mark = CommitFilter.generated_mark_after(aGesture);
		if (!mark) {
			return;
		}
		ReviewLedgerHost->persist_generated_mark(*mark, ledger_answer([this](const std::expected<void, application::ledger_store_failure>& aStored) {
			if (!aStored || Closing) {
				return;
			}
			adopt_generated_marks();
			refresh_commit_filter();
		}));
	}

	/// \brief RFC-BPR-0089 M2. Adopts the generated marks the ledger last
	/// loaded or committed for this review.
	void github_application::implementation::adopt_generated_marks()
	{
		if (ReviewLedgerHost == nullptr) {
			return;
		}
		if (auto observed = ReviewLedgerHost->private_observation(); observed.has_value()) {
			static_cast<void>(CommitFilter.adopt_generated_marks(std::move(observed->GeneratedMarks)));
		}
	}

	/// \brief The Commits tab showed: its file counts need the trees.
	void github_application::implementation::want_commit_filter_trees()
	{
		if (Closing || CommitFilterWanted) {
			return;
		}
		CommitFilterWanted = true;
		refresh_commit_filter();
	}
}
