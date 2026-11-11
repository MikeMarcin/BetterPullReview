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
		for (const auto& count : core::draft_counts_by_file(DraftRoster)) {
			counts.emplace(text_of(count.Path), overview_draft_count{ .InReview = count.InReview, .NotInReview = count.NotInReview });
		}
		if (Finder != nullptr) {
			Finder->set_saved_drafts(std::move(saved));
		}
		if (OverviewPage != nullptr) {
			OverviewPage->set_drafts(std::move(counts));
		}
		if (ReviewSubmissionEditor != nullptr) {
			ReviewSubmissionEditor->set_outside_drafts(std::move(out));
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
