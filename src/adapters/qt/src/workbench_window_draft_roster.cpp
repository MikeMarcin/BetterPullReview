#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/conversation_finder.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QStatusBar>
#include <QString>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-2092]] [[BPR-2093]] [[BPR-2094]]. My drafts, found: the core's
/// roster presented in Threads, Overview's Comments column and Finish your
/// review, and the one way each goes to a draft.
namespace bpr::qt {
	namespace {
		[[nodiscard]] auto text_of(const std::string& aText) -> QString
		{
			return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()));
		}

		[[nodiscard]] auto why_not(const core::draft_add_refusal aRefusal) -> QString
		{
			switch (aRefusal) {
			case core::draft_add_refusal::reply:
				return workbench_window::tr("A reply is sent to its thread on its own, never in a review.");
			case core::draft_add_refusal::place_unknown:
				return workbench_window::tr("Open it to add it: it was saved before drafts recorded where they go.");
			case core::draft_add_refusal::not_at_head:
				return workbench_window::tr("A review can't take it at its line: it was written on another diff or head. Open it to publish it on its own.");
			case core::draft_add_refusal::none:
				break;
			}
			return { };
		}
	} // namespace

	void workbench_window::present_draft_roster(core::draft_roster aRoster)
	{
		DraftRoster = std::move(aRoster);
		auto saved  = std::vector<finder_saved_draft>{ };
		auto counts = std::map<QString, overview_draft_count>{ };
		auto out    = std::vector<review_submission_editor::outside_draft>{ };
		for (const auto& entry : DraftRoster.Entries) {
			const auto reply = entry.Standing == core::draft_standing::reply;
			const auto in    = entry.Standing == core::draft_standing::in_review || entry.Standing == core::draft_standing::held;
			if (!entry.Path.empty()) {
				saved.push_back(finder_saved_draft{
					.Id       = entry.Id.value(),
					.Path     = text_of(entry.Path),
					.Line     = reply && core::thread_bound_anchor({ .Comparison = entry.Comparison, .Side = entry.Side, .Line = entry.Line }) ? 0U : entry.Line + 1U,
					.Body     = text_of(entry.Body),
					.InReview = in,
					.Reply    = reply,
				});
			}
			if (entry.Standing == core::draft_standing::not_in_review || reply) {
				out.push_back(review_submission_editor::outside_draft{
					.Id        = entry.Id,
					.Path      = entry.Path.empty() ? tr("File not opened yet") : text_of(entry.Path),
					.Side      = entry.Side,
					.Line      = entry.Line,
					.StartLine = entry.StartLine,
					.Body      = text_of(entry.Body),
					.Addable   = entry.Mark.has_value(),
					.Why       = why_not(entry.Refusal),
					.Reply     = reply,
				});
			}
		}
		if (Finder != nullptr) {
			Finder->set_saved_drafts(std::move(saved));
		}
		// RFC-BPR-0127 ([[BPR-2159]]). A draft kept Local leaves the editor
		// without a card refresh; its open row must go with it.
		present_finder_draft();
		present_mine_counts();
		if (ReviewSubmissionEditor != nullptr) {
			ReviewSubmissionEditor->set_outside_drafts(std::move(out));
		}
	}

	void workbench_window::present_mine_counts()
	{
		if (OverviewPage == nullptr) {
			return;
		}
		auto counts = std::map<QString, overview_draft_count>{ };
		for (const auto& count : core::draft_counts_by_file(DraftRoster)) {
			counts.emplace(text_of(count.Path), overview_draft_count{ .InReview = count.InReview, .NotInReview = count.NotInReview });
		}
		// RFC-BPR-0127 ([[BPR-2159]]). My Local comments beside them.
		for (const auto& [path, local] : LocalByPath) {
			counts[path].Local = local;
		}
		OverviewPage->set_drafts(std::move(counts));
		// My review counts my comments by state: a reply is not in the
		// review, a held draft is.
		OverviewPage->set_mine_totals(overview_draft_count{ .InReview    = DraftRoster.InReview + DraftRoster.Held,
															.NotInReview = DraftRoster.NotInReview + DraftRoster.Replies,
															.Local       = LocalTotal });
	}

	void workbench_window::present_local_found(const std::vector<local_comment_card>& aComments)
	{
		// RFC-BPR-0127 ([[BPR-2159]]). Found and counted where drafts are:
		// Threads' rows in their files' groups, Overview's Local chip per
		// file, and Finish your review's quiet line.
		auto rows   = std::vector<finder_local_comment>{ };
		auto byPath = std::map<QString, std::size_t>{ };
		for (const auto& comment : aComments) {
			if (comment.Path.isEmpty() || comment.Place == local_comment_place::pull_request) {
				continue;
			}
			rows.push_back(finder_local_comment{
				.Id    = comment.Id,
				.Path  = comment.Path,
				.Line  = comment.Line.value_or(comment.PathLine.value_or(0U)),
				.Body  = comment.Body,
				.Reply = comment.ReplyToThread.has_value(),
			});
			++byPath[comment.Path];
		}
		if (Finder != nullptr) {
			Finder->set_local_comments(std::move(rows));
		}
		if (byPath != LocalByPath || LocalTotal != aComments.size()) {
			LocalByPath = std::move(byPath);
			LocalTotal  = aComments.size();
			present_mine_counts();
		}
		if (ReviewSubmissionEditor != nullptr) {
			ReviewSubmissionEditor->set_local_count(LocalTotal);
		}
	}

	auto workbench_window::draft_roster() const noexcept -> const core::draft_roster&
	{
		return DraftRoster;
	}

	auto workbench_window::open_roster_draft(const core::review_draft_id aDraft) -> bool
	{
		const auto found = std::ranges::find(DraftRoster.Entries, aDraft, &core::draft_roster_entry::Id);
		if (found == DraftRoster.Entries.end()) {
			return false;
		}
		if (found->ReplyToThread) {
			// A reply opens in its thread, where it is written.
			return reveal_conversation_in_review("review-thread:" + *found->ReplyToThread);
		}
		if (found->Path.empty()) {
			statusBar()->showMessage(tr("This draft's file is not known yet. Open its file to find it."));
			return false;
		}
		auto comment          = core::review_submission_comment{ };
		comment.Draft         = found->Id;
		comment.DraftRevision = found->Revision;
		comment.Path          = found->Path;
		comment.Side          = found->Side;
		comment.Line          = found->Line;
		comment.StartLine     = found->StartLine;
		comment.Body          = found->Body;
		open_pending_comment(comment, core::workbench_thread_destination::review);
		return true;
	}

	auto workbench_window::show_state_in_threads(const finder_state aState, const QString& aPath) -> bool
	{
		if (Finder == nullptr) {
			return false;
		}
		choose_home(false);
		choose_modality(review_modality::threads);
		if (!Finder->show_state(aState, aPath)) {
			statusBar()->showMessage(aState == finder_state::local ? tr("No local comments of yours here.") : tr("Nothing of yours here."));
			return false;
		}
		return true;
	}

	void workbench_window::keep_listed_draft_local(const core::review_draft_id aDraft)
	{
		// RFC-BPR-0127 ([[BPR-2159]]). Keep Local from a list: the draft's
		// file goes with it, so it can be kept when its comparison is not
		// the one shown.
		const auto found   = std::ranges::find(DraftRoster.Entries, aDraft, &core::draft_roster_entry::Id);
		auto       gesture = comment_state_gesture{ .Action = card_action::keep_local, .Identity = std::string{ finder_saved_draft_prefix } + std::to_string(aDraft.value()), .Draft = aDraft };
		if (found != DraftRoster.Entries.end()) {
			gesture.Path = found->Path;
		}
		if (CommentStateDispatch) {
			CommentStateDispatch(gesture);
		}
	}

	void workbench_window::open_local_from_threads(const std::uint64_t aLocal, const bool aMakeDraft)
	{
		// RFC-BPR-0127 ([[BPR-2159]]). A Local row opens its card in Review,
		// at its file and line; Make Draft from the row's menu is then the
		// card's, decided against that file's comparison.
		if (ConversationPanel == nullptr) {
			return;
		}
		const auto& comments = ConversationPanel->cards()->local_comments();
		const auto  found    = std::ranges::find(comments, aLocal, &local_comment_card::Id);
		if (found == comments.end()) {
			return;
		}
		PendingLocalMakeDraft = aMakeDraft;
		if (found->Place == local_comment_place::other_file) {
			open_local_elsewhere(conversation_cards_view::local_comment_identity(aLocal));
			return;
		}
		PendingNoteOpen = aLocal;
		choose_home(false);
		choose_modality(review_modality::review);
		resume_private_note_open();
	}

	auto workbench_window::show_drafts_in_threads(const QString& aPath) -> bool
	{
		if (Finder == nullptr) {
			return false;
		}
		choose_home(false);
		choose_modality(review_modality::threads);
		if (!Finder->show_drafts(aPath)) {
			statusBar()->showMessage(tr("No drafts of yours here."));
			return false;
		}
		return true;
	}

	void workbench_window::set_draft_review_dispatch(std::function<void(std::vector<core::review_draft_id>, bool)> aDispatch)
	{
		DraftReviewDispatch = std::move(aDispatch);
	}
} // namespace bpr::qt
