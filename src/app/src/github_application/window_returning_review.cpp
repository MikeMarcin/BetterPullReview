/// \file
/// \brief RFC-BPR-0116 ([[BPR-2073]], [[BPR-2074]]). A returning pull request's
/// two questions in the window: what each offers now, the material each
/// carries up front, and the fresh, tool-free launch each answer runs on.
/// Reads only the review's own objects; it launches nothing until a press.

#include "github_application/window_composition.hpp"

#include "bpr/application/diff_line_matcher.hpp"
#include "bpr/core/diff_compute.hpp"

#include <algorithm>

#include <QTimer>
#include <memory>
#include <set>

#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/application/writing_hooks.hpp"
#endif

namespace bpr::app {
#if defined(BPR_AGENT_INTEGRATIONS)
	namespace {
		/// A file larger than this on either side is left out of the change
		/// rather than compared on the window's lane.
		constexpr std::size_t maximum_returning_side_bytes = 256U * 1024U;

		[[nodiscard]] auto hex(const core::git_commit_oid& aOid) -> std::string { return std::string{ core::canonical_hex(aOid).view() }; }

		[[nodiscard]] auto short_hex(const std::string& aHex) -> QString { return QString::fromStdString(aHex.substr(0U, 7U)); }

		/// One file's two sides as they arrive.
		struct returning_read final
		{
			std::string                Path;
			std::optional<std::string> Earlier;
			std::optional<std::string> Current;
			bool                       TooLarge = false;
		};
		struct returning_gather final
		{
			std::string                 Key;
			std::vector<returning_read> Files;
			std::size_t                 Waiting = 0U;
			bool                        Failed  = false;
			std::size_t                 Omitted = 0U;
		};
	}

	/// What the returning questions offer for the pull request as accepted
	/// now: the Pull Request menu and the Agent tab are told, and the threads
	/// Was this addressed? is offered on. Starts reading the earlier
	/// revision's material once per pair of heads; launches nothing.
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
			ReturningReview.Key         = key;
			ReturningReview.Earlier     = earlier;
			ReturningReview.CurrentHead = headHex;
			ReturningReview.Material.reset();
			ReturningReview.Retries  = key.empty() ? 0U : ReturningReview.Retries;
			ReturningReview.Readable = false;
			ReturningReview.Reading  = false;
			if (!key.empty()) {
				read_returning_material(*earlier, head);
			}
		}
		facts.Material        = ReturningReview.Reading ? core::returning_material::reading
			: ReturningReview.Readable                  ? core::returning_material::readable
														: core::returning_material::unreadable;
		ReturningReview.Offer = core::decide_latest_upload(facts);
		Window->present_latest_upload(ReturningReview.Offer);

		// Was this addressed?: the threads I commented in, once commits
		// follow my last comment there.
		auto offered = std::set<std::string>{ };
		auto threads = std::map<std::string, returning_review_state::thread_offer>{ };
		for (const auto& thread : snapshot->ReviewThreads) {
			const application::github_review_comment_record* mine = nullptr;
			for (const auto& comment : thread.Comments) {
				if (!viewer.empty() && comment.Author && comment.Author->Login == viewer && !comment.Deleted) {
					mine = &comment;
				}
			}
			const auto commit = mine == nullptr             ? std::string{ }
				: !mine->Observation.OriginalCommit.empty() ? mine->Observation.OriginalCommit
															: mine->Observation.Commit;
			const auto asked  = core::proposed_fix_facts{ .ReviewerCommented = mine != nullptr, .LastCommentCommit = commit, .CurrentHead = headHex };
			if (!core::proposed_fix_offered(asked) || thread.Path.empty()) {
				continue;
			}
			const auto identity = thread_identity(thread);
			offered.insert(identity);
			if (const auto kept = ReturningReview.Threads.find(identity); kept != ReturningReview.Threads.end() && kept->second.Commit == commit) {
				threads.insert(*kept);
				continue;
			}
			const auto last  = mine->OriginalLine ? mine->OriginalLine : thread.OriginalLine;
			const auto first = mine->Observation.OriginalStartLine ? mine->Observation.OriginalStartLine : last;
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

	/// Reads the change between the earlier revision and the head from the
	/// review's own objects: which files differ, then each at both commits.
	/// A commit the objects do not hold makes the question unavailable.
	void github_application::implementation::read_returning_material(const core::returning_point& aEarlier, const core::git_commit_oid& aHead)
	{
		const auto  key      = ReturningReview.Key;
		const auto  earlier  = core::parse_git_commit_oid(aEarlier.Head);
		const auto& snapshot = WindowRead->accepted_snapshot();
		if (!earlier || !LastAcceptedHandoff || !snapshot) {
			finish_returning_material(key, std::nullopt);
			return;
		}
		ReturningReview.Reading = true;
		auto paths              = std::vector<std::filesystem::path>{ };
		for (const auto& file : snapshot->Files) {
			paths.emplace_back(file.Path);
			if (file.PreviousPath) {
				paths.emplace_back(*file.PreviousPath);
			}
		}
		const auto authority = workbench_authority(*LastAcceptedHandoff);
		auto       request   = application::changed_paths_request{ .WorkbenchAuthority = authority, .Left = *earlier, .Right = aHead, .Paths = std::move(paths) };
		const auto admitted  = EpochHost->resolve_changed_paths(std::move(request), [this, key, authority, earlierOid = *earlier, head = aHead](const application::changed_paths_observation& aChanged) {
			if (Closing || ReturningReview.Key != key) {
				return;
			}
			if (aChanged.Status == application::changed_paths_status::stale || aChanged.Status == application::changed_paths_status::cancelled) {
				// Asked again shortly, once the cache serves the head.
				retry_returning_material();
				return;
			}
			if (aChanged.Status != application::changed_paths_status::resolved) {
				finish_returning_material(key, std::nullopt);
				return;
			}
			auto gather = std::make_shared<returning_gather>();
			gather->Key = key;
			for (const auto& path : aChanged.Changed) {
				if (gather->Files.size() == core::maximum_returning_files) {
					++gather->Omitted;
					continue;
				}
				gather->Files.push_back({ .Path = path.generic_string() });
			}
			if (gather->Files.empty()) {
				finish_returning_material(key, core::returning_changes{ });
				return;
			}
			gather->Waiting = gather->Files.size() * 2U;
			const auto done = [this, gather] {
				if (gather->Waiting != 0U || Closing || ReturningReview.Key != gather->Key) {
					return;
				}
				if (gather->Failed) {
					finish_returning_material(gather->Key, std::nullopt);
					return;
				}
				// The comparisons are bounded by `maximum_returning_side_bytes`
				// a side and `maximum_returning_files`, so they run here.
				auto files = std::vector<core::returning_change_file>{ };
				for (const auto& read : gather->Files) {
					if (read.TooLarge) {
						files.push_back({ .Path = read.Path, .Text = "File " + read.Path + " is too large to compare here\n" });
						continue;
					}
					const auto as_span = [](const std::optional<std::string>& aSide) -> std::optional<std::span<const std::uint8_t>> {
						return aSide ? std::optional{ std::span<const std::uint8_t>{ reinterpret_cast<const std::uint8_t*>(aSide->data()), aSide->size() } } : std::nullopt;
					};
					const auto options = core::diff_options{ };
					const auto left    = as_span(read.Earlier);
					const auto right   = as_span(read.Current);
					const auto mkey    = core::returning_comparison(read.Path, left, right, options);
					if (!mkey || !LineMatcher) {
						continue;
					}
					auto       matching = application::diff_line_matching{ *LineMatcher };
					const auto outcome  = core::compute_material_diff({ .Key = *mkey, .Options = options, .LeftBytes = left, .RightBytes = right, .Limits = core::diff_compute_limits{ } }, matching);
					files.push_back({ .Path = read.Path, .Text = outcome.Result ? core::render_returning_change(read.Path, *outcome.Result) : "File " + read.Path + " changed and could not be compared\n" });
				}
				auto changes = core::bound_returning_changes(std::move(files));
				changes.FilesOmitted += gather->Omitted;
				finish_returning_material(gather->Key, std::move(changes));
			};
			for (auto index = std::size_t{ 0U }; index < gather->Files.size(); ++index) {
				for (const auto earlierSide : { true, false }) {
					auto       read         = application::permalink_source_request{ .WorkbenchAuthority = authority, .Head = head, .Commit = earlierSide ? earlierOid : head, .Path = gather->Files[index].Path };
					const auto admittedRead = EpochHost->resolve_permalink_source(std::move(read), [gather, index, earlierSide, done](const application::permalink_source_observation& aRead) {
						auto& file   = gather->Files[index];
						using status = application::permalink_source_status;
						if (aRead.Status == status::resolved) {
							(earlierSide ? file.Earlier : file.Current) = aRead.Bytes;
							if (aRead.Bytes && aRead.Bytes->size() > maximum_returning_side_bytes) {
								file.TooLarge = true;
							}
						} else if (aRead.Status == status::too_large) {
							file.TooLarge = true;
						} else {
							gather->Failed = true;
						}
						--gather->Waiting;
						done();
					});
					if (!admittedRead) {
						gather->Failed = true;
						--gather->Waiting;
					}
				}
			}
			done();
		});
		if (!admitted) {
			// Not admitted now (ingestion in flight); asked again shortly.
			retry_returning_material();
		}
	}

	/// The earlier material could not be asked for yet (the cache not yet
	/// serving the accepted head): asked again shortly, a bounded number of
	/// times, and reading meanwhile. Past the bound it is unavailable.
	void github_application::implementation::retry_returning_material()
	{
		ReturningReview.Key.clear();
		if (++ReturningReview.Retries > 120U) {
			ReturningReview.Reading = false;
			return;
		}
		ReturningReview.Reading = true;
		QTimer::singleShot(250, Window.get(), [this] {
			if (!Closing) {
				refresh_returning_review();
			}
		});
	}

	void github_application::implementation::finish_returning_material(const std::string& aKey, std::optional<core::returning_changes> aChanges)
	{
		if (Closing || ReturningReview.Key != aKey) {
			return;
		}
		ReturningReview.Reading  = false;
		ReturningReview.Readable = aChanges.has_value();
		if (aChanges && ReturningReview.Earlier) {
			ReturningReview.Material = core::supplement_returning{
				.Operation    = core::returning_operation::latest_upload,
				.EarlierHead  = ReturningReview.Earlier->Head,
				.CurrentHead  = ReturningReview.CurrentHead,
				.Basis        = ReturningReview.Earlier->Basis,
				.Changes      = std::move(aChanges->Text),
				.FilesShown   = aChanges->FilesShown,
				.FilesOmitted = aChanges->FilesOmitted,
			};
		}
		refresh_returning_review();
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
				return;
			}
			(aThen ? kept.ThenSettled : kept.NowSettled) = true;
			if (aRead.Status == status::resolved && aRead.Bytes) {
				(aThen ? kept.Then : kept.Now) = core::returning_excerpt(commitHex, kept.Path, *aRead.Bytes, aThen ? kept.ThenFirst : kept.NowFirst, aThen ? kept.ThenLast : kept.NowLast);
			}
		});
		if (!admitted) {
			(aThen ? offer.ThenAsking : offer.NowAsking) = false;
		}
	}

	/// The Pull Request menu's What changed since my review: the Agent tab
	/// asks it, as its suggestion would.
	void github_application::implementation::ask_what_changed()
	{
		if (Closing || Window == nullptr || ReturningReview.Offer.State != core::latest_upload_state::offered) {
			return;
		}
		Window->open_agent_question();
		Window->agent_tab()->ask_returning(core::returning_operation::latest_upload);
	}

	/// A thread's Was this addressed?: the conversation is attached, as Ask
	/// agent attaches it, and the question is asked.
	void github_application::implementation::ask_was_addressed(const std::string& aIdentity)
	{
		if (Closing || Window == nullptr || !ReturningReview.Threads.contains(aIdentity)) {
			return;
		}
		ask_agent_about_thread(aIdentity);
		if (!AskedConversation || AskedConversation->Identity != aIdentity) {
			return;
		}
		Window->agent_tab()->ask_returning(core::returning_operation::proposed_fix);
	}

	auto github_application::implementation::compose_returning(application::agent_context_observation& aObservation, const core::agent_entry_point aEntryPoint) -> bool
	{
		switch (aEntryPoint) {
		case core::agent_entry_point::analyze_latest_upload: {
			if (ReturningReview.Offer.State != core::latest_upload_state::offered || !ReturningReview.Material) {
				const auto earlier = ReturningReview.Offer.Earlier ? short_hex(ReturningReview.Offer.Earlier->Head) : QString{ };
				Window->agent_panel()->refuse(ReturningReview.Offer.State == core::latest_upload_state::unavailable
												  ? QStringLiteral("What changed since my review: the code at %1 can't be read here. Nothing was sent.").arg(earlier).toStdString()
												  : QStringLiteral("What changed since my review: there are no commits since your review. Nothing was sent.")
														.toStdString());
				return false;
			}
			aObservation.Returning = ReturningReview.Material;
			break;
		}
		case core::agent_entry_point::evaluate_proposed_fix: {
			const auto& state = EpochHost->state();
			const auto  found = AskedConversation && !AskedConversation->Note ? ReturningReview.Threads.find(AskedConversation->Identity) : ReturningReview.Threads.end();
			if (found == ReturningReview.Threads.end() || !state.AcceptedRevision) {
				Window->agent_panel()->refuse("Was this addressed?: asked of a thread you commented in, once commits follow your comment. Nothing was sent.");
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
			: aInvocation.Context.EntryPoint == core::agent_entry_point::analyze_current_file             ? core::writing_hook::review_explain_file
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
