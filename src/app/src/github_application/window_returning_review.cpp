/// \file
/// \brief RFC-BPR-0116 ([[BPR-2073]], [[BPR-2074]]). A returning pull request's
/// two questions in the window: what each offers now, the material each
/// carries up front, and the fresh, tool-free launch each answer runs on.
/// Reads only the review's own objects; it launches nothing until a press.

#include "bpr/application/review_brief.hpp"
#include "github_application/window_composition.hpp"

#include "bpr/application/diff_line_matcher.hpp"
#include "bpr/core/agent_report.hpp"
#include "bpr/core/diff_compute.hpp"

#include <algorithm>

#include <QScopeGuard>
#include <QTimer>
#include <memory>
#include <set>

#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/application/writing_hooks.hpp"
#endif

namespace bpr::app {
#if defined(BPR_AGENT_INTEGRATIONS)
	namespace {
		[[nodiscard]] auto hex(const core::git_commit_oid& aOid) -> std::string { return std::string{ core::canonical_hex(aOid).view() }; }
	}

	/// What the returning questions offer for the pull request as accepted
	/// now: the Pull Request menu and the Agent tab are told, and the threads
	/// Was this addressed? is offered on. Reads nothing of the earlier
	/// revision ([[BPR-2251]]) and launches nothing.
	void github_application::implementation::refresh_returning_review()
	{
		if (Closing || Window == nullptr || EpochHost == nullptr) {
			return;
		}
		const auto& snapshot = WindowRead->accepted_snapshot();
		const auto& state    = EpochHost->state();
		if (!snapshot || !state.AcceptedRevision) {
			ReturningReview = { };
			Window->present_latest_upload({ });
			Window->set_addressed_threads({ });
			return;
		}
		const auto  head    = state.AcceptedRevision->Head;
		const auto  headHex = hex(head);
		const auto& viewer  = snapshot->Viewer.Login;
		auto        facts   = core::latest_upload_facts{ .CurrentHead = headHex };
		facts.Authored      = !viewer.empty() && snapshot->PullRequest.Author && snapshot->PullRequest.Author->Login == viewer;
		for (const auto& review : snapshot->Reviews) {
			if (!viewer.empty() && review.Author && review.Author->Login == viewer && review.State != application::github_review_state::pending && review.Commit) {
				facts.ReviewedHead = std::string{ review.Commit->value() };
			}
		}
	#if defined(BPR_BUILD_TESTING)
		if (!ReturningReviewHeadForTesting.empty()) {
			facts.ReviewedHead = ReturningReviewHeadForTesting;
		}
	#endif
		facts.SeenHead = Window->open_pull_request_seen_head();
		for (const auto& commit : snapshot->Commits) {
			facts.Commits.emplace_back(commit.Oid.value());
		}
		const auto earlier = core::returning_point_of(facts);
		const auto key     = earlier && earlier->Head != headHex ? earlier->Head + ".." + headHex : std::string{ };
		if (key != ReturningReview.Key) {
			// [[BPR-2251]]. Nothing of the past is read here: a press reads it.
			ReturningReview.Key         = key;
			ReturningReview.Earlier     = earlier;
			ReturningReview.CurrentHead = headHex;
			ReturningReview.Material.reset();
			ReturningReview.Reading     = false;
			ReturningReview.LineRetries = 0U;
		}
		ReturningReview.Offer = core::decide_latest_upload(facts);
		Window->present_latest_upload(ReturningReview.Offer);

		// Was this addressed?: RFC-BPR-0128 widened it to any review thread on
		// a file, whoever started it, once the head is past the commit the
		// thread started on (Q6: its baseline, not its last comment).
		auto offered = std::set<std::string>{ };
		auto threads = std::map<std::string, returning_review_state::thread_offer>{ };
		for (const auto& thread : snapshot->ReviewThreads) {
			const application::github_review_comment_record* opening = nullptr;
			for (const auto& comment : thread.Comments) {
				if (!comment.Deleted) {
					opening = &comment;
					break;
				}
			}
			// [[BPR-2184]] (review N4): the baseline is core's decision, the
			// same one the Ask menus' facts make.
			auto said = std::vector<core::thread_baseline_comment>{ };
			for (const auto& comment : thread.Comments) {
				said.push_back({ .Deleted = comment.Deleted, .OriginalCommit = comment.Observation.OriginalCommit, .Commit = comment.Observation.Commit });
			}
			const auto commit = core::thread_baseline(said);
			const auto asked  = core::proposed_fix_facts{ .ReviewerCommented = opening != nullptr, .LastCommentCommit = commit, .CurrentHead = headHex };
			if (!core::proposed_fix_offered(asked) || thread.Path.empty()) {
				continue;
			}
			const auto identity = thread_identity(thread);
			offered.insert(identity);
			if (const auto kept = ReturningReview.Threads.find(identity); kept != ReturningReview.Threads.end() && kept->second.Commit == commit) {
				threads.insert(*kept);
				continue;
			}
			const auto last  = opening->OriginalLine ? opening->OriginalLine : thread.OriginalLine;
			const auto first = opening->Observation.OriginalStartLine ? opening->Observation.OriginalStartLine : last;
			auto       offer = returning_review_state::thread_offer{ .Commit = commit, .Path = thread.Path };
			if (last) {
				offer.ThenFirst = *first;
				offer.ThenLast  = *last;
			} else {
				offer.ThenSettled = true;
			}
			if (thread.Line && !thread.Outdated) {
				offer.NowLast  = *thread.Line;
				offer.NowFirst = thread.Comments.empty() || !thread.Comments.front().Observation.StartLine ? *thread.Line : *thread.Comments.front().Observation.StartLine;
			} else if (last) {
				// GitHub no longer places an outdated thread at the head (its
				// lines changed, as a fix changes them): the same line numbers
				// at the head, with their context, stand for its place.
				offer.NowFirst = *first;
				offer.NowLast  = *last;
			} else {
				offer.NowSettled = true;
			}
			threads[identity] = std::move(offer);
		}
		ReturningReview.Threads = std::move(threads);
		// Each thread's lines then and now, asked once the map holds it; one
		// not admitted yet is asked again at the next refresh.
		for (auto& [identity, offer] : ReturningReview.Threads) {
			if (!offer.ThenSettled && !offer.ThenAsking) {
				read_addressed_lines(identity, true);
			}
			if (!offer.NowSettled && !offer.NowAsking) {
				read_addressed_lines(identity, false);
			}
		}
		Window->set_addressed_threads(std::move(offered));
	}

	/// [[BPR-2247]], [[BPR-2251]]. Reads the change since the review as core
	/// plans it, at the press: the earlier revision to the head, or, with
	/// merges of the base among the commits since, each real commit's own
	/// change, so none of the base's changes ride in it. The host fetches what
	/// the review's objects lack, off the actor the review's own reads use;
	/// the question is asked once the change is read, or refused saying why.
	void github_application::implementation::read_returning_material()
	{
		const auto  key      = ReturningReview.Key;
		const auto& snapshot = WindowRead->accepted_snapshot();
		const auto& state    = EpochHost->state();
		const auto  earlier  = ReturningReview.Earlier ? core::parse_git_commit_oid(ReturningReview.Earlier->Head) : std::nullopt;
		if (!snapshot || !state.AcceptedRevision || !LastAcceptedHandoff) {
			refuse_what_changed("the pull request has not been read yet.");
			return;
		}
		if (!earlier) {
			refuse_what_changed("the commit you reviewed is not a commit ID.");
			return;
		}
		const auto head    = state.AcceptedRevision->Head;
		auto       commits = std::vector<core::listed_commit>{ };
		for (const auto& commit : snapshot->Commits) {
			auto listed = core::listed_commit{ .Oid = std::string{ commit.Oid.value() }, .Headline = commit.Headline };
			if (commit.Parents) {
				for (const auto& parent : *commit.Parents) {
					listed.Parents.emplace_back(parent.value());
				}
			}
			commits.push_back(std::move(listed));
		}
		const auto plan    = core::plan_returning_change(ReturningReview.Earlier->Head, hex(head), commits, snapshot->PullRequest.BaseRef);
		auto       request = application::returning_change_request{ .WorkbenchAuthority = workbench_authority(*LastAcceptedHandoff), .Head = head, .Depth = plan.Depth, .Outside = earlier };
		auto       heads   = std::vector<std::pair<std::string, std::string>>{ };
		for (const auto& step : plan.Steps) {
			const auto parent = core::parse_git_commit_oid(step.Parent);
			const auto commit = core::parse_git_commit_oid(step.Commit);
			if (!parent || !commit) {
				refuse_what_changed("a commit since your review is not a commit ID.");
				return;
			}
			request.Steps.push_back({ .Parent = *parent, .Commit = *commit });
			heads.emplace_back(step.Commit, step.Headline);
		}
		const auto keep = [this](core::returning_changes aChanges) {
			ReturningReview.Material = core::supplement_returning{
				.Operation    = core::returning_operation::latest_upload,
				.EarlierHead  = ReturningReview.Earlier->Head,
				.CurrentHead  = ReturningReview.CurrentHead,
				.Basis        = ReturningReview.Earlier->Basis,
				.Changes      = std::move(aChanges.Text),
				.Commits      = returning_commit_listing(),
				.FilesShown   = aChanges.FilesShown,
				.FilesOmitted = aChanges.FilesOmitted,
			};
		};
		if (request.Steps.empty()) {
			// Only merges of the base since the review: nothing of the pull
			// request's own changed; the commit listing says what merged.
			keep(core::returning_changes{ });
			send_what_changed();
			return;
		}
		if (!plan.Listed) {
			// A rewritten branch: the earlier revision is not one of the pull
			// request's commits, so only its files are compared.
			for (const auto& file : snapshot->Files) {
				request.Paths.push_back(file.Path);
				if (file.PreviousPath) {
					request.Paths.push_back(*file.PreviousPath);
				}
			}
		}
	#if defined(BPR_BUILD_TESTING)
		++ReturningReadsForTesting;
	#endif
		ReturningReview.Reading = true;
		const auto answered     = std::make_shared<bool>(false);
		const auto admitted     = EpochHost->resolve_returning_change(std::move(request), [this, key, answered, keep, byCommit = plan.ByCommit, heads](const application::returning_change_observation& aRead) {
			*answered = true;
			if (Closing || ReturningReview.Key != key) {
				return;
			}
			ReturningReview.Reading = false;
			// A release this pass did not send was refused.
			const auto settled = qScopeGuard([this] { settle_pending_release(); });
			using status       = application::returning_change_status;
			switch (aRead.Status) {
			case status::resolved:            break;
			case status::earlier_unavailable: refuse_what_changed("the code you reviewed (" + ReturningReview.Earlier->Head.substr(0U, 7U) + ") could not be fetched from GitHub."); return;
			case status::limit_exceeded:      refuse_what_changed("the change since your review is too large to read here."); return;
			case status::stale:               refuse_what_changed("the review changed while the change since it was read. Ask again."); return;
			case status::cancelled:           refuse_what_changed("reading the change since your review was cancelled. Ask again."); return;
			case status::failure:             refuse_what_changed("the change since your review could not be read."); return;
			}
			// The comparisons are bounded by the host's side and file bounds,
			// so they run here.
			auto steps = std::vector<core::returning_commit_files>{ };
			for (std::size_t index = 0U; index < aRead.Steps.size() && index < heads.size(); ++index) {
				auto& step = steps.emplace_back(core::returning_commit_files{ .Commit = heads[index].first, .Headline = heads[index].second });
				for (const auto& file : aRead.Steps[index].Files) {
					step.Files.push_back(compare_change_file(file));
				}
			}
			auto changes = byCommit ? core::bound_returning_commits(std::move(steps)) : core::bound_returning_changes(steps.empty() ? std::vector<core::returning_change_file>{ } : std::move(steps.front().Files));
			changes.FilesOmitted += aRead.FilesOmitted;
			keep(std::move(changes));
			send_what_changed();
		});
		if (!admitted) {
			ReturningReview.Reading = false;
			refuse_what_changed("the review is still loading. Ask again in a moment.");
			return;
		}
		// The turn is sent once the change is read.
		if (!*answered) {
			note_release_deferred();
		}
	}

	/// [[BPR-2251]]. The question, its change read.
	void github_application::implementation::send_what_changed()
	{
		if (Closing || Window == nullptr || Window->agent_tab() == nullptr) {
			return;
		}
		Window->open_agent_question();
		Window->agent_tab()->ask_returning(core::returning_operation::latest_upload);
	}

	/// [[BPR-2251]]. A press whose change could not be read, said on the run.
	void github_application::implementation::refuse_what_changed(const std::string& aWhy)
	{
		if (Window != nullptr) {
			Window->open_agent_question();
			Window->agent_panel()->refuse("What changed since my review: " + aWhy + " Nothing was sent.");
		}
	}

	/// [[BPR-2247]], [[BPR-2248]]. One changed file of a read change as the
	/// agent reads it: its diff, or why there is none.
	auto github_application::implementation::compare_change_file(const application::returning_change_file& aFile) -> core::returning_change_file
	{
		if (aFile.TooLarge) {
			return { .Path = aFile.Path, .Text = "File " + aFile.Path + " is too large to compare here\n" };
		}
		const auto asSpan = [](const std::optional<std::string>& aSide) -> std::optional<std::span<const std::uint8_t>> {
			return aSide ? std::optional{ std::span<const std::uint8_t>{ reinterpret_cast<const std::uint8_t*>(aSide->data()), aSide->size() } } : std::nullopt;
		};
		const auto options = core::diff_options{ };
		const auto left    = asSpan(aFile.Earlier);
		const auto right   = asSpan(aFile.Current);
		const auto mkey    = core::returning_comparison(aFile.Path, left, right, options);
		if (!mkey || !LineMatcher) {
			return { .Path = aFile.Path };
		}
		auto       matching = application::diff_line_matching{ *LineMatcher };
		const auto outcome  = core::compute_material_diff({ .Key = *mkey, .Options = options, .LeftBytes = left, .RightBytes = right, .Limits = core::diff_compute_limits{ } }, matching);
		return { .Path = aFile.Path, .Text = outcome.Result ? core::render_returning_change(aFile.Path, *outcome.Result) : "File " + aFile.Path + " changed and could not be compared\n" };
	}

	/// RFC-BPR-0128 ([[BPR-2167]]). The commits since the review as the
	/// answer lists them: each merge of the base branch one line, with the
	/// commits it brought where GitHub's compare said, and whether it changed
	/// this pull request's files where the commit filter has read its tree;
	/// the rest grouped by area.
	auto github_application::implementation::returning_commit_listing() -> std::string
	{
		const auto& snapshot = WindowRead != nullptr ? WindowRead->accepted_snapshot() : std::nullopt;
		if (!snapshot || !ReturningReview.Earlier) {
			return { };
		}
		const auto& commits = snapshot->Commits;
		const auto  from    = std::ranges::find_if(commits, [this](const application::github_commit_record& aCommit) { return aCommit.Oid.value() == ReturningReview.Earlier->Head; });
		auto        listed  = std::vector<core::listed_commit>{ };
		auto        brought = std::vector<std::pair<std::string, std::uint32_t>>{ };
		for (auto each = from == commits.end() ? commits.begin() : std::next(from); each != commits.end(); ++each) {
			if (const auto counted = ReturningReview.MergeCounts.find(std::string{ each->Oid.value() }); counted != ReturningReview.MergeCounts.end()) {
				brought.emplace_back(counted->first, static_cast<std::uint32_t>(std::min<std::uint64_t>(counted->second, UINT32_MAX)));
			}
			auto commit = core::listed_commit{ .Oid = std::string{ each->Oid.value() }, .Headline = each->Headline };
			if (each->Parents) {
				for (const auto& parent : *each->Parents) {
					commit.Parents.emplace_back(parent.value());
				}
			}
			if (const auto oid = core::parse_git_commit_oid(commit.Oid)) {
				commit.Paths = CommitFilter.touched_paths(*oid);
			}
			listed.push_back(std::move(commit));
		}
		if (listed.empty()) {
			return { };
		}
		auto files = std::vector<std::string>{ };
		for (const auto& file : snapshot->Files) {
			files.push_back(file.Path);
		}
		auto all = std::vector<std::string>{ };
		for (const auto& commit : commits) {
			all.emplace_back(commit.Oid.value());
		}
		// [[BPR-2184]] (review N3). The merges worth counting, decided in
		// core, asked once for this head when the listing is first made; the
		// listing is made again when the counts arrive.
		const auto head = std::string{ snapshot->PullRequest.HeadOid.value() };
		if (ReturningReview.MergeCountsHead != head) {
			ReturningReview.MergeCountsHead = head;
			ReturningReview.MergeCounts.clear();
			auto asked = std::vector<application::github_merge_to_count>{ };
			for (const auto& merge : core::merges_to_count(listed, all, snapshot->PullRequest.BaseRef)) {
				asked.push_back({ .Merge = merge.Merge, .First = merge.First, .Second = merge.Second });
			}
			if (!asked.empty() && WindowRead != nullptr) {
				WindowRead->count_merges(std::move(asked), [this, head](std::vector<std::pair<std::string, std::uint64_t>> aCounts) {
					if (Closing || ReturningReview.MergeCountsHead != head || aCounts.empty()) {
						return;
					}
					for (auto& [merge, count] : aCounts) {
						ReturningReview.MergeCounts[merge] = count;
					}
					if (ReturningReview.Material && ReturningReview.Material->Operation == core::returning_operation::latest_upload) {
						ReturningReview.Material->Commits = returning_commit_listing();
					}
				});
			}
		}
		auto text = core::render_commit_listing(core::fold_commit_listing(listed, all, snapshot->PullRequest.BaseRef, files, brought));
		if (text.size() > core::maximum_returning_change_bytes) {
			text.resize(core::maximum_returning_change_bytes);
			while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0U) == 0x80U) {
				text.pop_back();
			}
		}
		return text;
	}

	/// A thread's lines as they were when I last commented there (`aThen`),
	/// or as they are at the head, from the review's own objects.
	void github_application::implementation::read_addressed_lines(const std::string& aIdentity, const bool aThen)
	{
		auto&      offer  = ReturningReview.Threads.at(aIdentity);
		const auto commit = aThen ? core::parse_git_commit_oid(offer.Commit) : std::optional{ LastAcceptedHandoff ? LastAcceptedHandoff->Head : core::git_commit_oid{ } };
		if (!commit || !LastAcceptedHandoff || EpochHost == nullptr) {
			(aThen ? offer.ThenSettled : offer.NowSettled) = true;
			return;
		}
		auto request                                 = application::permalink_source_request{ .WorkbenchAuthority = workbench_authority(*LastAcceptedHandoff), .Head = LastAcceptedHandoff->Head, .Commit = *commit, .Path = offer.Path };
		(aThen ? offer.ThenAsking : offer.NowAsking) = true;
		const auto admitted                          = EpochHost->resolve_permalink_source(std::move(request), [this, aIdentity, aThen, commitHex = hex(*commit), wanted = offer.Commit](const application::permalink_source_observation& aRead) {
			if (Closing) {
				return;
			}
			const auto found = ReturningReview.Threads.find(aIdentity);
			if (found == ReturningReview.Threads.end() || found->second.Commit != wanted) {
				return;
			}
			auto& kept                                 = found->second;
			(aThen ? kept.ThenAsking : kept.NowAsking) = false;
			using status                               = application::permalink_source_status;
			if (aRead.Status == status::stale || aRead.Status == status::cancelled) {
				retry_addressed_lines();
				return;
			}
			(aThen ? kept.ThenSettled : kept.NowSettled) = true;
			if (aRead.Status == status::resolved && aRead.Bytes) {
				(aThen ? kept.Then : kept.Now) = core::returning_excerpt(commitHex, kept.Path, *aRead.Bytes, aThen ? kept.ThenFirst : kept.NowFirst, aThen ? kept.ThenLast : kept.NowLast);
			}
		});
		if (!admitted) {
			(aThen ? offer.ThenAsking : offer.NowAsking) = false;
			retry_addressed_lines();
		}
	}

	/// [[BPR-2251]]. A thread's lines not admitted yet (ingestion in flight)
	/// or answered stale are asked again shortly, a bounded number of times;
	/// past the bound they are asked at the next refresh.
	void github_application::implementation::retry_addressed_lines()
	{
		if (ReturningReview.LineRetryScheduled || ReturningReview.LineRetries >= 120U || Window == nullptr) {
			return;
		}
		++ReturningReview.LineRetries;
		ReturningReview.LineRetryScheduled = true;
		QTimer::singleShot(250, Window.get(), [this] {
			ReturningReview.LineRetryScheduled = false;
			if (!Closing) {
				refresh_returning_review();
			}
		});
	}

	/// What changed since my review, from the Pull Request menu, its Agent
	/// tab suggestion or a queued Ask: [[BPR-2251]], the change since the
	/// review is read now, once for these heads, and then asked.
	void github_application::implementation::ask_what_changed()
	{
		if (Closing || Window == nullptr || EpochHost == nullptr || ReturningReview.Offer.State != core::latest_upload_state::offered || !ReturningReview.Earlier) {
			return;
		}
		if (ReturningReview.Material) {
			send_what_changed();
			return;
		}
		if (ReturningReview.Reading) {
			// The press under way asks once it has read the change.
			note_release_deferred();
			return;
		}
		read_returning_material();
	}

	/// A thread's Was this addressed?: the conversation is attached, as Ask
	/// agent attaches it, and the question is asked. RFC-BPR-0128: on any
	/// review thread, its baseline the commit it started on.
	void github_application::implementation::ask_was_addressed(const std::string& aIdentity)
	{
		AddressedMany.clear();
		if (Closing || Window == nullptr || !ReturningReview.Threads.contains(aIdentity)) {
			return;
		}
		ask_agent_about_thread(aIdentity);
		if (!AskedConversation || AskedConversation->Identity != aIdentity) {
			return;
		}
		Window->agent_tab()->ask_returning(core::returning_operation::proposed_fix);
	}

	/// RFC-BPR-0128 as amended (owner, look ask-v005, 2026-11-14: "Why would
	/// we do it this one instead of having one large prompt with all 6
	/// threads? We could still have it build structured output if we want to
	/// display each thread result separately."). One ask carries every thread
	/// the press reads; its answer lists each thread's claim, which reaches
	/// that thread's chip.
	void github_application::implementation::ask_were_addressed(const std::vector<std::string>& aIdentities)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		// [[BPR-2184]] (review B1). The row named exactly the threads a press
		// sends, capped in core; they are sent as named, or not at all when
		// the window no longer holds one of them, so the count a person saw
		// is the count asked about.
		auto threads = std::vector<std::string>{ };
		for (const auto& identity : aIdentities) {
			if (!ReturningReview.Threads.contains(identity) || accepted_thread(identity) == nullptr) {
				Window->open_agent_question();
				Window->agent_panel()->refuse("Was this addressed?: a thread it named has changed since the menu opened. Open the menu again. Nothing was sent.");
				return;
			}
			if (std::ranges::find(threads, identity) == threads.end()) {
				threads.push_back(identity);
			}
		}
		if (threads.size() > core::maximum_addressed_threads) {
			threads.resize(core::maximum_addressed_threads);
		}
		if (threads.size() == 1U) {
			ask_was_addressed(threads.front());
			return;
		}
		if (threads.empty()) {
			Window->open_agent_question();
			Window->agent_panel()->refuse("Was this addressed?: none of these threads is offered now. Nothing was sent.");
			return;
		}
		dismiss_asked_conversation();
		AddressedMany = std::move(threads);
		Window->open_agent_question();
		Window->agent_tab()->ask_returning(core::returning_operation::addressed_threads, AddressedMany.size());
	}

	auto github_application::implementation::compose_returning(application::agent_context_observation& aObservation, const core::agent_entry_point aEntryPoint) -> bool
	{
		switch (aEntryPoint) {
		case core::agent_entry_point::analyze_latest_upload: {
			if (ReturningReview.Offer.State != core::latest_upload_state::offered) {
				Window->agent_panel()->refuse("What changed since my review: there are no commits since your review. Nothing was sent.");
				return false;
			}
			if (!ReturningReview.Material) {
				Window->agent_panel()->refuse("What changed since my review: the change since your review was not read. Nothing was sent.");
				return false;
			}
			aObservation.Returning = ReturningReview.Material;
			break;
		}
		case core::agent_entry_point::evaluate_proposed_fix: {
			const auto& state = EpochHost->state();
			if (!AddressedMany.empty()) {
				const auto asked = std::exchange(AddressedMany, { });
				if (!state.AcceptedRevision) {
					Window->agent_panel()->refuse("Was this addressed?: the review is not open. Nothing was sent.");
					return false;
				}
				auto returning = core::supplement_returning{ .Operation = core::returning_operation::addressed_threads, .CurrentHead = hex(state.AcceptedRevision->Head) };
				for (const auto& identity : asked) {
					const auto  found  = ReturningReview.Threads.find(identity);
					const auto* thread = accepted_thread(identity);
					if (found == ReturningReview.Threads.end() || thread == nullptr) {
						continue;
					}
					const auto read         = application::read_conversation(*thread);
					auto       conversation = std::string{ };
					for (const auto& comment : read.Comments) {
						conversation += comment.Author + ": " + comment.Body + "\n";
					}
					if (conversation.size() > core::maximum_addressed_conversation_bytes) {
						conversation.resize(core::maximum_addressed_conversation_bytes);
						while (!conversation.empty() && (static_cast<unsigned char>(conversation.back()) & 0xC0U) == 0x80U) {
							conversation.pop_back();
						}
					}
					auto one = core::returning_thread{
						.Identity     = identity,
						.Path         = found->second.Path,
						.Line         = read.Line.value_or(found->second.NowFirst),
						.Baseline     = found->second.Commit,
						.Conversation = std::move(conversation),
						.Then         = found->second.Then,
						.Now          = found->second.Now,
					};
					returning.Threads.push_back(std::move(one));
				}
				// The earliest thread's start names the earlier side; each
				// thread carries its own.
				returning.EarlierHead = returning.Threads.empty() ? std::string{ } : returning.Threads.front().Baseline;
				if (!core::valid(returning)) {
					for (auto& one : returning.Threads) {
						one.Then.reset();
						one.Now.reset();
					}
				}
				if (!core::valid(returning)) {
					Window->agent_panel()->refuse("Was this addressed?: these threads can't be asked about together. Nothing was sent.");
					return false;
				}
				aObservation.Returning = std::move(returning);
				break;
			}
			const auto found = AskedConversation && !AskedConversation->Note && !AskedConversation->Draft ? ReturningReview.Threads.find(AskedConversation->Identity) : ReturningReview.Threads.end();
			if (found == ReturningReview.Threads.end() || !state.AcceptedRevision) {
				Window->agent_panel()->refuse("Was this addressed?: asked of a review thread on a file, once commits follow the commit it started on. Nothing was sent.");
				return false;
			}
			aObservation.Conversation = read_asked_conversation();
			if (!aObservation.Conversation) {
				dismiss_asked_conversation();
				Window->agent_panel()->refuse("the conversation asked about is no longer in this pull request");
				return false;
			}
			auto returning = core::supplement_returning{
				.Operation   = core::returning_operation::proposed_fix,
				.EarlierHead = found->second.Commit,
				.CurrentHead = hex(state.AcceptedRevision->Head),
				.Then        = found->second.Then,
			};
			returning.Now = found->second.Now;
			if (!core::valid(returning)) {
				returning.Then.reset();
				returning.Now.reset();
			}
			aObservation.Returning = std::move(returning);
			break;
		}
		case core::agent_entry_point::ask_about_selection:
		case core::agent_entry_point::analyze_current_file:
		case core::agent_entry_point::workshop_comment:
		case core::agent_entry_point::run_ai_review:
		case core::agent_entry_point::open_full_agent_session:
		case core::agent_entry_point::ask_about_conversation:  return true;
		}
		// A fresh first answer carries no earlier conversation (RFC-BPR-0101,
		// 2026-11-12): it is one call, and its context is the request.
		if (aObservation.Brief) {
			aObservation.Brief->Conversation.clear();
			aObservation.Brief->ConversationOmitted = 0U;
		}
		return true;
	}

	void github_application::implementation::launch_returning(application::agent_invocation_request& aInvocation, const core::agent_profile_id aProfile)
	{
		// RFC-BPR-0101 as amended 2026-11-12: the first answer is a fresh run,
		// one tool-free call where the agent has the mode (Claude), with its
		// own built-in prompt as the system prompt. No native session, no
		// scratch folder, no private run, nothing saved.
		aInvocation.Path = core::agent_run_path::assistance_first_answer;
		const auto hook  = aInvocation.Context.EntryPoint == core::agent_entry_point::analyze_latest_upload ? core::writing_hook::review_latest_upload
			: aInvocation.Context.EntryPoint == core::agent_entry_point::analyze_current_file               ? core::writing_hook::review_explain_file
			: aInvocation.Context.EntryPoint == core::agent_entry_point::workshop_comment                   ? core::writing_hook::review_suggest_change
																											: core::writing_hook::review_proposed_fix;
		auto       text  = std::string{ application::default_writing_hook(hook) };
		if (Profile && Profile->WorkbenchPreferences) {
			if (const auto& preferences = Profile->WorkbenchPreferences->accepted(); preferences) {
				text = std::string{ application::effective_writing_hook(preferences->WritingHooks, hook) };
			}
		}
		aInvocation.SystemInstructions = std::move(text);
		aInvocation.NativeAgentIdentity.clear();
		aInvocation.NativeScope.clear();
		aInvocation.TranscriptReplay = false;
		aInvocation.SelectedConversation.clear();
		aInvocation.ScratchFolder  = false;
		aInvocation.BuildsAndTests = false;
		auto protocol              = std::optional<core::agent_protocol>{ };
		if (AgentSetup) {
			const auto agents = AgentSetup->projection().Preferences.Agents;
			if (const auto* const entry = core::entry_of_profile(agents, aProfile); entry != nullptr) {
				protocol = entry->Protocol;
			}
		}
		if (aProfile == core::claude_stream_json_profile_id || protocol == core::agent_protocol::claude_stream_json) {
			aInvocation.WritingOnly = true;
			aInvocation.PrivateRun.reset();
			aInvocation.HistoryClone.reset();
			aInvocation.HistoryCloneManaged = false;
			aInvocation.FrozenFiles.clear();
		}
	}
#endif
}
