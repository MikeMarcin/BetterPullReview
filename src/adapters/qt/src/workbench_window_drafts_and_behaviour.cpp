#include "bpr/core/review_comment_range.hpp"
#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/suggestion_draft.hpp"
#include "bpr/core/thread_reply.hpp"
#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/feedback_label_settings.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/info_glyph.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/private_review_panel.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "workbench_window_support.hpp"

#include <QAbstractButton>
#include <QAbstractListModel>
#include <QAbstractSpinBox>
#include <QAction>
#include <QAnyStringView>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QChar>
#include <QCheckBox>
#include <QColor>
#include <QCursor>
#include <QDialog>
#include <QFlags>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLatin1Char>
#include <QLayout>
#include <QLineEdit>
#include <QMainWindow>
#include <QMetaObject>
#include <QModelIndex>
#include <QObject>
#include <QPaintDevice>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSize>
#include <QSpinBox>
#include <QStatusBar>
#include <QString>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::qt {
	namespace {
		/// [[BPR-1816]]. The finish switches' order: the finish buttons'.
		constexpr auto finish_verdicts = std::array{ core::review_disposition::comment, core::review_disposition::approve, core::review_disposition::request_changes };
	}

	void workbench_window::present_inline_draft()
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr || ReviewEditor == nullptr) {
			return;
		}
		auto* const cards = ConversationPanel->cards();
		const auto& draft = ReviewEditor->shown_draft();
		// [[RFC-BPR-0090]]. Finish your review's Convert, once its draft shows:
		// the text the window planned for it, at the same lines.
		if (PendingConvert.has_value() && draft.has_value() && draft->Id == *PendingConvert && !ReviewEditor->body_editor()->isReadOnly()) {
			const auto id = *std::exchange(PendingConvert, std::nullopt);
			if (PaneSuggestion.has_value() && PaneSuggestion->Anchor == draft->Anchor) {
				convert_pane_suggestion();
				return;
			}
			if (const auto found = HeldSuggestions.find(id); found != HeldSuggestions.end()) {
				const auto text = QString::fromStdString(found->second.Body);
				HeldSuggestions.erase(found);
				present_held_suggestions();
				ReviewEditor->body_editor()->setPlainText(text);
				return;
			}
		}
		const auto body = ReviewEditor->body_editor()->toPlainText();
		// The notes hold the accepted draft, so a draft left in the drawer
		// is its note at once; while it is written this lays nothing out.
		cards->set_draft(draft, ComparisonSurface != nullptr && ComparisonSurface->current_result() != nullptr ? std::optional{ ComparisonSurface->current_result()->Alignment.Digest } : std::nullopt);
		// What a suggestion block changes, against the exact head lines.
		auto change = std::optional<core::suggested_change>{ };
		// [[RFC-BPR-0090]]. Linked to the head pane's edit: the words in the
		// field, and what the code's change does under them.
		const auto linked = PaneSuggestion.has_value() && draft.has_value() && PaneSuggestion->Anchor == draft->Anchor;
		auto       hold   = std::pair<QString, QString>{ };
		if (linked) {
			// No change yet says to type it in the code.
			const auto plan = pane_suggestion_plan();
			change          = plan.has_value() && plan->Changed ? plan->Change : core::suggested_change{ .Fenced = true };
			// [[RFC-BPR-0090]] Q3. A suggestion GitHub cannot apply is held,
			// marked, and offered Convert; Finish your review holds it too.
			if (plan.has_value()) {
				hold = suggestion_hold(*plan);
			}
			const auto wasHeld = HeldSuggestions.contains(draft->Id);
			if (!hold.first.isEmpty() && plan.has_value()) {
				auto original = std::vector<std::string>{ };
				for (const auto& line : PaneSuggestion->Original) {
					original.push_back(line.Content);
				}
				HeldSuggestions[draft->Id] = held_suggestion{
					.Reason  = hold.first,
					.Convert = hold.second,
					.Body    = core::convert_suggestion_to_text(PaneSuggestion->Prose.toStdString(), PaneSuggestion->StartLine.value_or(PaneSuggestion->Anchor.Line), PaneSuggestion->Anchor.Line,
																original, plan->Block),
				};
			} else {
				HeldSuggestions.erase(draft->Id);
			}
			if (wasHeld != HeldSuggestions.contains(draft->Id) || !hold.first.isEmpty()) {
				present_held_suggestions();
			}
			// The range's edge reads amber while GitHub cannot apply it.
			if (auto* const head = PaneSuggestion->Open && ComparisonSurface != nullptr ? ComparisonSurface->head_editor() : nullptr; head != nullptr) {
				auto edge = hold.first.isEmpty() ? cards->accent() : cards->marker();
				auto wash = edge;
				wash.setAlphaF(0.14F);
				head->set_range_colours(wash, edge, edge.darker(135));
			}
		} else if (draft.has_value() && core::classify_review_body(body.toStdString()) == core::review_body_compatibility::requires_line_subject) {
			if (const auto lines = draft_source_lines(); lines.has_value()) {
				if (auto decided = core::suggested_change_of(body.toStdString(), *lines); decided.Fenced) {
					change = std::move(decided);
				}
			}
		}
		// [[BPR-1212]] slice 1. A draft on several lines names them; v002, a
		// draft on the base side says so.
		const auto base  = draft.has_value() && draft->Anchor.Side == core::diff_side::left;
		const auto words = !draft.has_value() ? QString{ }
			// [[BPR-1745]]. A reply bound to its outdated thread has no line.
			: core::thread_bound_anchor(draft->Anchor) ? tr("Outdated thread")
			: draft->StartLine.has_value() && base     ? tr("Base lines %1–%2").arg(*draft->StartLine + 1U).arg(draft->Anchor.Line + 1U)
			: draft->StartLine.has_value()             ? tr("Lines %1–%2").arg(*draft->StartLine + 1U).arg(draft->Anchor.Line + 1U)
			: base                                     ? tr("Base line %1").arg(draft->Anchor.Line + 1U)
													   : tr("Line %1").arg(draft->Anchor.Line + 1U);
		// The full composer's excerpt follows the same draft.
		if (DraftExcerpt != nullptr) {
			auto* const excerpt = static_cast<draft_excerpt_view*>(DraftExcerpt);
			auto        shown   = false;
			if (draft.has_value() && !core::thread_bound_anchor(draft->Anchor) && ComparisonSurface != nullptr && ComparisonSurface->current_result() != nullptr
				&& ComparisonSurface->current_context().has_value()
				&& draft->Anchor.Comparison == ComparisonSurface->current_result()->Alignment.Digest) {
				const auto& result   = *ComparisonSurface->current_result();
				const auto& document = draft->Anchor.Side == core::diff_side::right ? result.RightDocument : result.LeftDocument;
				if (document.has_value() && !document->Lines.empty()) {
					const auto last  = static_cast<std::uint32_t>(document->Lines.size() - 1U);
					const auto start = draft->StartLine.value_or(draft->Anchor.Line);
					const auto first = start > 3U ? start - 3U : 0U;
					const auto end   = std::min(last, draft->Anchor.Line + 3U);
					if (const auto lines = core::suggestion_source_lines(*document, first, end); lines.has_value()) {
						auto texts = std::vector<QString>{ };
						texts.reserve(lines->size());
						for (const auto& line : *lines) {
							texts.push_back(QString::fromStdString(line));
						}
						// The compared file's path, from the files drawer's rows.
						auto        path  = QString{ };
						const auto* model = Drawers[0].Panel != nullptr ? Drawers[0].Panel->contents() : nullptr;
						for (int row = 0; model != nullptr && row < model->rowCount() && path.isEmpty(); ++row) {
							const auto candidate = model->data(model->index(row, 0), Qt::DisplayRole).toString();
							if (core::file_identity_of(candidate.toStdString()) == ComparisonSurface->current_context()->Key.File) {
								path = candidate;
							}
						}
						excerpt->present(path, first + 1U, std::move(texts), start + 1U, draft->Anchor.Line + 1U,
										 cards->code_font());
						shown = true;
					}
				}
			}
			if (!shown) {
				excerpt->clear();
			}
			if (auto* const title = ComposeInlinePage != nullptr ? ComposeInlinePage->findChild<QLabel*>(QStringLiteral("composeInlineDraftPageTitle")) : nullptr; title != nullptr) {
				title->setText(shown ? excerpt->words() : QString{ });
			}
		}
		// The drawer's composer: the draft stops being written there once it
		// is published, answers a thread, or another line's draft shows.
		if (cards->writing_draft()
			&& (!draft.has_value() || draft->shared() || draft->ReplyToThread.has_value() || (WritingAnchor.has_value() && draft->Anchor != *WritingAnchor))) {
			WritingAnchor.reset();
			cards->end_draft();
			apply_thread_marks();
		}
		// [[RFC-BPR-0090]]. The head pane's edit is the shown draft's, or ends.
		if (PaneSuggestion.has_value() && (!draft.has_value() || draft->shared() || draft->ReplyToThread.has_value() || draft->Anchor != PaneSuggestion->Anchor)) {
			release_pane_suggestion();
		}
		if (!draft.has_value() || draft->shared() || draft->ReplyToThread.has_value()) {
			cards->present_draft(std::nullopt);
			return;
		}
		// [[BPR-1735]]. A comment GitHub cannot place in the review is held on
		// the comment itself too, its action Publish as file comment.
		if (hold.first.isEmpty()) {
			if (const auto found = UnplaceableComments.find(draft->Id); found != UnplaceableComments.end()) {
				hold = unplaceable_hold(found->second);
			}
		}
		// [[BPR-1329]]. What the last Publish came to comes first: publishing,
		// or why it did not happen. Otherwise, why Publish is unavailable.
		auto notice = ReviewEditor->publication_status();
		// RFC-BPR-0089 M4. Then where the comment goes, read before Publish.
		if (notice.isEmpty()) {
			notice = ReviewEditor->target_notice_text();
		}
		if (notice.isEmpty() && !ReviewEditor->publish_action()->isEnabled() && !body.trimmed().isEmpty()) {
			notice = ReviewEditor->publish_action()->toolTip();
		}
		if (!hold.first.isEmpty()) {
			notice = hold.first;
		}
		cards->present_draft(draft_composer_presentation{
			.Body        = linked ? PaneSuggestion->Prose : body,
			.Publishable = ReviewEditor->publish_action()->isEnabled() && hold.first.isEmpty(),
			.Editable    = !ReviewEditor->body_editor()->isReadOnly(),
			.Lines       = words,
			.Change      = std::move(change),
			.InCode      = linked,
			.Convert     = hold.second,
			.Notice      = notice,
			.Retry       = ReviewEditor->publication_retry_offered(),
			.Review      = draft_review(),
		});
		publish_pending_file_comment();
	}

	auto workbench_window::draft_review() const -> draft_review_state
	{
		const auto& shown = ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::optional<core::review_comment_draft>{ };
		if (shown.has_value() && core::pending_in_review(*shown)) {
			return draft_review_state::pending;
		}
		return PendingReviewComments > 0U ? draft_review_state::in_progress : draft_review_state::none;
	}

	void workbench_window::update_review_chip()
	{
		if (ReviewChip == nullptr || ReviewChipAction == nullptr) {
			return;
		}
		auto* const chip = static_cast<review_progress_chip*>(ReviewChip);
		const auto  held = HeldReviewComments > 0U;
		if (chip->pending() != PendingReviewComments || chip->submitting() != held) {
			chip->set_count(PendingReviewComments, held);
		}
		const auto shown = PendingReviewComments > 0U || held;
		if (ReviewChipAction->isVisible() != shown) {
			ReviewChipAction->setVisible(shown);
			// [[BPR-1700]]. Compose review is the plain icon beside the chip.
			present_compose_entry();
			apply_strip_width();
		}
		if (FinishReviewAction != nullptr) {
			FinishReviewAction->setEnabled(PendingReviewComments > 0U && !held);
		}
	}

	auto workbench_window::finish_review_action() const noexcept -> QAction*
	{
		return FinishReviewAction;
	}

	auto workbench_window::delete_draft_confirmation() const noexcept -> destructive_question*
	{
		return DeleteDraftConfirmation.data();
	}

	void workbench_window::present_draft_delete_refusal(const QString& aReason)
	{
		// [[BPR-1862]]. Where the drawer's composer has closed, the status
		// line is what still shows; the editor's own status keeps it too.
		statusBar()->showMessage(aReason, 15000);
		if (ReviewEditor != nullptr) {
			ReviewEditor->present_publication_refusal(aReason);
		}
		present_inline_draft();
	}

	void workbench_window::set_comment_draft_unavailable(QString aReason)
	{
		CommentDraftUnavailable = std::move(aReason);
		update_comment_target();
	}

	void workbench_window::set_pending_comment_delete_dispatch(std::function<void(core::review_draft_id, std::uint64_t)> aDispatch)
	{
		PendingCommentDeleteDispatch = std::move(aDispatch);
	}

	void workbench_window::set_carried_suggestion_dispatch(std::function<void(core::review_draft_id, std::uint64_t)> aDispatch)
	{
		CarriedSuggestionDispatch = std::move(aDispatch);
	}

	void workbench_window::present_moved_suggestions(std::vector<moved_suggestion_card> aMoved)
	{
		MovedSuggestions = aMoved;
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			ConversationPanel->cards()->set_moved_suggestions(std::move(aMoved));
			apply_thread_marks();
		}
	}

	void workbench_window::carry_moved_suggestion(const std::string& aIdentity)
	{
		// [[BPR-1733]]. Suggest change on the lines the core carried it to,
		// as a hand would choose them; the draft opened there takes its body.
		const auto        moved  = std::ranges::find_if(MovedSuggestions, [&aIdentity](const moved_suggestion_card& aMoved) {
			return conversation_cards_view::moved_suggestion_identity(aMoved.Draft.Id) == aIdentity;
		});
		const auto* const result = ComparisonSurface != nullptr ? ComparisonSurface->current_result().get() : nullptr;
		if (moved == MovedSuggestions.end() || !moved->Move.Carry.has_value() || result == nullptr || result->Alignment.Digest != moved->Comparison) {
			statusBar()->showMessage(tr("This suggestion cannot be carried now. Copy the change to write it again."), 10000);
			return;
		}
		const auto [first, last] = *moved->Move.Carry;
		const auto anchor        = core::source_line_ref{ .Comparison = moved->Comparison, .Side = core::diff_side::right, .Line = last };
		// The lines must be ones GitHub takes a suggestion on at this head too.
		if (const auto why = suggestion_range_reason(core::comment_range_decision{ .Anchor = anchor, .StartLine = first < last ? std::optional{ first } : std::nullopt }); !why.isEmpty()) {
			statusBar()->showMessage(why, 10000);
			return;
		}
		Carrying = carried_suggestion{
			.From      = moved->Draft.Id,
			.Revision  = moved->Revision,
			.Anchor    = anchor,
			.StartLine = first < last ? std::optional{ first } : std::nullopt,
			.Body      = moved->Draft.Body,
		};
		const auto selected = ComparisonSurface->selection().has_value() && ComparisonSurface->selection()->PreferredLine == anchor;
		if (!selected && !ComparisonSurface->select_conversation_line(core::diff_side::right, last)) {
			Carrying.reset();
			statusBar()->showMessage(tr("Select a line of this comparison."), 10000);
			return;
		}
		PendingLineComment = pending_line_comment{ .Anchor = anchor, .StartLine = Carrying->StartLine, .Suggestion = true };
		if (!resume_line_comment() && PendingLineComment.has_value()) {
			statusBar()->showMessage(tr("Opening the suggestion on its lines…"), line_comment_wait_ms);
			QTimer::singleShot(line_comment_wait_ms, this, [this, anchor] { report_line_comment_wait(anchor); });
		}
	}

	void workbench_window::write_carried_suggestion()
	{
		if (!Carrying.has_value() || Carrying->Written || ReviewEditor == nullptr || !PaneSuggestion.has_value() || PaneSuggestion->Anchor != Carrying->Anchor) {
			return;
		}
		Carrying->Written = true;
		// The body goes through the draft's own edit path, so the new draft
		// is saved as any typed one is; the edit then opens on its block.
		ReviewEditor->body_editor()->setPlainText(QString::fromStdString(Carrying->Body));
		static_cast<void>(begin_pane_suggestion(true));
		statusBar()->showMessage(tr("Carried to this head. The draft from the earlier head is deleted once this one is saved."), 6000);
	}

	void workbench_window::settle_carried_suggestion(const std::vector<core::review_comment_draft>& aDrafts)
	{
		if (!Carrying.has_value() || !Carrying->Written) {
			return;
		}
		const auto saved = std::ranges::any_of(aDrafts, [this](const core::review_comment_draft& aDraft) {
			return aDraft.Id != Carrying->From && aDraft.Anchor == Carrying->Anchor && aDraft.StartLine == Carrying->StartLine && aDraft.Body == Carrying->Body;
		});
		if (!saved) {
			return;
		}
		const auto carried = *std::exchange(Carrying, std::nullopt);
		if (CarriedSuggestionDispatch) {
			CarriedSuggestionDispatch(carried.From, carried.Revision);
		}
	}

	void workbench_window::confirm_delete_drawer_draft()
	{
		const auto* const body = ReviewEditor != nullptr ? ReviewEditor->body_editor() : nullptr;
		if (body == nullptr || body->toPlainText().trimmed().isEmpty()) {
			leave_drawer_draft(true);
			return;
		}
		if (DeleteDraftConfirmation != nullptr) {
			DeleteDraftConfirmation->close();
		}
		const auto& shown   = ReviewEditor->shown_draft();
		const auto  pending = shown.has_value() && core::pending_in_review(*shown);
		// [[BPR-1345]] v003. The shared question: the comment's place, then
		// the comment as this drawer reads it, in its box (owner,
		// delete-comment v002).
		auto* const box = new destructive_question{ *this, QStringLiteral("deleteDraftConfirmation"), pending ? tr("Delete this comment from your review?") : tr("Delete this comment?"),
													tr("This cannot be undone."), tr("Delete comment") };
		if (shown.has_value()) {
			const auto context = ComparisonSurface != nullptr ? ComparisonSurface->current_context() : std::nullopt;
			const auto path    = context.has_value() ? path_of(context->Key.File) : QString{ };
			auto*      cards   = ConversationPanel != nullptr ? ConversationPanel->cards() : nullptr;
			box->set_subject(path.section(QLatin1Char{ '/' }, -1), comment_lines(shown->Anchor.Side, shown->Anchor.Line, shown->StartLine), body->toPlainText().toStdString(),
							 "draft:" + std::to_string(shown->Id.value()), label_marks(), cards != nullptr ? cards->code_font() : font());
		}
		DeleteDraftConfirmation = box;
		const auto id           = shown.has_value() ? std::optional{ shown->Id } : std::nullopt;
		connect(box, &QDialog::accepted, this, [this, id] {
			// The draft asked about, and no other. [[BPR-1862]]: a confirmed
			// delete that cannot go ahead says so.
			const auto& now = ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::optional<core::review_comment_draft>{ };
			if (!now.has_value() || std::optional{ now->Id } != id) {
				present_draft_delete_refusal(tr("Not deleted: the comment changed while you were asked. Delete it again."));
				return;
			}
			if (!ReviewEditor->discard_action()->isEnabled()) {
				present_draft_delete_refusal(tr("Not deleted: this comment cannot be deleted now. %1").arg(ReviewEditor->discard_action()->toolTip()));
				return;
			}
			leave_drawer_draft(true);
		});
		box->open();
	}

	void workbench_window::reveal_pending_comment(const QString& aPath, const core::diff_side aSide, const std::uint32_t aLine, const std::optional<core::review_draft_id> aDraft,
												  const core::workbench_thread_destination aDestination)
	{
		if (CompositionPage != nullptr) {
			close_review_composition();
		}
		PendingCommentOpen.reset();
		PendingCommentPick = pending_comment_pick{
			.File        = core::file_identity_of(aPath.toStdString()),
			.Side        = aSide,
			.Line        = aLine,
			.Before      = ComparisonSurface != nullptr && ComparisonSurface->current_result() != nullptr
				? std::optional{ ComparisonSurface->current_result()->Alignment.Digest }
				: std::nullopt,
			.Draft       = aDraft,
			.Destination = aDestination,
		};
		choose_home(false);
		choose_modality(review_modality::review);
		const auto context = ComparisonSurface != nullptr ? ComparisonSurface->current_context() : std::nullopt;
		if (!context.has_value() || context->Key.File != PendingCommentPick->File) {
			if (const auto file = file_for_path(aPath); file.has_value() && Dispatch) {
				Dispatch(core::select_workbench_file_intent{ .File = *file });
			}
		}
		resume_pending_comment_pick();
	}

	void workbench_window::open_pending_comment(const core::review_submission_comment& aComment, const core::workbench_thread_destination aDestination)
	{
		// [[BPR-1581]]. The one route: my draft on a file opens beside its
		// line, in its composer for Discuss, in its row for Threads.
		const auto route = core::route_workbench_thread(core::open_workbench_thread_intent{
			.Destination = aDestination,
			.Origin      = core::workbench_thread_origin::review,
			.Draft       = true,
			.OnFile      = !aComment.Path.empty(),
			.Placed      = true,
		});
		if (!route.Available) {
			statusBar()->showMessage(tr("This comment is not on a file, so it has no line to open at."));
			return;
		}
		// Finish your review gives way: the sheet closes, the expanded page is left.
		if (CompositionPage != nullptr && CompositionPage == ComposeReviewPage) {
			close_review_composition(false);
		}
		close_review_sheet();
		reveal_pending_comment(QString::fromStdString(aComment.Path), aComment.Side, aComment.Line, aComment.Draft, aDestination);
	}

	void workbench_window::finish_pending_comment_pick(const pending_comment_pick& aPick)
	{
		if (!aPick.Draft.has_value() || ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		auto* const cards = ConversationPanel->cards();
		// The draft opens once the drawer holds it: choosing its line brings
		// a saved draft there, so until then it waits.
		const auto card = std::string{ "draft:unpublished" };
		if (ReviewEditor == nullptr || !ReviewEditor->shown_draft().has_value() || ReviewEditor->shown_draft()->Id != *aPick.Draft) {
			const auto saved = card + ":" + std::to_string(aPick.Draft->value());
			if (std::ranges::any_of(cards->cards(), [&saved](const thread_card& aCard) { return aCard.Id == saved; })) {
				PendingCommentOpen = aPick;
			} else {
				statusBar()->showMessage(tr("The comment's line is shown; its note opens once its draft is read."));
			}
			return;
		}
		PendingCommentOpen.reset();
		switch (aPick.Destination) {
		case core::workbench_thread_destination::discuss:
			static_cast<void>(show_draft_in_discuss(card));
			return;
		case core::workbench_thread_destination::threads:
			refresh_cards();
			if (!show_thread_in_threads(card, std::string{ })) {
				statusBar()->showMessage(tr("Threads lists your draft once it is open beside its line."));
			}
			return;
		case core::workbench_thread_destination::review:
			break;
		}
		if (Dispatch && Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			Dispatch(core::set_workbench_drawer_intent{ .Drawer = core::workbench_drawer::conversations, .State = core::workbench_drawer_state::expanded });
		}
		if (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_comments_tab) {
			TrayTabs->setCurrentIndex(tray_comments_tab);
		}
		cards->open_thread(card);
		if (auto* const open = cards->open_card(); open != nullptr) {
			open->setFocus(Qt::OtherFocusReason);
		} else {
			cards->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::resume_pending_comment_open()
	{
		if (!PendingCommentOpen.has_value() || ReviewEditor == nullptr || ComparisonSurface == nullptr) {
			return;
		}
		// The reviewer went on to another file: the opening is dropped.
		if (const auto context = ComparisonSurface->current_context(); !context.has_value() || context->Key.File != PendingCommentOpen->File) {
			PendingCommentOpen.reset();
			return;
		}
		if (const auto& shown = ReviewEditor->shown_draft(); shown.has_value() && shown->Id == PendingCommentOpen->Draft) {
			const auto pick = *PendingCommentOpen;
			PendingCommentOpen.reset();
			finish_pending_comment_pick(pick);
		}
	}

	void workbench_window::resume_pending_comment_pick()
	{
		if (!PendingCommentPick.has_value() || ComparisonSurface == nullptr || (Modality != nullptr && Modality->current() != review_modality::review)) {
			return;
		}
		const auto context = ComparisonSurface->current_context();
		const auto digest  = ComparisonSurface->current_result() != nullptr ? std::optional{ ComparisonSurface->current_result()->Alignment.Digest } : std::nullopt;
		if (context.has_value() && context->Key.File == PendingCommentPick->File && digest.has_value()) {
			const auto pick = *PendingCommentPick;
			PendingCommentPick.reset();
			static_cast<void>(ComparisonSurface->select_conversation_line(pick.Side, pick.Line));
			finish_pending_comment_pick(pick);
			return;
		}
		// Another file's comparison arrived first: the reviewer went on.
		if (context.has_value() && context->Key.File != PendingCommentPick->File && digest.has_value() && digest != PendingCommentPick->Before) {
			PendingCommentPick.reset();
		}
	}

	auto workbench_window::review_chip() const noexcept -> QToolButton*
	{
		return ReviewChip;
	}

	auto workbench_window::review_chip_action() const noexcept -> QAction*
	{
		return ReviewChipAction;
	}

	void workbench_window::set_start_review_dispatch(std::function<bool()> aDispatch)
	{
		StartReviewDispatch = std::move(aDispatch);
	}

	void workbench_window::set_unsent_reply_dispatch(std::function<void(core::review_draft_id, unsent_reply_action)> aDispatch)
	{
		UnsentReplyDispatch = std::move(aDispatch);
	}

	void workbench_window::act_on_kept_reply(const card_action aAction, const std::string& aIdentity)
	{
		// [[BPR-1374]]. The kept reply the note names, and no other.
		const auto& cards = ConversationPanel->cards()->cards();
		const auto  card  = std::ranges::find(cards, aIdentity, &thread_card::Id);
		if (card == cards.end() || !card->Draft || !card->UnsentReply.has_value() || !UnsentReplyDispatch) {
			return;
		}
		const auto prefix = conversation_cards_view::kept_reply_identity(core::review_draft_id{ 0U });
		const auto stem   = std::string_view{ prefix }.substr(0U, prefix.size() - 1U);
		auto       value  = std::uint64_t{ 0U };
		const auto digits = std::string_view{ aIdentity }.substr(std::min(stem.size(), aIdentity.size()));
		if (!aIdentity.starts_with(stem) || std::from_chars(digits.data(), digits.data() + digits.size(), value).ec != std::errc{ } || value == 0U) {
			return;
		}
		const auto draft = core::review_draft_id{ value };
		if (aAction == card_action::keep_reply_as_comment) {
			UnsentReplyDispatch(draft, unsent_reply_action::keep_as_comment);
			return;
		}
		if (DiscardReplyConfirmation != nullptr) {
			DiscardReplyConfirmation->close();
		}
		auto* const box = new destructive_question{ *this, QStringLiteral("discardReplyConfirmation"), tr("Discard this reply?"),
													tr("It was never sent, so GitHub does not have these words. This cannot be undone."), tr("Discard reply") };
		if (!card->Posts.empty() && card->Line.has_value()) {
			const auto context = ComparisonSurface != nullptr ? ComparisonSurface->current_context() : std::nullopt;
			const auto path    = context.has_value() ? path_of(context->Key.File) : QString{ };
			box->set_subject(path.section(QLatin1Char{ '/' }, -1), comment_lines(card->Side.value_or(core::diff_side::right), *card->Line - 1U, std::nullopt),
							 card->Posts.front().Body.toStdString(), aIdentity, label_marks(), ConversationPanel->cards()->code_font());
		}
		DiscardReplyConfirmation = box;
		connect(box, &QDialog::accepted, this, [this, draft] {
			if (UnsentReplyDispatch) {
				UnsentReplyDispatch(draft, unsent_reply_action::discard);
			}
		});
		box->open();
	}

	auto workbench_window::overview_review_key() const -> QString
	{
		const auto* const page = OverviewPage;
		if (page == nullptr || !page->projection()) {
			return { };
		}
		const auto& shown = *page->projection();
		return QStringLiteral("%1/%2#%3").arg(QString::fromStdString(shown.Owner), QString::fromStdString(shown.RepositoryName), QString::number(shown.Number.value()));
	}

	void workbench_window::enter_review_at(const QString& aPath)
	{
		// [[BPR-1588]]. A file the reviewer picked is never replaced by the
		// first unseen change.
		ReviewEnteredFor = overview_review_key();
		// [[BPR-1348]]. Review first, then the file, as a conversation pick
		// does. Choosing Review schedules its active tab's presentation
		// ([[BPR-1132]]), so a file chosen before it lost to the tab's file:
		// a double-click on main.cpp opened renderer.cpp.
		choose_modality(review_modality::review);
		PendingReviewPath.clear();
		if (const auto file = file_for_path(aPath); file.has_value() && Dispatch) {
			Dispatch(core::select_workbench_file_intent{ .File = *file });
		}
	}

	void workbench_window::open_pending_review_path()
	{
		// The reviewer may have gone elsewhere since: only an Overview still
		// shown for the review opens the file it was asked for.
		if (PendingReviewPath.isEmpty() || !HasGithubPullRequest || !showing_overview()) {
			return;
		}
		if (file_for_path(PendingReviewPath).has_value()) {
			const auto path = std::exchange(PendingReviewPath, QString{ });
			enter_review_at(path);
		}
	}

	auto workbench_window::thread_reply_anchor_of(const std::string& aIdentity) const -> thread_reply_anchor
	{
		auto anchor = thread_reply_anchor{ };
		if (ConversationPanel == nullptr) {
			return anchor;
		}
		// The thread's anchor, from the panel's list when it holds the thread.
		// [[BPR-1133]]. Otherwise from its card: while a line is selected the
		// list holds only that line's threads, and a card beside the code can
		// answer a thread on another line.
		auto root = ConversationPanel->model()->index_for_id(aIdentity);
		while (root.isValid() && root.parent().isValid()) {
			root = root.parent();
		}
		if (root.isValid()) {
			anchor.Known   = true;
			anchor.Current = root.data(github_conversation_model::anchor_availability_role).toInt()
				== static_cast<int>(application::github_conversation_anchor_availability::current);
			anchor.Path          = root.data(github_conversation_model::path_role).toString();
			anchor.Outdated      = root.data(github_conversation_model::outdated_role).toBool();
			auto       validLine = false;
			const auto line      = root.data(github_conversation_model::line_role).toULongLong(&validLine);
			anchor.Line          = validLine ? line : 0U;
			if (const auto sideValue = root.data(github_conversation_model::diff_side_role); sideValue.isValid()) {
				anchor.Side = github_conversation_source_side(sideValue.toInt());
			}
		} else if (ConversationPanel->cards() != nullptr) {
			const auto& cards = ConversationPanel->cards()->cards();
			const auto  card  = std::ranges::find(cards, aIdentity, &thread_card::Id);
			if (card != cards.end() && !card->Draft) {
				anchor.Known    = true;
				anchor.Current  = !card->AnchorUnavailable;
				anchor.Outdated = card->ReplyOnThread;
				anchor.Path     = card->Path;
				anchor.Side     = card->Side;
				anchor.Line     = card->Line.value_or(0U);
			}
		}
		return anchor;
	}

	auto workbench_window::select_thread_line(const std::string& aIdentity) -> bool
	{
		if (ConversationPanel == nullptr || ComparisonSurface == nullptr) {
			return false;
		}
		const auto anchor   = thread_reply_anchor_of(aIdentity);
		const auto current  = anchor.Current;
		const auto path     = anchor.Path;
		const auto side     = anchor.Side;
		const auto oneBased = anchor.Line;
		const auto context  = ComparisonSurface->current_context();
		if (!current || !context.has_value() || path.isEmpty()
			|| context->Key.File != core::file_identity_of(path.toStdString())) {
			return false;
		}
		if (oneBased == 0U || oneBased > std::numeric_limits<std::uint32_t>::max()) {
			return false;
		}
		if (!side.has_value()
			|| (*side == core::diff_side::left
					? !LeftConversationEndpointCurrent
					: !RightConversationEndpointCurrent)) {
			return false;
		}
		return ComparisonSurface->select_conversation_line(*side, static_cast<std::uint32_t>(oneBased - 1U));
	}

	auto workbench_window::thread_comparison_loading(const std::string& aIdentity) const -> bool
	{
		if (ConversationPanel == nullptr || ComparisonSurface == nullptr
			|| ComparisonSurface->current_result() != nullptr) {
			return false;
		}
		auto root = ConversationPanel->model()->index_for_id(aIdentity);
		while (root.isValid() && root.parent().isValid()) {
			root = root.parent();
		}
		const auto context = ComparisonSurface->current_context();
		const auto path    = root.isValid()
			? root.data(github_conversation_model::path_role).toString()
			: QString{ };
		return root.isValid()
			&& root.data(github_conversation_model::anchor_availability_role).toInt()
			== static_cast<int>(application::github_conversation_anchor_availability::current)
			&& context.has_value() && !path.isEmpty()
			&& context->Key.File == core::file_identity_of(path.toStdString());
	}

	void workbench_window::begin_thread_reply(const std::string& aIdentity, const bool aInPlace)
	{
		clear_reply_refusal();
		// [[BPR-1746]]. A thread GitHub says this viewer cannot reply
		// in is refused here, before anything is written, with GitHub's
		// reason in plain words.
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			const auto& cards = ConversationPanel->cards()->cards();
			if (const auto card = std::ranges::find(cards, aIdentity, &thread_card::Id); card != cards.end() && card->ReplyRefusal != core::thread_reply_refusal::none) {
				ReplyRefusalShown = true;
				ConversationPanel->present_notice(thread_reply_refusal_text(card->ReplyRefusal));
				return;
			}
		}
		// [[BPR-1290]]. Beside the code the reply is written in place, at the
		// foot of the open thread; it is the same draft, and nothing publishes
		// but its Publish.
		const auto inPlace = [this, &aIdentity, aInPlace] {
			const auto discuss = Modality != nullptr && (Modality->current() == review_modality::discuss || Modality->current() == review_modality::threads);
			return aInPlace && !discuss && Drawers[1].Panel->state() == core::workbench_drawer_state::expanded
				&& TrayTabs != nullptr && TrayTabs->currentIndex() == tray_comments_tab
				&& std::ranges::any_of(ConversationPanel->cards()->cards(), [&aIdentity](const thread_card& aCard) { return aCard.Id == aIdentity; });
		};
		// [[BPR-1745]]. The owner, 2026-10-22: "Allow replying to an
		// outdated thread from the drawer, like GitHub does". The core decides
		// the route from GitHub's thread data; an outdated thread is answered
		// by the thread, with no line to select, in Review, Discuss and
		// Threads alike.
		const auto anchor   = thread_reply_anchor_of(aIdentity);
		const auto decision = core::decide_thread_reply({ .ReviewThread = anchor.Known && !anchor.Path.isEmpty(), .Outdated = anchor.Outdated });
		if (decision.Route == core::thread_reply_route::on_thread && ThreadReplyDispatch && ReviewEditor != nullptr) {
			ThreadReplyDispatch(aIdentity);
			update_comment_target();
			if (inPlace()) {
				ConversationPanel->cards()->begin_reply(aIdentity);
				present_inline_reply();
				return;
			}
			open_review_composition(true);
			return;
		}
		if (select_thread_line(aIdentity) && ReviewReplyDispatch && ReviewEditor != nullptr) {
			ReviewReplyDispatch(aIdentity);
			update_comment_target();
			if (inPlace()) {
				ConversationPanel->cards()->begin_reply(aIdentity);
				present_inline_reply();
				return;
			}
			if (CommentAction->isEnabled()) {
				open_review_composition(true);
			} else {
				ConversationPanel->present_notice(CommentAction->toolTip());
			}
			return;
		}
		refuse_thread_reply(aIdentity);
	}

	void workbench_window::refuse_thread_reply(const std::string& aIdentity)
	{
		if (ConversationPanel == nullptr) {
			return;
		}
		// [[BPR-1740]]. The owner, 2026-10-22, of "Open this thread in
		// code before replying": "I also have no idea what that means I'm on
		// the code". The words say what is true of this thread, and a thread
		// on a file that is not in the code gets the click that shows it.
		const auto anchor  = thread_reply_anchor_of(aIdentity);
		const auto context = ComparisonSurface != nullptr ? ComparisonSurface->current_context() : std::nullopt;
		const auto shown   = context.has_value() && !anchor.Path.isEmpty()
			&& context->Key.File == core::file_identity_of(anchor.Path.toStdString());
		ReplyRefusalShown = true;
		if (anchor.Known && anchor.Current && !anchor.Path.isEmpty() && !shown) {
			const auto origin = Modality != nullptr && Modality->current() == review_modality::discuss
				? core::workbench_thread_origin::discuss
				: core::workbench_thread_origin::review;
			ConversationPanel->present_notice(
				tr("This thread is on %1, which isn't the file in the code.").arg(anchor.Path.section(QLatin1Char{ '/' }, -1)),
				tr("Show in code"),
				[this, aIdentity, origin] {
					clear_reply_refusal();
					static_cast<void>(move_thread(aIdentity, origin, core::workbench_thread_destination::review));
				});
			return;
		}
		if (anchor.Known && anchor.Current && anchor.Side.has_value() && anchor.Line > 0U) {
			if (*anchor.Side == core::diff_side::left && !LeftConversationEndpointCurrent) {
				ConversationPanel->present_notice(tr("This thread is on the pull request's base, and this comparison starts from another commit."));
				return;
			}
			if (*anchor.Side == core::diff_side::right && !RightConversationEndpointCurrent) {
				ConversationPanel->present_notice(tr("This thread is on the pull request's latest commit, and this comparison ends at another one."));
				return;
			}
		}
		ConversationPanel->present_notice(tr("This thread's line isn't in the code shown, so it can't take a reply here."));
	}

	void workbench_window::clear_reply_refusal()
	{
		if (!ReplyRefusalShown) {
			return;
		}
		ReplyRefusalShown = false;
		if (ConversationPanel != nullptr) {
			ConversationPanel->present_notice(QString{ });
		}
	}

	void workbench_window::resume_deferred_reply()
	{
		// The first comparison to arrive decides: the thread's own completes
		// the reply, any other refuses it, and nothing waits past it.
		if (!DeferredReply.has_value() || ComparisonSurface == nullptr
			|| ComparisonSurface->current_result() == nullptr) {
			return;
		}
		const auto identity = std::exchange(DeferredReply, std::nullopt);
		begin_thread_reply(*identity, DeferredReplyInPlace);
	}

	void workbench_window::set_review_reply_dispatch(workbench_review_reply_dispatch aDispatch)
	{
		ReviewReplyDispatch = std::move(aDispatch);
	}

	void workbench_window::set_thread_reply_dispatch(workbench_review_reply_dispatch aDispatch)
	{
		ThreadReplyDispatch = std::move(aDispatch);
	}

	void workbench_window::set_thread_resolution_dispatch(workbench_thread_resolution_dispatch aDispatch)
	{
		ThreadResolutionDispatch = std::move(aDispatch);
	}

	void workbench_window::set_reaction_dispatch(workbench_reaction_dispatch aDispatch)
	{
		ReactionDispatch = std::move(aDispatch);
	}

	void workbench_window::present_conversation_state(std::map<std::string, core::reaction_summary> aReactions, std::map<std::string, thread_resolution_presentation> aResolutions)
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		// [[BPR-1841]]. The thread list's row and the Threads view show the
		// same projected resolution as the cards, changed in place.
		auto resolved = std::map<std::string, bool>{ };
		for (const auto& [thread, presentation] : aResolutions) {
			resolved.emplace_hint(resolved.end(), thread, presentation.Resolved);
		}
		ConversationPanel->cards()->present_reactions(std::move(aReactions));
		ConversationPanel->cards()->present_thread_resolutions(std::move(aResolutions));
		if (Finder != nullptr) {
			Finder->present_thread_resolutions(resolved);
		}
		ConversationPanel->present_thread_resolutions(std::move(resolved));
	}

	void workbench_window::set_thread_resolution_provider(const bool aResolves)
	{
		set_thread_resolution_connection(core::thread_resolution_refusal::none, aResolves);
	}

	void workbench_window::set_thread_resolution_connection(const core::thread_resolution_refusal aRefusal, const bool aResolves)
	{
		ThreadResolutionConnection = aRefusal;
		ProviderResolvesThreads    = aRefusal == core::thread_resolution_refusal::none && aResolves;
		present_thread_resolution_offer();
	}

	// [[BPR-1824]]. The cards are told why Resolve cannot go, not only that
	// it cannot: the composition's reason first, then the server's.
	void workbench_window::present_thread_resolution_offer()
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		const auto serverResolves = Capabilities.ResolveThread == core::github_capability_state::supported
			|| Capabilities.UnresolveThread == core::github_capability_state::supported;
		const auto refusal = ThreadResolutionConnection != core::thread_resolution_refusal::none ? ThreadResolutionConnection
			: !ProviderResolvesThreads || !serverResolves                                        ? core::thread_resolution_refusal::capability_absent
																								 : core::thread_resolution_refusal::none;
		ConversationPanel->cards()->set_resolution_refusal(refusal);
	}

	void workbench_window::set_behaviour_dispatch(review_behaviour_dispatch aDispatch)
	{
		BehaviourDispatch = std::move(aDispatch);
	}

	auto workbench_window::behaviour() const noexcept -> const application::review_behaviour_settings&
	{
		return Behaviour;
	}

	auto workbench_window::behaviour_editor() const noexcept -> QWidget*
	{
		return BehaviourEditor;
	}

	auto workbench_window::feedback_marks_preview() const noexcept -> feedback_mark_preview*
	{
		return FeedbackMarksPreview;
	}

	auto workbench_window::label_helper_switch() const noexcept -> QCheckBox* { return LabelHelperSwitch; }

	auto workbench_window::label_columns_editor() const noexcept -> feedback_label_columns_editor* { return LabelColumnsEditor; }

	auto workbench_window::label_columns_recovery() const noexcept -> QLabel* { return LabelColumnsRecovery; }

	auto workbench_window::mark_colours_link() const noexcept -> QToolButton* { return MarkColoursLink; }

	auto workbench_window::workshop_guidance_link() const noexcept -> QToolButton* { return WorkshopGuidanceLink; }

	auto workbench_window::conventional_comments_caption() const noexcept -> QLabel* { return ConventionalCommentsCaption; }

	auto workbench_window::conventional_comments_link() const noexcept -> QToolButton* { return ConventionalCommentsLink; }

	auto workbench_window::conventional_comments_status() const noexcept -> QLabel* { return ConventionalCommentsStatus; }

	// [[BPR-0661]]. Two groups of segmented rows: the modality rules (which
	// modality a pull request opens in; which one Continue review enters) and
	// the reading behaviour (when a file is marked read; the comparison on
	// return; the comments drawer's first filter). Each group resets on its
	// own. A row dispatches the whole record; the host validates, persists and
	// publishes it, and the page follows what is published, so a refused
	// record reverts.
	void workbench_window::build_behaviour_editor(QWidget* const aReviewPage, QVBoxLayout* const aReviewColumn, QWidget* const aCommentsPage, QVBoxLayout* const aCommentsColumn)
	{
		BehaviourEditor = new QWidget{ aReviewPage };
		BehaviourEditor->setObjectName(QStringLiteral("reviewBehaviourEditor"));
		BehaviourEditor->setAccessibleName(QStringLiteral("Review behaviour"));
		auto* const column = new QVBoxLayout{ BehaviourEditor };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(6);
		// [[BPR-1597]]. Settings › Comments: the comment rows, on their own page.
		CommentsEditor = new QWidget{ aCommentsPage };
		CommentsEditor->setObjectName(QStringLiteral("commentsBehaviourEditor"));
		CommentsEditor->setAccessibleName(QStringLiteral("Comments"));
		auto* const commentsColumn = new QVBoxLayout{ CommentsEditor };
		commentsColumn->setContentsMargins(0, 0, 0, 0);
		commentsColumn->setSpacing(6);

		struct option
		{
			QString Label;
			QString Name;
		};
		struct row
		{
			std::size_t         Index;
			QString             Label;
			QString             Name;
			std::vector<option> Options;
			/// Drawn after the choices, as the Label marks preview is.
			QWidget* Trailing = nullptr;
			/// [[BPR-1088]]. A row that is one control rather than a choice
			/// of buttons: a switch, a link or a list.
			QWidget* Control = nullptr;
			/// The space before `Trailing`: wide for a preview, narrow for an
			/// info glyph.
			int TrailingSpacing = 12;
			/// The row's name at the top of a tall control.
			bool Top = false;
			/// [[BPR-1566]]. A line of its own under the heading, with no
			/// name column: `Control` takes the whole width.
			bool Wide = false;
		};
		const auto addGroup = [this](QVBoxLayout* const      aColumn,
									 const QString&          aHeading,
									 const QString&          aResetName,
									 const std::vector<row>& aRows,
									 auto                    aReset) {
			auto* const headingRow = new QHBoxLayout{ };
			auto* const heading    = new QLabel{ aHeading, BehaviourEditor };
			heading->setObjectName(aResetName + QStringLiteral("Heading"));
			headingRow->addWidget(heading, 1);
			auto* const reset = new quiet_tool_button{ BehaviourEditor };
			reset->setObjectName(aResetName);
			reset->setText(QStringLiteral("Reset"));
			reset->setAccessibleName(QStringLiteral("Reset %1").arg(aHeading.toLower()));
			reset->setAccessibleDescription(
				QStringLiteral("Returns this group to its defaults; the other group is left alone"));
			reset->setFocusPolicy(Qt::StrongFocus);
			connect(reset, &QToolButton::clicked, this, [this, aReset] {
				if (!BehaviourDispatch) {
					return;
				}
				auto next = behaviour_from_controls();
				aReset(next);
				AskedBehaviour = next;
				BehaviourDispatch(next);
			});
			headingRow->addWidget(reset);
			aColumn->addLayout(headingRow);
			for (const auto& one : aRows) {
				if (one.Wide && one.Control != nullptr) {
					aColumn->addWidget(one.Control);
					continue;
				}
				auto* const line  = new QHBoxLayout{ };
				auto* const label = new QLabel{ one.Label, BehaviourEditor };
				label->setObjectName(one.Name + QStringLiteral("Label"));
				label->setMinimumWidth(180);
				line->addWidget(label, 0, one.Top ? Qt::AlignTop : Qt::Alignment{ });
				if (one.Control != nullptr) {
					// [[BPR-1597]]. A control made before its group's heading
					// follows it in the page's child order, which is the
					// order Settings search reads headings and rows in.
					one.Control->raise();
					if (one.Trailing != nullptr) {
						one.Trailing->raise();
					}
					label->setBuddy(one.Control);
					line->addWidget(one.Control, 0, one.Top ? Qt::AlignTop : Qt::Alignment{ });
					if (one.Trailing != nullptr) {
						line->addSpacing(one.TrailingSpacing);
						line->addWidget(one.Trailing, 0, Qt::AlignVCenter);
					}
					line->addStretch(1);
					aColumn->addLayout(line);
					continue;
				}
				auto& buttons = BehaviourOptions[one.Index];
				// One exclusive group per row: autoExclusive would make every
				// button under the editor one group.
				auto* const group = new QButtonGroup{ BehaviourEditor };
				group->setExclusive(true);
				for (const auto& choice : one.Options) {
					auto* const button = new quiet_tool_button{ BehaviourEditor };
					button->setObjectName(choice.Name);
					button->setText(choice.Label);
					button->setAccessibleName(one.Label + QStringLiteral(": ") + choice.Label);
					button->setCheckable(true);
					group->addButton(button);
					button->setFocusPolicy(Qt::StrongFocus);
					connect(button, &QToolButton::toggled, this, [this](const bool aChecked) {
						if (aChecked && !ApplyingBehaviour) {
							dispatch_behaviour_from_controls();
						}
					});
					line->addWidget(button);
					buttons.push_back(button);
				}
				if (one.Trailing != nullptr) {
					line->addSpacing(one.TrailingSpacing);
					line->addWidget(one.Trailing, 0, Qt::AlignVCenter);
				}
				line->addStretch(1);
				aColumn->addLayout(line);
			}
			return reset;
		};
		// [[BPR-1597]]. Settings › Review, by task (owner's pick A,
		// 2026-10-17): how a pull request opens, how it reads, and its layout.
		// Each group's Reset covers its own rows.
		addGroup(
			column,
			QStringLiteral("Opening"),
			QStringLiteral("resetOpening"),
			{
				row{ 0U,
					 QStringLiteral("A pull request opens in"),
					 QStringLiteral("openModality"),
					 { { QStringLiteral("Overview"), QStringLiteral("openModalityOverview") },
					   { QStringLiteral("Review"), QStringLiteral("openModalityReview") },
					   { QStringLiteral("Last used"), QStringLiteral("openModalityLastUsed") } } },
				// [[BPR-1295]]. Row 1, "Continue review enters", went with
				// Read: Start and Continue review both enter Review.
				row{ 3U,
					 QStringLiteral("On return, compare"),
					 QStringLiteral("returnComparison"),
					 { { QStringLiteral("Whole PR"), QStringLiteral("returnComparisonWholePr") },
					   { QStringLiteral("Since my last review"), QStringLiteral("returnComparisonSinceLastReview") },
					   { QStringLiteral("Since last upload"), QStringLiteral("returnComparisonSinceLastUpload") } } },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.OpenModality     = application::review_behaviour_settings{ }.OpenModality;
				aNext.ReturnComparison = application::review_behaviour_settings{ }.ReturnComparison;
			});
		// [[BPR-1584]]. The owner, 2026-10-17: "We can make that capped width
		// a setting." Threads and Discuss only; never the code comparison.
		// [[BPR-1597]]: a row of Review › Reading.
		auto* const readingInfo = new info_glyph{ QStringLiteral("Threads and Discuss stop at this width on a wide screen. The code comparison always uses the whole window."), BehaviourEditor };
		readingInfo->setObjectName(QStringLiteral("readingWidthInfo"));
		addGroup(
			column,
			QStringLiteral("Reading"),
			QStringLiteral("resetReadingBehaviour"),
			{
				row{ 2U,
					 QStringLiteral("Mark a file read"),
					 QStringLiteral("markRead"),
					 { { QStringLiteral("By hand"), QStringLiteral("markReadByHand") },
					   { QStringLiteral("When fully seen"), QStringLiteral("markReadWhenFullySeen") } } },
				row{ 4U,
					 QStringLiteral("Comments drawer opens on"),
					 QStringLiteral("commentsFilter"),
					 { { QStringLiteral("All"), QStringLiteral("commentsFilterAll") },
					   { QStringLiteral("Unread"), QStringLiteral("commentsFilterUnread") },
					   { QStringLiteral("Unresolved"), QStringLiteral("commentsFilterUnresolved") } } },
				row{ 10U,
					 QStringLiteral("Reading width"),
					 QStringLiteral("readingWidth"),
					 { { QStringLiteral("Narrow"), QStringLiteral("readingWidthNarrow") },
					   { QStringLiteral("Medium"), QStringLiteral("readingWidthMedium") },
					   { QStringLiteral("Wide"), QStringLiteral("readingWidthWide") },
					   { QStringLiteral("Full width"), QStringLiteral("readingWidthFull") } },
					 readingInfo,
					 nullptr,
					 4 },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.MarkRead       = application::review_behaviour_settings{ }.MarkRead;
				aNext.CommentsFilter = application::review_behaviour_settings{ }.CommentsFilter;
				aNext.ReadingWidth   = core::default_reading_width;
			});
		// [[BPR-1816]]. The owner, 2026-10-29: "Finishing a review should take
		// you to Home and close that review", a setting per outcome, on by
		// default. Request changes has its own box as well: each finish
		// button is its own gesture. One line reads as a sentence, its boxes
		// named as the buttons are.
		auto* const finishRow    = new QWidget{ BehaviourEditor };
		auto* const finishLayout = new QHBoxLayout{ finishRow };
		finishLayout->setContentsMargins(0, 0, 0, 0);
		finishLayout->setSpacing(4);
		auto* const finishBoxes = new QHBoxLayout{ };
		finishBoxes->setSpacing(16);
		finishLayout->addLayout(finishBoxes);
		{
			const auto verdicts = std::array{ std::pair{ QStringLiteral("Comment"), QStringLiteral("finishReviewComment") },
											  std::pair{ QStringLiteral("Approve"), QStringLiteral("finishReviewApprove") },
											  std::pair{ QStringLiteral("Request changes"), QStringLiteral("finishReviewRequestChanges") } };
			for (std::size_t index = 0U; index < verdicts.size(); ++index) {
				auto* const box = new QCheckBox{ verdicts[index].first, finishRow };
				box->setObjectName(verdicts[index].second);
				box->setAccessibleName(QStringLiteral("Close the review and go Home after %1").arg(verdicts[index].first));
				box->setFocusPolicy(Qt::StrongFocus);
				connect(box, &QCheckBox::toggled, this, [this] {
					if (!ApplyingBehaviour) {
						dispatch_behaviour_from_controls();
					}
				});
				finishBoxes->addWidget(box);
				FinishReviewSwitches[index] = box;
			}
		}
		auto* const finishInfo = new info_glyph{ QStringLiteral("Once GitHub confirms a review you finished with a ticked verdict, the review closes and Home shows. Unticked, you stay on the pull request. A review that isn't published stays open, with its summary and comments."), finishRow };
		finishInfo->setObjectName(QStringLiteral("finishReviewInfo"));
		finishLayout->addWidget(finishInfo);
		for (auto* const box : FinishReviewSwitches) {
			box->setAccessibleDescription(finishInfo->consequence());
		}
		addGroup(
			column,
			QStringLiteral("Finishing a review"),
			QStringLiteral("resetFinishReview"),
			{
				row{ .Index = 0U, .Label = QStringLiteral("Close and go Home after"), .Name = QStringLiteral("finishReview"), .Options = { }, .Control = finishRow },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.FinishReview = core::review_finish_settings{ };
			});
		// [[BPR-0721]]. The owner found the collapsed rails and the Hide
		// buttons took room the strip's toggles already cover, and asked for
		// a setting rather than their removal. Off is the default.
		CenterRibbonWidth = new QSpinBox{ BehaviourEditor };
		CenterRibbonWidth->setObjectName(QStringLiteral("centerRibbonWidth"));
		CenterRibbonWidth->setAccessibleName(QStringLiteral("Center ribbon width"));
		CenterRibbonWidth->setAccessibleDescription(QStringLiteral("Width between the two source panes, in logical pixels"));
		CenterRibbonWidth->setRange(static_cast<int>(application::minimum_center_ribbon_width), static_cast<int>(application::maximum_center_ribbon_width));
		CenterRibbonWidth->setSuffix(QStringLiteral(" px"));
		CenterRibbonWidth->setKeyboardTracking(false);
		connect(CenterRibbonWidth, &QSpinBox::valueChanged, this, [this] {
			if (!ApplyingBehaviour) {
				dispatch_behaviour_from_controls();
			}
		});
		// [[BPR-1334]]. The owner, 2026-10-08: "an optional on by default view
		// page below the side by side". The same choice as View > Line Details.
		// [[BPR-0721]] drawer controls and the comparison's line details and
		// ribbon are one Layout group since [[BPR-1597]].
		addGroup(
			column,
			QStringLiteral("Layout"),
			QStringLiteral("resetLayout"),
			{
				row{ 5U,
					 QStringLiteral("Drawer controls"),
					 QStringLiteral("drawerControls"),
					 { { QStringLiteral("Hidden"), QStringLiteral("drawerControlsHidden") },
					   { QStringLiteral("Shown"), QStringLiteral("drawerControlsShown") } } },
				row{ 8U,
					 QStringLiteral("Line details"),
					 QStringLiteral("lineDetails"),
					 { { QStringLiteral("Shown"), QStringLiteral("lineDetailsShown") },
					   { QStringLiteral("Hidden"), QStringLiteral("lineDetailsHidden") } } },
				row{ 0U, QStringLiteral("Center ribbon width"), QStringLiteral("centerRibbonWidth"), { }, nullptr, CenterRibbonWidth },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.DrawerControls    = application::review_behaviour_settings{ }.DrawerControls;
				aNext.LineDetails       = application::review_behaviour_settings{ }.LineDetails;
				aNext.CenterRibbonWidth = application::default_center_ribbon_width;
			});
		// RFC-BPR-0084 Q3. The owner, 2026-10-16, accepted a "Load images in
		// descriptions and comments" preference, on by default.
		auto* const imagesInfo = new info_glyph{ QStringLiteral("Images in pull request descriptions and comments load as they do on GitHub. Don't load: each stays a link with its description, and nothing is fetched."), BehaviourEditor };
		imagesInfo->setObjectName(QStringLiteral("markdownImagesInfo"));
		// [[BPR-1083]]. The owner, 2026-10-05: "i think settings should also
		// have a toggle to switch between the icons and the shortcode icons,
		// because i liked both of them." Icons are the default. The Comment
		// labels section's other rows are BPR-1088's. The owner, accepting it
		// (mark-style-v001): "it'd be nice to see a preview", so a few marks
		// in the published style sit beside the choices.
		FeedbackMarksPreview = new feedback_mark_preview{ BehaviourEditor };
		// [[BPR-1088]]. The rest of the section the owner accepted (BPR-1079
		// Settings v1, adjusted to Overview chips in v2): where the Overview's
		// counts start, Open by default; a link to the label tints; and the
		// Overview chips, ticked and put in order. Each row dispatches the
		// whole record and shows what the host publishes, so a refused or
		// failed save reverts here.
		//
		// [[BPR-1568]]. The label helper, which since [[BPR-1565]] only
		// decides whether a comment workshop request carries the Review
		// feedback guidance ([[BPR-1091]]), is the Comment workshop group's
		// own row below (owner, 2026-10-16: "Just give it it's own section
		// for comments or something."), with its own Reset and a link to
		// where the guidance text is edited. Same stored preference.
		LabelHelperSwitch = new QCheckBox{ QStringLiteral("Send with comment workshop requests"), BehaviourEditor };
		LabelHelperSwitch->setObjectName(QStringLiteral("labelHelperSwitch"));
		LabelHelperSwitch->setAccessibleName(QStringLiteral("Feedback guidance: send with comment workshop requests"));
		LabelHelperSwitch->setFocusPolicy(Qt::StrongFocus);
		connect(LabelHelperSwitch, &QCheckBox::toggled, this, [this] {
			if (!ApplyingBehaviour) {
				dispatch_behaviour_from_controls();
			}
		});
		auto* const helperRow    = new QWidget{ BehaviourEditor };
		auto* const helperLayout = new QHBoxLayout{ helperRow };
		helperLayout->setContentsMargins(0, 0, 0, 0);
		helperLayout->setSpacing(4);
		helperLayout->addWidget(LabelHelperSwitch);
		auto* const helperInfo = new info_glyph{ QStringLiteral("The Review feedback guidance tells the agent how comment labels and decorations work. On: each comment workshop request carries it. Off, the default: requests go without it. Labels count either way."), helperRow };
		helperInfo->setObjectName(QStringLiteral("labelHelperInfo"));
		helperLayout->addWidget(helperInfo);
		LabelHelperSwitch->setAccessibleDescription(helperInfo->consequence());
		// Where the guidance text lives, as Mark colours points at the tints.
		WorkshopGuidanceLink = new QToolButton{ BehaviourEditor };
		WorkshopGuidanceLink->setObjectName(QStringLiteral("workshopGuidanceLink"));
		// "&&" draws one ampersand: a single "&" would be a mnemonic.
		WorkshopGuidanceLink->setText(QStringLiteral("Agents && context › Review feedback ›"));
		WorkshopGuidanceLink->setAutoRaise(true);
		WorkshopGuidanceLink->setCursor(Qt::PointingHandCursor);
		WorkshopGuidanceLink->setFocusPolicy(Qt::StrongFocus);
		WorkshopGuidanceLink->setToolTip(QStringLiteral("Opens the Review feedback writing instructions, where the guidance text is edited"));
		WorkshopGuidanceLink->setAccessibleName(QStringLiteral("Guidance text: Agents & context, Review feedback"));
		WorkshopGuidanceLink->setAccessibleDescription(WorkshopGuidanceLink->toolTip());
		anchor_tooltips(*WorkshopGuidanceLink);
		{
			auto linkPalette = WorkshopGuidanceLink->palette();
			linkPalette.setColor(QPalette::ButtonText, readable_link_colour(linkPalette));
			WorkshopGuidanceLink->setPalette(linkPalette);
		}
		connect(WorkshopGuidanceLink, &QToolButton::clicked, this, [this] {
			if (SettingsWindow == nullptr) {
				return;
			}
			SettingsWindow->present(settings_page::agents_and_context);
			if (WritingHooks != nullptr) {
				WritingHooks->select_hook(core::writing_hook::feedback);
			}
		});
		auto* const countsInfo = new info_glyph{ QStringLiteral("Where the Overview's comment counts start. Open counts unresolved threads; All counts every one. Switching on an Overview changes only that view."), BehaviourEditor };
		countsInfo->setObjectName(QStringLiteral("overviewCountsInfo"));
		MarkColoursLink = new QToolButton{ BehaviourEditor };
		MarkColoursLink->setObjectName(QStringLiteral("markColoursLink"));
		MarkColoursLink->setText(QStringLiteral("Appearance › Colours ›"));
		MarkColoursLink->setAutoRaise(true);
		MarkColoursLink->setCursor(Qt::PointingHandCursor);
		MarkColoursLink->setFocusPolicy(Qt::StrongFocus);
		MarkColoursLink->setToolTip(QStringLiteral("Opens Appearance with the filter set to Label"));
		MarkColoursLink->setAccessibleName(QStringLiteral("Mark colours: Appearance, Colours"));
		MarkColoursLink->setAccessibleDescription(MarkColoursLink->toolTip());
		anchor_tooltips(*MarkColoursLink);
		{
			auto linkPalette = MarkColoursLink->palette();
			linkPalette.setColor(QPalette::ButtonText, readable_link_colour(linkPalette));
			MarkColoursLink->setPalette(linkPalette);
		}
		connect(MarkColoursLink, &QToolButton::clicked, this, [this] {
			if (SettingsWindow == nullptr) {
				return;
			}
			SettingsWindow->present(settings_page::appearance);
			if (AppearanceSettings != nullptr && AppearanceSettings->colours() != nullptr) {
				auto* const colours = AppearanceSettings->colours();
				if (colours->filter_field() != nullptr) {
					colours->filter_field()->setText(QStringLiteral("label"));
				}
				colours->apply_filter(QStringLiteral("label"));
			}
		});
		auto* const chipsColumn = new QWidget{ BehaviourEditor };
		auto* const chipsLayout = new QVBoxLayout{ chipsColumn };
		chipsLayout->setContentsMargins(0, 0, 0, 0);
		chipsLayout->setSpacing(6);
		auto* const chipsLine = new QHBoxLayout{ };
		chipsLine->setSpacing(4);
		LabelColumnsEditor = new feedback_label_columns_editor{ chipsColumn };
		LabelColumnsEditor->set_dispatch([this](const core::feedback_label_columns&) {
			if (!ApplyingBehaviour) {
				dispatch_behaviour_from_controls();
			}
		});
		chipsLine->addWidget(LabelColumnsEditor, 1);
		auto* const chipsInfo = new info_glyph{ QStringLiteral("Ticked labels get their own count in the Overview's Comments column, in this order. The speech bubble counts the rest. Replies don't count."), chipsColumn };
		chipsInfo->setObjectName(QStringLiteral("labelColumnsInfo"));
		chipsLine->addWidget(chipsInfo, 0, Qt::AlignTop);
		chipsLayout->addLayout(chipsLine);
		LabelColumnsRecovery = new QLabel{ chipsColumn };
		LabelColumnsRecovery->setObjectName(QStringLiteral("labelColumnsRecovery"));
		LabelColumnsRecovery->setWordWrap(true);
		LabelColumnsRecovery->setMaximumWidth(420);
		LabelColumnsRecovery->hide();
		chipsLayout->addWidget(LabelColumnsRecovery);
		// [[BPR-1566]]. The owner, 2026-10-16: "add a small thing to the
		// settings page [...] that links to [Conventional Comments] for
		// people who don't know where those come from or how to use them."
		// One quiet sentence under the heading and the same link the label
		// menu ends with. It names the page; the application opens it. Not a
		// preference: the section's Reset leaves it alone, and it is hidden
		// until the window is bound to the application.
		ConventionalCommentsRow     = new QWidget{ BehaviourEditor };
		auto* const conventionalRow = new QVBoxLayout{ ConventionalCommentsRow };
		conventionalRow->setContentsMargins(0, 0, 0, 2);
		conventionalRow->setSpacing(2);
		auto* const conventionalLine = new QHBoxLayout{ };
		conventionalLine->setSpacing(2);
		ConventionalCommentsCaption = new QLabel{ QStringLiteral("These labels follow a widely used format for review comments."), ConventionalCommentsRow };
		ConventionalCommentsCaption->setObjectName(QStringLiteral("conventionalCommentsCaption"));
		ConventionalCommentsCaption->setForegroundRole(QPalette::PlaceholderText);
		ConventionalCommentsCaption->setTextFormat(Qt::PlainText);
		conventionalLine->addWidget(ConventionalCommentsCaption);
		ConventionalCommentsLink = new QToolButton{ ConventionalCommentsRow };
		ConventionalCommentsLink->setObjectName(QStringLiteral("conventionalCommentsLink"));
		ConventionalCommentsLink->setText(QStringLiteral("About Conventional Comments"));
		ConventionalCommentsLink->setAutoRaise(true);
		ConventionalCommentsLink->setCursor(Qt::PointingHandCursor);
		ConventionalCommentsLink->setFocusPolicy(Qt::StrongFocus);
		ConventionalCommentsLink->setToolTip(QStringLiteral("Where the labels come from and how to use them.\nOpens conventionalcomments.org in your browser."));
		ConventionalCommentsLink->setAccessibleName(QStringLiteral("About Conventional Comments"));
		ConventionalCommentsLink->setAccessibleDescription(QStringLiteral("Where the comment labels come from and how to use them. Opens conventionalcomments.org in your browser."));
		anchor_tooltips(*ConventionalCommentsLink);
		{
			auto linkPalette = ConventionalCommentsLink->palette();
			linkPalette.setColor(QPalette::ButtonText, readable_link_colour(linkPalette));
			ConventionalCommentsLink->setPalette(linkPalette);
		}
		connect(ConventionalCommentsLink, &QToolButton::clicked, this, [this] {
			open_reference_page(core::reference_page::conventional_comments, true);
		});
		conventionalLine->addWidget(ConventionalCommentsLink);
		conventionalLine->addStretch(1);
		conventionalRow->addLayout(conventionalLine);
		ConventionalCommentsStatus = new QLabel{ ConventionalCommentsRow };
		ConventionalCommentsStatus->setObjectName(QStringLiteral("conventionalCommentsStatus"));
		ConventionalCommentsStatus->setTextFormat(Qt::PlainText);
		ConventionalCommentsStatus->setWordWrap(true);
		ConventionalCommentsStatus->hide();
		conventionalRow->addWidget(ConventionalCommentsStatus);
		ConventionalCommentsRow->setVisible(static_cast<bool>(ReferenceDispatch));
		auto* const commentLabelsReset = addGroup(
			commentsColumn,
			QStringLiteral("Labels"),
			QStringLiteral("resetCommentLabels"),
			{
				row{ .Index = 0U, .Label = { }, .Name = QStringLiteral("conventionalComments"), .Options = { }, .Control = ConventionalCommentsRow, .Wide = true },
				row{ .Index    = 6U,
					 .Label    = QStringLiteral("Label marks"),
					 .Name     = QStringLiteral("feedbackMarks"),
					 .Options  = { { QStringLiteral("Icons"), QStringLiteral("feedbackMarksIcons") },
								   { QStringLiteral("Letters"), QStringLiteral("feedbackMarksLetters") } },
					 .Trailing = FeedbackMarksPreview },
				row{ .Index           = 7U,
					 .Label           = QStringLiteral("Overview counts"),
					 .Name            = QStringLiteral("overviewCounts"),
					 .Options         = { { QStringLiteral("Open"), QStringLiteral("overviewCountsOpen") },
										  { QStringLiteral("All"), QStringLiteral("overviewCountsAll") } },
					 .Trailing        = countsInfo,
					 .TrailingSpacing = 4 },
				row{ .Index = 0U, .Label = QStringLiteral("Mark colours"), .Name = QStringLiteral("markColours"), .Options = { }, .Control = MarkColoursLink },
				row{ .Index = 0U, .Label = QStringLiteral("Overview chips"), .Name = QStringLiteral("labelColumns"), .Options = { }, .Control = chipsColumn, .Top = true },
			},
			[](application::review_behaviour_settings& aNext) {
				const auto defaults  = application::review_behaviour_settings{ };
				aNext.FeedbackMarks  = defaults.FeedbackMarks;
				aNext.OverviewCounts = defaults.OverviewCounts;
				aNext.LabelColumns   = defaults.LabelColumns;
			});
		commentLabelsReset->setToolTip(QStringLiteral("Reset comment labels"));
		commentLabelsReset->setAccessibleName(QStringLiteral("Reset comment labels"));
		commentLabelsReset->setAccessibleDescription(QStringLiteral("Returns label marks, Overview counts and Overview chips to their defaults; nothing else changes"));
		auto* const commentWorkshopReset = addGroup(
			commentsColumn,
			QStringLiteral("Comment workshop"),
			QStringLiteral("resetCommentWorkshop"),
			{
				row{ .Index = 0U, .Label = QStringLiteral("Feedback guidance"), .Name = QStringLiteral("labelHelper"), .Options = { }, .Control = helperRow },
				row{ .Index = 0U, .Label = QStringLiteral("Guidance text"), .Name = QStringLiteral("workshopGuidance"), .Options = { }, .Control = WorkshopGuidanceLink },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.LabelHelper = application::review_behaviour_settings{ }.LabelHelper;
			});
		commentWorkshopReset->setToolTip(QStringLiteral("Reset comment workshop"));
		commentWorkshopReset->setAccessibleDescription(QStringLiteral("Turns the feedback guidance off, its default; nothing else changes"));
		addGroup(
			commentsColumn,
			QStringLiteral("Descriptions and comments"),
			QStringLiteral("resetDescriptionsAndComments"),
			{
				row{ 9U,
					 QStringLiteral("Images"),
					 QStringLiteral("markdownImages"),
					 { { QStringLiteral("Load"), QStringLiteral("markdownImagesLoad") },
					   { QStringLiteral("Don't load"), QStringLiteral("markdownImagesOff") } },
					 imagesInfo,
					 nullptr,
					 4 },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.MarkdownImages = application::review_behaviour_settings{ }.MarkdownImages;
			});
		aReviewColumn->addWidget(BehaviourEditor);
		aCommentsColumn->addWidget(CommentsEditor);
		apply_behaviour_controls();
		refresh_open_count_mark();
	}

	void workbench_window::set_label_tints(const feedback_label_tints& aTints)
	{
		LabelTints = aTints;
		apply_label_marks();
	}

	void workbench_window::set_attention_colour(const QColor& aColour)
	{
		AttentionColour = aColour;
		apply_label_marks();
	}

	auto workbench_window::label_marks() const -> feedback_mark_theme
	{
		auto theme  = LabelTints.has_value() ? feedback_mark_theme{ .Tints = *LabelTints } : default_feedback_mark_theme(palette());
		theme.Style = Behaviour.FeedbackMarks == application::review_feedback_marks::letters ? feedback_mark_style::letters : feedback_mark_style::icons;
		return theme;
	}

	void workbench_window::apply_label_marks()
	{
		const auto theme = label_marks();
		if (ReviewEditor != nullptr) {
			ReviewEditor->set_label_marks(theme);
		}
		if (ReviewSubmissionEditor != nullptr) {
			ReviewSubmissionEditor->set_label_marks(theme);
			if (AttentionColour.has_value()) {
				ReviewSubmissionEditor->set_attention_colour(*AttentionColour);
			}
		}
		if (ConversationPanel != nullptr) {
			ConversationPanel->set_label_marks(theme);
		}
		if (Finder != nullptr) {
			Finder->set_label_marks(theme);
		}
		if (FeedbackMarksPreview != nullptr) {
			FeedbackMarksPreview->set_theme(theme);
		}
		if (LabelColumnsEditor != nullptr) {
			LabelColumnsEditor->set_theme(theme);
		}
		// [[BPR-1089]]. The Overview's chips, and the round open mark beside
		// Settings' Open, in the theme's attention amber.
		auto states = default_feedback_state_colours(palette());
		if (AttentionColour.has_value()) {
			states.Open = *AttentionColour;
		}
		if (OverviewPage != nullptr) {
			OverviewPage->set_label_marks(theme);
			OverviewPage->set_attention_colour(states.Open);
		}
		refresh_open_count_mark();
	}

	void workbench_window::refresh_open_count_mark()
	{
		// [[BPR-1089]]. Settings' Open carries the round open state mark the
		// Overview draws beside its own Open, in the theme's attention amber.
		if (BehaviourOptions[7].empty()) {
			return;
		}
		auto states = default_feedback_state_colours(palette());
		if (AttentionColour.has_value()) {
			states.Open = *AttentionColour;
		}
		auto* const open = BehaviourOptions[7].front();
		open->setIcon(QIcon{ feedback_state_mark_pixmap(core::feedback_resolution::unresolved, states, devicePixelRatioF()) });
		open->setIconSize(QSize{ 16, 16 });
		open->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	}

	void workbench_window::dispatch_line_details_toggle()
	{
		// The action's check already moved; the published behaviour decides,
		// so it goes back until the host answers.
		const auto current = AskedBehaviour.value_or(Behaviour);
		apply_line_details();
		if (!BehaviourDispatch) {
			return;
		}
		auto next        = current;
		next.LineDetails = current.LineDetails == application::review_line_details::shown
			? application::review_line_details::hidden
			: application::review_line_details::shown;
		AskedBehaviour   = next;
		BehaviourDispatch(next);
	}

	void workbench_window::apply_line_details()
	{
		const auto shown = Behaviour.LineDetails == application::review_line_details::shown;
		if (LineDetailsAction != nullptr) {
			const auto blocker = QSignalBlocker{ LineDetailsAction };
			LineDetailsAction->setChecked(shown);
		}
		if (ComparisonSurface != nullptr) {
			ComparisonSurface->set_line_details_visible(shown);
		}
	}

	void workbench_window::apply_reading_width()
	{
		// [[BPR-1584]]. Threads' rows and Discuss's notes stop at the reading
		// width, left-aligned; the code comparison is never touched.
		if (Finder != nullptr) {
			Finder->set_reading_width(Behaviour.ReadingWidth);
		}
		apply_modality_surface();
	}

	void workbench_window::apply_markdown_images()
	{
		const auto shown = Behaviour.MarkdownImages == application::review_markdown_images::load;
		if (OverviewPage != nullptr) {
			OverviewPage->body_view()->set_images_shown(shown);
		}
		if (ConversationPanel != nullptr) {
			ConversationPanel->set_images_shown(shown);
		}
	}

	void workbench_window::apply_behaviour_controls()
	{
		if (BehaviourEditor == nullptr) {
			return;
		}
		ApplyingBehaviour = true;
		const auto check  = [this](const std::size_t aRow, const std::size_t aChoice) {
			auto& buttons = BehaviourOptions[aRow];
			if (aChoice < buttons.size()) {
				buttons[aChoice]->setChecked(true);
			}
		};
		check(0U, static_cast<std::size_t>(Behaviour.OpenModality));
		check(2U, static_cast<std::size_t>(Behaviour.MarkRead));
		check(3U, static_cast<std::size_t>(Behaviour.ReturnComparison));
		check(4U, static_cast<std::size_t>(Behaviour.CommentsFilter));
		check(5U, static_cast<std::size_t>(Behaviour.DrawerControls));
		check(6U, static_cast<std::size_t>(Behaviour.FeedbackMarks));
		check(7U, static_cast<std::size_t>(Behaviour.OverviewCounts));
		check(8U, static_cast<std::size_t>(Behaviour.LineDetails));
		check(9U, static_cast<std::size_t>(Behaviour.MarkdownImages));
		check(10U, static_cast<std::size_t>(Behaviour.ReadingWidth));
		if (CenterRibbonWidth != nullptr) {
			CenterRibbonWidth->setValue(static_cast<int>(Behaviour.CenterRibbonWidth));
		}
		if (LabelHelperSwitch != nullptr) {
			LabelHelperSwitch->setChecked(Behaviour.LabelHelper == application::review_label_helper::on);
		}
		// [[BPR-1816]]. Ticked is Home.
		for (std::size_t index = 0U; index < FinishReviewSwitches.size(); ++index) {
			if (FinishReviewSwitches[index] != nullptr) {
				FinishReviewSwitches[index]->setChecked(core::finish_return(Behaviour.FinishReview, finish_verdicts[index]) == core::review_finish_return::home);
			}
		}
		// [[BPR-1568]]. The Review feedback hook's line says what this
		// window's published record says.
		if (WritingHooks != nullptr) {
			WritingHooks->present_label_helper(Behaviour.LabelHelper);
		}
		if (LabelColumnsEditor != nullptr) {
			LabelColumnsEditor->set_columns(Behaviour.LabelColumns);
		}
		if (LabelColumnsRecovery != nullptr) {
			const auto& recovery = Behaviour.LabelColumnsRecovery;
			LabelColumnsRecovery->setVisible(recovery.has_value());
			LabelColumnsRecovery->setText(recovery.has_value() ? feedback_label_columns_recovery_text(recovery->Unknown, recovery->Repeated) : QString{ });
		}
		ApplyingBehaviour = false;
	}

	auto workbench_window::finish_review_switch(const core::review_disposition aDisposition) const noexcept -> QCheckBox*
	{
		for (std::size_t index = 0U; index < finish_verdicts.size(); ++index) {
			if (finish_verdicts[index] == aDisposition) {
				return FinishReviewSwitches[index];
			}
		}
		return nullptr;
	}
}
