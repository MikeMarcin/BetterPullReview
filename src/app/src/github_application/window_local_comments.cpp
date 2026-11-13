// RFC-BPR-0127 ([[BPR-2158]]). Comments absorbs Notes: the composition places
// my Local comments for the Comments drawer and carries Keep local, Make
// draft, Save and Delete through the core's decisions and one store move.
#include "github_application/window_composition.hpp"

#include "bpr/core/local_comment.hpp"
#include "bpr/core/note_placement.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"

#include <QString>

#include <algorithm>
#include <vector>

namespace bpr::app {
	namespace {
		[[nodiscard]] auto outdated_words(const core::note_outdated_reason aReason) -> QString
		{
			switch (aReason) {
			case core::note_outdated_reason::content_changed: return QStringLiteral("Outdated: the code it was written on has changed.");
			case core::note_outdated_reason::side_absent:     return QStringLiteral("Outdated: that side of the file is not in this comparison.");
			case core::note_outdated_reason::unmatched:       return QStringLiteral("Outdated: written on another comparison, whose line cannot be matched here.");
			case core::note_outdated_reason::line_absent:     return QStringLiteral("Outdated: its line is past the end of the file here.");
			case core::note_outdated_reason::none:            break;
			}
			return { };
		}

		/// Owner look v002 ([[BPR-2158]]): two Local comments on one line,
		/// one offering Make draft and one not, read as arbitrary. A line
		/// and a thread each hold one draft of mine; the words say which
		/// draft stands in the way and what frees it.
		[[nodiscard]] auto make_draft_words(const core::make_draft_refusal aRefusal, const bool aReply = false) -> QString
		{
			switch (aRefusal) {
			case core::make_draft_refusal::select_a_line: return QStringLiteral("Select a line in the code to make it a draft there.");
			case core::make_draft_refusal::draft_stands:
				return aReply ? QStringLiteral("Can't make a draft: your draft reply in this thread is already open. A thread holds one draft reply of yours; send or delete it first.")
							  : QStringLiteral("Can't make a draft: your draft on this line already stands. A line holds one draft of yours; publish or delete it first.");
			}
			return { };
		}

		[[nodiscard]] auto keep_local_words(const core::keep_local_refusal aRefusal) -> QString
		{
			switch (aRefusal) {
			case core::keep_local_refusal::held_by_review: return QStringLiteral("Kept as it is: your submitted review holds this draft until GitHub answers.");
			case core::keep_local_refusal::publishing:     return QStringLiteral("Kept as it is: this draft is being published.");
			case core::keep_local_refusal::published:      return QStringLiteral("Kept as it is: it is on GitHub.");
			case core::keep_local_refusal::discarded:      return QStringLiteral("Kept as it is: the draft was discarded.");
			}
			return { };
		}
	}

	auto github_application::implementation::shown_drafts() const -> std::vector<core::review_comment_draft>
	{
		auto drafts = std::vector<core::review_comment_draft>{ };
		if (SharedDrafts) {
			for (const auto& draft : SharedDrafts->recovery_snapshot()) {
				drafts.push_back(draft.Draft);
			}
		}
		return drafts;
	}

	auto github_application::implementation::make_draft_input_of(const application::local_comment_record& aComment) const -> core::make_draft_input
	{
		const auto* const surface   = Window != nullptr ? Window->comparison_surface() : nullptr;
		const auto*       alignment = surface != nullptr && surface->current_result() != nullptr ? &surface->current_result()->Alignment : nullptr;
		auto              input     = core::make_draft_input{ .Anchor = application::local_comment_place_of(aComment), .Placement = core::place_note(application::local_comment_anchor_of(aComment), alignment) };
		if (alignment != nullptr) {
			input.Shown = alignment->Digest;
		}
		if (surface != nullptr && surface->selection().has_value() && alignment != nullptr && surface->selection()->PreferredLine.Comparison == alignment->Digest) {
			input.Selected = surface->selection()->PreferredLine;
		}
		return input;
	}

	void github_application::implementation::present_local_comments()
	{
		if (Window == nullptr) {
			return;
		}
		const auto drafts = shown_drafts();
		auto       cards  = std::vector<qt::local_comment_card>{ };
		for (const auto& comment : AcceptedPrivateNotes) {
			auto input   = make_draft_input_of(comment);
			input.Drafts = drafts;
			auto card    = qt::local_comment_card{ .Id = comment.Id.value(), .Revision = comment.Revision.value(), .Body = QString::fromStdString(comment.Body), .ReplyToThread = comment.ReplyToThread };
			if (comment.Attachment && comment.Attachment->Line) {
				card.Side = comment.Attachment->Line->Side;
			}
			switch (input.Placement.Kind) {
			case core::note_placement_kind::placed:
				card.Comparison = input.Placement.At->Comparison;
				card.Side       = input.Placement.At->Side;
				card.Line       = input.Placement.At->Line + 1U;
				if (comment.StartLine) {
					card.FirstLine = *comment.StartLine + 1U;
				}
				break;
			case core::note_placement_kind::outdated:
				card.Place  = qt::local_comment_place::outdated;
				card.Notice = outdated_words(input.Placement.Reason);
				break;
			// RFC-BPR-0127 "Where a Local comment can be": the pull request
			// chip and Overview's Comments tab, the file's chip, and the
			// Other files chip. None is drawn at a line.
			case core::note_placement_kind::pull_request: card.Place = qt::local_comment_place::pull_request; break;
			case core::note_placement_kind::whole_file:   card.Place = qt::local_comment_place::whole_file; break;
			case core::note_placement_kind::other_file:
				card.Place = qt::local_comment_place::other_file;
				if (comment.Attachment && comment.Attachment->Line) {
					card.PathLine = comment.Attachment->Line->Line + 1U;
				}
				break;
			}
			if (comment.FilePath) {
				card.Path = QString::fromStdString(*comment.FilePath);
			}
			if (const auto target = core::decide_make_draft(input); target.has_value()) {
				card.MakeDraft = target->Reanchored ? QStringLiteral("Make draft on line %1").arg(target->Anchor.Line + 1U) : QStringLiteral("Make draft");
			} else {
				card.MakeDraftWhy = make_draft_words(target.error(), comment.ReplyToThread.has_value());
			}
			cards.push_back(std::move(card));
		}
		Window->present_local_comments(std::move(cards));
	}

	void github_application::implementation::reload_local_comments()
	{
		if (ReviewLedgerHost == nullptr) {
			return;
		}
		ReviewLedgerHost->load_private(ledger_answer([this](const std::expected<void, application::ledger_store_failure>&) { refresh_agent_private_context_choices(); }));
	}

	void github_application::implementation::dispatch_comment_state(const qt::comment_state_gesture& aGesture)
	{
		if (Window == nullptr || Closing) {
			return;
		}
		const auto refuse = [this](const QString& aWhy) {
			if (Window != nullptr) {
				Window->refuse_comment_state(aWhy);
			}
		};
		if (!ReviewLedgerHost || !ReviewLedgerAvailable || !PrivateReview) {
			refuse(QStringLiteral("Local comments are unavailable for this review. Nothing changed."));
			return;
		}
		const auto found = aGesture.Local ? std::ranges::find(AcceptedPrivateNotes, application::local_comment_id{ *aGesture.Local }, &application::local_comment_record::Id) : AcceptedPrivateNotes.end();
		switch (aGesture.Action) {
		case qt::card_action::delete_local: {
			if (found == AcceptedPrivateNotes.end()) {
				return;
			}
			ReviewLedgerHost->erase(found->Id, found->Revision, ledger_answer([this, refuse](const std::expected<void, application::local_comment_failure>& aErased) {
										if (!aErased) {
											refuse(QStringLiteral("The local comment was not deleted: it changed. Nothing else changed."));
										}
										refresh_agent_private_context_choices();
									}));
			return;
		}
		case qt::card_action::edit_local: {
			// RFC-BPR-0127. Edited in place: the same comment, new words, at
			// the revision editing began from; nothing else changes.
			if (found == AcceptedPrivateNotes.end() || !aGesture.Revision || found->Revision.value() != *aGesture.Revision) {
				Window->refuse_local_edit(QStringLiteral("The local comment changed elsewhere. Your text is kept; copy it before cancelling."));
				return;
			}
			if (aGesture.Body.empty()) {
				Window->refuse_local_edit(QStringLiteral("Write something to save it, or delete the comment. Nothing changed."));
				return;
			}
			auto edited = *found;
			edited.Body = aGesture.Body;
			ReviewLedgerHost->persist(edited, found->Revision, ledger_answer([this](const std::expected<application::local_comment_record, application::local_comment_failure>& aStored) {
										  if (!aStored && Window != nullptr) {
											  Window->refuse_local_edit(QStringLiteral("The local comment was not saved. Your text is kept."));
										  }
										  refresh_agent_private_context_choices();
									  }));
			return;
		}
		case qt::card_action::make_draft: {
			if (found == AcceptedPrivateNotes.end()) {
				return;
			}
			auto       input  = make_draft_input_of(*found);
			const auto drafts = shown_drafts();
			input.Drafts      = drafts;
			const auto target = core::decide_make_draft(input);
			if (!target) {
				refuse(make_draft_words(target.error(), found->ReplyToThread.has_value()));
				return;
			}
			PrivateReview->make_draft(found->Id, found->Revision, found->Body, target->Anchor, target->StartLine, target->ReplyToThread, [this, refuse](auto aCommit) {
				if (!aCommit) {
					refuse(QStringLiteral("Make draft was refused: the comment or its line changed. Nothing changed."));
				}
				reload_local_comments();
			});
			return;
		}
		case qt::card_action::keep_local:
		case qt::card_action::save_local: {
			if (!aGesture.Draft) {
				refuse(QStringLiteral("There is no draft to keep local. Nothing changed."));
				return;
			}
			const auto drafts = SharedDrafts ? SharedDrafts->recovery_snapshot() : std::vector<core::revisioned_review_draft>{ };
			const auto draft  = std::ranges::find(drafts, *aGesture.Draft, [](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Id; });
			const auto shown  = current_note_attachment();
			// Save on a comment the store does not hold yet: a new Local
			// comment at the composer's lines (or in its thread), and the
			// composer's unsaved draft, which nothing else knows, goes. A draft the store holds, even one still
			// empty there, moves instead (below), so its identity is kept.
			if (aGesture.Action == qt::card_action::save_local && aGesture.Anchor && draft == drafts.end()) {
				const auto* const surface = Window->comparison_surface();
				if (!shown || shown->Comparison != aGesture.Anchor->Comparison || surface == nullptr || surface->current_result() == nullptr
					|| surface->current_result()->Alignment.Digest != aGesture.Anchor->Comparison) {
					refuse(QStringLiteral("Return to the comment's comparison to save it. Your text is kept."));
					return;
				}
				if (aGesture.Body.empty()) {
					refuse(QStringLiteral("Write something to save it. Nothing changed."));
					return;
				}
				auto next = std::uint64_t{ 1U };
				for (const auto& comment : AcceptedPrivateNotes) {
					next = std::max(next, comment.Id.value() + 1U);
				}
				auto attachment  = application::local_comment_attachment{ .File = shown->File, .Comparison = shown->Comparison, .Line = core::note_line_anchor_at(surface->current_result()->Alignment, aGesture.Anchor->Side, aGesture.Anchor->Line) };
				auto comment     = application::local_comment_record{ .Id = application::local_comment_id{ next }, .Attachment = attachment, .Body = aGesture.Body, .StartLine = aGesture.StartLine, .ReplyToThread = aGesture.ReplyToThread };
				comment.FilePath = SelectedDiffPublication ? std::optional{ SelectedDiffPublication->Request.File.PresentationPath } : std::nullopt;
				ReviewLedgerHost->persist(comment, std::nullopt, ledger_answer([this, refuse](const std::expected<application::local_comment_record, application::local_comment_failure>& aStored) {
											  if (!aStored) {
												  refuse(QStringLiteral("The local comment was not saved. Your text is kept."));
												  return;
											  }
											  if (Window != nullptr) {
												  Window->close_drawer_composer(true);
											  }
											  refresh_agent_private_context_choices();
										  }));
				return;
			}
			if (draft == drafts.end() || !shown || shown->Comparison != draft->Draft.Anchor.Comparison) {
				refuse(QStringLiteral("Return to the draft's comparison to keep it local. Nothing changed."));
				return;
			}
			auto input = core::keep_local_input{ .Draft = draft->Draft, .File = shown->File };
			if (aGesture.Action == qt::card_action::save_local && !aGesture.Body.empty()) {
				input.Draft.Body = aGesture.Body;
			}
			const auto* const surface = Window->comparison_surface();
			if (surface != nullptr && surface->current_result() != nullptr && surface->current_result()->Alignment.Digest == draft->Draft.Anchor.Comparison) {
				if (const auto line = core::note_line_anchor_at(surface->current_result()->Alignment, draft->Draft.Anchor.Side, draft->Draft.Anchor.Line); line.has_value()) {
					input.ShownBlob = line->Blob;
				}
			}
			const auto decided = core::decide_keep_local(input);
			if (!decided) {
				refuse(keep_local_words(decided.error()));
				return;
			}
			if (input.Draft.Body.empty()) {
				refuse(QStringLiteral("Write something to keep it local."));
				return;
			}
			auto comment = application::local_comment_record{ .Body = input.Draft.Body, .StartLine = decided->StartLine, .ReplyToThread = decided->ReplyToThread };
			if (decided->Place.File && decided->Place.Comparison) {
				comment.Attachment = application::local_comment_attachment{ .File = *decided->Place.File, .Comparison = *decided->Place.Comparison, .Line = decided->Place.Line };
				comment.FilePath   = SelectedDiffPublication ? std::optional{ SelectedDiffPublication->Request.File.PresentationPath } : std::nullopt;
			}
			// Save with words the store has not seen yet: they are the
			// draft's first, then the draft moves with them.
			auto move = [this, refuse, id = draft->Draft.Id, comment](const std::uint64_t aRevision) {
				PrivateReview->keep_local(id, aRevision, comment, [this, refuse](auto aKept) {
					if (!aKept) {
						refuse(QStringLiteral("Keep local was refused: the draft changed or is being sent. Nothing changed."));
					}
					reload_local_comments();
					// As after Delete ([[BPR-1862]]): the kept draft ended
					// discarded, so it is no longer its line's draft, and a new
					// comment there is a new draft.
					if (aKept) {
						leave_deleted_draft();
					}
				});
			};
			if (input.Draft.Body != draft->Draft.Body) {
				PrivateReview->change_draft({ { draft->Draft.Id, core::review_draft_command_kind::edit, input.Draft.Body }, draft->Revision, false, { }, draft->Scope },
											[move, refuse](auto aCommit) {
												if (!aCommit || aCommit->records().empty()) {
													refuse(QStringLiteral("Keep local was refused: the draft changed. Nothing changed."));
													return;
												}
												move(aCommit->records().front().Draft.Revision);
											});
			} else {
				move(draft->Revision);
			}
			if (aGesture.Action == qt::card_action::save_local) {
				Window->close_drawer_composer(false);
			}
			return;
		}
		default:
			return;
		}
	}

#if defined(BPR_BUILD_TESTING)
	auto github_application::implementation::seed_local_comment(const std::string& aBody, const bool aWholeFile) -> bool
	{
		const auto shown = current_note_attachment();
		if (!ReviewLedgerHost || (aWholeFile && !shown)) {
			return false;
		}
		auto next = std::uint64_t{ 1U };
		for (const auto& comment : AcceptedPrivateNotes) {
			next = std::max(next, comment.Id.value() + 1U);
		}
		auto comment = application::local_comment_record{ .Id = application::local_comment_id{ next }, .Body = aBody };
		if (aWholeFile) {
			comment.Attachment = application::local_comment_attachment{ .File = shown->File, .Comparison = shown->Comparison };
			comment.FilePath   = SelectedDiffPublication ? std::optional{ SelectedDiffPublication->Request.File.PresentationPath } : std::nullopt;
		}
		ReviewLedgerHost->persist(comment, std::nullopt, ledger_answer([this](const std::expected<application::local_comment_record, application::local_comment_failure>&) { refresh_agent_private_context_choices(); }));
		return true;
	}

	auto github_application::implementation::seed_local_replies_for_look(const std::string& aThread, const core::source_line_ref aLine) -> bool
	{
		const auto  shown   = current_note_attachment();
		auto* const surface = Window != nullptr ? Window->comparison_surface() : nullptr;
		if (!shown || shown->Comparison != aLine.Comparison || surface == nullptr || surface->current_result() == nullptr || !ReviewLedgerHost || !PrivateReview) {
			return false;
		}
		auto next = std::uint64_t{ 1U };
		for (const auto& comment : AcceptedPrivateNotes) {
			next = std::max(next, comment.Id.value() + 1U);
		}
		auto       attachment = application::local_comment_attachment{ .File = shown->File, .Comparison = shown->Comparison, .Line = core::note_line_anchor_at(surface->current_result()->Alignment, aLine.Side, aLine.Line) };
		const auto reply      = [&](const std::uint64_t aId, std::string aBody) {
			return application::local_comment_record{ .Id = application::local_comment_id{ aId }, .Attachment = attachment, .Body = std::move(aBody), .ReplyToThread = aThread };
		};
		const auto first  = reply(next, "Check retained() after a resize before believing this. Ask at standup.");
		const auto second = reply(next + 1U, "Thanks. Does begin_frame() also skip it after forget()?");
		ReviewLedgerHost->persist(first, std::nullopt, ledger_answer([this, second, aLine, aThread](const std::expected<application::local_comment_record, application::local_comment_failure>& aFirst) {
									  if (!aFirst) {
										  return;
									  }
									  ReviewLedgerHost->persist(second, std::nullopt, ledger_answer([this, aLine, aThread](const std::expected<application::local_comment_record, application::local_comment_failure>& aSecond) {
																	if (!aSecond) {
																		return;
																	}
																	// The second becomes my draft reply, through Make draft.
																	PrivateReview->make_draft(aSecond->Id, aSecond->Revision, aSecond->Body, aLine, std::nullopt, aThread, [this](auto) { reload_local_comments(); });
																}));
								  }));
		return true;
	}
#endif
}
