/// \file
/// \brief RFC-BPR-0118 ([[BPR-2078]]). Suggest the change on the drawer
/// draft: the press, the agent's answer offered to the core, the accepted
/// change opened in the head pane unsaved (RFC-BPR-0090's edit), Keep change,
/// Discard, Undo and the refusal. The core decides; the window shows.

#include "bpr/core/pull_request_suggestion.hpp"
#include "bpr/core/suggest_change.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/suggestion_editor.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QPlainTextEdit>
#include <QStatusBar>

#include <string>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto lines_words(const std::uint32_t aFirst, const std::uint32_t aLast) -> QString
		{
			return aFirst == aLast ? QObject::tr("line %1").arg(aLast + 1U) : QObject::tr("lines %1–%2").arg(aFirst + 1U).arg(aLast + 1U);
		}

		/// Which check failed, in a few words after "did not apply to lines a–b:".
		[[nodiscard]] auto refusal_words(const core::suggest_change_refusal aRefusal) -> QString
		{
			switch (aRefusal) {
			case core::suggest_change_refusal::unreadable:      return QObject::tr("the reply was not a change to these lines.");
			case core::suggest_change_refusal::head_moved:      return QObject::tr("the pull request changed these lines while it ran.");
			case core::suggest_change_refusal::range_differs:   return QObject::tr("it named other lines.");
			case core::suggest_change_refusal::current_differs: return QObject::tr("the text it quoted differs from the head.");
			case core::suggest_change_refusal::identical:       return QObject::tr("it changes nothing.");
			case core::suggest_change_refusal::invalid_text:    return QObject::tr("its text is not valid.");
			case core::suggest_change_refusal::too_large:       return QObject::tr("the change is too long for a suggestion.");
			case core::suggest_change_refusal::not_suggestible: return QObject::tr("GitHub can't take it as a suggestion here.");
			case core::suggest_change_refusal::none:            break;
			}
			return { };
		}
	} // namespace

	void workbench_window::set_suggest_change_dispatch(std::function<void(const suggest_change_press&)> aDispatch, std::function<void()> aCancel)
	{
		SuggestChangeDispatch = std::move(aDispatch);
		SuggestChangeCancel   = std::move(aCancel);
		present_inline_draft();
	}

	void workbench_window::set_suggest_change_ready(const bool aReady)
	{
		if (SuggestChangeReady != aReady) {
			SuggestChangeReady = aReady;
			present_inline_draft();
		}
	}

	auto workbench_window::suggest_change_offered() const -> bool
	{
		if (ReviewEditor == nullptr || ComparisonSurface == nullptr || !SuggestChangeDispatch) {
			return false;
		}
		const auto& draft  = ReviewEditor->shown_draft();
		const auto* result = ComparisonSurface->current_result().get();
		if (!draft.has_value() || draft->ReplyToThread.has_value() || draft->shared() || ReviewEditor->body_editor()->isReadOnly() || result == nullptr
			|| draft->Anchor.Side != core::diff_side::right || draft->Anchor.Comparison != result->Alignment.Digest || !result->RightDocument.has_value()) {
			return false;
		}
		// RFC-BPR-0090's range rules, as Suggest change has them.
		const auto decision = core::comment_range_decision{ .Anchor = draft->Anchor, .StartLine = draft->StartLine };
		return suggestion_range_reason(decision).isEmpty();
	}

	auto workbench_window::agent_suggestion_presentation_now() const -> agent_suggestion_presentation
	{
		auto        shown = agent_suggestion_presentation{ .Offered = suggest_change_offered(), .Ready = SuggestChangeReady };
		const auto& draft = ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::nullopt;
		if (AgentSuggestion.has_value() && draft.has_value() && draft->Id == AgentSuggestion->Draft) {
			shown.Phase = AgentSuggestion->Phase;
			shown.Line  = AgentSuggestion->Line;
			shown.Reply = AgentSuggestion->Reply;
		}
		return shown;
	}

	void workbench_window::press_suggest_change()
	{
		if (AgentSuggestion.has_value() && AgentSuggestion->Phase == agent_suggestion_phase::running) {
			// Stop: the run ends and nothing changes.
			if (SuggestChangeCancel) {
				SuggestChangeCancel();
			}
			AgentSuggestion->Phase = agent_suggestion_phase::refused;
			AgentSuggestion->Line  = tr("Stopped. Your comment is unchanged.");
			AgentSuggestion->Reply.clear();
			present_inline_draft();
			return;
		}
		if (!suggest_change_offered()) {
			return;
		}
		const auto& draft  = *ReviewEditor->shown_draft();
		const auto& result = *ComparisonSurface->current_result();
		const auto  bytes  = std::string{ result.RightDocument->Bytes.begin(), result.RightDocument->Bytes.end() };
		auto        head   = core::split_text_lines(bytes);
		const auto  first  = draft.StartLine.value_or(draft.Anchor.Line);
		const auto  last   = draft.Anchor.Line;
		if (first > last || last >= head.size()) {
			return;
		}
		// The reviewer's words: the field's while the code holds the block,
		// otherwise the body without a suggestion block of its own.
		auto prose = std::string{ };
		if (PaneSuggestion.has_value() && PaneSuggestion->Anchor == draft.Anchor) {
			prose = PaneSuggestion->Prose.toStdString();
		} else {
			prose = ReviewEditor->body_editor()->toPlainText().toStdString();
			if (const auto split = core::split_suggestion_body(prose); split.has_value()) {
				prose = split->Prose;
			}
		}
		AgentSuggestion = agent_suggestion_state{
			.Draft  = draft.Id,
			.Anchor = draft.Anchor,
			.First  = first,
			.Last   = last,
			.Asked  = std::vector<core::text_line>{ head.begin() + first, head.begin() + last + 1U },
			.Phase  = agent_suggestion_phase::running,
			.Line   = tr("Writing the change for %1…").arg(lines_words(first, last)),
		};
		if (!SuggestChangeReady) {
			refuse_suggest_change(tr("Set up an agent in the Agent tab first."));
			return;
		}
		present_inline_draft();
		SuggestChangeDispatch(suggest_change_press{ .Draft = draft.Id, .Anchor = draft.Anchor, .First = first, .Last = last, .Head = std::move(head), .Prose = std::move(prose) });
	}

	void workbench_window::refuse_suggest_change(const QString& aWords)
	{
		if (!AgentSuggestion.has_value() || AgentSuggestion->Phase != agent_suggestion_phase::running) {
			return;
		}
		AgentSuggestion->Phase = agent_suggestion_phase::refused;
		AgentSuggestion->Line  = aWords;
		AgentSuggestion->Reply.clear();
		present_inline_draft();
	}

	auto workbench_window::offer_suggested_change(const std::string& aAnswer) -> core::suggest_change_refusal
	{
		if (!AgentSuggestion.has_value() || AgentSuggestion->Phase != agent_suggestion_phase::running || ReviewEditor == nullptr || ComparisonSurface == nullptr) {
			return core::suggest_change_refusal::head_moved;
		}
		auto&       asked  = *AgentSuggestion;
		const auto& draft  = ReviewEditor->shown_draft();
		const auto* result = ComparisonSurface->current_result().get();
		// The head the press read must still be the one shown, with the draft.
		const auto same = draft.has_value() && draft->Id == asked.Draft && draft->Anchor == asked.Anchor && result != nullptr && result->Alignment.Digest == asked.Anchor.Comparison
			&& result->RightDocument.has_value();
		auto now = std::vector<core::text_line>{ };
		if (result != nullptr && result->RightDocument.has_value()) {
			now = core::split_text_lines(std::string{ result->RightDocument->Bytes.begin(), result->RightDocument->Bytes.end() });
		}
		auto prose = std::string{ };
		if (PaneSuggestion.has_value() && PaneSuggestion->Anchor == asked.Anchor) {
			prose = PaneSuggestion->Prose.toStdString();
		}
		auto input = core::suggest_change_input{ .Answer = aAnswer, .First = asked.First, .Last = asked.Last, .Asked = asked.Asked, .SameHead = same, .Head = now, .Prose = prose };
		// On a filtered or Whole PR diff, where the host places the line.
		if (FilteredComments) {
			input.Filtered = FilteredPlacement.has_value() && FilteredPlacement->Selection == asked.Anchor
				? FilteredPlacement->Target
				: core::filtered_comment_target{ .Placement = core::filtered_comment_placement::refused, .Refusal = core::filtered_comment_refusal::lines_pending };
		} else if (WholeDiffPlacement.has_value() && WholeDiffPlacement->Selection == asked.Anchor) {
			input.Filtered = WholeDiffPlacement->Target;
		}
		auto decision = core::decide_suggested_change(input);
		if (!draft.has_value() || draft->Id != asked.Draft) {
			// The draft is no longer the one shown: nothing to open it on.
			AgentSuggestion.reset();
			statusBar()->showMessage(tr("The agent's change arrived after you left that comment. Nothing changed."), 8000);
			return decision.accepted() ? core::suggest_change_refusal::head_moved : decision.Refusal;
		}
		const auto refuse = [this, &aAnswer](const core::suggest_change_refusal aRefusal) {
			AgentSuggestion->Phase = agent_suggestion_phase::refused;
			AgentSuggestion->Line  = tr("The agent's change did not apply to %1: %2").arg(lines_words(AgentSuggestion->First, AgentSuggestion->Last), refusal_words(aRefusal));
			AgentSuggestion->Reply = QString::fromStdString(aAnswer);
			present_inline_draft();
			return aRefusal;
		};
		if (!decision.accepted()) {
			return refuse(decision.Refusal);
		}
		// Open RFC-BPR-0090's edit on the draft's lines, keep what it holds,
		// then write the agent's lines as one edit Undo takes back.
		asked.WasOpen = PaneSuggestion.has_value() && PaneSuggestion->Open && PaneSuggestion->Anchor == asked.Anchor;
		if (!asked.WasOpen && !begin_pane_suggestion(false)) {
			return refuse(core::suggest_change_refusal::not_suggestible);
		}
		auto* const editor = ComparisonSurface->head_editor();
		const auto  range  = editor != nullptr ? editor->model().editable_range() : std::nullopt;
		if (editor == nullptr || !range.has_value() || !PaneSuggestion.has_value()) {
			return refuse(core::suggest_change_refusal::not_suggestible);
		}
		const auto& lines    = editor->model().state().Lines;
		PaneSuggestion->Kept = std::vector<core::text_line>{ lines.begin() + static_cast<std::ptrdiff_t>(range->first), lines.begin() + static_cast<std::ptrdiff_t>(range->second) + 1 };
		PaneSuggestion->Proposed = true;
		auto text                = std::string{ };
		for (std::size_t index = 0U; index < decision.Block.size(); ++index) {
			text += (index == 0U ? "" : "\n") + decision.Block[index];
		}
		static_cast<void>(editor->apply(core::select_all{ }));
		if (text.empty()) {
			static_cast<void>(editor->apply(core::delete_backward{ }));
		} else {
			static_cast<void>(editor->apply(core::insert_text{ .Text = text }));
		}
		// The edit must hold exactly the block the core accepted.
		const auto plan = pane_suggestion_plan();
		if (!plan.has_value() || plan->Block != decision.Block) {
			PaneSuggestion->Proposed = false;
			if (asked.WasOpen) {
				static_cast<void>(editor->apply(core::undo_edit{ }));
			} else {
				release_pane_suggestion();
			}
			return refuse(core::suggest_change_refusal::not_suggestible);
		}
		asked.Phase = agent_suggestion_phase::proposed;
		asked.Line  = tr("The agent's change is in the code. It is not in your comment yet.");
		asked.Reply.clear();
		statusBar()->showMessage(tr("Review the agent's change to %1 in the code. Keep change puts it in your comment.").arg(lines_words(asked.First, asked.Last)), 8000);
		present_inline_draft();
		return core::suggest_change_refusal::none;
	}

	void workbench_window::act_on_agent_suggestion(const card_action aAction)
	{
		switch (aAction) {
		case card_action::suggest_change: press_suggest_change(); return;
		case card_action::keep_suggested_change:
			if (AgentSuggestion.has_value() && AgentSuggestion->Phase == agent_suggestion_phase::proposed && PaneSuggestion.has_value() && PaneSuggestion->Proposed && ReviewEditor != nullptr) {
				// The block goes into the draft; the words stay as written.
				AgentSuggestion->BodyBefore = ReviewEditor->body_editor()->toPlainText();
				PaneSuggestion->Proposed    = false;
				compose_pane_suggestion();
				AgentSuggestion->Phase = agent_suggestion_phase::kept;
				AgentSuggestion->Line  = tr("Added to your comment. Your words are as you wrote them.");
				present_inline_draft();
			}
			return;
		case card_action::discard_suggested_change:
			if (AgentSuggestion.has_value() && PaneSuggestion.has_value() && PaneSuggestion->Proposed) {
				const auto wasOpen       = AgentSuggestion->WasOpen;
				PaneSuggestion->Proposed = false;
				AgentSuggestion.reset();
				if (auto* const editor = ComparisonSurface != nullptr ? ComparisonSurface->head_editor() : nullptr; wasOpen && editor != nullptr) {
					// Back to the lines the edit held before the agent's.
					auto restore = std::string{ };
					for (std::size_t index = 0U; index < PaneSuggestion->Kept.size(); ++index) {
						restore += (index == 0U ? "" : "\n") + PaneSuggestion->Kept[index].Content;
					}
					static_cast<void>(editor->apply(core::select_all{ }));
					static_cast<void>(editor->apply(restore.empty() ? core::suggestion_edit{ core::delete_backward{ } } : core::suggestion_edit{ core::insert_text{ .Text = restore } }));
				} else {
					release_pane_suggestion();
				}
				statusBar()->showMessage(tr("Discarded the agent's change. Your comment is unchanged."), 6000);
				present_inline_draft();
			}
			return;
		case card_action::undo_suggested_change:
			if (AgentSuggestion.has_value() && AgentSuggestion->Phase == agent_suggestion_phase::kept && ReviewEditor != nullptr && !ReviewEditor->body_editor()->isReadOnly()) {
				// One Undo: the draft exactly as it was before Keep change.
				const auto before  = AgentSuggestion->BodyBefore;
				const auto wasOpen = AgentSuggestion->WasOpen;
				AgentSuggestion.reset();
				release_pane_suggestion();
				ReviewEditor->body_editor()->setPlainText(before);
				if (wasOpen && core::split_suggestion_body(before.toStdString()).has_value()) {
					static_cast<void>(begin_pane_suggestion(false));
				}
				statusBar()->showMessage(tr("Your comment is back as it was."), 6000);
				present_inline_draft();
			}
			return;
		case card_action::close_suggested_change:
			if (AgentSuggestion.has_value() && AgentSuggestion->Phase != agent_suggestion_phase::running && AgentSuggestion->Phase != agent_suggestion_phase::proposed) {
				AgentSuggestion.reset();
				present_inline_draft();
			}
			return;
		default: return;
		}
	}
} // namespace bpr::qt
