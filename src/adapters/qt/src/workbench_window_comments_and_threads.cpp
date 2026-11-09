#include "bpr/core/overview_tour.hpp"
#include "bpr/core/review_comment_range.hpp"
#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/suggestion_draft.hpp"
#include "bpr/core/thread_reply.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/compose_review_sheet.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/private_review_panel.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "conversation_glyphs.hpp"
#include "drawer_tab_bar.hpp"
#include "workbench_window_support.hpp"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractListModel>
#include <QAction>
#include <QAnyStringView>
#include <QBoxLayout>
#include <QByteArrayView>
#include <QChar>
#include <QFlags>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLatin1Char>
#include <QLayout>
#include <QList>
#include <QListSpecialMethods>
#include <QMainWindow>
#include <QMargins>
#include <QMenu>
#include <QMetaObject>
#include <QModelIndex>
#include <QObject>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QString>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVariant>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::qt {
	void workbench_window::refresh_cards()
	{
		update_older_pair_notice();
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		auto* const cards = ConversationPanel->cards();
		cards->set_draft(
			ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::nullopt,
			ComparisonSurface != nullptr && ComparisonSurface->current_result() != nullptr
				? std::optional{ ComparisonSurface->current_result()->Alignment.Digest }
				: std::nullopt);
		cards->refresh_geometry();
		apply_thread_marks();
		// [[BPR-1293]]. My draft is a row of Threads, with its lock.
		if (Finder != nullptr) {
			auto draft = std::optional<finder_draft>{ };
			if (const auto& shown = ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::optional<core::review_comment_draft>{ };
				shown.has_value() && !shown->ReplyToThread.has_value() && !shown->shared() && !shown->Body.empty() && ComparisonSurface != nullptr
				&& ComparisonSurface->current_result() != nullptr && ComparisonSurface->current_context().has_value()
				&& ComparisonSurface->current_result()->Alignment.Digest == shown->Anchor.Comparison) {
				draft = finder_draft{
					.File = ComparisonSurface->current_context()->Key.File,
					.Line = shown->Anchor.Line + 1U,
					.Body = QString::fromStdString(shown->Body),
				};
			}
			Finder->set_draft(std::move(draft));
		}
		resume_conversation_pick();
		resume_pending_comment_pick();
		// [[BPR-1592]]. The notes follow the code as the threads do.
		if (PrivateNotes != nullptr) {
			if (PrivateNotes->isVisible()) {
				PrivateNotes->refresh_geometry();
			}
			resume_private_note_open();
		}
	}

	void workbench_window::present_saved_drafts(std::vector<core::review_comment_draft> aDrafts)
	{
		present_saved_drafts(std::move(aDrafts), { });
	}

	void workbench_window::present_saved_drafts(std::vector<core::review_comment_draft> aDrafts, std::vector<core::reply_draft_decision> aReplies)
	{
		// [[BPR-1345]]. The review in progress is what the saved drafts say:
		// each pending in it, or held while it is submitted.
		const auto pending    = static_cast<std::size_t>(std::ranges::count_if(aDrafts, [](const core::review_comment_draft& aDraft) { return core::pending_in_review(aDraft); }));
		const auto held       = static_cast<std::size_t>(std::ranges::count_if(aDrafts, [](const core::review_comment_draft& aDraft) { return core::held_by_review(aDraft); }));
		const auto changed    = pending != PendingReviewComments || held != HeldReviewComments;
		PendingReviewComments = pending;
		HeldReviewComments    = held;
		update_review_chip();
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		settle_carried_suggestion(aDrafts);
		ConversationPanel->cards()->set_saved_drafts(std::move(aDrafts), std::move(aReplies));
		apply_thread_marks();
		if (changed && ConversationPanel->cards()->writing_draft()) {
			present_inline_draft();
		}
	}

	void workbench_window::present_filtered_comment(const bool aFiltered, std::optional<filtered_comment_presentation> aPlacement)
	{
		FilteredComments  = aFiltered;
		FilteredPlacement = aFiltered ? std::move(aPlacement) : std::nullopt;
		present_filtered_notice();
		update_comment_target();
		present_inline_draft();
	}

	auto workbench_window::filtered_comment_notice() const -> std::pair<QString, bool>
	{
		using placement = core::filtered_comment_placement;
		using refusal   = core::filtered_comment_refusal;
		if (ComparisonSurface == nullptr) {
			return { };
		}
		const auto selection = ComparisonSurface->selection();
		if (!selection.has_value() || comment_reply_thread().has_value()) {
			return { };
		}
		// [[BPR-1735]]. On the Whole PR diff only a placement off the line
		// itself is said; with none, Publish is as it always was.
		if (!FilteredComments) {
			if (!WholeDiffPlacement || WholeDiffPlacement->Selection != selection->PreferredLine) {
				return { };
			}
		} else if (!FilteredPlacement || FilteredPlacement->Selection != selection->PreferredLine) {
			return { tr("Working out where GitHub puts a comment on this line…"), true };
		}
		const auto& shown  = FilteredComments ? *FilteredPlacement : *WholeDiffPlacement;
		const auto& target = shown.Target;
		const auto  line   = static_cast<qulonglong>(target.Line) + 1U;
		switch (target.Placement) {
		case placement::head:
			if (target.Line == selection->PreferredLine.Line) {
				return { };
			}
			return { target.Side == core::diff_side::left ? tr("Goes on base line %1, where GitHub counts this line in the merge base.").arg(line)
														  : tr("Goes on the head at line %1, where this line is now.").arg(line),
					 false };
		case placement::earlier_commit:
			return { tr("Goes on commit %1, where this line was written. A later commit changed it, so GitHub shows this comment as outdated: in the conversation, not in Files changed.")
						 .arg(shown.Commit),
					 false };
		case placement::file_comment: {
			// [[BPR-1736]]. On the Whole PR diff, what was learned: GitHub
			// refused such a comment here, or nothing is known yet and Publish
			// asks GitHub once. Never what the server's version supports.
			if (shown.Learned == core::off_hunk_creation_capability::unsupported) {
				return { tr("GitHub can't place a comment on this line. It will be posted as a file comment that links to the line."), false };
			}
			if (shown.Learned == core::off_hunk_creation_capability::unknown) {
				return { tr("GitHub may not place a comment on this line. If it refuses, the comment is posted as a file comment that links to the line."), false };
			}
			// [[BPR-1706]]. Why GitHub takes no line comment here, then what
			// Publish does instead.
			const auto why = target.Refusal == refusal::base_line_changed ? tr("This line changed after the merge base, where GitHub counts base lines, so GitHub can't attach a comment to it.")
				: target.Refusal == refusal::not_on_commit
				? tr("A later commit changed this line, and GitHub shows no change of commit %1 near it, so GitHub can't attach a comment to it.").arg(shown.Commit)
				: tr("GitHub shows no change near this line at the head, so GitHub can't attach a comment to it.");
			return { tr("%1 Publish adds a comment on the file instead, linking to line %2 of commit %3.").arg(why).arg(line).arg(shown.Commit), false };
		}
		case placement::refused:
			break;
		}
		switch (target.Refusal) {
		case refusal::lines_pending:
			return { tr("Working out where GitHub puts a comment on this line…"), true };
		case refusal::range:
			return { tr("A range can be commented on only where this diff's side is the pull request's own. Comment on one line."), true };
		case refusal::not_on_commit:
			return { tr("A later commit changed this line, and GitHub shows no change of commit %1 near it, so no comment can go there. Comment on it in the whole pull request instead.")
						 .arg(shown.Commit),
					 true };
		case refusal::base_line_changed:
			return { tr("This line changed after the merge base, where GitHub counts base lines. Comment on a right-side line instead."), true };
		case refusal::outside_github_diff:
			return { tr("This line is unchanged at the head, where GitHub shows no change near it, so no line comment can go there. Comment on a changed line instead."), true };
		case refusal::range_outside_github_diff:
			// [[BPR-1735]]. A range has no file comment: one line's link would
			// say less than the reviewer selected.
			return { tr("GitHub shows no change near some of these lines, so it can't attach a comment to them, and a comment on several lines can't go on the file. Comment on one line."), true };
		case refusal::outside_pull_request:
			return { tr("This diff's commits are not the pull request's own, so nothing can be published on it."), true };
		case refusal::lines_unavailable:
		case refusal::none:
			break;
		}
		return { tr("Where this line is at the head could not be read. Comment on it in the whole pull request instead."), true };
	}

	auto workbench_window::filtered_comment_placement() const noexcept -> const std::optional<filtered_comment_presentation>&
	{
		return FilteredComments ? FilteredPlacement : WholeDiffPlacement;
	}

	void workbench_window::present_whole_diff_comment(std::optional<filtered_comment_presentation> aPlacement)
	{
		if (WholeDiffPlacement == aPlacement) {
			return;
		}
		WholeDiffPlacement = std::move(aPlacement);
		present_filtered_notice();
		update_comment_target();
		present_inline_draft();
		publish_pending_file_comment();
	}

	void workbench_window::present_unplaceable_review_comments(std::map<core::review_draft_id, unplaceable_review_comment> aHeld)
	{
		if (UnplaceableComments == aHeld) {
			return;
		}
		UnplaceableComments = std::move(aHeld);
		present_held_suggestions();
		present_inline_draft();
	}

	auto workbench_window::unplaceable_review_comments() const noexcept -> const std::map<core::review_draft_id, unplaceable_review_comment>&
	{
		return UnplaceableComments;
	}

	auto workbench_window::unplaceable_hold(const unplaceable_review_comment& aComment) const -> std::pair<QString, QString>
	{
		// [[BPR-1735]]. Why GitHub refuses it in a review, and what to do: a
		// line is published on its own as a file comment; a range, which has
		// no file comment, is deleted or rewritten on one line.
		const auto line = static_cast<qulonglong>(aComment.Target.Placement == core::filtered_comment_placement::file_comment ? aComment.Target.Line : 0U) + 1U;
		if (aComment.Target.Placement == core::filtered_comment_placement::file_comment) {
			return { tr("GitHub shows no change near line %1, so it can't place this comment in a review.").arg(line), tr("Publish as file comment") };
		}
		return { tr("GitHub shows no change near some of these lines, so it can't place this comment in a review, and a comment on several lines can't go on the file. Delete it, or write it on one line."),
				 QString{ } };
	}

	void workbench_window::publish_pending_file_comment()
	{
		// [[BPR-1735]]. Finish your review's Publish as file comment, once the
		// comment shows at its line and the core has placed it on the file:
		// the drawer's own Publish, which takes it out of the review.
		if (!PendingFilePublish.has_value() || ReviewEditor == nullptr) {
			return;
		}
		const auto& draft = ReviewEditor->shown_draft();
		if (!draft.has_value() || draft->Id != *PendingFilePublish) {
			return;
		}
		if (!UnplaceableComments.contains(draft->Id)) {
			PendingFilePublish.reset();
			return;
		}
		if (!WholeDiffPlacement.has_value() || WholeDiffPlacement->Selection != draft->Anchor || WholeDiffPlacement->Target.Placement != core::filtered_comment_placement::file_comment
			|| !ReviewEditor->publish_action()->isEnabled()) {
			return;
		}
		PendingFilePublish.reset();
		ReviewEditor->publish_action()->trigger();
	}

	void workbench_window::present_filtered_notice()
	{
		if (ReviewEditor == nullptr) {
			return;
		}
		const auto [text, blocks] = filtered_comment_notice();
		ReviewEditor->present_target_notice(text, blocks);
	}

	void workbench_window::set_thread_marks_suspended(const bool aSuspended)
	{
		if (ThreadMarksSuspended == aSuspended) {
			return;
		}
		ThreadMarksSuspended = aSuspended;
		apply_thread_marks();
	}

	void workbench_window::apply_thread_marks()
	{
		if (ComparisonSurface == nullptr || ConversationPanel == nullptr) {
			return;
		}
		// Amber for a thread on the line; blue for a thread with a post newer
		// than what the reader has read ([[BPR-0656]]). [[BPR-1335]]: the
		// thread's line number is tinted, and the open thread's line is washed
		// and its number barred.
		auto* const cards = ConversationPanel->cards();
		ComparisonSurface->set_mark_colours(cards->marker(), cards->accent());
		for (const auto side : { core::diff_side::left, core::diff_side::right }) {
			auto       marks      = std::vector<diff_line_mark>{ };
			const auto onNotesTab = TrayTabs != nullptr && TrayTabs->currentIndex() == tray_notes_tab;
			for (const auto& mark : ThreadMarksSuspended ? std::vector<thread_line_mark>{ } : cards->line_marks(side)) {
				marks.push_back(diff_line_mark{
					.Line        = mark.Line,
					.Kind        = mark.Unread ? diff_line_mark_kind::new_thread : diff_line_mark_kind::thread,
					.Resolved    = mark.Resolved,
					.Draft       = mark.Draft,
					.Open        = mark.Open && !onNotesTab,
					.Hovered     = mark.Hovered && !onNotesTab,
					.Writing     = mark.Writing && !onNotesTab,
					.FirstLine   = mark.FirstLine,
					.Description = mark.Description,
				});
			}
			// [[BPR-1592]]. A noted line's number is a dashed cell, as my
			// draft's is; a line that has a thread keeps the thread's mark.
			// The open note washes its line in the writing blue.
			if (PrivateNotes != nullptr) {
				for (const auto& note : PrivateNotes->line_marks(side)) {
					const auto taken = std::ranges::find(marks, note.Line, &diff_line_mark::Line);
					if (taken != marks.end()) {
						taken->Writing = taken->Writing || (note.Open && onNotesTab);
						continue;
					}
					auto mark        = diff_line_mark{ };
					mark.Line        = note.Line;
					mark.Draft       = true;
					mark.Writing     = note.Open && onNotesTab;
					mark.Description = note.Description;
					marks.push_back(std::move(mark));
				}
			}
			ComparisonSurface->set_line_marks(side, std::move(marks));
		}
		update_new_comment_group();
	}

	auto workbench_window::thread_on_line(const core::diff_side aSide, const std::uint32_t aLine) -> std::optional<std::string>
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return std::nullopt;
		}
		const auto& cards = ConversationPanel->cards()->cards();
		const auto  card  = std::ranges::find_if(cards, [aSide, aLine](const thread_card& aCard) {
			return aCard.Side == aSide && aCard.Line == aLine + 1U && !aCard.Outdated;
		});
		if (card == cards.end()) {
			return std::nullopt;
		}
		// Copied before the drawer is shown, which may lay the notes out anew.
		auto identity = card->Id;
		// The marker is a gesture on the conversation: the drawer it opens in
		// is shown, on its Comments tab.
		if (Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			Drawers[1].Toggle->trigger();
		}
		if (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_comments_tab) {
			TrayTabs->setCurrentIndex(tray_comments_tab);
		}
		return identity;
	}

	void workbench_window::toggle_thread_at(const core::diff_side aSide, const std::uint32_t aLine)
	{
		// [[BPR-1592]]. On the Notes tab a noted line's mark opens its note;
		// elsewhere, a line with no thread but a note opens the note too.
		const auto note       = PrivateNotes != nullptr ? PrivateNotes->note_on_line(aSide, aLine) : std::nullopt;
		const auto onNotesTab = TrayTabs != nullptr && TrayTabs->currentIndex() == tray_notes_tab;
		if (const auto identity = thread_on_line(aSide, aLine); identity.has_value() && !(note && onNotesTab)) {
			ConversationPanel->cards()->toggle_thread(*identity);
			return;
		}
		if (note) {
			show_private_note(*note);
		}
	}

	void workbench_window::show_private_note(const std::uint64_t aId)
	{
		if (Dispatch && Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			Dispatch(core::set_workbench_drawer_intent{ .Drawer = core::workbench_drawer::conversations, .State = core::workbench_drawer_state::expanded });
		}
		if (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_notes_tab) {
			TrayTabs->setCurrentIndex(tray_notes_tab);
		}
		if (PrivateNotes->open_note_id() == aId) {
			PrivateNotes->close_note();
			return;
		}
		PrivateNotes->open_note(aId);
	}

	void workbench_window::show_agent(const bool aAsk)
	{
		// [[BPR-1781]]. On Overview the Agent tab is the drawer's, and a
		// question there is about the pull request: no lines go with it.
		if (showing_overview() && ConversationTabsLent && OverviewPage->status_drawer() != nullptr) {
			OverviewPage->status_drawer()->show_tab(overview_drawer_tab::agent);
			if (AgentTab != nullptr) {
				AgentTab->refresh();
				AgentTab->focus_question();
			}
			return;
		}
		if (Dispatch && Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			Dispatch(core::set_workbench_drawer_intent{ .Drawer = core::workbench_drawer::conversations, .State = core::workbench_drawer_state::expanded });
		} else if (!Dispatch && Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			conversations_drawer_toggle_action()->trigger();
		}
		if (TrayTabs != nullptr) {
			TrayTabs->setCurrentIndex(tray_findings_tab);
		}
		if (AgentTab == nullptr) {
			return;
		}
		// The selected lines go with the question, through the chat's own
		// Add context, which the core decides; nothing is sent.
		if (aAsk && SideChat != nullptr && SideChat->add_context_action()->isEnabled()) {
			SideChat->add_context_action()->trigger();
		}
		AgentTab->refresh();
		AgentTab->focus_question();
	}

	void workbench_window::fit_tray_tabs(const bool aRepaint)
	{
		constexpr auto tabCount = 4;
		if (TrayTabs == nullptr || TrayHeader == nullptr || TrayTabs->count() != tabCount) {
			return;
		}
		auto* const tabs = static_cast<tray_tab_bar*>(TrayTabs);
		// [[BPR-1594]]. Four tabs. [[BPR-1755]], the owner's accept of
		// look v007 ("Get rid of teh icon on agent when it's full width like
		// the other tabs do. Only use it for the narrow."): at full width
		// every tab is its name alone.
		const QString tabNames[] = { tr("Comments"), tr("Notes"), tr("Tour"), tr("Agent") };
		const QString about[]    = {
			tr("The pull request's threads, with their scope and filters"),
			tr("Your private notes at their lines. Nothing here is published; Draft comment copies a note to a draft."),
			tr("Your review tour, stop by stop. Nothing in it is published until you publish the tour."),
			tr("Agent conversations, findings and private history. Nothing here is published; promote an item to a draft in Comments to publish it."),
		};
		// [[BPR-1684]]. The faces, widest first: every name; the open
		// tab's glyph and name with the others' glyphs; glyphs only. The row
		// takes the first that fits the header, whose tools move to their own
		// row before any name goes.
		enum class face : std::uint8_t { names,
										 open_named,
										 glyphs };
		const auto current = tabs->currentIndex();
		const auto textOf  = [&tabNames, current](const face aFace, const int aIndex) {
			return aFace == face::names || (aFace == face::open_named && aIndex == current) ? tabNames[aIndex] : QString{ };
		};
		const auto glyphOf = [](const face aFace, const int /*aIndex*/) { return aFace != face::names; };
		// The room is what the header's grid gives the tabs alone on their
		// row: its width less its left margin and the spacing of its columns.
		auto room = TrayHeader->width();
		if (const auto* const grid = qobject_cast<const QGridLayout*>(TrayHeader->layout()); grid != nullptr && room > 0) {
			room -= grid->contentsMargins().left() + 2 * std::max(0, grid->horizontalSpacing());
		}
		auto chosen = face::names;
		if (room > 0) {
			for (const auto candidate : { face::names, face::open_named, face::glyphs }) {
				chosen     = candidate;
				auto width = 0;
				for (auto index = 0; index < tabCount; ++index) {
					width += tabs->face_width(index, textOf(candidate, index), glyphOf(candidate, index));
				}
				if (width <= room) {
					break;
				}
			}
		}
		const auto glyph = [this](const int aIndex) {
			switch (aIndex) {
			case tray_comments_tab:
				return conversation_icon(conversation_glyph::comment, palette());
			case tray_notes_tab:
				return drawer_icon(drawer_glyph::page, palette());
			case tray_tour_tab:
				return drawer_icon(drawer_glyph::tour, palette());
			default:
				return drawer_icon(drawer_glyph::agent, palette());
			}
		};
		auto reshaped = false;
		for (auto index = 0; index < tabCount; ++index) {
			const auto text  = textOf(chosen, index);
			const auto shown = glyphOf(chosen, index);
			if (aRepaint || tabs->tabIcon(index).isNull() == shown) {
				reshaped = reshaped || tabs->tabIcon(index).isNull() == shown;
				tabs->setTabIcon(index, shown ? glyph(index) : QIcon{ });
			}
			if (tabs->tabText(index) != text) {
				reshaped = true;
				tabs->setTabText(index, text);
			}
			// A tab showing only its glyph says its name first.
			const auto tip = text.isEmpty() ? QStringLiteral("%1\n%2").arg(tabs->accessibleTabName(index), about[index]) : about[index];
			if (tabs->tabToolTip(index) != tip) {
				tabs->setTabToolTip(index, tip);
			}
		}
		// [[BPR-1757]]. A hidden QTabBar only marks its tabs dirty when a
		// face changes; the header's own sizeHint() query then lays them out,
		// so the bar's showEvent has nothing left to announce and the header's
		// grid keeps the old width (the window opens on Overview with the tray
		// hidden, so a restored width left Tour and Agent behind scroll
		// arrows). The bar says it changed size itself.
		if (reshaped) {
			tabs->updateGeometry();
		}
	}

	void workbench_window::wire_private_notes()
	{
		// [[BPR-1592]]. The notes hang at the code's lines: where a line is,
		// which comparison is shown and which line is selected are the
		// surface's; a note elsewhere is reached through the files drawer.
		PrivateNotes->set_line_locator([this](const core::diff_side aSide, const std::uint32_t aLine) -> std::optional<int> {
			if (ComparisonSurface == nullptr || PrivateNotes == nullptr) {
				return std::nullopt;
			}
			const auto rect = ComparisonSurface->line_rect(aSide, aLine);
			if (rect.isEmpty()) {
				return std::nullopt;
			}
			const auto inWindow = ComparisonSurface->mapTo(this, rect.topLeft());
			return PrivateNotes->mapFrom(this, inWindow).y();
		});
		PrivateNotes->set_comparison_source([this]() -> const core::file_alignment* {
			if (ComparisonSurface == nullptr || ComparisonSurface->current_result() == nullptr) {
				return nullptr;
			}
			return &ComparisonSurface->current_result()->Alignment;
		});
		PrivateNotes->set_selected_line_source([this]() -> std::optional<private_note_line> {
			if (ComparisonSurface == nullptr || ComparisonSurface->current_result() == nullptr || !ComparisonSurface->selection()) {
				return std::nullopt;
			}
			const auto& chosen = *ComparisonSurface->selection();
			if (chosen.PreferredLine.Comparison != ComparisonSurface->current_result()->Alignment.Digest) {
				return std::nullopt;
			}
			return private_note_line{ .Side = chosen.PreferredLine.Side, .Line = chosen.PreferredLine.Line };
		});
		PrivateNotes->set_line_dispatch([this](const core::diff_side aSide, const std::uint32_t aLine) {
			if (ComparisonSurface != nullptr) {
				static_cast<void>(ComparisonSurface->select_conversation_line(aSide, aLine));
			}
		});
		PrivateNotes->set_elsewhere_dispatch([this](const private_note_item& aNote) { open_private_note_elsewhere(aNote); });
		PrivateNotes->set_marks_observer([this] { apply_thread_marks(); });
		PrivateNotes->set_scroll_forward([this](QWheelEvent& aEvent) { return ComparisonSurface != nullptr && ComparisonSurface->scroll_by_wheel(aEvent); });
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			PrivateNotes->set_colours(ConversationPanel->cards()->accent(), ConversationPanel->cards()->marker());
		}
	}

	void workbench_window::open_private_note_elsewhere(const private_note_item& aNote)
	{
		const auto file = file_for_path(QString::fromStdString(aNote.Label));
		if (!file) {
			statusBar()->showMessage(tr("That note's file is not in this pull request now."));
			return;
		}
		PendingNoteOpen = aNote.Id;
		choose_home(false);
		choose_modality(review_modality::review);
		if (Dispatch) {
			Dispatch(core::select_workbench_file_intent{ .File = *file });
		}
	}

	auto workbench_window::open_file_in_review(const QString& aPath) -> bool
	{
		const auto file = file_for_path(aPath);
		if (!file || !Dispatch) {
			return false;
		}
		choose_home(false);
		choose_modality(review_modality::review);
		Dispatch(core::select_workbench_file_intent{ .File = *file });
		return true;
	}

	auto workbench_window::review_file_named(const QString& aPath) const -> std::optional<QString>
	{
		const auto* const model = Drawers[0].Panel != nullptr ? Drawers[0].Panel->contents() : nullptr;
		if (model == nullptr || aPath.isEmpty()) {
			return std::nullopt;
		}
		auto       found  = std::optional<QString>{ };
		const auto suffix = aPath.startsWith(QLatin1Char{ '/' }) ? aPath : QStringLiteral("/") + aPath;
		for (int row = 0; row < model->rowCount(); ++row) {
			auto path = model->data(model->index(row, 0), Qt::DisplayRole).toString();
			if (path == aPath) {
				return path;
			}
			if (path.endsWith(suffix)) {
				if (found) {
					return std::nullopt;
				}
				found = path;
			}
		}
		return found;
	}

	auto workbench_window::go_to_file_line(const QString& aPath, const std::uint32_t aLine, const bool aBefore) -> bool
	{
		const auto path = review_file_named(aPath);
		if (!path || aLine == 0U) {
			statusBar()->showMessage(tr("%1 is not in this pull request at its current head.").arg(aPath), 6000);
			return false;
		}
		PendingLineGo      = pending_line_go{ .File = core::file_identity_of(path->toStdString()), .Side = aBefore ? core::diff_side::left : core::diff_side::right, .Line = aLine - 1U };
		const auto context = ComparisonSurface != nullptr ? ComparisonSurface->current_context() : std::nullopt;
		if (!context.has_value() || context->Key.File != PendingLineGo->File || Modality == nullptr || Modality->current() != review_modality::review) {
			if (!open_file_in_review(*path)) {
				PendingLineGo.reset();
				return false;
			}
		}
		resume_line_go();
		return true;
	}

	void workbench_window::resume_line_go()
	{
		if (!PendingLineGo || ComparisonSurface == nullptr) {
			return;
		}
		// Owner look v005 ("The review link in the response isn't scrolling the
		// window to the line"): the line is revealed, not only selected, and
		// a long file is waited for until its geometry is adopted
		// ([[BPR-1264]]); the conversation anchor keeps the reveal through
		// the file's measurement, as a thread's pick does ([[BPR-1698]]).
		const auto context = ComparisonSurface->current_context();
		if (context.has_value() && context->Key.File == PendingLineGo->File && ComparisonSurface->current_result() != nullptr && !ComparisonSurface->geometry_pending()) {
			const auto go = *PendingLineGo;
			PendingLineGo.reset();
			if (!ComparisonSurface->reveal_conversation_line(go.Side, go.Line)) {
				statusBar()->showMessage(tr("Line %1 is not in this file at the %2.").arg(go.Line + 1U).arg(go.Side == core::diff_side::left ? tr("base") : tr("head")), 6000);
			}
			return;
		}
		if (++PendingLineGo->Tries > 100) {
			PendingLineGo.reset();
			return;
		}
		QTimer::singleShot(100, this, [this] { resume_line_go(); });
	}

	void workbench_window::schedule_agent_suggestions()
	{
		if (AgentSuggestionsTimer != nullptr) {
			AgentSuggestionsTimer->start();
		}
	}

	void workbench_window::go_to_private_note(const std::uint64_t aId)
	{
		if (PrivateNotes == nullptr) {
			return;
		}
		if (Dispatch && Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			Dispatch(core::set_workbench_drawer_intent{ .Drawer = core::workbench_drawer::conversations, .State = core::workbench_drawer_state::expanded });
		}
		if (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_notes_tab) {
			TrayTabs->setCurrentIndex(tray_notes_tab);
		}
		if (PrivateNotes->open_note_id() != aId) {
			PrivateNotes->open_note(aId);
		}
	}

	void workbench_window::resume_private_note_open()
	{
		if (!PendingNoteOpen || PrivateNotes == nullptr) {
			return;
		}
		const auto place = PrivateNotes->placement(*PendingNoteOpen);
		if (place.Kind == core::note_placement_kind::other_file) {
			return;
		}
		const auto id = *PendingNoteOpen;
		PendingNoteOpen.reset();
		show_private_note(id);
	}

	void workbench_window::go_to_thread_at(const core::diff_side aSide, const std::uint32_t aLine)
	{
		const auto identity = thread_on_line(aSide, aLine);
		if (!identity.has_value()) {
			return;
		}
		if (ConversationPanel->cards()->open_thread_id() != *identity) {
			ConversationPanel->cards()->open_thread(*identity);
		}
		// Opening reveals the line only when it changes the current thread;
		// the tick asks for the line whatever was current before.
		reveal_thread_line(*identity);
	}

	void workbench_window::reveal_thread_line(const std::string& aIdentity)
	{
		// [[BPR-1290]]. Reveal the thread's line where the reader is, without
		// changing the modality.
		const auto index = ConversationPanel->model()->index_for_id(aIdentity);
		if (index.isValid()) {
			ConversationPanel->choose(index);
			synchronize_selected_conversation(index);
		} else {
			static_cast<void>(select_thread_line(aIdentity));
		}
	}

	void workbench_window::write_inline_reply(const std::string& aThread, const QString& aText)
	{
		if (ReviewEditor == nullptr) {
			return;
		}
		const auto& draft = ReviewEditor->shown_draft();
		if (!draft.has_value() || !draft->ReplyToThread.has_value() || "review-thread:" + *draft->ReplyToThread != aThread
			|| ReviewEditor->body_editor()->isReadOnly()) {
			return;
		}
		if (ReviewEditor->body_editor()->toPlainText() != aText) {
			ReviewEditor->body_editor()->setPlainText(aText);
		}
	}

	void workbench_window::present_reply_placement()
	{
		// [[BPR-1084]]. The draft editor's suggested-change mark reads what the
		// thread a reply answers is attached to, from the card the accepted
		// projection built; with no such card it claims nothing.
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr || ReviewEditor == nullptr) {
			return;
		}
		auto        placement = std::optional<core::suggestion_placement_observation>{ };
		const auto& draft     = ReviewEditor->shown_draft();
		if (draft.has_value() && draft->ReplyToThread.has_value()) {
			const auto  identity = "review-thread:" + *draft->ReplyToThread;
			const auto& cards    = ConversationPanel->cards()->cards();
			const auto  found    = std::ranges::find(cards, identity, &thread_card::Id);
			if (found != cards.end()) {
				placement = found->Suggestion;
			}
		}
		ReviewEditor->describe_reply_placement(placement);
	}

	void workbench_window::present_inline_reply()
	{
		if (ConversationReply) {
			ConversationPanel->cards()->present_reply(ConversationReply);
			return;
		}
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr || ReviewEditor == nullptr) {
			return;
		}
		present_reply_placement();
		const auto& draft = ReviewEditor->shown_draft();
		if (!draft.has_value() || !draft->ReplyToThread.has_value() || draft->shared()) {
			ConversationPanel->cards()->present_reply(std::nullopt);
			return;
		}
		ConversationPanel->cards()->present_reply(thread_reply_presentation{
			.Thread      = "review-thread:" + *draft->ReplyToThread,
			.Body        = ReviewEditor->body_editor()->toPlainText(),
			.Publishable = ReviewEditor->publish_action()->isEnabled() && !ReviewEditor->body_editor()->isReadOnly(),
		});
	}

	void workbench_window::set_conversation_reply_dispatch(std::function<void(core::conversation_reply_action, core::conversation_reply_draft)> aDispatch) { ConversationReplyDispatch = std::move(aDispatch); }

	void workbench_window::present_conversation_reply(std::optional<thread_reply_presentation> aReply)
	{
		ConversationReply = std::move(aReply);
		if (ConversationPanel) {
			ConversationPanel->cards()->present_reply(ConversationReply);
			// [[BPR-1781]]. A new comment's draft is the composer's.
			ConversationPanel->present_new_comment(ConversationReply);
		}
	}

	auto workbench_window::dispatch_conversation_reply(const core::conversation_reply_action aAction, const std::string& aIdentity, const QString& aText) -> bool
	{
		if (!ConversationPanel) {
			return false;
		}
		const auto& nodes = ConversationPanel->cards()->retained();
		const auto  found = std::ranges::find(nodes, aIdentity, &application::github_conversation_projection::Id);
		if (found == nodes.end() || !found->Path.empty() || (found->Kind != application::github_conversation_kind::issue_comment && found->Kind != application::github_conversation_kind::review)) {
			return false;
		}
		if (ConversationReplyDispatch) {
			ConversationReplyDispatch(aAction, { found->Id, found->AuthorLogin, found->Body, aText.toStdString() });
			if (aAction == core::conversation_reply_action::open) {
				ConversationPanel->cards()->begin_reply(aIdentity);
			}
		} else {
			ConversationPanel->present_notice(tr("Replies are unavailable for this connection."));
		}
		return true;
	}

	void workbench_window::set_conversation_read_dispatch(conversation_read_dispatch aDispatch)
	{
		ConversationReadDispatch = std::move(aDispatch);
	}

	void workbench_window::set_conversation_unread_dispatch(conversation_read_dispatch aDispatch)
	{
		ConversationUnreadDispatch = std::move(aDispatch);
	}

	void workbench_window::set_conversations_read_dispatch(conversations_read_dispatch aDispatch)
	{
		ConversationsReadDispatch = std::move(aDispatch);
		update_new_comment_group();
	}

	auto workbench_window::mark_all_comments_read_action() const noexcept -> QAction*
	{
		return MarkAllCommentsRead;
	}

	void workbench_window::present_conversations_read(const std::optional<conversations_read_result> aResult, const std::optional<std::string>& aAuthor)
	{
		// [[BPR-1699]]. The counts and badges have already moved with
		// the read state; this says what the gesture did, once it is durable.
		const auto who = aAuthor.has_value() ? QString::fromStdString(*aAuthor) : QString{ };
		if (!aResult.has_value()) {
			statusBar()->showMessage(tr("Your read state could not be saved, so no comment was marked read. Try again."), 10000);
			return;
		}
		const auto comments = [](const std::uint32_t aCount) {
			return aCount == 1U ? QStringLiteral("1 comment") : QStringLiteral("%1 comments").arg(aCount);
		};
		auto said = QString{ };
		if (aResult->Marked == 0U) {
			said = who.isEmpty() ? tr("Nothing on this pull request is new.") : tr("Nothing %1 wrote is new.").arg(who);
		} else {
			said = who.isEmpty() ? tr("Marked %1 as read.").arg(comments(aResult->Marked)) : tr("Marked %1 from %2 as read.").arg(comments(aResult->Marked), who);
		}
		if (aResult->Kept != 0U) {
			said += QLatin1Char(' ')
				+ (aResult->Kept == 1U ? tr("1 more stays new: it follows a reply from someone else that you have not read.")
									   : tr("%1 more stay new: they follow a reply from someone else that you have not read.").arg(aResult->Kept));
		}
		statusBar()->showMessage(said, 10000);
	}

	void workbench_window::update_read_state(const std::string& aThread, std::vector<std::string> aUnreadPosts)
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		if (Finder != nullptr) {
			Finder->set_thread_read_state(aThread, aUnreadPosts);
		}
		static_cast<void>(ConversationPanel->cards()->set_thread_read_state(aThread, std::move(aUnreadPosts)));
		apply_thread_marks();
		present_new_threads();
	}

	void workbench_window::present_read_state(std::vector<thread_read_state> aState)
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		if (Finder != nullptr) {
			Finder->set_read_state(aState);
		}
		ConversationPanel->cards()->set_read_state(std::move(aState));
		apply_thread_marks();
		present_new_threads();
	}

	void workbench_window::present_new_conversation_comments(const std::uint32_t aCount)
	{
		NewConversationComments = aCount;
		apply_thread_marks();
		if (OverviewPage != nullptr) {
			OverviewPage->set_new_conversation_comments(aCount);
		}
	}

	void workbench_window::present_new_threads()
	{
		// [[BPR-1589]]. The Status drawer counts what Threads' Only new lists:
		// the threads with a post I have not read.
		if (OverviewPage != nullptr && ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			OverviewPage->set_new_threads(ConversationPanel->cards()->unread_threads_in_file_order().size());
		}
	}

	void workbench_window::show_new_threads()
	{
		if (Modality == nullptr || !HasGithubPullRequest) {
			return;
		}
		choose_home(false);
		choose_modality(review_modality::threads);
		if (Finder != nullptr) {
			auto filters    = Finder->filters();
			filters.OnlyNew = true;
			Finder->set_filters(std::move(filters));
		}
	}

	auto workbench_window::next_new_comment_action() const noexcept -> QAction*
	{
		return NextNewCommentAction;
	}

	auto workbench_window::review_bar() const noexcept -> QToolBar*
	{
		return ReviewBar;
	}

	auto workbench_window::tray_tabs() const noexcept -> QTabBar*
	{
		return TrayTabs;
	}

	auto workbench_window::comments_tab_scroll() const noexcept -> QScrollArea*
	{
		return CommentsScroll;
	}

	auto workbench_window::findings_tab_scroll() const noexcept -> QScrollArea*
	{
		return ConversationScroll;
	}

	auto workbench_window::next_new_comment_button() const noexcept -> QToolButton*
	{
		return NextNewCommentButton;
	}

	auto workbench_window::comments_count_text() const -> QString
	{
		return TrayCommentsCount == nullptr ? QString{ } : static_cast<const drawer_count_pill*>(TrayCommentsCount)->text();
	}

	auto workbench_window::updates_disclosure_button() const noexcept -> updates_disclosure*
	{
		return UpdatesDisclosure;
	}

	auto workbench_window::updates_line_action() const noexcept -> QAction*
	{
		return UpdatesLineAction;
	}

	auto workbench_window::updates_line_open() const noexcept -> bool
	{
		return UpdatesLineState.State == core::workbench_updates_line::open;
	}

	auto workbench_window::prepare_tour_action() const noexcept -> QAction* { return PrepareTourAction; }

	auto workbench_window::add_tour_stop_action() const noexcept -> QAction* { return AddTourStopAction; }

	auto workbench_window::tour_editor() const noexcept -> review_tour_editor* { return TourEditor; }

	void workbench_window::open_review_tour(const bool aKeepStep)
	{
		close_review_composition(false);
		close_review_sheet();
		choose_home(false);
		choose_modality(review_modality::review);
		if (Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			conversations_drawer_toggle_action()->trigger();
		}
		const auto already = TrayTabs->currentIndex() == tray_tour_tab;
		TrayTabs->setCurrentIndex(tray_tour_tab);
		// [[BPR-1914]] v004. Going to a stop from the open Tour tab keeps the
		// step it is on: opening the Problem first moved the keyboard and
		// the tab's height, and the tab jumped.
		if (!aKeepStep || !already) {
			TourEditor->show_problem();
		}
	}

	void workbench_window::wire_tour_page()
	{
		// [[BPR-1593]]. RFC-BPR-0085 D4: Expand opens the tour page in the
		// main pane (layout C), and Back to the code returns to the route.
		TourPage = new QWidget{ CentreStack };
		TourPage->setObjectName(QStringLiteral("workbenchTourPage"));
		auto* const pageColumn = new QVBoxLayout{ TourPage };
		pageColumn->setContentsMargins(0, 0, 0, 0);
		pageColumn->setSpacing(0);
		auto* const bar = new QHBoxLayout{ };
		bar->setContentsMargins(12, 6, 12, 0);
		bar->addStretch(1);
		auto* const back = new quiet_push_button{ tr("Back to the code"), TourPage };
		back->setObjectName(QStringLiteral("workbenchTourPageBack"));
		back->setFlat(true);
		back->setIcon(conversation_icon(conversation_glyph::minimize, palette()));
		back->setToolTip(tr("Back to the code  Esc"));
		connect(back, &QPushButton::clicked, this, [this] { close_tour_page(); });
		bar->addWidget(back);
		pageColumn->addLayout(bar);
		TourPageBody = new QWidget{ TourPage };
		// [[BPR-1925]]. The tour, and beside it the chat about its marked
		// part, as in the Tour tab.
		auto* const bodyColumn = new QHBoxLayout{ TourPageBody };
		bodyColumn->setContentsMargins(0, 0, 0, 0);
		bodyColumn->setSpacing(0);
		pageColumn->addWidget(TourPageBody, 1);
		auto* const escape = new QShortcut{ shortcut_key(shortcut_command::leave_composer), TourPage };
		escape->setObjectName(QStringLiteral("workbenchTourPageEscape"));
		escape->setContext(Qt::WidgetWithChildrenShortcut);
		connect(escape, &QShortcut::activated, this, [this] { close_tour_page(); });
		CentreStack->addWidget(TourPage);
		// [[BPR-1878]], owner look v001. The Tour tab has the Agent tab's own
		// agent picker at its bottom: the same component over the same source.
		TourEditor->set_agent_picker(new agent_run_settings_picker{ AgentTab->run_settings_source(), TourEditor });
		// [[BPR-1881]]. The Tour tab's agent features follow the Agent tab's
		// own setup verdict, and its call to action shows that setup.
		TourEditor->set_agent_setup_dispatch([this] {
			if (AgentTab != nullptr) {
				AgentTab->open_setup();
				show_agent(false);
			}
		});
		// [[BPR-1892]]. A failed request's notice sends the person to the
		// Agent tab's card for the run; nothing switches tabs by itself.
		TourEditor->set_agent_failure_dispatch([this] {
			if (AgentTab != nullptr) {
				show_agent(false);
				AgentTab->reveal_run_failure();
			}
		});
		TourEditor->set_shown_observer([this] {
			if (AgentTab != nullptr) {
				AgentTab->look_for_agents();
			}
		});
		AgentTab->set_readiness_observer([this] {
			if (TourEditor != nullptr && AgentTab != nullptr) {
				TourEditor->set_agent_readiness(AgentTab->setup_readiness());
			}
		});
		TourEditor->set_expand_dispatch([this](const bool aPublication) { open_tour_page(aPublication); });
		TourEditor->set_collapse_dispatch([this] { close_tour_page(); });
		TourEditor->set_excerpt_source([this](const core::tour_source_range& aRange) -> std::optional<tour_excerpt> {
			if (ComparisonSurface == nullptr || ComparisonSurface->current_result() == nullptr || !ComparisonSurface->current_context().has_value()) {
				return std::nullopt;
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
			if (path.toStdString() != aRange.Path) {
				return std::nullopt;
			}
			const auto& result   = *ComparisonSurface->current_result();
			const auto& document = aRange.First.Side == core::diff_side::left ? result.LeftDocument : result.RightDocument;
			if (!document.has_value() || document->Lines.empty()) {
				return std::nullopt;
			}
			const auto last  = static_cast<std::uint32_t>(document->Lines.size() - 1U);
			const auto first = aRange.First.Line > 3U ? aRange.First.Line - 3U : 0U;
			const auto end   = std::min(last, aRange.Last.Line + 3U);
			const auto lines = core::suggestion_source_lines(*document, first, end);
			if (!lines.has_value()) {
				return std::nullopt;
			}
			auto excerpt      = tour_excerpt{ };
			excerpt.First     = first + 1U;
			excerpt.StopFirst = aRange.First.Line + 1U;
			excerpt.StopLast  = aRange.Last.Line + 1U;
			for (const auto& line : *lines) {
				excerpt.Lines.push_back(QString::fromStdString(line));
			}
			return excerpt;
		});
	}

	void workbench_window::open_tour_page(const bool aPublication)
	{
		if (!TourPageOpen) {
			TourPageOpen     = true;
			auto* const body = static_cast<QHBoxLayout*>(TourPageBody->layout());
			body->addWidget(TourEditor, 1);
			// [[BPR-1925]]. The chat goes with the tour to its page.
			auto* const chat = TourEditor->side_chat();
			chat->setFixedWidth(std::max(chat->minimumSizeHint().width(), 420));
			body->addWidget(chat, 0);
			chat->setVisible(chat->column_open());
			TourEditor->set_mode(tour_editor_mode::page);
			if (Modality != nullptr && Modality->current() != review_modality::review) {
				choose_modality(review_modality::review);
			}
			apply_modality_surface();
			// The page is the whole review area: no drawers beside it, which
			// keep their state for Back to the code.
			apply_drawer_layout();
		}
		TourEditor->show();
		if (aPublication) {
			if (auto* const prepare = TourEditor->findChild<QPushButton*>(QStringLiteral("tourPreparePublication")); prepare != nullptr) {
				prepare->setFocus(Qt::OtherFocusReason);
			}
		} else if (TourEditor->stop_body()->isVisible()) {
			TourEditor->stop_body()->setFocus(Qt::OtherFocusReason);
		} else if (auto* const edit = TourEditor->findChild<QWidget*>(QStringLiteral("tourStopEdit")); edit != nullptr && edit->isVisible()) {
			// [[BPR-1805]]. A read stop: its pencil.
			edit->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::close_tour_page()
	{
		if (!TourPageOpen) {
			return;
		}
		TourPageOpen = false;
		TourHolder->layout()->addWidget(TourEditor);
		// [[BPR-1925]]. Back in the Tour tab's column.
		auto* const chat = TourEditor->side_chat();
		chat->setMinimumWidth(300);
		chat->setMaximumWidth(QWIDGETSIZE_MAX);
		TourChatSplit->insertWidget(1, chat);
		chat->setVisible(chat->column_open());
		TourEditor->set_mode(tour_editor_mode::route);
		TourEditor->show();
		apply_modality_surface();
		apply_drawer_layout();
		if (TrayTabs != nullptr) {
			TrayTabs->setCurrentIndex(tray_tour_tab);
		}
	}

	auto workbench_window::tour_page_open() const noexcept -> bool
	{
		return TourPageOpen;
	}

	auto workbench_window::tour_chat_wide() const -> bool
	{
		return TourChatSplit != nullptr && TourEditor != nullptr && !TourPageOpen && TourEditor->side_chat()->column_open() && TrayTabs != nullptr && TrayTabs->currentIndex() == tray_tour_tab
			&& DrawerStates.Conversations == core::workbench_drawer_state::expanded;
	}

	void workbench_window::show_tour_chat(const bool aShown)
	{
		// [[BPR-1925]]. The column is the Tour tab's (or, on the tour's page,
		// beside the tour there), so it shows with the tab and nowhere else.
		auto* const chat = TourEditor->side_chat();
		chat->setVisible(aShown);
		// The first time, the tour and the chat share the drawer evenly. The
		// mockup gave the chat a little more, but an open stop's card needs
		// its 340 pixels or its actions are cut off ([[BPR-1925]] v002).
		if (TourChatSplit != nullptr && TourChatSplit->count() > 0) {
			TourChatSplit->widget(0)->setMinimumWidth(aShown ? 340 : 0);
		}
		if (aShown && !TourChatSized && chat->parentWidget() == TourChatSplit) {
			TourChatSized = true;
			TourChatSplit->setSizes({ 10'000, 10'000 });
		}
		if (aShown && !TourPageOpen && Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			conversations_drawer_toggle_action()->trigger();
		}
		if (aShown && !TourPageOpen && TrayTabs != nullptr && TrayTabs->currentIndex() != tray_tour_tab) {
			TrayTabs->setCurrentIndex(tray_tour_tab);
		}
		apply_drawer_widths();
		update_tour_chat_badge();
	}

	void workbench_window::update_tour_chat_badge()
	{
		if (TrayTabs == nullptr || TrayTourCount == nullptr || TourEditor == nullptr) {
			return;
		}
		const auto  parts = TourEditor->side_chat()->chat_parts();
		auto* const pill  = static_cast<drawer_count_pill*>(TrayTourCount);
		pill->set_colours(std::pair{ agent_accent(palette()), palette().color(QPalette::Active, QPalette::Window) });
		pill->set_count(parts);
		if (auto* const shown = parts != 0U ? TrayTourCount : nullptr; TrayTabs->tabButton(tray_tour_tab, QTabBar::RightSide) != shown) {
			TrayTabs->setTabButton(tray_tour_tab, QTabBar::RightSide, shown);
			TrayTourCount->setVisible(shown != nullptr);
		}
		TrayTabs->setAccessibleTabName(tray_tour_tab, parts == 0U ? tr("Tour") : parts == 1U ? tr("Tour, one part has a chat")
																							 : tr("Tour, %1 parts have chats").arg(parts));
		fit_tray_tabs();
	}

	auto workbench_window::compose_review_action() const noexcept -> QAction*
	{
		return ComposeReviewAction;
	}

	auto workbench_window::comment_action() const noexcept -> QAction*
	{
		return CommentAction;
	}

	auto workbench_window::comment_menu() const noexcept -> QMenu*
	{
		return CommentMenu;
	}

	void workbench_window::publish(const application::document_tabs_projection& aProjection)
	{
		TabsProjection = aProjection;
		update_go_to_line_action();
		present_document_tabs();
		update_older_pair_notice();
		update_comment_target();
	}

	void workbench_window::set_document_tab_dispatch(document_tab_dispatch aDispatch)
	{
		DocumentTabDispatch = std::move(aDispatch);
		update_tab_actions();
	}

	auto workbench_window::document_tabs() const noexcept -> document_tab_strip*
	{
		return DocumentTabs;
	}

	auto workbench_window::next_tab_action() const noexcept -> QAction*
	{
		return NextTabAction;
	}

	auto workbench_window::previous_tab_action() const noexcept -> QAction*
	{
		return PreviousTabAction;
	}

	auto workbench_window::close_tab_action() const noexcept -> QAction*
	{
		return CloseTabAction;
	}

	auto workbench_window::older_pair_notice() const noexcept -> QWidget*
	{
		return OlderPairNotice;
	}

	auto workbench_window::older_pair_notice_text() const -> QString
	{
		return OlderPairText != nullptr ? OlderPairText->text() : QString{ };
	}

	auto workbench_window::show_later_threads_button() const noexcept -> QPushButton*
	{
		return ShowLaterThreads;
	}

	auto workbench_window::path_of(const core::file_identity& aFile) const -> QString
	{
		const auto* const model = Drawers[0].Panel != nullptr ? Drawers[0].Panel->contents() : nullptr;
		if (model == nullptr) {
			return { };
		}
		for (int row = 0; row < model->rowCount(); ++row) {
			const auto index = model->index(row, 0);
			if (model->file_at(index) == aFile) {
				return model->data(index, Qt::DisplayRole).toString();
			}
		}
		return { };
	}

	auto workbench_window::pair_text(const core::comparison_pair& aPair, const bool aShort) const -> QString
	{
		const auto* const line  = EpochSelectors != nullptr ? EpochSelectors->line() : nullptr;
		const auto        dates = line != nullptr ? line->dates_of(aPair.Left.Identity, aPair.Right.Identity) : std::nullopt;
		if (!dates.has_value()) {
			return QStringLiteral("update %1 → %2").arg(aPair.Left.Identity.Epoch.value()).arg(aPair.Right.Identity.Epoch.value());
		}
		// "Feb 4 → 9" when both are in one month, as the design's pill reads.
		const auto& [from, to] = *dates;
		const auto month       = from.section(QLatin1Char{ ' ' }, 0, 0);
		if (aShort && !month.isEmpty() && to.section(QLatin1Char{ ' ' }, 0, 0) == month) {
			return QStringLiteral("%1 → %2").arg(from, to.section(QLatin1Char{ ' ' }, 1));
		}
		return QStringLiteral("%1 → %2").arg(from, to);
	}

	auto workbench_window::comparing_off_current_head() const -> bool
	{
		return ComparisonSurface != nullptr && ComparisonSurface->current_context().has_value()
			&& !LeftConversationEndpointCurrent && !RightConversationEndpointCurrent;
	}

	void workbench_window::present_document_tabs()
	{
		if (DocumentTabs == nullptr) {
			return;
		}
		auto tabs = std::vector<document_tab_presentation>{ };
		if (TabsProjection.has_value()) {
			tabs.reserve(TabsProjection->Tabs.size());
			for (const auto& entry : TabsProjection->Tabs) {
				tabs.push_back(document_tab_presentation{
					.Id            = entry.Id,
					.Path          = entry.View.File ? path_of(*entry.View.File) : QString{ },
					.OwnPair       = entry.OwnComparison ? pair_text(entry.View.Comparison, true) : QString{ },
					.Pair          = pair_text(entry.View.Comparison, false),
					.State         = entry.State,
					.Active        = entry.Active,
					.OwnComparison = entry.OwnComparison,
				});
			}
			// A tab's name is its file's, with as much of the path before it
			// as tells it apart from another tab's file of the same name.
			const auto suffix = [](const QString& aPath, const qsizetype aParts) {
				const auto parts = aPath.split(QLatin1Char{ '/' }, Qt::SkipEmptyParts);
				return parts.mid(std::max<qsizetype>(0, parts.size() - aParts)).join(QLatin1Char{ '/' });
			};
			for (auto& tab : tabs) {
				if (tab.Path.isEmpty()) {
					tab.Name = tr("Unlisted file");
					continue;
				}
				for (qsizetype parts = 1;; ++parts) {
					tab.Name         = suffix(tab.Path, parts);
					const auto clash = std::ranges::any_of(tabs, [&tab, &suffix, parts](const document_tab_presentation& aOther) {
						return !aOther.Path.isEmpty() && aOther.Path != tab.Path && suffix(aOther.Path, parts) == tab.Name;
					});
					if (!clash || tab.Name == tab.Path) {
						break;
					}
				}
			}
		}
		DocumentTabs->present(std::move(tabs));
		update_tab_actions();
	}

	void workbench_window::update_tab_actions()
	{
		const auto count  = TabsProjection.has_value() ? TabsProjection->Tabs.size() : 0U;
		const auto active = TabsProjection.has_value()
			&& std::ranges::any_of(TabsProjection->Tabs, &application::document_tab_entry::Active);
		const auto wired = static_cast<bool>(DocumentTabDispatch);
		if (NextTabAction != nullptr) {
			NextTabAction->setEnabled(wired && count > 1U);
			PreviousTabAction->setEnabled(wired && count > 1U);
			CloseTabAction->setEnabled(wired && active);
		}
	}

	void workbench_window::update_older_pair_notice()
	{
		if (OlderPairNotice == nullptr || ConversationPanel == nullptr) {
			return;
		}
		const auto active    = TabsProjection.has_value()
			? std::ranges::find(TabsProjection->Tabs, true, &application::document_tab_entry::Active)
			: std::vector<application::document_tab_entry>::const_iterator{ };
		const auto hasActive = TabsProjection.has_value() && active != TabsProjection->Tabs.end() && active->View.File.has_value();
		auto       later     = std::size_t{ 0U };
		if (hasActive && comparing_off_current_head()) {
			later = ConversationPanel->current_head_thread_count(*active->View.File);
		}
		if (later == 0U) {
			OlderPairNotice->hide();
			return;
		}
		static constexpr auto words   = std::array{ "One thread is", "Two threads are", "Three threads are", "Four threads are", "Five threads are",
													"Six threads are", "Seven threads are", "Eight threads are", "Nine threads are" };
		const auto            counted = later <= words.size() ? QString::fromLatin1(words[later - 1U]) : tr("%1 threads are").arg(later);
		const auto            text    = tr("Comparing %1. %2 on later updates of this file.").arg(pair_text(active->View.Comparison, false), counted);
		if (OlderPairText->text() != text) {
			OlderPairText->setText(text);
			OlderPairText->setAccessibleDescription(text);
		}
		OlderPairNotice->show();
	}

	void workbench_window::show_later_threads()
	{
		if (!DocumentTabDispatch || !TabsProjection.has_value()) {
			return;
		}
		const auto& tabs   = TabsProjection->Tabs;
		const auto  active = std::ranges::find(tabs, true, &application::document_tab_entry::Active);
		if (active == tabs.end() || !active->View.File.has_value()) {
			return;
		}
		// A tab of this file on the window's comparison shows its threads
		// where they sit; then this tab back on the window's comparison, which
		// the reviewer asked for; and the pull request's conversation in
		// Discuss when the window's comparison is no later than this tab's.
		const auto sibling = std::ranges::find_if(tabs, [&active](const application::document_tab_entry& aTab) {
			return !aTab.Active && !aTab.OwnComparison && aTab.View.File == active->View.File;
		});
		if (sibling != tabs.end()) {
			DocumentTabDispatch(core::activate_tab_intent{ .Tab = sibling->Id });
			return;
		}
		if (active->OwnComparison) {
			DocumentTabDispatch(core::return_tab_to_window_comparison_intent{ .Tab = active->Id });
			return;
		}
		choose_modality(review_modality::discuss);
	}

	void workbench_window::update_new_comment_group()
	{
		if (NextNewCommentAction == nullptr || ConversationPanel == nullptr
			|| ConversationPanel->cards() == nullptr) {
			return;
		}
		const auto& unread = ConversationPanel->cards()->unread_conversations_in_navigation_order();
		NextNewCommentAction->setEnabled(!unread.empty());
		// [[BPR-1699]]. Offered while anything on the pull request is new.
		if (MarkAllCommentsRead != nullptr) {
			MarkAllCommentsRead->setEnabled(!unread.empty() && static_cast<bool>(ConversationsReadDispatch));
		}
		// A refresh may retain the drawer's visual anchor without a selected
		// file. Only standalone diff presentation falls back to that anchor.
		const auto file       = LastWorkbenchContext.has_value()
			? (Cursor.has_value() ? Cursor->File : std::nullopt)
			: (HasGithubPullRequest ? std::nullopt : conversation_anchor_file());
		const auto fileUnread = file.has_value() ? ConversationPanel->cards()->unread_thread_count(*file) : 0U;
		// [[BPR-1511]]. The file-context badges count this file; navigation
		// continues to walk unread threads across the pull request.
		if (TrayTabs != nullptr && TrayCommentsCount != nullptr) {
			const auto newCount = fileUnread + NewConversationComments;
			static_cast<drawer_count_pill*>(TrayCommentsCount)->set_count(newCount);
			// [[BPR-1684]]. Only a showing count takes room in its tab.
			if (auto* const shown = newCount != 0U ? TrayCommentsCount : nullptr; TrayTabs->tabButton(tray_comments_tab, QTabBar::RightSide) != shown) {
				TrayTabs->setTabButton(tray_comments_tab, QTabBar::RightSide, shown);
				TrayCommentsCount->setVisible(shown != nullptr);
			}
			TrayTabs->setAccessibleTabName(
				tray_comments_tab,
				NewConversationComments > 0U ? tr("Comments, %1 new file threads and %2 new comments on the pull request").arg(fileUnread).arg(NewConversationComments) : fileUnread == 0U ? tr("Comments")
																																														   : tr("Comments, %1 with new comments in this file").arg(fileUnread == 1U ? tr("one thread") : tr("%1 threads").arg(fileUnread)));
			// The count's width and the Comments tooltip follow it.
			fit_tray_tabs();
		}
		const auto threadCount  = ConversationPanel->cards()->unread_threads_in_file_order().size();
		const auto commentCount = unread.size() - threadCount;
		auto       counts       = QStringList{ };
		if (threadCount != 0U) {
			counts.push_back(threadCount == 1U ? QStringLiteral("1 thread") : QStringLiteral("%1 threads").arg(threadCount));
		}
		if (commentCount != 0U) {
			counts.push_back(commentCount == 1U ? QStringLiteral("1 comment not on a file") : QStringLiteral("%1 comments not on a file").arg(commentCount));
		}
		NextNewCommentAction->setToolTip(
			unread.empty() ? QStringLiteral("Next new comment: nothing unread")
						   : QStringLiteral("Next new comment (%1)").arg(counts.join(QStringLiteral(", "))));
		// [[BPR-1129]]. The comments toggle carries the count of threads with a
		// post newer than what I have read, and says it to accessibility.
		if (auto* const toggle = qobject_cast<QToolButton*>(PrimaryToolbar->widgetForAction(Drawers[1].Toggle));
			toggle != nullptr) {
			set_strip_badge(*toggle, fileUnread == 0U ? QString{ } : QString::number(fileUnread));
			const auto counted = fileUnread == 0U
				? Drawers[1].Toggle->toolTip()
				: QStringLiteral("%1 %2 with new comments in this file.").arg(Drawers[1].Toggle->toolTip()).arg(fileUnread == 1U ? QStringLiteral("One thread") : QStringLiteral("%1 threads").arg(fileUnread));
			toggle->setAccessibleDescription(counted);
		}
	}

	void workbench_window::update_compose_mark()
	{
		// [[BPR-1590]]. The sheet's header names the review as its page does.
		if (ReviewSheet != nullptr && ReviewSubmissionEditor != nullptr) {
			ReviewSheet->title()->setText(ReviewSubmissionEditor->title_label()->text());
		}
		if (ComposeReviewAction == nullptr || CommentAction == nullptr) {
			return;
		}
		// [[BPR-1129]]. The mark is the review workflow's draft state, as its
		// editor was last given it; the rule for "kept" is the core's.
		const auto& shown     = ReviewSubmissionEditor != nullptr ? ReviewSubmissionEditor->shown_draft() : std::optional<core::review_submission_draft>{ };
		const auto  keptDraft = shown.has_value() && core::kept(*shown);
		const auto  key       = [this](const QAction* const aAction) {
			return presented_key(aAction);
		};
		// [[BPR-1700]]. The tooltip names the three verdicts the review
		// page offers, so the command says what it is for in every state.
		// [[BPR-1853]]. On my own pull request the page offers Comment only.
		const auto verdicts = own_pull_request_review()
			? QStringLiteral("Comment on the whole pull request. You opened it, so GitHub takes only a comment.")
			: QStringLiteral("Approve, request changes or comment on the whole pull request.");
		const auto compose  = keptDraft
			? QStringLiteral("Compose review (%1)\n%2\nA review draft is kept for this pull request.").arg(key(ComposeReviewAction), verdicts)
			: QStringLiteral("Compose review (%1)\n%2").arg(key(ComposeReviewAction), verdicts);
		ComposeReviewAction->setToolTip(compose);
		ComposeReviewAction->setStatusTip(compose);
		update_comment_target();
		present_compose_entry();
	}

	auto workbench_window::own_pull_request_review() const -> bool
	{
		// [[BPR-1853]]. The core's decision, as the review page was last given it.
		return ReviewSubmissionEditor != nullptr && ReviewSubmissionEditor->shown_projection().Controls.Authored;
	}

	void workbench_window::present_compose_entry()
	{
		if (ComposeReviewAction == nullptr || ReviewBar == nullptr) {
			return;
		}
		auto* const button = qobject_cast<QToolButton*>(ReviewBar->widgetForAction(ComposeReviewAction));
		if (button == nullptr) {
			return;
		}
		// [[BPR-1700]]. The owner, 2026-10-21: "I think the compose review
		// button is a little too subtle when there's not a active review. It
		// might be difficult for the uninitiated to find out how to approve a
		// review." With no review in progress it says what it is for, beside
		// its pencil, on the accent's edge. While a review is in progress the
		// chip before it leads, and it is the plain icon again. It only ever
		// opens the review page; the verdict is chosen there.
		const auto& shown      = ReviewSubmissionEditor != nullptr ? ReviewSubmissionEditor->shown_draft() : std::optional<core::review_submission_draft>{ };
		const auto  keptDraft  = shown.has_value() && core::kept(*shown);
		const auto  inProgress = PendingReviewComments > 0U || HeldReviewComments > 0U;
		const auto  worded     = !inProgress && ComposeReviewWords != compose_words::none;
		// The bar's presenter takes its words from the action's icon text; the
		// menus keep the command's own name. Where the bar is short of room
		// the words keep Approve, the verdict a newcomer looks for.
		// [[BPR-1853]]. On my own pull request there is nothing to approve.
		const auto own   = own_pull_request_review();
		const auto words = !worded                       ? tr("Compose review")
			: own                                        ? tr("Comment…")
			: ComposeReviewWords == compose_words::brief ? tr("Approve…")
														 : tr("Approve or comment…");
		if (ComposeReviewAction->iconText() != words) {
			ComposeReviewAction->setIconText(words);
		}
		const auto style = worded ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly;
		if (button->toolButtonStyle() != style) {
			button->setToolButtonStyle(style);
		}
		// Its name keeps the command's and, with no review in progress, holds
		// the words it shows, also when the bar is too narrow for them.
		button->setAccessibleName(inProgress ? tr("Compose review") : own ? tr("Compose review: comment")
																		  : tr("Compose review: approve or comment"));
		const auto purpose = own ? tr("Open the review page, where you comment on the whole pull request; you opened it, so GitHub takes only a comment")
								 : tr("Open the review page, where you choose Approve, Request changes or Comment for the whole pull request");
		button->setAccessibleDescription(keptDraft ? purpose + tr("; a review draft is kept for this pull request") : purpose);
		if (button->property(strip_style::mark_property).toBool() != keptDraft) {
			button->setProperty(strip_style::mark_property, keptDraft);
			button->update();
		}
		if (button->property(strip_style::lead_property).toBool() == inProgress) {
			button->setProperty(strip_style::lead_property, !inProgress);
			button->update();
		}
	}

	auto workbench_window::comment_reply_thread() const -> std::optional<std::string>
	{
		if (ConversationPanel == nullptr) {
			return std::nullopt;
		}
		auto root = ConversationPanel->view()->currentIndex();
		while (root.isValid() && root.parent().isValid()) {
			root = root.parent();
		}
		if (!root.isValid()
			|| root.data(github_conversation_model::anchor_availability_role).toInt()
				!= static_cast<int>(application::github_conversation_anchor_availability::current)) {
			return std::nullopt;
		}
		auto identity = ConversationPanel->model()->id_for_index(root);
		if (!identity.starts_with("review-thread:")) {
			return std::nullopt;
		}
		// The reviewer's choice of thread, or the thread on the line they
		// selected: outside Discuss the panel lists only that line's threads
		// while a line is selected, so its current row is one of them. A row
		// that is current only by default otherwise answers nothing.
		// [[BPR-1293]]. In Threads, as in Discuss, Comment answers the
		// thread the reviewer picked and never a selected line.
		const auto discuss = Modality != nullptr && (Modality->current() == review_modality::discuss || Modality->current() == review_modality::threads);
		if (ConversationPanel->current_is_default() && (discuss || !ConversationPanel->anchor_line().has_value())) {
			return std::nullopt;
		}
		return identity;
	}

	auto workbench_window::comment_target_text() const -> QString
	{
		if (const auto thread = comment_reply_thread(); thread.has_value()) {
			const auto author = ConversationPanel->model()->index_for_id(*thread).data(github_conversation_model::author_login_role).toString();
			return author.isEmpty() ? tr("&Reply to the selected thread…") : tr("&Reply to %1's thread…").arg(author);
		}
		const auto discuss = Modality != nullptr && (Modality->current() == review_modality::discuss || Modality->current() == review_modality::threads);
		if (!discuss && ComparisonSurface != nullptr) {
			if (const auto selection = ComparisonSurface->selection(); selection.has_value()) {
				// [[BPR-1724]]. Comment takes several selected lines on
				// one side together, and says so.
				if (const auto range = ComparisonSurface->line_draft_range(); range.has_value() && range->first.Side == range->second.Side && range->first.Line != range->second.Line) {
					return tr("Co&mment on lines %1–%2…")
						.arg(static_cast<qulonglong>(std::min(range->first.Line, range->second.Line)) + 1U)
						.arg(static_cast<qulonglong>(std::max(range->first.Line, range->second.Line)) + 1U);
				}
				return tr("Co&mment on line %1…").arg(static_cast<qulonglong>(selection->PreferredLine.Line) + 1U);
			}
		}
		return { };
	}

	auto workbench_window::comment_target_reason(const bool aDraftRequired, QString* const aShort) const -> QString
	{
		const auto say = [aShort](QString aReason, QString aWords) {
			if (aShort != nullptr) {
				*aShort = std::move(aWords);
			}
			return aReason;
		};
		const auto selection = ComparisonSurface != nullptr ? ComparisonSurface->selection() : std::nullopt;
		if (comparing_off_current_head()) {
			return say(tr("Select the pull request's current head; this tab compares an earlier update."), tr("Comments need the current head"));
		}
		if (comment_target_text().isEmpty()) {
			return say(tr("Select a current-head diff line or thread to comment."), tr("Select a line to comment on"));
		}
		// [[BPR-1212]]. A new comment on a base line is GitHub's `side: LEFT`,
		// counted in the merge base's version of the file. A reply goes to
		// its thread, wherever it was drafted from.
		// RFC-BPR-0089 M4. On a commit-filtered diff the host's core places
		// the line; its refusal is the reason, its pending answer is not one.
		if (const auto [notice, blocks] = filtered_comment_notice(); blocks && filtered_comment_placement() && filtered_comment_placement()->Target.Refusal != core::filtered_comment_refusal::lines_pending
			&& selection && filtered_comment_placement()->Selection == selection->PreferredLine) {
			return say(notice, tr("Cannot comment on this line here"));
		}
		if (selection && selection->PreferredLine.Side == core::diff_side::left && !LeftConversationEndpointCurrent && !comment_reply_thread().has_value() && !FilteredComments) {
			return say(tr("GitHub counts base lines in the merge base's version of this file, and this comparison's base is not known to match it. Comment on a right-side line, or reply to an existing thread."),
					   tr("This file's base is not GitHub's"));
		}
		const auto draftReady = ReviewEditor != nullptr
			&& (!ReviewEditor->body_editor()->isReadOnly() || ReviewEditor->reload_draft_action()->isEnabled() || ReviewEditor->reapply_draft_action()->isEnabled())
			&& ReviewEditor->shown_draft() && selection
			&& ReviewEditor->shown_draft()->Anchor == selection->PreferredLine;
		if (aDraftRequired && !draftReady && !CommentDraftUnavailable.isEmpty()) {
			return say(CommentDraftUnavailable, tr("Open Saved comments to recover"));
		}
		if (aDraftRequired && !draftReady) {
			return say(tr("No editable draft is available. Wait for the review to load, or open an active pull request with an account that can comment."), tr("No draft available"));
		}
		return { };
	}

	void workbench_window::update_comment_target()
	{
		if (CommentAction == nullptr) {
			return;
		}
		// [[BPR-1133]]. The command names what it would start, in the bar's
		// tooltip, in its arrow's menu and in the Pull Request menu.
		const auto target = comment_target_text();
		auto       words  = target;
		words.remove(QLatin1Char{ '&' });
		words.remove(QChar{ 0x2026 });
		const auto key                = presented_key(CommentAction);
		const auto reason             = comment_target_reason(true);
		const auto promotionAvailable = reason.isEmpty() && !ReviewEditor->body_editor()->isReadOnly();
		const auto promotionReason    = !reason.isEmpty() ? reason : tr("Resolve the retained draft conflict before promoting an item.");
		if (PrivateNotes != nullptr) {
			PrivateNotes->set_promotion_target(promotionAvailable, promotionReason);
		}
		if (SideChat != nullptr) {
			SideChat->set_promotion_target(promotionAvailable, promotionReason);
		}
		CommentAction->setEnabled(reason.isEmpty());
		CommentAction->setText(reason.isEmpty() ? target : tr("Co&mment unavailable — %1").arg(reason));
		const auto tip = reason.isEmpty() ? QStringLiteral("%1 (%2)").arg(words, key) : tr("Comment (%1): %2").arg(key, reason);
		CommentAction->setToolTip(tip);
		CommentAction->setStatusTip(tip);
		if (ReviewBar == nullptr) {
			return;
		}
		if (auto* const button = qobject_cast<QToolButton*>(ReviewBar->widgetForAction(CommentAction)); button != nullptr) {
			button->setAccessibleName(QStringLiteral("Comment"));
			button->setAccessibleDescription(reason.isEmpty() ? QStringLiteral("%1, in the drawer beside the code, without publishing").arg(words) : reason);
		}
	}

	void workbench_window::comment_or_reply()
	{
		update_comment_target();
		if (!CommentAction->isEnabled()) {
			return;
		}
		if (const auto thread = comment_reply_thread(); thread.has_value()) {
			reply_to_thread(*thread, true);
			return;
		}
		// [[BPR-1724]]. Several selected lines take one comment on all
		// of them, through the path Return and Add comment take; it opened the
		// selected line's own draft.
		if (const auto range = ComparisonSurface != nullptr ? ComparisonSurface->line_draft_range() : std::nullopt; range.has_value() && range->first != range->second) {
			static_cast<void>(comment_on_selected_lines(false));
			return;
		}
		static_cast<void>(begin_new_comment(false));
	}

	void workbench_window::set_conversation_ask_dispatch(std::function<void(const std::string&)> aDispatch)
	{
		ConversationAskDispatch = std::move(aDispatch);
	}

	void workbench_window::open_agent_question()
	{
		show_agent(false);
		if (AgentTab != nullptr) {
			AgentTab->focus_question();
		}
	}

	auto workbench_window::compared_lines(const std::string& aPath, const core::diff_side aSide, const std::uint32_t aFirst, const std::uint32_t aLast) const
		-> std::optional<std::pair<std::uint32_t, std::vector<std::string>>>
	{
		if (ComparisonSurface == nullptr || ComparisonSurface->current_result() == nullptr || !ComparisonSurface->current_context().has_value()
			|| ComparisonSurface->current_context()->Key.File != core::file_identity_of(aPath) || aFirst == 0U || aLast < aFirst) {
			return std::nullopt;
		}
		const auto& result   = *ComparisonSurface->current_result();
		const auto& document = aSide == core::diff_side::right ? result.RightDocument : result.LeftDocument;
		if (!document.has_value() || document->Lines.empty() || aFirst > document->Lines.size()) {
			return std::nullopt;
		}
		const auto last  = std::min<std::uint32_t>(aLast, static_cast<std::uint32_t>(document->Lines.size()));
		auto       lines = core::suggestion_source_lines(*document, aFirst - 1U, last - 1U);
		if (!lines.has_value()) {
			return std::nullopt;
		}
		return std::pair{ aFirst, std::move(*lines) };
	}
}
