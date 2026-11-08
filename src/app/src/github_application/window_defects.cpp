#include "github_application/window_composition.hpp"

#include "bpr/application/defect_correlation.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/qt/diff_workbench.hpp"

#include <algorithm>
#include <iterator>

namespace bpr::app {
	auto github_application::implementation::thread_identity(
		const application::github_review_thread_record& aThread) -> std::string
	{
		return "review-thread:" + std::string{ aThread.Thread.value() };
	}

	auto github_application::implementation::accepted_thread(const std::string_view aIdentity) const
		-> const application::github_review_thread_record*
	{
		if (Host == nullptr || !WindowRead->accepted_snapshot().has_value()) {
			return nullptr;
		}
		const auto& threads = WindowRead->accepted_snapshot()->ReviewThreads;
		// [[BPR-0972]]. The read-state cache names each thread once.
		if (const auto cached = cached_conversation(aIdentity)) {
			// [[BPR-1601]]. Singleton receipts follow file threads in this
			// cache; they are not indices into ReviewThreads.
			return *cached < threads.size() ? &threads[*cached] : nullptr;
		}
		const application::github_review_thread_record* result = nullptr;
		for (const auto& thread : threads) {
			const auto identity = thread_identity(thread);
			if (identity != aIdentity) {
				continue;
			}
			if (result != nullptr) {
				return nullptr;
			}
			result = &thread;
		}
		return result;
	}

	void github_application::implementation::present_defects()
	{
		if (Window == nullptr) {
			return;
		}
		auto histories = std::vector<core::defect_history>{ };
		histories.reserve(DurableDefects.size());
		for (const auto& defect : DurableDefects) {
			histories.push_back(defect.History);
		}
		auto threads = std::map<core::defect_id, std::string>{ };
		// Retain the identity join per accepted immutable snapshot. Ordinary
		// defect updates visit only the retained defect records.
		if (!DefectThreadNames && WindowRead->accepted_snapshot()) {
			DefectThreadNames.emplace();
			for (const auto& thread : WindowRead->accepted_snapshot()->ReviewThreads) {
				++ConversationThreadsVisited;
				const auto identity          = thread_identity(thread);
				const auto [entry, inserted] = DefectThreadNames->emplace(defect_identity(identity), identity);
				if (!inserted) {
					entry->second.clear();
				} // Ambiguous identities have no thread-specific controls.
			}
		}
		if (DefectThreadNames) {
			for (const auto& defect : DurableDefects) {
				const auto at = DefectThreadNames->find({ defect.Conversation.value(), defect.History.Defect.Id.value() });
				if (at != DefectThreadNames->end() && !at->second.empty()) {
					threads.emplace(defect.History.Defect.Id, at->second);
				}
			}
		}
		Window->present_defects(std::move(histories), std::move(threads));
	}

	/// \brief Persists a correlated defect; correlation does not wait for it.
	void github_application::implementation::persist_correlated_defect(
		const application::durable_defect_record& aDefect)
	{
#if defined(BPR_BUILD_TESTING)
		if (DefectCompositionProbe.has_value()
			&& DefectCompositionProbe->RefuseCorrelatedPersistence) {
			++DefectCompositionProbe->RefusedPersistenceAttempts;
			return;
		}
#endif
		if (ReviewLedgerHost != nullptr) {
			ReviewLedgerHost->persist(aDefect, { });
		}
	}

	/// \brief Persists one changed defect and presents it once durable.
	void github_application::implementation::persist_defect(application::durable_defect_record aDefect, std::function<void()> aAfter)
	{
		if (ReviewLedgerHost == nullptr) {
			return;
		}
		PendingDefectWrites.insert(aDefect.History.Defect.Id);
		ReviewLedgerHost->persist(aDefect, ledger_answer([this, aDefect, after = std::move(aAfter)](const std::expected<void, application::ledger_store_failure>& aStored) {
									  PendingDefectWrites.erase(aDefect.History.Defect.Id);
									  if (!aStored) {
										  return;
									  }
									  const auto found = std::ranges::find(DurableDefects, aDefect.History.Defect.Id, [](const application::durable_defect_record& aRecord) {
										  return aRecord.History.Defect.Id;
									  });
									  if (found == DurableDefects.end()) {
										  DurableDefects.push_back(aDefect);
									  } else {
										  *found = aDefect;
									  }
									  if (after) {
										  after();
									  }
								  }));
	}

	void github_application::implementation::correlate_defects(
		const std::span<const core::source_line_ref> aChangedAnchors)
	{
		if (ReviewLedgerHost == nullptr || Host == nullptr
			|| !WindowRead->accepted_snapshot().has_value()) {
			return;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		auto        records  = std::vector<core::defect_record>{ };
		auto        links    = std::vector<application::defect_thread_link>{ };
		auto        observations
			= std::vector<application::defect_thread_observation>{ };
		records.reserve(DurableDefects.size());
		links.reserve(DurableDefects.size());
		observations.reserve(DurableDefects.size());
		auto changedAnchors = std::vector<core::source_line_ref>{
			aChangedAnchors.begin(), aChangedAnchors.end()
		};
		for (auto& durable : DurableDefects) {
			const application::github_review_thread_record* found     = nullptr;
			bool                                            collision = false;
			for (const auto& thread : snapshot.ReviewThreads) {
				const auto identity = defect_identity(thread_identity(thread));
				if (identity.Conversation != durable.Conversation.value()
					|| identity.Defect != durable.History.Defect.Id.value()) {
					continue;
				}
				if (found != nullptr) {
					collision = true;
					break;
				}
				found = &thread;
			}
			if (collision) {
				records.push_back(durable.History.Defect);
				continue;
			}
			if (found == nullptr) {
				const auto history = core::lose_anchor(
					durable.History,
					core::defect_anchor_availability::thread_deleted);
				if (history != durable.History) {
					auto changed    = durable;
					changed.History = history;
					persist_correlated_defect(changed);
					durable = std::move(changed);
				}
				records.push_back(durable.History.Defect);
				continue;
			}
			const auto identity = thread_identity(*found);
			links.push_back(application::defect_thread_link{
				.Defect = durable.History.Defect.Id, .ThreadId = identity });
			const auto author        = snapshot.PullRequest.Author.has_value()
				? snapshot.PullRequest.Author->Login
				: std::string{ };
			const auto authorReplied = found->Comments.size() > 1U
				&& std::ranges::any_of(
										   std::next(found->Comments.begin()),
										   found->Comments.end(),
										   [&author](const application::github_review_comment_record& aComment) {
											   return !author.empty() && aComment.Author.has_value()
												   && aComment.Author->Login == author;
										   });
			observations.push_back(application::defect_thread_observation{
				.ThreadId           = identity,
				.Resolved           = found->Resolved,
				.AuthorRepliedSince = authorReplied,
			});
			const auto availability = defect_anchor_is_current(durable, *found)
				? core::defect_anchor_availability::available
				: core::defect_anchor_availability::outdated;
			if (found->Outdated) {
				changedAnchors.push_back(durable.History.Defect.Anchor);
			}
			if (availability != durable.History.Availability) {
				auto changed                 = durable;
				changed.History.Availability = availability;
				persist_correlated_defect(changed);
				durable = std::move(changed);
			}
			records.push_back(durable.History.Defect);
		}
		const auto correlated
			= application::correlate(records, links, observations, changedAnchors);
		for (const auto& record : correlated.Defects) {
			const auto found = std::ranges::find(
				DurableDefects,
				record.Id,
				[](const application::durable_defect_record& aDefect) {
					return aDefect.History.Defect.Id;
				});
			if (found == DurableDefects.end()
				|| found->History.Defect == record) {
				continue;
			}
			auto changed           = *found;
			changed.History.Defect = record;
			// [[BPR-1211]]. Kept and presented once it is durable.
			persist_defect(std::move(changed), [this] { present_defects(); });
		}
		present_defects();
	}

	/// \brief Adopts the ledger's private defects and correlates them.
	/// \details Reads what the ledger last loaded; `reload_private_records`
	/// brings it back here when the ledger has read the store.
	void github_application::implementation::load_defects()
	{
		DurableDefects.clear();
		if (ReviewLedgerHost == nullptr) {
			if (Window != nullptr) {
				Window->clear_defects();
			}
			return;
		}
		if (auto privateState = ReviewLedgerHost->private_observation();
			privateState.has_value()) {
			DurableDefects = std::move(privateState->Defects);
		}
		correlate_defects();
		present_defects();
	}

	/// \brief [[BPR-1211]]. The review ledger's store answered its opening.
	void github_application::implementation::review_ledger_opened(const bool aAvailable)
	{
		{
			const auto lock       = std::scoped_lock{ ReviewLedgerStateMutex };
			ReviewLedgerAvailable = aAvailable;
		}
		if (Window != nullptr) {
			Window->clear_review_ledger(!aAvailable);
		}
		if (!aAvailable) {
			publish_overview(false);
			return;
		}
		// [[BPR-1898]]. File progress need not queue behind conversation
		// receipts or private records. Every operation stays on its store lane.
		publish_overview();
		publish_read_state();
		reload_private_records(true);
	}

	/// \brief Reads the private records again, off the decision lane.
	///
	/// \details `aCorrelate` correlates the defects with the accepted
	/// threads, as opening a review does; a sibling's change is adopted as
	/// it was accepted, without correlating or persisting anything.
	void github_application::implementation::reload_private_records(const bool aCorrelate)
	{
		if (ReviewLedgerHost == nullptr) {
			return;
		}
		ReviewLedgerHost->load_private(ledger_answer([this, aCorrelate](const std::expected<void, application::ledger_store_failure>& aLoaded) {
			PrivateRecordsFailed = !aLoaded.has_value();
			if (!aLoaded) {
				publish_overview(false);
				return;
			}
			if (aCorrelate) {
				load_defects();
			} else if (auto observed = ReviewLedgerHost->private_observation(); observed.has_value()) {
				DurableDefects = std::move(observed->Defects);
				present_defects();
			}
			refresh_agent_private_context_choices();
			// RFC-BPR-0089 M2. Marks written here or in a sibling window.
			adopt_generated_marks();
			refresh_commit_filter();
			publish_overview();
		}));
	}

	auto github_application::implementation::conversation_endpoint_is_current(
		const core::comparison_pair& aComparison,
		const core::diff_side        aSide) const -> bool
	{
		// RFC-BPR-0089 M4. A commit-filtered diff's threads and comments are
		// placed on it by the core, on either side.
		if (selected_diff_filtered() && aComparison == SelectedDiffPublication->Request.Diff.Comparison) {
			return true;
		}
		// [[BPR-1212]]. GitHub's `LEFT` lines count the merge base's
		// version of the file; the left side is current only for the
		// selected comparison whose left document is known to be that.
		if (aSide == core::diff_side::left) {
			return SelectedDiffPublication.has_value() && aComparison == SelectedDiffPublication->Request.Diff.Comparison
				&& base_numbered_as_github();
		}
		if (Host == nullptr) {
			return false;
		}
		const auto& snapshot = WindowRead->accepted_snapshot();
		const auto  head     = snapshot.has_value()
			? core::parse_git_commit_oid(snapshot->PullRequest.HeadOid.value())
			: std::nullopt;
		return head.has_value() && aComparison.Right.Commit == *head;
	}

	/// \brief [[BPR-1212]]. The question whose answer says whether the
	/// selected comparison's left document is numbered as GitHub numbers a
	/// `LEFT` line: GitHub's base OID, the accepted head, and the
	/// comparison's left commit and path. None unless the comparison's
	/// right side is the current head and its left side has a file.
	/// [[BPR-1560]]. GitHub numbers a `LEFT` line in the merge base it
	/// named for the snapshot whenever it named one, so that is the base
	/// asked about; the recorded base only when it named none.
	auto github_application::implementation::base_numbering_request() const -> std::optional<application::github_base_numbering_request>
	{
		// RFC-BPR-0089 M4. A commit-filtered diff's base lines are placed in
		// the merge base by the core, not asked about here.
		if (!SelectedDiffPublication || !WindowRead || !WindowRead->accepted_snapshot() || selected_diff_filtered()) {
			return std::nullopt;
		}
		const auto& selected   = *SelectedDiffPublication;
		const auto& comparison = selected.Request.Diff.Comparison;
		const auto& snapshot   = *WindowRead->accepted_snapshot();
		const auto  base       = core::parse_git_commit_oid(
			snapshot.MergeBaseAvailability == application::github_merge_base_availability::available && snapshot.MergeBaseOid
				? snapshot.MergeBaseOid->value()
				: snapshot.PullRequest.BaseOid.value());
		if (!base || !selected.LeftPath || !selected.Key.LeftBlob || !conversation_endpoint_is_current(comparison, core::diff_side::right)) {
			return std::nullopt;
		}
		return application::github_base_numbering_request{
			.WorkbenchAuthority = selected.Request.WorkbenchAuthority,
			.BaseTip            = *base,
			.Head               = comparison.Right.Commit,
			.Left               = comparison.Left.Commit,
			.LeftPath           = *selected.LeftPath,
		};
	}

	/// \brief [[BPR-1570]]. Whether the kept answer answers this question:
	/// it was asked for it, or it is definite (`same` or `differs`) and
	/// about the same base, head, left commit and path. A definite answer
	/// is a fact about those commits, so a refresh that keeps the file and
	/// adopts it under a new workbench authority ([[BPR-1277]]) keeps it;
	/// `unavailable` depends on what the cache held and is asked again.
	auto github_application::implementation::base_numbering_answers(const application::github_base_numbering_request& aRequest) const -> bool
	{
		if (!BaseNumbering) {
			return false;
		}
		const auto& kept = BaseNumbering->Request;
		if (kept == aRequest) {
			return true;
		}
		const auto definite = BaseNumbering->Status == application::github_base_numbering_status::same
			|| BaseNumbering->Status == application::github_base_numbering_status::differs;
		return definite && kept.BaseTip == aRequest.BaseTip && kept.Head == aRequest.Head && kept.Left == aRequest.Left && kept.LeftPath == aRequest.LeftPath;
	}

	auto github_application::implementation::base_numbered_as_github() const -> bool
	{
		if (selected_diff_filtered()) {
			return true;
		}
		if (!BaseNumbering || BaseNumbering->Status != application::github_base_numbering_status::same) {
			return false;
		}
		const auto request = base_numbering_request();
		return request && base_numbering_answers(*request);
	}

	/// \brief [[BPR-1212]]. Asks the epoch host once per comparison, and
	/// applies a kept answer.
	void github_application::implementation::observe_base_numbering()
	{
		const auto request = base_numbering_request();
		if (!request || EpochHost == nullptr || base_numbering_answers(*request)) {
			apply_base_numbering();
			return;
		}
		BaseNumbering = base_numbering_state{ .Request = *request };
		// [[BPR-1560]]. A left side that is the merge base GitHub named is
		// numbered as GitHub numbers it; there is nothing to ask the cache,
		// which on a selected-objects review could not answer.
		if (const auto& snapshot = *WindowRead->accepted_snapshot();
			snapshot.MergeBaseAvailability == application::github_merge_base_availability::available
			&& request->Left == request->BaseTip) {
			BaseNumbering->Status = application::github_base_numbering_status::same;
			apply_base_numbering();
			return;
		}
		const auto admitted = EpochHost->resolve_github_base_numbering(
			*request,
			[this](const application::github_base_numbering_observation& aObservation) {
				if (Closing || !BaseNumbering || BaseNumbering->Request != aObservation.Request) {
					return;
				}
				auto status = aObservation.Status;
#if defined(BPR_BUILD_TESTING)
				if (ForcedBaseNumberingCancellations != 0U) {
					--ForcedBaseNumberingCancellations;
					status = application::github_base_numbering_status::cancelled;
				}
#endif
				// An accept that keeps the comparison stops queued epoch
				// work, this question among it, and may bring no new
				// publication: a cancelled question is asked again at once.
				if (status == application::github_base_numbering_status::cancelled) {
					BaseNumbering.reset();
					rearm_base_numbering();
					return;
				}
				// The cache no longer serves this review: asked again on the
				// next publication or accepted review.
				if (status == application::github_base_numbering_status::stale) {
					BaseNumbering.reset();
					return;
				}
				BaseNumberingCancellations = 0U;
				BaseNumbering->Status      = status;
				apply_base_numbering();
			});
		// An authority being replaced admits no epoch work yet: asked
		// again once the accept has had its turn.
		if (!admitted) {
			BaseNumbering.reset();
			rearm_base_numbering();
		}
		apply_base_numbering();
	}

	/// \brief [[BPR-1212]]. Asks a cancelled or not yet admitted question
	/// again shortly, a bounded number of times in a row (about two
	/// seconds), so an accept in progress can finish first.
	void github_application::implementation::rearm_base_numbering()
	{
		constexpr auto maximumCancellations = std::size_t{ 80U };
		if (Closing || Window == nullptr || BaseNumberingCancellations >= maximumCancellations) {
			return;
		}
		++BaseNumberingCancellations;
		QTimer::singleShot(25, Window.get(), [this] {
			if (!Closing && !BaseNumbering) {
				observe_base_numbering();
			}
		});
	}

	void github_application::implementation::apply_base_numbering()
	{
		const auto numbered = base_numbered_as_github();
		if (ReviewSourceMaterial && SelectedDiffPublication && ReviewSourceMaterial->Key == SelectedDiffPublication->Key) {
			ReviewSourceMaterial->BaseNumberedAsGitHub = numbered;
		}
		if (Window == nullptr || !SelectedDiffPublication) {
			return;
		}
		const auto& comparison = SelectedDiffPublication->Request.Diff.Comparison;
		if (Window->left_conversation_endpoint_current() == numbered) {
			return;
		}
		Window->set_conversation_endpoint_placement(numbered, conversation_endpoint_is_current(comparison, core::diff_side::right));
		// Threads on base lines are placed now, and a base line selected
		// before the answer takes its draft.
		refresh_review_conversations();
		if (numbered && SelectedSemanticSelection && SelectedSemanticSelection->PreferredLine.Side == core::diff_side::left) {
			present_review_draft(*SelectedSemanticSelection);
		}
	}

	auto github_application::implementation::current_thread_anchor(
		const application::github_review_thread_record& aThread) const
		-> std::optional<core::source_line_ref>
	{
		if (aThread.Subject != application::github_review_thread_subject::line || selected_diff_filtered()
			|| aThread.Outdated || !aThread.Line.has_value()
			|| *aThread.Line == 0U || aThread.Path.empty()
			|| !SelectedDiffPublication.has_value() || Window == nullptr
			|| Window->comparison_surface() == nullptr
			|| Window->comparison_surface()->current_result() == nullptr) {
			return std::nullopt;
		}
		const auto  side     = aThread.DiffSide == application::github_diff_side::left
			? std::optional{ core::diff_side::left }
			: aThread.DiffSide == application::github_diff_side::right
			? std::optional{ core::diff_side::right }
			: std::nullopt;
		const auto& selected = *SelectedDiffPublication;
		if (!side.has_value()
			|| core::file_identity_of(aThread.Path)
				!= selected.Request.File.File
			|| !conversation_endpoint_is_current(
				selected.Request.Diff.Comparison, *side)) {
			return std::nullopt;
		}
		const auto anchor = core::source_line_ref{
			.Comparison = core::comparison_digest(selected.Key),
			.Side       = *side,
			.Line       = *aThread.Line - 1U,
		};
		const auto& alignment
			= Window->comparison_surface()->current_result()->Alignment;
		const auto lineCount = anchor.Side == core::diff_side::left
			? alignment.LeftLineCount
			: alignment.RightLineCount;
		return alignment.Digest == anchor.Comparison && anchor.Line < lineCount
			? std::optional{ anchor }
			: std::nullopt;
	}

	auto github_application::implementation::defect_anchor_is_current(
		const application::durable_defect_record&       aDefect,
		const application::github_review_thread_record& aThread) const -> bool
	{
		const auto anchor = current_thread_anchor(aThread);
		return anchor.has_value()
			&& aDefect.File == core::file_identity_of(aThread.Path)
			&& aDefect.History.Defect.Anchor == *anchor
			&& aDefect.History.OriginalAnchor == *anchor;
	}

	void github_application::implementation::classify_defect(qt::workbench_defect_classification aClassification)
	{
		if (Closing || ReviewLedgerHost == nullptr
			|| !SelectedDiffPublication.has_value() || Window == nullptr
			|| Window->comparison_surface() == nullptr
			|| Window->comparison_surface()->current_result() == nullptr) {
			return;
		}
		const auto* const thread = accepted_thread(aClassification.Conversation);
		const auto        anchor = thread == nullptr
			? std::nullopt
			: current_thread_anchor(*thread);
		if (!anchor.has_value() || aClassification.Anchor != *anchor) {
			return;
		}
		const auto identity = defect_identity(aClassification.Conversation);
		if (std::ranges::any_of(
				DurableDefects,
				[identity](const application::durable_defect_record& aDefect) {
					return aDefect.Conversation.value() == identity.Conversation
						&& aDefect.History.Defect.Id.value() == identity.Defect;
				})) {
			return;
		}
		auto durable = application::durable_defect_record{
			.File         = SelectedDiffPublication->Request.File.File,
			.Conversation = core::review_conversation_id{ identity.Conversation },
			.History      = core::defect_history{
				.Defect = core::defect_record{
					.Id = core::defect_id{ identity.Defect }, .Anchor = *anchor },
				.OriginalAnchor = *anchor,
			},
		};
		persist_defect(std::move(durable), [this] { correlate_defects(); });
	}

	void github_application::implementation::dispatch_defect(core::defect_command aCommand)
	{
		if (Closing || ReviewLedgerHost == nullptr) {
			return;
		}
		const auto found = std::ranges::find(
			DurableDefects,
			aCommand.Id,
			[](const application::durable_defect_record& aDefect) {
				return aDefect.History.Defect.Id;
			});
		if (found == DurableDefects.end()) {
			return;
		}
		if (PendingDefectWrites.contains(aCommand.Id)) {
			return;
		}
		const auto outcome = core::apply(found->History.Defect, aCommand);
		if (outcome.Rejection != core::defect_rejection::none
			|| !outcome.Changed) {
			return;
		}
		auto changed           = *found;
		changed.History.Defect = outcome.Defect;
		persist_defect(std::move(changed), [this] { present_defects(); });
	}
}
