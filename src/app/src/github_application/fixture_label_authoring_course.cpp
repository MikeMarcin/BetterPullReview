#include "github_application/fixture_course_support.hpp"
#include "review_cycle_desktop.hpp"
#include "review_submission_workflow.hpp"

#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/application/document_tab_host.hpp"
	#include "bpr/application/github_epoch_handoff.hpp"
	#include "bpr/application/github_read_model.hpp"
	#include "bpr/application/github_window_read_host.hpp"
	#include "bpr/application/review_ledger_application_host.hpp"
	#include "bpr/application/review_ledger_port.hpp"
	#include "bpr/application/review_mutation_application_host.hpp"
	#include "bpr/application/workbench_application_host.hpp"
	#include "bpr/application/workbench_runtime_ports.hpp"
	#include "bpr/core/defect_workflow.hpp"
	#include "bpr/core/diff_compute.hpp"
	#include "bpr/core/diff_model.hpp"
	#include "bpr/core/diff_semantics.hpp"
	#include "bpr/core/document_tabs.hpp"
	#include "bpr/core/epoch_model.hpp"
	#include "bpr/core/feedback_convention.hpp"
	#include "bpr/core/filtered_comment.hpp"
	#include "bpr/core/github_model.hpp"
	#include "bpr/core/model.hpp"
	#include "bpr/core/pull_request_suggestion.hpp"
	#include "bpr/core/review_comment_range.hpp"
	#include "bpr/core/review_ledger.hpp"
	#include "bpr/core/review_round_trip.hpp"
	#include "bpr/core/review_tour_markdown.hpp"
	#include "bpr/core/suggestion_applicability.hpp"
	#include "bpr/core/suggestion_draft.hpp"
	#include "bpr/core/suggestion_text_editing.hpp"
	#include "bpr/core/window_review_restoration.hpp"
	#include "bpr/qt/conversation_cards_view.hpp"
	#include "bpr/qt/diff_workbench.hpp"
	#include "bpr/qt/feedback_label_controls.hpp"
	#include "bpr/qt/feedback_label_marks.hpp"
	#include "bpr/qt/github_conversation_model.hpp"
	#include "bpr/qt/github_conversation_panel.hpp"
	#include "bpr/qt/review_draft_editor.hpp"
	#include "bpr/qt/suggestion_editor.hpp"
	#include "bpr/qt/suggestion_mark.hpp"
	#include "bpr/qt/workbench_drawer.hpp"
	#include "bpr/qt/workbench_strip.hpp"
	#include "bpr/qt/workbench_window.hpp"
	#include "github_application/defect_identity.hpp"
	#include "github_application/window_composition.hpp"
	#include "github_application/window_probe_states.hpp"
	#include "github_application_fixture_access.hpp"
	#include "github_public_path_fixture.hpp"
	#include <QAbstractButton>
	#include <QAbstractItemView>
	#include <QAbstractScrollArea>
	#include <QAction>
	#include <QAnyStringView>
	#include <QApplication>
	#include <QByteArray>
	#include <QByteArrayView>
	#include <QChar>
	#include <QCoreApplication>
	#include <QCursor>
	#include <QDesktopServices>
	#include <QDir>
	#include <QEvent>
	#include <QEventLoop>
	#include <QFlags>
	#include <QKeyEvent>
	#include <QLabel>
	#include <QList>
	#include <QListView>
	#include <QMainWindow>
	#include <QMenu>
	#include <QModelIndex>
	#include <QObject>
	#include <QPixmap>
	#include <QPlainTextEdit>
	#include <QPoint>
	#include <QPointF>
	#include <QPushButton>
	#include <QRect>
	#include <QScrollArea>
	#include <QStandardPaths>
	#include <QStatusBar>
	#include <QString>
	#include <QTextBlock>
	#include <QTextCursor>
	#include <QTextDocument>
	#include <QTextStream>
	#include <QToolButton>
	#include <QVariant>
	#include <QWidget>
	#include <algorithm>
	#include <array>
	#include <cstddef>
	#include <filesystem>
	#include <qpa/qwindowsysteminterface.h>
	#include <span>
	#include <string_view>
	#include <utility>
	#include <variant>
	#include <vector>
#endif
#if defined(BPR_LOCAL_AUTOMATION)
	#include "cycle_owner_service.hpp"
#endif

namespace bpr::app {
#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::reveal_fixture_file(
		github_application& aApplication, const std::string_view aPath) -> detail::fixture_file_reveal_result
	{
		using result               = detail::fixture_file_reveal_result;
		using state_type           = fixture_file_reveal_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->FileReveal.has_value() || implementation->FileReveal->Path != aPath) {
			implementation->FileReveal = state_type{ .Path = std::string{ aPath } };
		}
		auto& reveal = *implementation->FileReveal;
		if (reveal.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (reveal.Phase == state_type::phase::shown) {
			return result::shown;
		}
		if (implementation->Window == nullptr || implementation->WorkbenchHost == nullptr
			|| implementation->WorkbenchHost->outstanding_requests() != 0U) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const view   = window->files_drawer() != nullptr ? window->files_drawer()->contents_view() : nullptr;
		if (reveal.Phase == state_type::phase::awaiting_files) {
			const auto row = drawer_row(*window, aPath);
			if (view == nullptr || !row.isValid() || window->showing_home()) {
				return result::pending;
			}
			// The reviewer's way in: Review on the strip, then the file's row.
			window->choose_modality(qt::review_modality::review);
			QCoreApplication::processEvents();
			// [[BPR-1345]]. Choosing Review can re-project the drawer, so the
			// row is found again: a stale index selects nothing and the reveal
			// would wait for a diff no one asked for.
			const auto shown = drawer_row(*window, aPath);
			if (!shown.isValid()) {
				return result::pending;
			}
			view->setCurrentIndex(shown);
			reveal.Phase = state_type::phase::awaiting_diff;
			return result::pending;
		}
		const auto& last = implementation->LastSelectedDiff;
		if (!last.has_value() || last->Request.File.PresentationPath != aPath
			|| window->comparison_surface()->current_result() == nullptr
			|| window->current_modality() != qt::review_modality::review) {
			return result::pending;
		}
		reveal.Phase = state_type::phase::shown;
		return result::shown;
	}

	auto detail::github_application_fixture_access::accepted_head(const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->LastAcceptedHandoff.has_value()) {
			return { };
		}
		return std::string{ core::canonical_hex(implementation->LastAcceptedHandoff->Head).view() };
	}

	auto detail::github_application_fixture_access::window_read_progress(const github_application& aApplication) -> window_read_progress_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->WindowRead) {
			return { };
		}
		const auto& snapshot = implementation->WindowRead->accepted_snapshot();
		return {
			.Snapshot = snapshot ? std::optional{ snapshot->Snapshot.value() } : std::nullopt,
			.Reading  = implementation->WindowRead->state().PendingPullRequest.has_value() || implementation->RereadOwed || implementation->RereadQueued,
		};
	}

	auto detail::github_application_fixture_access::seed_attended_comment_draft(
		github_application& aApplication, const core::diff_side aSide, const std::string& aBody) -> detail::fixture_file_reveal_result
	{
		using result               = detail::fixture_file_reveal_result;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Window == nullptr) {
			return result::failed;
		}
		auto* const window = &*implementation->Window;
		auto* const editor = window->review_editor();
		auto* const model  = window->conversation_model();
		if (editor == nullptr || model == nullptr) {
			return result::pending;
		}
		const auto& draft = implementation->ReviewDraft;
		if (draft && draft->Body == aBody && draft->State == core::review_draft_state::drafting && editor->body_editor()->toPlainText().toStdString() == aBody) {
			return result::shown;
		}
		if (window->current_modality() != qt::review_modality::review) {
			window->choose_modality(qt::review_modality::review);
			QCoreApplication::processEvents();
			return result::pending;
		}
		// Chosen once and then kept selected, as the label-authoring course does.
		auto& line = implementation->AttendedDraftLine;
		if (!line) {
			// [[BPR-1735]]. A line GitHub places in its own diff, once that
			// diff is read where GitHub names a merge base: a line outside
			// every hunk publishes as a file comment, not the line comment
			// these courses write.
			const auto lines = implementation->shown_github_diff_lines();
			if (!lines) {
				return result::pending;
			}
			for (auto candidate = std::uint32_t{ 0U }; candidate < 64U && !line; ++candidate) {
				if (core::place_on_github_diff(*lines, aSide, candidate, std::nullopt) != core::github_diff_placement::outside_diff
					&& window->comparison_surface()->select_conversation_line(aSide, candidate)
					&& window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0) {
					line = candidate;
				}
			}
			return line ? result::pending : result::failed;
		}
		if (const auto selected = window->comparison_surface()->selection();
			!selected || selected->PreferredLine.Side != aSide || selected->PreferredLine.Line != *line) {
			static_cast<void>(window->comparison_surface()->select_conversation_line(aSide, *line));
		}
		if (!draft || draft->ReplyToThread || draft->Anchor.Line != *line || draft->Anchor.Side != aSide
			|| draft->State != core::review_draft_state::drafting || !window->comment_action()->isEnabled()) {
			return result::pending;
		}
		window->comment_action()->trigger();
		QCoreApplication::processEvents();
		if (editor->body_editor()->isReadOnly()) {
			return result::failed;
		}
		editor->body_editor()->setPlainText(QString::fromStdString(aBody));
		return result::pending;
	}

	auto detail::github_application_fixture_access::exercise_label_authoring(
		github_application& aApplication, const std::optional<std::string>& aEvidenceDirectory, const bool aLineDraftOnly) -> detail::label_authoring_probe_result
	{
		using result               = detail::label_authoring_probe_result;
		using state_type           = label_authoring_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->LabelAuthoringProbe.has_value()) {
			implementation->LabelAuthoringProbe.emplace();
			implementation->LabelAuthoringProbe->LineDraftOnly = aLineDraftOnly;
			// [[BPR-1735]]. This course's lines lie outside GitHub's diff of its
			// fixture and it publishes them natively; its subject is the
			// labels, so it places as a build before the hunk rule did.
			implementation->WholeDiffPlacementDisabled = true;
		}
		auto& probe = *implementation->LabelAuthoringProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr || implementation->ReviewMutationHost == nullptr) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const cards  = window->conversation_panel() != nullptr ? window->conversation_panel()->cards() : nullptr;
		auto* const editor = window->review_editor();
		auto* const model  = window->conversation_model();
		if (cards == nullptr || editor == nullptr || model == nullptr || editor->label_controls() == nullptr) {
			return result::pending;
		}
		auto* const controls = editor->label_controls();
		auto* const body     = editor->body_editor();
		const auto  fail     = [&probe](const std::string_view aWhy) {
			probe.Diagnostic += " | phase=" + std::to_string(static_cast<int>(probe.Phase)) + " failed=" + std::string{ aWhy };
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		const auto note = [&probe](std::string aFacts) {
			const auto key   = "phase=" + std::to_string(static_cast<int>(probe.Phase)) + " ";
			const auto start = probe.Diagnostic.find(key);
			if (start == std::string::npos) {
				probe.Diagnostic += (probe.Diagnostic.empty() ? "" : " | ") + key + std::move(aFacts);
				return;
			}
			const auto end = probe.Diagnostic.find(" | ", start);
			probe.Diagnostic.replace(start, end == std::string::npos ? std::string::npos : end - start, key + std::move(aFacts));
		};
		// Layout evidence for the implementer's own look, never a substitute
		// for the owner's.
		const auto render = [&probe, window, &aEvidenceDirectory](const char* const aName) {
			if (!aEvidenceDirectory.has_value()) {
				return;
			}
			++probe.Renders;
			static_cast<void>(window->grab().save(QDir{ QString::fromStdString(*aEvidenceDirectory) }.filePath(QStringLiteral("%1-%2.png").arg(probe.Renders).arg(QString::fromLatin1(aName)))));
		};
		// One key as a keyboard delivers it: the shortcut pass first, then the press.
		const auto key = [body](const int aKey, const Qt::KeyboardModifiers aModifiers, const QString& aText) {
			auto overrideEvent = QKeyEvent{ QEvent::ShortcutOverride, aKey, aModifiers, aText };
			QCoreApplication::sendEvent(body, &overrideEvent);
			auto press = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers, aText };
			QCoreApplication::sendEvent(body, &press);
			auto release = QKeyEvent{ QEvent::KeyRelease, aKey, aModifiers, aText };
			QCoreApplication::sendEvent(body, &release);
			QCoreApplication::processEvents();
		};
		// [[BPR-1212]]. The same, to a code pane.
		const auto paneKey = [](QWidget& aPane, const int aKey, const Qt::KeyboardModifiers aModifiers) {
			auto overrideEvent = QKeyEvent{ QEvent::ShortcutOverride, aKey, aModifiers };
			QCoreApplication::sendEvent(&aPane, &overrideEvent);
			auto press = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
			QCoreApplication::sendEvent(&aPane, &press);
			auto release = QKeyEvent{ QEvent::KeyRelease, aKey, aModifiers };
			QCoreApplication::sendEvent(&aPane, &release);
			QCoreApplication::processEvents();
		};
		const auto text = [body] { return body->toPlainText(); };
		// Types the next queued character once the draft holds what the body
		// shows; true when nothing is left to type and the two agree.
		const auto typed = [&probe, &key, &text, implementation] {
			const auto& draft  = implementation->ReviewDraft;
			const auto  caught = draft.has_value() && QString::fromStdString(draft->Body) == text();
			if (!caught) {
				return false;
			}
			if (probe.Typing.isEmpty()) {
				return true;
			}
			const auto character = probe.Typing.front();
			probe.Typing.remove(0, 1);
			key(static_cast<int>(character.toUpper().unicode()), Qt::NoModifier, QString{ character });
			return false;
		};
		const auto cardFor = [cards](const std::string_view aId) -> const qt::thread_card* {
			const auto found = std::ranges::find(cards->cards(), std::string{ aId }, &qt::thread_card::Id);
			return found == cards->cards().end() ? nullptr : &*found;
		};
		const auto rootLabel = [](const qt::thread_card* const aCard) -> std::optional<core::feedback_label> {
			return aCard == nullptr || aCard->Posts.empty() ? std::nullopt : aCard->Posts.front().Label;
		};
		constexpr auto path      = std::string_view{ "src/engine/renderer.cpp" };
		constexpr auto subject   = std::string_view{ "Should the width be clamped to one pixel?" };
		constexpr auto published = std::string_view{ "issue: Should the width be clamped to one pixel?" };
		// [[BPR-1084]]. The suggested-change bodies: a multiline replacement
		// with tabs, prose with no patch, a patch the fixture's GitHub refuses
		// an inline comment for, and a reply with a patch.
		const auto patchBody   = QStringLiteral("issue: Clamp the width to one pixel.\n\n```suggestion\n\tconst auto width = std::max(1, requested);\n\treturn width;\n```\n");
		const auto proseBody   = QStringLiteral("suggestion: Consider clamping the width to one pixel.");
		const auto refusedBody = QStringLiteral("issue: BPR-1084 off-hunk suggestion.\n\n```suggestion\n\tclamp(width, 1);\n```\n");
		const auto replyBody   = QStringLiteral("question: Would this read better?\n\n```suggestion:-1+0\n\tdraw_scene(view);\n```\n");
		const auto markState   = [editor]() -> std::string {
			const auto* const mark = editor->suggestion_mark_widget();
			if (mark == nullptr) {
				return "-";
			}
			return mark->isVisibleTo(editor) ? mark->property("suggestionApplicability").toString().toStdString() : std::string{ "hidden" };
		};
		// A root comment on the next current-head line no thread is on.
		const auto rootDraft = [&](const QString& aBody, const state_type::phase aNext) -> result {
			// Back from the last comment's page to the code, in Review.
			auto* const inlinePage = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
			if (inlinePage != nullptr && inlinePage->isVisibleTo(window)) {
				auto* const back = inlinePage->findChild<QPushButton*>(QStringLiteral("composeInlineDraftPageBack"));
				if (back == nullptr) {
					return fail("no-back");
				}
				back->click();
				QCoreApplication::processEvents();
				return result::pending;
			}
			if (window->current_modality() != qt::review_modality::review) {
				window->choose_modality(qt::review_modality::review);
				QCoreApplication::processEvents();
				return result::pending;
			}
			// [[BPR-1305]]. Its refreshed comparison must answer before the line is selected.
			const auto& publication = implementation->SelectedDiffPublication;
			const auto& comparison  = window->comparison_surface()->current_result();
			if (!publication || !comparison || comparison->Alignment.Key != publication->Key
				|| implementation->WorkbenchHost->generation() != publication->Request.Context.Generation
				|| implementation->WorkbenchHost->selected() != std::optional{ publication->Request.File.File }
				|| implementation->WorkbenchHost->outstanding_requests() != 0U) {
				note("awaiting-current-comparison");
				return result::pending;
			}
			// Chosen once and then kept selected: choosing again each tick
			// would move the selection away and back before its draft arrives.
			if (!probe.Line) {
				for (auto candidate = std::uint32_t{ 0U }; candidate < 64U && !probe.Line; ++candidate) {
					if (window->comparison_surface()->select_conversation_line(core::diff_side::right, candidate)
						&& window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0) {
						probe.Line = candidate;
					}
				}
			} else if (const auto selected = window->comparison_surface()->selection();
					   !selected || selected->PreferredLine.Side != core::diff_side::right || selected->PreferredLine.Line != *probe.Line) {
				static_cast<void>(window->comparison_surface()->select_conversation_line(core::diff_side::right, *probe.Line));
			}
			const auto  line  = probe.Line;
			const auto& draft = implementation->ReviewDraft;
			note("line=" + std::to_string(line.value_or(0U)) + " draft=" + (draft ? std::to_string(draft->Anchor.Line) + ":" + std::string{ core::name(draft->State) } : std::string{ "-" })
				 + " comment=" + std::to_string(window->comment_action()->isEnabled() ? 1 : 0));
			if (!line || !draft || draft->ReplyToThread || draft->Anchor.Line != *line || draft->State != core::review_draft_state::drafting
				|| !window->comment_action()->isEnabled()) {
				return result::pending;
			}
			window->comment_action()->trigger();
			QCoreApplication::processEvents();
			if (body->isReadOnly()) {
				return fail("suggestion-page-not-editable");
			}
			probe.Anchor = draft->Anchor;
			probe.Line.reset();
			probe.NativeBefore = implementation->Fixture->review_mutation_evidence().NativePosts;
			body->setPlainText(aBody);
			probe.Phase = aNext;
			return result::pending;
		};
		// The draft holds the body; the mark beside Publish is checked before
		// Publish is pressed.
		const auto rootBody = [&](const QString& aBody, const std::string_view aMark, bool& aEvidence, const char* const aRender, const state_type::phase aNext) -> result {
			const auto& draft = implementation->ReviewDraft;
			note("draft=" + (draft ? draft->Body : std::string{ "-" }) + " publish=" + std::to_string(editor->publish_action()->isEnabled() ? 1 : 0) + " mark=" + markState());
			if (!draft || draft->Body != aBody.toStdString() || !editor->publish_action()->isEnabled() || text() != aBody
				|| implementation->PrivateDraftCommandPending || implementation->PrivateDraftSelectionPending
				|| !window->github_refresh_action()->isEnabled() || window->github_status_text() != "Pull request ready.") {
				return result::pending;
			}
			aEvidence = markState() == aMark && probe.Anchor && draft->Anchor == *probe.Anchor;
			render(aRender);
			if (!aEvidence) {
				return fail("mark-before-publish expected=" + std::string{ aMark } + " shown=" + markState());
			}
			editor->publish_action()->trigger();
			probe.Phase = aNext;
			return result::pending;
		};
		// One native post, received exactly as drafted on the drafted line,
		// and its thread back from the application's own refresh.
		const auto rootPublished = [&](const QString& aBody, const core::feedback_label aLabel, bool& aEvidence, const bool aCombine, const state_type::phase aNext) -> result {
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			const auto found     = std::ranges::find_if(cards->cards(), [&](const qt::thread_card& aCard) { return !aCard.Posts.empty() && aCard.Posts.front().Body == aBody; });
			note("pending=" + std::to_string(implementation->PendingReviewMutation.has_value() ? 1 : 0) + " native=" + std::to_string(mutations.NativePosts)
				 + " lastLine=" + std::to_string(mutations.LastLine.value_or(0U)) + " anchor=" + std::to_string(probe.Anchor ? probe.Anchor->Line : 0U)
				 + " side=" + mutations.LastSide + " card=" + std::to_string(found != cards->cards().end() ? 1 : 0) + " status=" + editor->mutation_status_label()->text().toStdString());
			if (implementation->PendingReviewMutation.has_value() || mutations.NativePosts == probe.NativeBefore || found == cards->cards().end()
				|| !window->github_refresh_action()->isEnabled() || window->github_status_text() != "Pull request ready."
				|| implementation->WorkbenchHost->outstanding_requests() != 0U) {
				return result::pending;
			}
			const auto bytes = aBody.toStdString();
			// The prose case also carries its before-Publish check.
			aEvidence = (!aCombine || aEvidence) && mutations.NativePosts == probe.NativeBefore + 1U && mutations.FilePosts == 0U && !mutations.LastFile
				&& mutations.LastBody == bytes && mutations.LastPath == path && mutations.LastSide == "RIGHT" && probe.Anchor && mutations.LastLine == probe.Anchor->Line + 1U
				&& found->Posts.front().Label == aLabel;
			if (!aEvidence) {
				return fail("published-not-exact body=" + mutations.LastBody + " path=" + mutations.LastPath);
			}
			probe.Phase = aNext;
			return result::pending;
		};
		switch (probe.Phase) {
		case state_type::phase::awaiting_review_empty: {
			// [[BPR-1287]]. Review before any file is chosen.
			auto* const surface = window->comparison_surface();
			note("files=" + std::to_string(drawer_row(*window, path).isValid() ? 1 : 0) + " idle=" + std::to_string(surface->idle() ? 1 : 0));
			if (implementation->WorkbenchHost->outstanding_requests() != 0U || !drawer_row(*window, path).isValid() || window->showing_home()) {
				return result::pending;
			}
			window->choose_modality(qt::review_modality::review);
			QCoreApplication::processEvents();
			probe.Evidence.ReviewEmptyDirected = window->current_modality() == qt::review_modality::review && surface->idle()
				&& surface->idle_page()->isVisibleTo(window) && !surface->left_pane()->isVisibleTo(window)
				&& surface->idle_open_first_button()->isEnabled();
			render("review-no-file");
			probe.Phase = state_type::phase::awaiting_file;
			return result::pending;
		}
		case state_type::phase::awaiting_file: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed");
			}
			if (revealed == detail::fixture_file_reveal_result::pending) {
				return result::pending;
			}
			probe.Evidence.FileShown = true;
			probe.Phase              = state_type::phase::awaiting_marks;
			return result::pending;
		}
		case state_type::phase::awaiting_marks: {
			const auto* const first  = cardFor("review-thread:PRRT_cc_1");
			const auto* const second = cardFor("review-thread:PRRT_cc_2");
			note("cards=" + std::to_string(cards->cards().size()) + " first=" + std::to_string(first != nullptr ? 1 : 0)
				 + " second=" + std::to_string(second != nullptr ? 1 : 0) + " visible=" + std::to_string(cards->isVisibleTo(window) ? 1 : 0));
			if (first == nullptr || second == nullptr || !cards->isVisibleTo(window)) {
				return result::pending;
			}
			// renderer.cpp's two threads: issue (blocking), question
			// (non-blocking), praise and a plain reply; suggestion
			// (non-blocking, if-minor), todo (blocking, if-minor, perf) and
			// nitpick (style). Several and custom decorations are kept, shown
			// as written in the text, and styled by weight and the if-minor
			// corner (owner, decorations-v003).
			using weight      = qt::feedback_mark_weight;
			const auto marked = rootLabel(first) == core::feedback_label::issue && first->Posts.front().Emphasis == qt::feedback_mark_emphasis{ .Weight = weight::blocking }
				&& first->Posts.size() == 4U && first->Posts[1].Label == core::feedback_label::question && first->Posts[1].Emphasis == qt::feedback_mark_emphasis{ .Weight = weight::non_blocking }
				&& first->Posts[2].Label == core::feedback_label::praise && !first->Posts[3].Label.has_value()
				&& rootLabel(second) == core::feedback_label::suggestion && second->Posts.front().Emphasis == qt::feedback_mark_emphasis{ .Weight = weight::non_blocking, .IfMinor = true }
				&& second->Posts.front().Body.startsWith(QStringLiteral("suggestion (non-blocking, if-minor): ")) && second->Posts.size() == 3U
				&& second->Posts[1].Emphasis == qt::feedback_mark_emphasis{ .Weight = weight::blocking, .IfMinor = true } && second->Posts[1].Name.endsWith(QStringLiteral("todo (blocking, if-minor, perf)"))
				&& second->Posts[2].Label == core::feedback_label::nitpick && second->Posts[2].Emphasis == qt::feedback_mark_emphasis{ } && second->Posts[2].Body.startsWith(QStringLiteral("nitpick (style): "))
				// The drawer's notes show their root's mark under its avatar
				// (owner, thread-marks-v002: "Marks don't show on inline
				// comments").
				&& !cards->mark_rect(*first).isEmpty() && !cards->mark_rect(*second).isEmpty();
			if (!marked) {
				return fail("thread-marks-not-drawn");
			}
			probe.Evidence.ThreadMarksDrawn = true;
			render("read-renderer-marks");
			// [[BPR-1212]]. `--line-draft-course` goes straight to the code's
			// keys, before any publication.
			probe.Phase = probe.LineDraftOnly ? state_type::phase::awaiting_left_refused : state_type::phase::awaiting_line_draft;
			return result::pending;
		}
		case state_type::phase::awaiting_line_draft: {
			// The first current-head line no thread is on, chosen in Review.
			auto line = std::optional<std::uint32_t>{ };
			for (auto candidate = std::uint32_t{ 0U }; candidate < 32U && !line; ++candidate) {
				if (window->comparison_surface()->select_conversation_line(core::diff_side::right, candidate)
					&& window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0) {
					line = candidate;
				}
			}
			const auto& draft = implementation->ReviewDraft;
			note("line=" + std::to_string(line.value_or(0U)) + " draft=" + std::to_string(draft.has_value() ? 1 : 0)
				 + " comment=" + std::to_string(window->comment_action()->isEnabled() ? 1 : 0));
			if (!line || !draft || draft->ReplyToThread || !window->comment_action()->isEnabled()) {
				return result::pending;
			}
			// [[BPR-1212]]. Comment opens the draft in the drawer; the label
			// authoring here is the full composer's, which Maximize opens in
			// Discuss with the same draft.
			window->comment_action()->trigger();
			QCoreApplication::processEvents();
			if (cards == nullptr || !cards->writing_draft() || cards->draft_editor() == nullptr) {
				return fail("comment-drawer-draft-not-open");
			}
			window->open_draft_in_discuss();
			QCoreApplication::processEvents();
			auto* const inlinePage = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
			if (inlinePage == nullptr || !inlinePage->isVisibleTo(window) || body->isReadOnly() || !controls->isVisibleTo(window)) {
				return fail("comment-page-not-editable");
			}
			probe.Evidence.CommentEditable = true;
			body->setFocus();
			// [[BPR-1565]]. The `:xx:` codes are gone (owner, 2026-10-16): a
			// typed `:qu:` stays the text it is, with the helper on.
			probe.Typing = QStringLiteral(":qu: ") + QString::fromUtf8(subject.data(), static_cast<qsizetype>(subject.size()));
			probe.Phase  = state_type::phase::awaiting_literal_code;
			return result::pending;
		}
		case state_type::phase::awaiting_literal_code: {
			const auto subjectText = QString::fromUtf8(subject.data(), static_cast<qsizetype>(subject.size()));
			const auto literal     = QStringLiteral(":qu: ") + subjectText;
			note("body=" + text().toStdString() + " draft=" + (implementation->ReviewDraft ? implementation->ReviewDraft->Body : std::string{ "-" })
				 + " typing=" + probe.Typing.toStdString() + " status=" + editor->mutation_status_label()->text().toStdString());
			if (!typed()) {
				return result::pending;
			}
			if (text() == literal) {
				probe.Evidence.CodeStaysLiteral = !controls->state().Label.has_value() && controls->statistics().Edits == 0U;
				render("editor-literal-code");
				// The reviewer takes the code out by hand: one selection and
				// Backspace, so the draft follows as typing does.
				auto cursor = body->textCursor();
				cursor.setPosition(0);
				cursor.setPosition(5, QTextCursor::KeepAnchor);
				body->setTextCursor(cursor);
				key(Qt::Key_Backspace, Qt::NoModifier, QString{ });
				return result::pending;
			}
			if (text() != subjectText) {
				return fail("literal-code-changed body=" + text().toStdString());
			}
			controls->chip()->click();
			QCoreApplication::processEvents();
			auto* const choice = controls->menu() != nullptr ? controls->menu()->label_button(core::feedback_label::suggestion) : nullptr;
			if (choice == nullptr || !controls->menu()->isVisible()) {
				return fail("chip-menu-not-open");
			}
			choice->click();
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_chip;
			return result::pending;
		}
		case state_type::phase::awaiting_chip: {
			note("body=" + text().toStdString() + " menu=" + std::to_string(controls->menu()->isVisible() ? 1 : 0));
			if (!text().startsWith(QStringLiteral("suggestion: "))) {
				return result::pending;
			}
			probe.Evidence.ChipApplied = controls->state().Label == core::feedback_label::suggestion && !controls->menu()->isVisible()
				&& text() == QStringLiteral("suggestion: ") + QString::fromUtf8(subject.data(), static_cast<qsizetype>(subject.size()));
			const auto opens = controls->statistics().MenuOpens;
			key(Qt::Key_L, Qt::ControlModifier, QString{ });
			auto* const choice = controls->menu()->label_button(core::feedback_label::issue);
			if (controls->statistics().MenuOpens != opens + 1U || !controls->menu()->isVisible() || choice == nullptr) {
				return fail("command-l-did-not-open-the-menu");
			}
			render("label-menu");
			choice->click();
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_shortcut;
			return result::pending;
		}
		case state_type::phase::awaiting_shortcut: {
			const auto& draft = implementation->ReviewDraft;
			note("body=" + text().toStdString() + " draft=" + (draft ? draft->Body : std::string{ "-" })
				 + " publish=" + std::to_string(editor->publish_action()->isEnabled() ? 1 : 0));
			if (text().toStdString() != published || !draft || draft->Body != published || !editor->publish_action()->isEnabled()) {
				return result::pending;
			}
			probe.Evidence.ShortcutApplied = controls->state().Label == core::feedback_label::issue;
			render("editor-labelled");
			editor->publish_action()->trigger();
			probe.Phase = state_type::phase::awaiting_published;
			return result::pending;
		}
		case state_type::phase::awaiting_published: {
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			note("pending=" + std::to_string(implementation->PendingReviewMutation.has_value() ? 1 : 0)
				 + " native=" + std::to_string(mutations.NativePosts) + " status=" + editor->mutation_status_label()->text().toStdString());
			if (implementation->PendingReviewMutation.has_value() || mutations.NativePosts == 0U) {
				return result::pending;
			}
			probe.Evidence.Published = mutations.NativePosts == 1U && mutations.StoredComments == 1U;
			auto* const refresh      = window->github_refresh_action();
			if (refresh == nullptr) {
				return fail("refresh-unavailable");
			}
			// The application refreshes once after publishing, and that
			// refresh holds the action until it answers.
			if (!refresh->isEnabled()) {
				return result::pending;
			}
			refresh->trigger();
			// [[BPR-1292]]. The core takes the refresh on the dispatch: the
			// action is held and the status says so before trigger returns.
			// A refresh it refused would leave nothing to wait for.
			if (refresh->isEnabled() || window->github_status_text() != "Refreshing the pull request.") {
				return fail("refresh-not-taken status=" + window->github_status_text());
			}
			probe.Phase = state_type::phase::awaiting_refreshed;
			return result::pending;
		}
		case state_type::phase::awaiting_refreshed: {
			const auto  thread = model->index_for_id("review-thread:PRRT_composed_1");
			const auto* card   = cardFor("review-thread:PRRT_composed_1");
			const auto  ready  = window->github_status_text() == "Pull request ready." && window->github_refresh_action()->isEnabled();
			note("thread=" + std::to_string(thread.isValid() ? 1 : 0) + " card=" + std::to_string(card != nullptr ? 1 : 0)
				 + " cards=" + std::to_string(cards->cards().size()) + " ready=" + std::to_string(ready ? 1 : 0));
			// [[BPR-1292]]. The card is already there from the application's
			// own refresh, so it says nothing about this one. The course's
			// refresh has answered when the pull request is ready again and
			// the action is released; until then its reads are in flight, and
			// ending the run there cut the fixture's script short.
			if (!ready || card == nullptr) {
				return result::pending;
			}
			probe.Evidence.PublishedMarked = rootLabel(card) == core::feedback_label::issue && card->Posts.front().Emphasis == qt::feedback_mark_emphasis{ }
				&& card->Posts.front().Name.endsWith(QStringLiteral(", issue")) && !cards->mark_rect(*card).isEmpty();
			render("published-marked");
			if (!probe.Evidence.PublishedMarked) {
				return fail("published-thread-unmarked name=" + card->Posts.front().Name.toStdString());
			}
			probe.Phase = state_type::phase::awaiting_patch_draft;
			return result::pending;
		}
		// [[BPR-1084]]. Suggested changes through the same public path: an
		// issue with a patch, a suggestion with none, a patch on a line GitHub
		// refuses an inline comment on, and a reply in place with a patch.
		case state_type::phase::awaiting_patch_draft:
			return rootDraft(patchBody, state_type::phase::awaiting_patch_body);
		case state_type::phase::awaiting_patch_body:
			return rootBody(patchBody, "native", probe.Evidence.PatchMarkedNative, "editor-patch-native", state_type::phase::awaiting_patch_published);
		case state_type::phase::awaiting_patch_published:
			return rootPublished(patchBody, core::feedback_label::issue, probe.Evidence.PatchPublishedExact, false, state_type::phase::awaiting_prose_draft);
		case state_type::phase::awaiting_prose_draft:
			return rootDraft(proseBody, state_type::phase::awaiting_prose_body);
		case state_type::phase::awaiting_prose_body:
			return rootBody(proseBody, "hidden", probe.Evidence.ProseUnmarkedExact, "editor-prose-unmarked", state_type::phase::awaiting_prose_published);
		case state_type::phase::awaiting_prose_published:
			return rootPublished(proseBody, core::feedback_label::suggestion, probe.Evidence.ProseUnmarkedExact, true, state_type::phase::awaiting_refused_draft);
		case state_type::phase::awaiting_refused_draft:
			return rootDraft(refusedBody, state_type::phase::awaiting_refused_body);
		case state_type::phase::awaiting_refused_body: {
			// Before the first attempt nothing says GitHub will refuse this
			// line, so the mark says native: GitHub is asked for an inline
			// comment, and a refused suggestion never becomes a file comment.
			auto asked = false;
			return rootBody(refusedBody, "native", asked, "editor-refused-before", state_type::phase::awaiting_refused);
		}
		case state_type::phase::awaiting_refused: {
			const auto  mutations = implementation->Fixture->review_mutation_evidence();
			const auto& draft     = implementation->ReviewDraft;
			const auto  status    = editor->mutation_status_label()->text();
			note("pending=" + std::to_string(implementation->PendingReviewMutation.has_value() ? 1 : 0) + " native=" + std::to_string(mutations.NativePosts)
				 + " file=" + std::to_string(mutations.FilePosts) + " lastLine=" + std::to_string(mutations.LastLine.value_or(0U)) + " anchor=" + std::to_string(probe.Anchor ? probe.Anchor->Line : 0U)
				 + " state=" + (draft ? std::string{ core::name(draft->State) } : std::string{ "-" }) + " mark=" + markState() + " status=" + status.toStdString());
			if (implementation->PendingReviewMutation.has_value() || !status.contains(QStringLiteral("needs a supported inline line anchor"))
				|| !draft || draft->State != core::review_draft_state::drafting) {
				return result::pending;
			}
			probe.Evidence.RefusedTextKept = mutations.NativePosts == probe.NativeBefore + 1U && mutations.FilePosts == 0U && !mutations.LastFile
				&& mutations.LastBody == refusedBody.toStdString() && probe.Anchor && mutations.LastLine == probe.Anchor->Line + 1U && mutations.LastSide == "RIGHT"
				&& draft->Anchor == *probe.Anchor && draft->Body == refusedBody.toStdString() && text() == refusedBody
				// [[BPR-1326]]. No thread holds it; my kept draft is a note of its own.
				&& std::ranges::none_of(cards->cards(), [&](const qt::thread_card& aCard) { return !aCard.Draft && !aCard.Posts.empty() && aCard.Posts.front().Body == refusedBody; });
			const auto* const mark               = editor->suggestion_mark_widget();
			probe.Evidence.RefusedMarkedTextOnly = markState() == "textual_only" && mark->decision().Reason == core::suggestion_text_reason::inline_comment_unsupported;
			render("editor-refused-text-only");
			if (!probe.Evidence.RefusedTextKept || !probe.Evidence.RefusedMarkedTextOnly) {
				return fail("refused-patch kept=" + std::to_string(probe.Evidence.RefusedTextKept ? 1 : 0) + " marked=" + std::to_string(probe.Evidence.RefusedMarkedTextOnly ? 1 : 0));
			}
			probe.Phase = state_type::phase::awaiting_reply_open;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_open: {
			// A reply written in place in renderer.cpp's first thread, beside
			// the code in Review, where the drawer writes replies in place.
			auto* const inlinePage = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
			note("page=" + std::to_string(inlinePage != nullptr && inlinePage->isVisibleTo(window) ? 1 : 0) + " modality=" + std::to_string(static_cast<int>(window->current_modality())));
			if (inlinePage != nullptr && inlinePage->isVisibleTo(window)) {
				auto* const back = inlinePage->findChild<QPushButton*>(QStringLiteral("composeInlineDraftPageBack"));
				if (back == nullptr) {
					return fail("no-back");
				}
				back->click();
				QCoreApplication::processEvents();
				return result::pending;
			}
			if (window->current_modality() != qt::review_modality::review) {
				window->choose_modality(qt::review_modality::review);
				QCoreApplication::processEvents();
				return result::pending;
			}
			cards->open_thread("review-thread:PRRT_cc_1");
			auto* const card  = cards->open_card();
			auto* const field = card != nullptr ? card->findChild<QAbstractButton*>(QStringLiteral("threadReplyField")) : nullptr;
			if (field == nullptr || !field->isEnabled()) {
				return fail("no-reply-field");
			}
			field->click();
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_reply_draft;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_draft: {
			const auto& draft = implementation->ReviewDraft;
			note("reply=" + (draft && draft->ReplyToThread ? *draft->ReplyToThread : std::string{ "-" }) + " editor=" + std::to_string(cards->reply_editor() != nullptr ? 1 : 0));
			if (!draft || draft->ReplyToThread != std::optional<std::string>{ "PRRT_cc_1" } || cards->reply_editor() == nullptr) {
				return result::pending;
			}
			cards->reply_editor()->setPlainText(replyBody);
			probe.Phase = state_type::phase::awaiting_reply_body;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_body: {
			const auto& draft   = implementation->ReviewDraft;
			auto* const card    = cards->open_card();
			auto* const mark    = card != nullptr ? dynamic_cast<qt::suggestion_mark*>(card->findChild<QWidget*>(QStringLiteral("threadReplySuggestionMark"))) : nullptr;
			const auto  inPlace = mark == nullptr ? std::string{ "-" } : !mark->isVisibleTo(card) ? std::string{ "hidden" }
																								  : mark->property("suggestionApplicability").toString().toStdString();
			note("body=" + (draft ? draft->Body : std::string{ "-" }) + " inPlace=" + inPlace + " editorMark=" + markState());
			if (!draft || draft->Body != replyBody.toStdString()) {
				return result::pending;
			}
			// The draft editor reads the thread's projected facts through the
			// window, the reply editor from its own card: both say native.
			const auto* const editorMark     = editor->suggestion_mark_widget();
			probe.Evidence.ReplyMarkedNative = inPlace == "native" && editorMark->decision().Applicability == core::suggestion_applicability::native
				&& cards->reply_editor() != nullptr && cards->reply_editor()->toPlainText() == replyBody;
			render("reply-in-place-native");
			const auto& evidence = probe.Evidence;
			const auto  all      = evidence.ReviewEmptyDirected && evidence.FileShown && evidence.ThreadMarksDrawn && evidence.CommentEditable && evidence.CodeStaysLiteral
				&& evidence.ChipApplied && evidence.ShortcutApplied && evidence.Published && evidence.PublishedMarked && evidence.PatchMarkedNative
				&& evidence.PatchPublishedExact && evidence.ProseUnmarkedExact && evidence.RefusedTextKept && evidence.RefusedMarkedTextOnly && evidence.ReplyMarkedNative;
			if (!all) {
				return fail("reply-mark inPlace=" + inPlace + " editorMark=" + markState());
			}
			probe.Phase = state_type::phase::awaiting_defect_offer;
			return result::pending;
		}
		// [[BPR-1086]]. Private tracking beside public labels (RFC-BPR-0048 R16):
		// the issue root offers Classify as defect, only that gesture tracks
		// it, and the website's label edit and resolution reach the defect as
		// evidence at most. Then a tour stop with a labelled narrative (R19).
		case state_type::phase::awaiting_defect_offer: {
			auto* const ledger = implementation->ReviewLedgerHost.get();
			auto        menu   = std::unique_ptr<QMenu>{ cards->thread_menu("review-thread:PRRT_cc_1") };
			auto* const offer  = menu != nullptr ? menu->findChild<QAction*>(QStringLiteral("threadClassifyDefect")) : nullptr;
			const auto  issues = std::ranges::count_if(cards->cards(), [&](const qt::thread_card& aCard) { return rootLabel(&aCard) == core::feedback_label::issue; });
			note("ledger=" + std::to_string(ledger != nullptr && ledger->is_quiescent() ? 1 : 0) + " defects=" + std::to_string(implementation->DurableDefects.size())
				 + " issues=" + std::to_string(issues) + " offer=" + std::to_string(offer == nullptr ? -1 : offer->isEnabled() ? 1
																															   : 0));
			if (ledger == nullptr || !ledger->is_quiescent() || offer == nullptr || !offer->isEnabled()) {
				return result::pending;
			}
			// Several issue roots were read, written and published above; none
			// became a defect.
			probe.Evidence.IssueOffersTracking = implementation->DurableDefects.empty() && issues >= 2 && rootLabel(cardFor("review-thread:PRRT_cc_1")) == core::feedback_label::issue;
			if (!probe.Evidence.IssueOffersTracking) {
				return fail("issue-read-made-a-defect defects=" + std::to_string(implementation->DurableDefects.size()));
			}
			const auto mutations    = implementation->Fixture->review_mutation_evidence();
			probe.SubmissionsBefore = mutations.Submissions;
			probe.NativeBefore      = mutations.NativePosts;
			offer->trigger();
			probe.Phase = state_type::phase::awaiting_defect_tracked;
			return result::pending;
		}
		case state_type::phase::awaiting_defect_tracked: {
			const auto identity = defect_identity("review-thread:PRRT_cc_1");
			const auto found    = std::ranges::find_if(implementation->DurableDefects, [identity](const application::durable_defect_record& aDefect) {
				return aDefect.Conversation.value() == identity.Conversation && aDefect.History.Defect.Id.value() == identity.Defect;
			});
			const auto quiet    = implementation->ReviewLedgerHost->is_quiescent();
			note("defects=" + std::to_string(implementation->DurableDefects.size()) + " tracked="
				 + (found != implementation->DurableDefects.end() ? std::string{ core::name(found->History.Defect.State) } : std::string{ "-" }) + " quiet=" + std::to_string(quiet ? 1 : 0));
			if (found == implementation->DurableDefects.end() || !quiet || !window->github_refresh_action()->isEnabled()) {
				return result::pending;
			}
			const auto mutations                = implementation->Fixture->review_mutation_evidence();
			probe.Tracked                       = found->History.Defect;
			probe.Evidence.TrackedOnlyByGesture = implementation->DurableDefects.size() == 1U && core::outstanding(probe.Tracked->State)
				&& mutations.Submissions == probe.SubmissionsBefore && mutations.NativePosts == probe.NativeBefore;
			if (!probe.Evidence.TrackedOnlyByGesture) {
				return fail("tracking state=" + std::string{ core::name(probe.Tracked->State) });
			}
			// The website edits only the root's label away, without a commit.
			const auto* const root    = cardFor("review-thread:PRRT_cc_1");
			const auto        written = root != nullptr ? root->Posts.front().Body.toStdString() : std::string{ };
			const auto        read    = core::recognize_feedback_prefix(written);
			if (!read.Subject) {
				return fail("issue-root-unreadable");
			}
			probe.EditedRoot = written.substr(read.Subject->Begin);
			stage_server_root(aApplication, "PRRT_cc_1", probe.EditedRoot, std::nullopt);
			window->github_refresh_action()->trigger();
			probe.Phase = state_type::phase::awaiting_relabel_refreshed;
			return result::pending;
		}
		case state_type::phase::awaiting_relabel_refreshed: {
			const auto* const root    = cardFor("review-thread:PRRT_cc_1");
			const auto        ready   = window->github_status_text() == "Pull request ready." && window->github_refresh_action()->isEnabled();
			const auto        edited  = root != nullptr && root->Posts.front().Body.toStdString() == probe.EditedRoot;
			const auto        quiet   = implementation->ReviewLedgerHost->is_quiescent();
			const auto        tracked = std::ranges::find(implementation->DurableDefects, probe.Tracked->Id, [](const application::durable_defect_record& aDefect) { return aDefect.History.Defect.Id; });
			note("ready=" + std::to_string(ready ? 1 : 0) + " edited=" + std::to_string(edited ? 1 : 0) + " quiet=" + std::to_string(quiet ? 1 : 0)
				 + " tracked=" + (tracked != implementation->DurableDefects.end() ? std::string{ core::name(tracked->History.Defect.State) } : std::string{ "-" }));
			if (!ready || !edited || !quiet) {
				return result::pending;
			}
			if (tracked == implementation->DurableDefects.end()) {
				return fail("relabel-removed-the-defect");
			}
			const auto& after                = tracked->History.Defect;
			probe.Evidence.RelabelKeptDefect = !rootLabel(root).has_value() && after == *probe.Tracked && implementation->DurableDefects.size() == 1U;
			if (!probe.Evidence.RelabelKeptDefect) {
				return fail("relabel-moved-the-defect state=" + std::string{ core::name(after.State) });
			}
			stage_server_root(aApplication, "PRRT_cc_1", std::nullopt, true);
			window->github_refresh_action()->trigger();
			probe.Phase = state_type::phase::awaiting_resolve_refreshed;
			return result::pending;
		}
		case state_type::phase::awaiting_resolve_refreshed: {
			const auto* const thread   = implementation->accepted_thread("review-thread:PRRT_cc_1");
			const auto        ready    = window->github_status_text() == "Pull request ready." && window->github_refresh_action()->isEnabled();
			const auto        resolved = thread != nullptr && thread->Resolved;
			const auto        quiet    = implementation->ReviewLedgerHost->is_quiescent();
			const auto        tracked  = std::ranges::find(implementation->DurableDefects, probe.Tracked->Id, [](const application::durable_defect_record& aDefect) { return aDefect.History.Defect.Id; });
			note("ready=" + std::to_string(ready ? 1 : 0) + " resolved=" + std::to_string(resolved ? 1 : 0) + " quiet=" + std::to_string(quiet ? 1 : 0)
				 + " tracked=" + (tracked != implementation->DurableDefects.end() ? std::string{ core::name(tracked->History.Defect.State) } : std::string{ "-" }));
			if (!ready || !resolved || !quiet) {
				return result::pending;
			}
			if (tracked == implementation->DurableDefects.end()) {
				return fail("resolution-removed-the-defect");
			}
			const auto& after                  = tracked->History.Defect;
			auto        menu                   = std::unique_ptr<QMenu>{ cards->thread_menu("review-thread:PRRT_cc_1") };
			auto* const verify                 = menu != nullptr ? menu->findChild<QAction*>(QStringLiteral("defectVerify")) : nullptr;
			const auto  mutations              = implementation->Fixture->review_mutation_evidence();
			probe.Evidence.ResolveOnlyEvidence = core::outstanding(after.State) && after.State != core::defect_state::verified && after.State != core::defect_state::withdrawn
				&& after.Severity == probe.Tracked->Severity && after.Category == probe.Tracked->Category && verify != nullptr && verify->isEnabled()
				&& mutations.Submissions == probe.SubmissionsBefore && mutations.NativePosts == probe.NativeBefore;
			if (!probe.Evidence.ResolveOnlyEvidence) {
				return fail("resolution-settled-the-defect state=" + std::string{ core::name(after.State) } + " verify=" + std::to_string(verify != nullptr && verify->isEnabled() ? 1 : 0));
			}
			// The website shows a tour stop on renderer.cpp's other thread,
			// its narrative deliberately labelled.
			const std::array navigation{ core::tour_markdown_link{ "Tour overview", "https://github.com/octo-org/nested-folders/pull/7#pullrequestreview-9" } };
			const auto       stop = core::render_tour_stop("Why the width is clamped", 2, "question: Should the clamp live in the caller?\n\nThe renderer trusts it today.", navigation);
			if (!stop) {
				return fail("tour-stop-unrendered");
			}
			probe.TourStop = *stop;
			stage_server_root(aApplication, "PRRT_cc_2", probe.TourStop, std::nullopt);
			window->github_refresh_action()->trigger();
			probe.Phase = state_type::phase::awaiting_tour_refreshed;
			return result::pending;
		}
		case state_type::phase::awaiting_tour_refreshed: {
			const auto* const stop  = cardFor("review-thread:PRRT_cc_2");
			const auto        ready = window->github_status_text() == "Pull request ready." && window->github_refresh_action()->isEnabled();
			const auto        shown = stop != nullptr && stop->Posts.front().Body.toStdString() == probe.TourStop;
			note("ready=" + std::to_string(ready ? 1 : 0) + " shown=" + std::to_string(shown ? 1 : 0));
			if (!ready || !shown) {
				return result::pending;
			}
			// [[BPR-1086]] owner look. Its note reads from the narrative, so the
			// label, set as the header, sits beside the mark.
			const auto& post                = stop->Posts.front();
			probe.Evidence.TourStopLabelled = rootLabel(stop) == core::feedback_label::question && post.Lead > 0 && post.Body.mid(post.Lead).startsWith(QStringLiteral("question: Should the clamp"))
				&& post.HeaderLength == QStringLiteral("question: ").size() && !cards->mark_rect(*stop).isEmpty();
			render("tour-stop-labelled");
			if (!probe.Evidence.TourStopLabelled) {
				return fail("tour-stop-unmarked name=" + stop->Posts.front().Name.toStdString());
			}
			probe.Phase = state_type::phase::awaiting_left_refused;
			return result::pending;
		}
		// [[BPR-1212]]. The keyboard line cursor and Return in the code, by
		// keys delivered to the focused pane as a keyboard delivers them. The
		// draft opens in the drawer (slice 2's begin_new_comment), the one
		// path every new comment takes.
		case state_type::phase::awaiting_left_refused: {
			auto* const inlinePage = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
			if (inlinePage != nullptr && inlinePage->isVisibleTo(window)) {
				window->close_review_composition();
				QCoreApplication::processEvents();
				return result::pending;
			}
			if (cards->writing_draft() && cards->draft_editor() != nullptr) {
				paneKey(*cards->draft_editor(), Qt::Key_Escape, Qt::NoModifier);
				return result::pending;
			}
			if (window->current_modality() != qt::review_modality::review) {
				window->choose_modality(qt::review_modality::review);
				QCoreApplication::processEvents();
				return result::pending;
			}
			auto* const surface = window->comparison_surface();
			auto* const left    = surface->left_pane();
			if (!surface->select_conversation_line(core::diff_side::left, 0U)) {
				return result::pending;
			}
			window->statusBar()->clearMessage();
			left->setFocus(Qt::OtherFocusReason);
			const auto natives = implementation->Fixture->review_mutation_evidence().NativePosts;
			paneKey(*left, Qt::Key_Return, Qt::NoModifier);
			const auto message = window->statusBar()->currentMessage();
			note("message=" + message.toStdString());
			// [[BPR-1212]] v002. Line 1 of the base is outside every hunk, or
			// its numbering is not yet known to be GitHub's: refused either way.
			probe.Evidence.LeftLineRefused = (message.contains(QStringLiteral("inside a changed region")) || message.contains(QStringLiteral("not known to match"))) && !cards->writing_draft()
				&& (inlinePage == nullptr || !inlinePage->isVisibleTo(window)) && implementation->Fixture->review_mutation_evidence().NativePosts == natives;
			if (!probe.Evidence.LeftLineRefused) {
				return fail("left-line-not-refused");
			}
			probe.Phase = state_type::phase::awaiting_single_line;
			return result::pending;
		}
		case state_type::phase::awaiting_single_line: {
			auto* const inlinePage = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
			auto* const surface    = window->comparison_surface();
			if (!probe.KeySent) {
				if (!probe.Line) {
					for (auto candidate = std::uint32_t{ 0U }; candidate < 64U && !probe.Line; ++candidate) {
						if (surface->select_conversation_line(core::diff_side::right, candidate) && window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0) {
							probe.Line = candidate;
						}
					}
					if (!probe.Line) {
						return fail("no-free-line");
					}
				}
				if (const auto selected = surface->selection(); !selected || selected->PreferredLine.Side != core::diff_side::right || selected->PreferredLine.Line != *probe.Line) {
					static_cast<void>(surface->select_conversation_line(core::diff_side::right, *probe.Line));
				}
				const auto& draft  = implementation->ReviewDraft;
				const auto* active = implementation->TabHost && implementation->TabHost->state().Active
					? core::find_tab(implementation->TabHost->state(), *implementation->TabHost->state().Active)
					: nullptr;
				note("line=" + std::to_string(*probe.Line) + " draft=" + (draft ? std::to_string(draft->Anchor.Line) : std::string{ "-" })
					 + " selected_file=" + std::to_string(implementation->WorkbenchHost->selected().has_value())
					 + " active_tab=" + std::to_string(active != nullptr)
					 + " tab_file=" + std::to_string(active && active->View.File.has_value())
					 + " chosen=" + std::to_string(implementation->TabHost && implementation->TabHost->state().Chosen.has_value())
					 + " material=" + std::to_string(implementation->ReviewSourceMaterial.has_value())
					 + " tab_queued=" + std::to_string(implementation->TabAdvanceQueued));
				if (!draft || draft->Anchor.Line != *probe.Line || draft->Anchor.Side != core::diff_side::right || draft->State != core::review_draft_state::drafting) {
					return result::pending;
				}
				auto* const right = surface->right_pane();
				right->setFocus(Qt::OtherFocusReason);
				paneKey(*right, Qt::Key_Return, Qt::NoModifier);
				probe.KeySent = true;
			}
			const auto expected = QStringLiteral("Your draft on line %1").arg(*probe.Line + 1U);
			note("writing=" + std::to_string(cards->writing_draft() ? 1 : 0) + " card=" + (cards->open_card() != nullptr ? cards->open_card()->accessibleName().toStdString() : std::string{ "-" }));
			if (!cards->writing_draft() || cards->open_card() == nullptr) {
				return result::pending;
			}
			probe.Evidence.SingleLineOpenedPlain = cards->draft_editor() != nullptr && !cards->draft_editor()->isReadOnly() && !body->isReadOnly()
				&& (inlinePage == nullptr || !inlinePage->isVisibleTo(window)) && implementation->ReviewDraft && !implementation->ReviewDraft->StartLine
				&& implementation->ReviewDraft->Anchor.Line == *probe.Line && cards->open_card()->accessibleName() == expected;
			render("drawer-one-line");
			if (!probe.Evidence.SingleLineOpenedPlain) {
				return fail("return-did-not-open-one-line");
			}
			probe.Line.reset();
			probe.KeySent = false;
			// Esc with nothing written drops the draft and returns to the code.
			paneKey(*cards->draft_editor(), Qt::Key_Escape, Qt::NoModifier);
			probe.Phase = state_type::phase::awaiting_keyboard_range;
			return result::pending;
		}
		case state_type::phase::awaiting_keyboard_range: {
			auto* const surface  = window->comparison_surface();
			const auto& compared = surface->current_result();
			if (!compared || !compared->RightDocument) {
				return fail("no-comparison");
			}
			if (cards->writing_draft()) {
				return result::pending;
			}
			// Three right lines in one hunk with no thread on any of them,
			// from the last hunk up, away from the lines used above.
			if (!probe.RangeStart) {
				const auto hunks = core::github_review_hunks(compared->Alignment, core::diff_side::right);
				for (auto hunk = hunks.rbegin(); hunk != hunks.rend() && !probe.RangeStart; ++hunk) {
					for (auto start = hunk->End >= 3U ? hunk->End - 3U : 0U; start >= hunk->Begin && start + 2U < hunk->End && !probe.RangeStart; --start) {
						auto unthreaded = true;
						for (auto line = start; line < start + 3U && unthreaded; ++line) {
							unthreaded = surface->select_conversation_line(core::diff_side::right, line) && window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0;
						}
						if (unthreaded) {
							probe.RangeStart = start;
						}
						if (start == 0U) {
							break;
						}
					}
				}
				if (!probe.RangeStart) {
					return fail("no-free-range");
				}
			}
			const auto start = *probe.RangeStart;
			if (!surface->select_conversation_line(core::diff_side::right, start)) {
				return result::pending;
			}
			auto* const right = surface->right_pane();
			right->setFocus(Qt::OtherFocusReason);
			paneKey(*right, Qt::Key_Down, Qt::ShiftModifier);
			paneKey(*right, Qt::Key_Down, Qt::ShiftModifier);
			const auto range = surface->line_draft_range();
			note("start=" + std::to_string(start) + " range=" + (range ? std::to_string(range->first.Line) + "-" + std::to_string(range->second.Line) : std::string{ "-" }));
			probe.Evidence.KeyboardRangeSelected = range && range->first.Line == start && range->second.Line == start + 2U && range->first.Side == core::diff_side::right
				&& range->second.Side == core::diff_side::right && surface->selection() && surface->selection()->PreferredLine.Line == start + 2U
				&& core::decide_comment_range(compared->Alignment, range->first, range->second).StartLine == start;
			if (!probe.Evidence.KeyboardRangeSelected) {
				return fail("keyboard-range-not-selected");
			}
			auto lines = core::suggestion_source_lines(*compared->RightDocument, start, start + 2U);
			if (!lines) {
				return fail("no-prefill");
			}
			// [[RFC-BPR-0090]]. The change typed at the end of the last line,
			// as the core composes a suggestion with no words.
			lines->back() += " // probe";
			probe.Prefill      = core::compose_suggestion_body("", *lines);
			probe.RangeTyped   = false;
			probe.NativeBefore = implementation->Fixture->review_mutation_evidence().NativePosts;
			probe.Anchor       = core::source_line_ref{ compared->Alignment.Digest, core::diff_side::right, start + 2U };
			paneKey(*right, Qt::Key_Return, Qt::ShiftModifier);
			probe.Phase = state_type::phase::awaiting_range_prefilled;
			return result::pending;
		}
		case state_type::phase::awaiting_range_prefilled: {
			const auto& draft  = implementation->ReviewDraft;
			const auto  start  = probe.RangeStart.value_or(0U);
			auto* const writer = cards->writing_draft() ? cards->draft_editor() : nullptr;
			auto* const head   = window->comparison_surface()->head_editor();
			note("draft=" + (draft ? std::to_string(draft->Anchor.Line) + ":" + (draft->StartLine ? std::to_string(*draft->StartLine) : std::string{ "-" }) : std::string{ "-" })
				 + " writing=" + std::to_string(writer != nullptr ? 1 : 0) + " pane=" + std::to_string(head != nullptr ? 1 : 0) + " typed=" + std::to_string(probe.RangeTyped ? 1 : 0)
				 + " status=" + window->statusBar()->currentMessage().toStdString());
			// [[RFC-BPR-0090]]. Shift+Return opens the three lines for writing
			// in the head pane, with the caret at the end of the last, and the
			// drawer holds the words, none yet. The change is typed there.
			if (!probe.RangeTyped) {
				if (!draft || !probe.Anchor || draft->Anchor != *probe.Anchor || draft->StartLine != std::optional{ start } || writer == nullptr || head == nullptr
					|| head->model().editable_range() != std::optional{ std::pair<std::uint64_t, std::uint64_t>{ start, start + 2U } }) {
					return result::pending;
				}
				if (!writer->toPlainText().isEmpty() || !head->apply(core::insert_text{ .Text = " // probe" })) {
					return fail("suggestion-not-in-the-pane words=" + writer->toPlainText().toStdString());
				}
				probe.RangeTyped = true;
				return result::pending;
			}
			if (!draft || draft->Anchor != *probe.Anchor || draft->Body != probe.Prefill || writer == nullptr || head == nullptr || !editor->publish_action()->isEnabled()) {
				return result::pending;
			}
			// The drawer names the lines, the gutter's blue bracket spans them,
			// the drawer holds no block, and the body is the core's composition.
			const auto& marks                  = window->comparison_surface()->line_marks(core::diff_side::right);
			const auto  bracket                = std::ranges::any_of(marks, [start](const qt::diff_line_mark& aMark) {
				return aMark.Writing && aMark.Line == start + 2U && aMark.FirstLine == std::optional<std::uint32_t>{ start };
			});
			probe.Evidence.SuggestionPrefilled = draft->StartLine == start && !draft->ReplyToThread && cards->open_card() != nullptr
				&& cards->open_card()->accessibleName() == QStringLiteral("Your draft on lines %1–%2").arg(start + 1U).arg(start + 3U) && bracket && writer->toPlainText().isEmpty()
				&& markState() == "native";
			render("drawer-range-suggestion");
			if (!probe.Evidence.SuggestionPrefilled) {
				return fail("suggestion-not-in-the-pane words=" + writer->toPlainText().toStdString() + " mark=" + markState() + " bracket=" + std::to_string(bracket ? 1 : 0)
							+ " card=" + (cards->open_card() != nullptr ? cards->open_card()->accessibleName().toStdString() : std::string{ "-" }));
			}
			// The drawer draft's own Publish.
			auto* const publish = cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadPublishDraft"));
			if (publish == nullptr || !publish->isEnabled()) {
				return fail("no-drawer-publish");
			}
			publish->click();
			probe.Phase = state_type::phase::awaiting_range_published;
			return result::pending;
		}
		case state_type::phase::awaiting_range_published: {
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			const auto prefill   = QString::fromStdString(probe.Prefill);
			const auto found     = std::ranges::find_if(cards->cards(), [&](const qt::thread_card& aCard) { return !aCard.Posts.empty() && aCard.Posts.front().Body == prefill; });
			const auto start     = probe.RangeStart.value_or(0U);
			note("pending=" + std::to_string(implementation->PendingReviewMutation.has_value() ? 1 : 0) + " native=" + std::to_string(mutations.NativePosts) + " line="
				 + std::to_string(mutations.LastLine.value_or(0U)) + " start=" + std::to_string(mutations.LastStartLine.value_or(0U)) + " startSide=" + mutations.LastStartSide.value_or("-")
				 + " card=" + std::to_string(found != cards->cards().end() ? 1 : 0) + " status=" + editor->mutation_status_label()->text().toStdString());
			if (implementation->PendingReviewMutation.has_value() || mutations.NativePosts == probe.NativeBefore || found == cards->cards().end()) {
				return result::pending;
			}
			probe.Evidence.RangePublishedExact = mutations.NativePosts == probe.NativeBefore + 1U && !mutations.LastFile && mutations.LastBody == probe.Prefill && mutations.LastPath == path
				&& mutations.LastSide == "RIGHT" && mutations.LastLine == start + 3U && mutations.LastStartLine == start + 1U && mutations.LastStartSide == std::optional<std::string>{ "RIGHT" };
			if (!probe.Evidence.RangePublishedExact) {
				return fail("range-not-published-exact");
			}
			probe.Phase = state_type::phase::awaiting_mouse_range;
			return result::pending;
		}
		// [[BPR-1721]]. The owner's course: several right lines selected
		// by a drag, a right-click inside the selection, and Add comment on
		// the menu the window opens, each delivered to the window as the
		// platform delivers it, with the event loop turning between them.
		// The draft takes all the lines, and its Publish sends them.
		case state_type::phase::awaiting_mouse_range: {
			if (cards->writing_draft() && cards->draft_editor() != nullptr) {
				paneKey(*cards->draft_editor(), Qt::Key_Escape, Qt::NoModifier);
				return result::pending;
			}
			// The range's publication refreshes the conversations first, and the
			// kept tab revalidates; a line chosen before that lands is replaced
			// by the tab's remembered one ([[BPR-1713]]).
			if (!window->github_refresh_action()->isEnabled() || window->github_status_text() != "Pull request ready."
				|| implementation->WorkbenchHost->outstanding_requests() != 0U || implementation->RereadOwed || implementation->RereadQueued
				|| implementation->WindowRead->state().PendingPullRequest.has_value() || implementation->TabAdvanceQueued || implementation->TabLoad.has_value()
				|| implementation->RefreshKeptTabView) {
				return result::pending;
			}
			auto* const surface  = window->comparison_surface();
			const auto& compared = surface->current_result();
			if (!compared || !compared->RightDocument) {
				return fail("no-comparison");
			}
			// Three right lines in one hunk with no thread on any, the last
			// with text for the drag to end in.
			if (!probe.MouseRangeStart) {
				const auto hunks = core::github_review_hunks(compared->Alignment, core::diff_side::right);
				for (const auto& hunk : hunks) {
					for (auto start = hunk.Begin; start + 2U < hunk.End && !probe.MouseRangeStart; ++start) {
						auto unthreaded = !surface->right_pane()->document()->findBlockByNumber(static_cast<int>(start + 2U)).text().isEmpty();
						for (auto line = start; line < start + 3U && unthreaded; ++line) {
							unthreaded = surface->select_conversation_line(core::diff_side::right, line) && window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0;
						}
						if (unthreaded) {
							probe.MouseRangeStart = start;
						}
					}
				}
				if (!probe.MouseRangeStart) {
					return fail("no-free-mouse-range");
				}
				// Somewhere else first, so the drag is what selects the lines.
				static_cast<void>(surface->select_conversation_line(core::diff_side::right, *probe.MouseRangeStart + 3U));
				return result::pending;
			}
			const auto  start = *probe.MouseRangeStart;
			auto* const right = surface->right_pane();
			// A point in the window on a line's code, or at its end.
			const auto point = [window, surface, right](const std::uint32_t aLine, const bool aEnd) {
				const auto row    = surface->line_rect(core::diff_side::right, aLine);
				const auto inPane = right->viewport()->mapFrom(surface, row.center());
				return right->viewport()->mapTo(window, QPoint{ aEnd ? right->viewport()->width() - 4 : 40, inPane.y() });
			};
			const auto mouse = [window](const QPoint aAt, const QEvent::Type aType, const Qt::MouseButton aButton, const Qt::MouseButtons aHeld) {
				QCursor::setPos(window->mapToGlobal(aAt));
				QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(
					window->windowHandle(), QPointF{ aAt }, QPointF{ window->mapToGlobal(aAt) }, aHeld, aButton, aType, Qt::NoModifier);
				QCoreApplication::processEvents();
			};
			if (!probe.MouseDragged) {
				const auto from = point(start, false);
				const auto to   = point(start + 2U, true);
				mouse(from, QEvent::MouseMove, Qt::NoButton, Qt::NoButton);
				mouse(from, QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton);
				mouse((from + to) / 2, QEvent::MouseMove, Qt::NoButton, Qt::LeftButton);
				mouse(to, QEvent::MouseMove, Qt::NoButton, Qt::LeftButton);
				mouse(to, QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton);
				probe.MouseDragged = true;
				return result::pending;
			}
			const auto range = surface->line_draft_range();
			note("start=" + std::to_string(start) + " range=" + (range ? std::to_string(range->first.Line) + "-" + std::to_string(range->second.Line) : std::string{ "-" })
				 + " selected=" + (surface->selection() ? std::to_string(surface->selection()->PreferredLine.Line) : std::string{ "-" }));
			if (!range || range->first.Line != start || range->second.Line != start + 2U) {
				return fail("drag-did-not-select-the-lines");
			}
			// The right-click: the window opens its menu as a popup.
			const auto inside = point(start + 1U, false);
			mouse(inside, QEvent::MouseMove, Qt::NoButton, Qt::NoButton);
			mouse(inside, QEvent::MouseButtonPress, Qt::RightButton, Qt::RightButton);
			auto* const menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
			if (menu == nullptr || menu->objectName() != QStringLiteral("codeContextMenu") || menu->windowHandle() == nullptr) {
				return fail("right-click-opened-no-code-menu");
			}
			const auto corner = QPointF{ 1.0, 1.0 };
			QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(
				menu->windowHandle(), corner, QPointF{ menu->mapToGlobal(corner) }, Qt::NoButton, Qt::RightButton, QEvent::MouseButtonRelease, Qt::NoModifier);
			QCoreApplication::processEvents();
			const auto actions = menu->actions();
			const auto found   = std::ranges::find_if(actions, [](const QAction* aAction) { return aAction->objectName() == QStringLiteral("addCommentAction"); });
			const auto label   = QStringLiteral("Add comment on lines %1–%2").arg(start + 1U).arg(start + 3U);
			note("start=" + std::to_string(start) + " item=" + (found != actions.end() ? (*found)->text().toStdString() : std::string{ "-" })
				 + " kept=" + std::to_string(right->textCursor().hasSelection() ? 1 : 0));
			if (found == actions.end() || !(*found)->isEnabled() || (*found)->text() != label) {
				return fail("add-comment-not-on-the-lines");
			}
			// The item, clicked on the popup as a hand does.
			const auto centre = QPointF{ menu->actionGeometry(*found).center() };
			const auto global = QPointF{ menu->mapToGlobal(centre) };
			QCursor::setPos(global.toPoint());
			QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(menu->windowHandle(), centre, global, Qt::NoButton, Qt::NoButton, QEvent::MouseMove, Qt::NoModifier);
			QCoreApplication::processEvents();
			QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(menu->windowHandle(), centre, global, Qt::LeftButton, Qt::LeftButton, QEvent::MouseButtonPress, Qt::NoModifier);
			QCoreApplication::processEvents();
			QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(menu->windowHandle(), centre, global, Qt::NoButton, Qt::LeftButton, QEvent::MouseButtonRelease, Qt::NoModifier);
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_mouse_range_opened;
			return result::pending;
		}
		case state_type::phase::awaiting_mouse_range_opened: {
			const auto  start  = probe.MouseRangeStart.value_or(0U);
			const auto& draft  = implementation->ReviewDraft;
			auto* const writer = cards->writing_draft() ? cards->draft_editor() : nullptr;
			const auto  words  = QStringLiteral("question: Do these three lines need the same guard?");
			note("draft=" + (draft ? std::to_string(draft->Anchor.Line) + ":" + (draft->StartLine ? std::to_string(*draft->StartLine) : std::string{ "-" }) : std::string{ "-" })
				 + " writing=" + std::to_string(writer != nullptr ? 1 : 0) + " card=" + (cards->open_card() != nullptr ? cards->open_card()->accessibleName().toStdString() : std::string{ "-" })
				 + " status=" + window->statusBar()->currentMessage().toStdString());
			if (writer == nullptr || !draft || draft->Anchor.Line != start + 2U) {
				return ++probe.Waited < 2'000U ? result::pending : fail("mouse-range-not-opened");
			}
			if (writer->toPlainText() != words) {
				writer->setPlainText(words);
				return result::pending;
			}
			if (draft->Body != words.toStdString() || !editor->publish_action()->isEnabled()) {
				return result::pending;
			}
			probe.Waited                    = 0U;
			probe.Evidence.MouseRangeOpened = draft->StartLine == std::optional<std::uint32_t>{ start } && draft->Anchor.Side == core::diff_side::right && !draft->ReplyToThread
				&& cards->open_card() != nullptr && cards->open_card()->accessibleName() == QStringLiteral("Your draft on lines %1–%2").arg(start + 1U).arg(start + 3U);
			render("drawer-mouse-range");
			if (!probe.Evidence.MouseRangeOpened) {
				return fail("mouse-range-draft-not-on-the-lines");
			}
			auto* const publish = cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadPublishDraft"));
			if (publish == nullptr || !publish->isEnabled()) {
				return fail("no-drawer-publish");
			}
			probe.Prefill      = words.toStdString();
			probe.NativeBefore = implementation->Fixture->review_mutation_evidence().NativePosts;
			publish->click();
			probe.Phase = state_type::phase::awaiting_mouse_range_published;
			return result::pending;
		}
		case state_type::phase::awaiting_mouse_range_published: {
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			const auto start     = probe.MouseRangeStart.value_or(0U);
			note("native=" + std::to_string(mutations.NativePosts) + " line=" + std::to_string(mutations.LastLine.value_or(0U)) + " start=" + std::to_string(mutations.LastStartLine.value_or(0U)));
			// The thread it makes, once the conversations are read again, is on
			// all three lines, as GitHub has it.
			const auto words  = QString::fromStdString(probe.Prefill);
			const auto thread = std::ranges::find_if(cards->cards(), [&words](const qt::thread_card& aCard) { return !aCard.Draft && !aCard.Posts.empty() && aCard.Posts.front().Body == words; });
			if (implementation->PendingReviewMutation.has_value() || mutations.NativePosts == probe.NativeBefore || thread == cards->cards().end()) {
				return result::pending;
			}
			probe.Evidence.MouseRangePublishedExact = mutations.NativePosts == probe.NativeBefore + 1U && !mutations.LastFile && mutations.LastBody == probe.Prefill && mutations.LastPath == path
				&& mutations.LastSide == "RIGHT" && mutations.LastLine == start + 3U && mutations.LastStartLine == start + 1U && mutations.LastStartSide == std::optional<std::string>{ "RIGHT" }
				&& thread->Line == std::optional<std::uint32_t>{ start + 3U } && qt::thread_first_line(*thread) == std::optional<std::uint32_t>{ start + 1U };
			if (!probe.Evidence.MouseRangePublishedExact) {
				return fail("mouse-range-not-published-exact");
			}
			probe.Phase = state_type::phase::awaiting_base_numbering;
			return result::pending;
		}
		// [[BPR-1212]] v002. The epoch host has told whether this file's base is
		// the merge base's version GitHub numbers `LEFT` lines in; the
		// fixture's base is its branch point, so it is.
		case state_type::phase::awaiting_base_numbering: {
			if (cards->writing_draft() && cards->draft_editor() != nullptr) {
				paneKey(*cards->draft_editor(), Qt::Key_Escape, Qt::NoModifier);
				return result::pending;
			}
			// The range's publication refreshes the conversations; the next
			// publication waits for that read to finish, as a reviewer's
			// would, so the fixture's recorded reads stay in order.
			// [[BPR-1570]]. Its owed reread of the pull request too, which
			// starts only once a pending read settles.
			if (!window->github_refresh_action()->isEnabled() || window->github_status_text() != "Pull request ready."
				|| implementation->WorkbenchHost->outstanding_requests() != 0U || implementation->RereadOwed || implementation->RereadQueued
				|| implementation->WindowRead->state().PendingPullRequest.has_value()) {
				return result::pending;
			}
			note("left-current=" + std::to_string(window->left_conversation_endpoint_current() ? 1 : 0)
				 + " numbered=" + std::to_string(implementation->ReviewSourceMaterial && implementation->ReviewSourceMaterial->BaseNumberedAsGitHub ? 1 : 0));
			probe.Evidence.BaseNumberedKnown = window->left_conversation_endpoint_current() && implementation->ReviewSourceMaterial
				&& implementation->ReviewSourceMaterial->BaseNumberedAsGitHub;
			if (!probe.Evidence.BaseNumberedKnown) {
				return result::pending;
			}
			probe.RefreshedFrom.reset();
			probe.Phase = state_type::phase::awaiting_kept_refresh;
			return result::pending;
		}
		// [[BPR-1570]]. A refresh that keeps the file adopts it under a new
		// workbench authority without publishing it again, as the range's
		// publication refresh does at a time of its own. Forced here before the
		// base line is chosen, so the course always meets that order: the
		// numbering holds across it and the base line still takes its draft.
		case state_type::phase::awaiting_kept_refresh: {
			const auto& selected = implementation->SelectedDiffPublication;
			if (!selected) {
				return fail("no-selected-diff");
			}
			if (!probe.RefreshedFrom) {
				if (!window->github_refresh_action()->isEnabled()) {
					return result::pending;
				}
				probe.RefreshedFrom = selected->Request.WorkbenchAuthority;
				window->github_refresh_action()->trigger();
				return result::pending;
			}
			const auto adopted = selected->Request.WorkbenchAuthority != *probe.RefreshedFrom;
			note("adopted=" + std::to_string(adopted ? 1 : 0) + " numbered=" + std::to_string(implementation->base_numbered_as_github() ? 1 : 0)
				 + " left-current=" + std::to_string(window->left_conversation_endpoint_current() ? 1 : 0));
			if (!adopted || !window->github_refresh_action()->isEnabled() || window->github_status_text() != "Pull request ready."
				|| implementation->WorkbenchHost->outstanding_requests() != 0U || implementation->RereadOwed || implementation->RereadQueued
				|| implementation->WindowRead->state().PendingPullRequest.has_value()) {
				return result::pending;
			}
			probe.Evidence.BaseNumberingKept = implementation->base_numbered_as_github() && window->left_conversation_endpoint_current()
				&& implementation->ReviewSourceMaterial && implementation->ReviewSourceMaterial->BaseNumberedAsGitHub;
			if (!probe.Evidence.BaseNumberingKept) {
				return fail("base-numbering-lost-on-kept-refresh");
			}
			// `--stale-base-numbering`: the kept answer becomes one about
			// another path with the window's left side still placed, the state
			// such a refresh left before [[BPR-1570]]. The base line's own
			// question is then answered at once, and must still take a draft.
			if (std::exchange(implementation->StaleBaseNumberingQuestion, false) && implementation->BaseNumbering) {
				implementation->BaseNumbering->Request.LeftPath = std::filesystem::path{ "bpr-1570/another-path" };
			}
			probe.Line.reset();
			probe.KeySent = false;
			probe.Phase   = state_type::phase::awaiting_base_line;
			return result::pending;
		}
		case state_type::phase::awaiting_base_line: {
			auto* const surface  = window->comparison_surface();
			const auto& compared = surface->current_result();
			if (!compared || !compared->LeftDocument) {
				return fail("no-base-document");
			}
			if (!probe.KeySent) {
				// A base line in a hunk with no thread on it.
				if (!probe.Line) {
					for (const auto& hunk : core::github_review_hunks(compared->Alignment, core::diff_side::left)) {
						for (auto line = hunk.Begin; line < hunk.End && !probe.Line; ++line) {
							if (surface->select_conversation_line(core::diff_side::left, line) && model->rowCount() == 0) {
								probe.Line = line;
							}
						}
					}
					if (!probe.Line) {
						return fail("no-free-base-line");
					}
				}
				if (const auto selected = surface->selection(); !selected || selected->PreferredLine.Side != core::diff_side::left || selected->PreferredLine.Line != *probe.Line) {
					static_cast<void>(surface->select_conversation_line(core::diff_side::left, *probe.Line));
				}
				const auto& draft = implementation->ReviewDraft;
				note("base-line=" + std::to_string(*probe.Line) + " draft=" + (draft ? std::to_string(draft->Anchor.Line) + (draft->Anchor.Side == core::diff_side::left ? "L" : "R") : std::string{ "-" }));
				if (!draft || draft->Anchor.Line != *probe.Line || draft->Anchor.Side != core::diff_side::left || draft->State != core::review_draft_state::drafting) {
					return result::pending;
				}
				auto* const left = surface->left_pane();
				left->setFocus(Qt::OtherFocusReason);
				paneKey(*left, Qt::Key_Return, Qt::NoModifier);
				probe.KeySent = true;
			}
			const auto expected = QStringLiteral("Your draft on base line %1").arg(*probe.Line + 1U);
			note("writing=" + std::to_string(cards->writing_draft() ? 1 : 0) + " card=" + (cards->open_card() != nullptr ? cards->open_card()->accessibleName().toStdString() : std::string{ "-" }));
			if (!cards->writing_draft() || cards->open_card() == nullptr || cards->draft_editor() == nullptr) {
				return result::pending;
			}
			if (cards->draft_editor()->toPlainText().isEmpty()) {
				cards->draft_editor()->setPlainText(QStringLiteral("question: BPR-1212 base-side comment, why was this removed?"));
				return result::pending;
			}
			auto* const publish = cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadPublishDraft"));
			const auto& draft   = implementation->ReviewDraft;
			if (!draft || draft->Body.empty() || publish == nullptr || !publish->isEnabled()) {
				return result::pending;
			}
			probe.Evidence.BaseLineOpened = cards->open_card()->accessibleName() == expected && draft->Anchor.Side == core::diff_side::left
				&& draft->Anchor.Line == *probe.Line && !draft->StartLine && !draft->ReplyToThread;
			render("drawer-base-line");
			if (!probe.Evidence.BaseLineOpened) {
				return fail("base-line-not-opened card=" + cards->open_card()->accessibleName().toStdString());
			}
			probe.NativeBefore = implementation->Fixture->review_mutation_evidence().NativePosts;
			publish->click();
			probe.Phase = state_type::phase::awaiting_base_published;
			return result::pending;
		}
		case state_type::phase::awaiting_base_published: {
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			note("pending=" + std::to_string(implementation->PendingReviewMutation.has_value() ? 1 : 0) + " native=" + std::to_string(mutations.NativePosts) + " side="
				 + mutations.LastSide + " line=" + std::to_string(mutations.LastLine.value_or(0U)) + " status=" + editor->mutation_status_label()->text().toStdString());
			// [[BPR-1570]]. The publication's owed reread finishes before the
			// course does, or the run ends with it reading and the fixture's
			// recording unconsumed.
			if (implementation->PendingReviewMutation.has_value() || mutations.NativePosts == probe.NativeBefore
				|| !window->github_refresh_action()->isEnabled() || window->github_status_text() != "Pull request ready."
				|| implementation->WorkbenchHost->outstanding_requests() != 0U || implementation->RereadOwed || implementation->RereadQueued
				|| implementation->WindowRead->state().PendingPullRequest.has_value()) {
				return result::pending;
			}
			probe.Evidence.BasePublishedLeft = mutations.NativePosts == probe.NativeBefore + 1U && !mutations.LastFile && mutations.LastPath == path
				&& mutations.LastSide == "LEFT" && mutations.LastLine == probe.Line.value_or(0U) + 1U && !mutations.LastStartLine && !mutations.LastStartSide
				&& mutations.LastBody == "question: BPR-1212 base-side comment, why was this removed?";
			if (!probe.Evidence.BasePublishedLeft) {
				return fail("base-line-not-published-left");
			}
			probe.Line.reset();
			probe.Phase = state_type::phase::awaiting_no_line;
			return result::pending;
		}
		case state_type::phase::awaiting_no_line: {
			auto* const inlinePage = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
			if (inlinePage != nullptr && inlinePage->isVisibleTo(window)) {
				window->close_review_composition();
				QCoreApplication::processEvents();
				return result::pending;
			}
			if (cards->writing_draft() && cards->draft_editor() != nullptr) {
				paneKey(*cards->draft_editor(), Qt::Key_Escape, Qt::NoModifier);
				return result::pending;
			}
			// The comparison emptied, as when no file is chosen: the pane
			// still takes Return, and nothing opens.
			auto* const surface = window->comparison_surface();
			surface->clear();
			QCoreApplication::processEvents();
			window->statusBar()->clearMessage();
			surface->right_pane()->setFocus(Qt::OtherFocusReason);
			paneKey(*surface->right_pane(), Qt::Key_Return, Qt::NoModifier);
			const auto message = window->statusBar()->currentMessage();
			note("message=" + message.toStdString());
			probe.Evidence.NoLineExplained = message.contains(QStringLiteral("Select a line first")) && message.contains(QStringLiteral("Up and Down")) && !cards->writing_draft()
				&& (inlinePage == nullptr || !inlinePage->isVisibleTo(window));
			if (!probe.Evidence.NoLineExplained) {
				return fail("no-line-not-explained");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::passed:
		case state_type::phase::failed:
			break;
		}
		return result::failed;
	}

	auto detail::github_application_fixture_access::label_authoring_evidence(
		const github_application& aApplication) -> detail::label_authoring_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->LabelAuthoringProbe.has_value()) {
			return { };
		}
		return implementation->LabelAuthoringProbe->Evidence;
	}

	auto detail::github_application_fixture_access::label_authoring_diagnostic(
		const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->LabelAuthoringProbe.has_value()) {
			return { };
		}
		return implementation->LabelAuthoringProbe->Diagnostic;
	}
#endif
}
