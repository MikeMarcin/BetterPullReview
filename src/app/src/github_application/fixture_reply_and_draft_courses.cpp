#include "github_application/fixture_course_support.hpp"
#include "review_cycle_desktop.hpp"
#include "review_submission_workflow.hpp"

#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/qt/agent_tab_view.hpp"
#endif
#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/application/document_tab_host.hpp"
	#include "bpr/application/github_read_model.hpp"
	#include "bpr/application/github_window_read_host.hpp"
	#include "bpr/application/shared_draft_application_host.hpp"
	#include "bpr/application/workbench_application_host.hpp"
	#include "bpr/application/workbench_runtime_ports.hpp"
	#include "bpr/core/appearance_model.hpp"
	#include "bpr/core/diff_model.hpp"
	#include "bpr/core/diff_semantics.hpp"
	#include "bpr/core/document_tabs.hpp"
	#include "bpr/core/epoch_model.hpp"
	#include "bpr/core/filtered_comment.hpp"
	#include "bpr/core/github_model.hpp"
	#include "bpr/core/model.hpp"
	#include "bpr/core/reply_draft_standing.hpp"
	#include "bpr/core/review_round_trip.hpp"
	#include "bpr/core/revisioned_review_draft.hpp"
	#include "bpr/core/window_review_restoration.hpp"
	#include "bpr/qt/appearance_settings_panel.hpp"
	#include "bpr/qt/conversation_cards_view.hpp"
	#include "bpr/qt/destructive_question.hpp"
	#include "bpr/qt/diff_workbench.hpp"
	#include "bpr/qt/github_conversation_model.hpp"
	#include "bpr/qt/github_conversation_panel.hpp"
	#include "bpr/qt/review_draft_editor.hpp"
	#include "bpr/qt/review_submission_editor.hpp"
	#include "bpr/qt/workbench_drawer.hpp"
	#include "bpr/qt/workbench_strip.hpp"
	#include "bpr/qt/workbench_window.hpp"
	#include "github_application/window_composition.hpp"
	#include "github_application/window_probe_states.hpp"
	#include "github_application/window_review_lanes.hpp"
	#include "github_application_fixture_access.hpp"
	#include "github_public_path_fixture.hpp"
	#include "private_review_workflow.hpp"
	#include <QAbstractButton>
	#include <QAbstractItemView>
	#include <QAction>
	#include <QAnyStringView>
	#include <QByteArrayView>
	#include <QChar>
	#include <QClipboard>
	#include <QCoreApplication>
	#include <QDesktopServices>
	#include <QDialog>
	#include <QDir>
	#include <QGuiApplication>
	#include <QLabel>
	#include <QLatin1Char>
	#include <QList>
	#include <QListView>
	#include <QMainWindow>
	#include <QModelIndex>
	#include <QObject>
	#include <QPixmap>
	#include <QPlainTextEdit>
	#include <QPushButton>
	#include <QRect>
	#include <QScrollArea>
	#include <QStatusBar>
	#include <QString>
	#include <QTextStream>
	#include <QToolButton>
	#include <QWidget>
	#include <algorithm>
	#include <array>
	#include <cstddef>
	#include <functional>
	#include <string_view>
	#include <tuple>
	#include <utility>
	#include <vector>
	#if defined(BPR_AGENT_INTEGRATIONS)
		#include "bpr/core/conversation_reply.hpp"
		#include <map>
	#endif
#endif

namespace bpr::app {
#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::exercise_published_file_switch(
		github_application& aApplication, const detail::published_file_switch_timing aTiming) -> detail::published_file_switch_probe_result
	{
		using result               = detail::published_file_switch_probe_result;
		using state_type           = published_file_switch_probe_state;
		using timing               = detail::published_file_switch_timing;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->PublishedFileSwitchProbe.has_value()) {
			implementation->PublishedFileSwitchProbe.emplace();
		}
		auto& probe = *implementation->PublishedFileSwitchProbe;
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
		if (cards == nullptr || editor == nullptr || model == nullptr) {
			return result::pending;
		}
		auto* const body = editor->body_editor();
		const auto  fail = [&probe](const std::string_view aWhy) {
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
		constexpr auto path    = std::string_view{ "src/engine/renderer.cpp" };
		constexpr auto other   = std::string_view{ "src/engine/renderer.h" };
		const auto     written = QStringLiteral("BPR-1326: this comment stays after a file switch.");
		const auto     ready   = [&] {
			return window->github_refresh_action()->isEnabled() && window->github_status_text() == "Pull request ready."
				&& implementation->WorkbenchHost->outstanding_requests() == 0U && !implementation->PendingReviewMutation.has_value();
		};
		const auto byBody = [&](const qt::thread_card& aCard) { return !aCard.Draft && !aCard.Posts.empty() && aCard.Posts.front().Body == written; };
		// What shows of the comment on the open file: the thread in the
		// accepted pull request or the draft still saved; its note, measured,
		// placed at its line and in the shown drawer; its gutter marker and
		// its scroll-bar tick.
		struct shown_facts
		{
			bool Kept   = false;
			bool Note   = false;
			bool Drawn  = false;
			bool Marker = false;
			bool Tick   = false;

			[[nodiscard]] auto all() const noexcept -> bool { return Kept && Note && Drawn && Marker && Tick; }
			[[nodiscard]] auto text() const -> std::string
			{
				return "kept=" + std::to_string(Kept ? 1 : 0) + " note=" + std::to_string(Note ? 1 : 0) + " drawn=" + std::to_string(Drawn ? 1 : 0)
					+ " marker=" + std::to_string(Marker ? 1 : 0) + " tick=" + std::to_string(Tick ? 1 : 0);
			}
		};
		const auto shown = [&]() -> shown_facts {
			auto facts = shown_facts{ };
			if (!probe.Anchor) {
				return facts;
			}
			const auto line = probe.Anchor->Line;
			const auto card = aTiming == timing::draft
				? std::ranges::find_if(cards->cards(), [line](const qt::thread_card& aCard) { return aCard.Draft && aCard.Line == std::optional{ line + 1U }; })
				: std::ranges::find_if(cards->cards(), byBody);
			if (aTiming == timing::draft) {
				facts.Kept = implementation->SharedDrafts && std::ranges::any_of(implementation->SharedDrafts->recovery_snapshot(), [&](const core::revisioned_review_draft& aSaved) {
								 return aSaved.Draft.Anchor == *probe.Anchor && aSaved.Draft.Body == written.toStdString() && aSaved.Draft.State == core::review_draft_state::drafting;
							 });
			} else if (card != cards->cards().end()) {
				facts.Kept = implementation->WindowRead && implementation->WindowRead->accepted_snapshot()
					&& std::ranges::any_of(implementation->WindowRead->accepted_snapshot()->ReviewThreads, [&](const application::github_review_thread_record& aThread) {
								 return "review-thread:" + std::string{ aThread.Thread.value() } == card->Id;
							 });
			}
			facts.Note   = card != cards->cards().end() && card->Line == std::optional{ line + 1U };
			facts.Drawn  = facts.Note && card->Placed && card->Rect.height() > 0 && cards->isVisibleTo(window);
			facts.Marker = std::ranges::any_of(window->comparison_surface()->line_marks(core::diff_side::right), [line](const qt::diff_line_mark& aMark) { return aMark.Line == line; })
				&& std::ranges::any_of(cards->line_marks(core::diff_side::right), [line](const qt::thread_line_mark& aMark) { return aMark.Line == line; });
			facts.Tick = std::ranges::any_of(window->comparison_surface()->thread_ticks(), [line](const qt::diff_thread_tick& aTick) { return aTick.Side == core::diff_side::right && aTick.Line == line; });
			return facts;
		};
		switch (probe.Phase) {
		case state_type::phase::awaiting_file: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed");
			}
			if (revealed == detail::fixture_file_reveal_result::shown) {
				probe.Phase = state_type::phase::awaiting_line_draft;
			}
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
			window->comment_action()->trigger();
			QCoreApplication::processEvents();
			if (body->isReadOnly()) {
				return fail("comment-page-not-editable");
			}
			probe.Anchor       = draft->Anchor;
			probe.NativeBefore = implementation->Fixture->review_mutation_evidence().NativePosts;
			body->setPlainText(written);
			probe.Phase = state_type::phase::awaiting_body;
			return result::pending;
		}
		case state_type::phase::awaiting_body: {
			const auto& draft = implementation->ReviewDraft;
			note("draft=" + (draft ? draft->Body : std::string{ "-" }) + " publish=" + std::to_string(editor->publish_action()->isEnabled() ? 1 : 0));
			if (!draft || draft->Body != written.toStdString() || !editor->publish_action()->isEnabled()
				|| implementation->PrivateDraftCommandPending || implementation->PrivateDraftSelectionPending || !ready()) {
				return result::pending;
			}
			if (!probe.Anchor || draft->Anchor != *probe.Anchor) {
				return fail("draft-moved");
			}
			if (aTiming == timing::draft) {
				// Not published: back from the comment's page to the code,
				// where the draft is my note at its line.
				if (auto* const page = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage")); page != nullptr && page->isVisibleTo(window)) {
					auto* const back = page->findChild<QPushButton*>(QStringLiteral("composeInlineDraftPageBack"));
					if (back == nullptr) {
						return fail("no-back");
					}
					back->click();
					QCoreApplication::processEvents();
					return result::pending;
				}
				const auto facts = shown();
				note("draft=" + draft->Body + " " + facts.text());
				if (!facts.all()) {
					return result::pending;
				}
				probe.Evidence.Written     = implementation->Fixture->review_mutation_evidence().NativePosts == probe.NativeBefore;
				probe.Evidence.ShownBefore = true;
				probe.Phase                = state_type::phase::awaiting_other_file;
				return result::pending;
			}
			// A slow server: the application's own refresh after Publish
			// answers only once the other file shows.
			if (aTiming == timing::late) {
				window->github_refresh_action()->blockSignals(true);
			}
			editor->publish_action()->trigger();
			probe.Phase = state_type::phase::awaiting_published;
			return result::pending;
		}
		case state_type::phase::awaiting_published: {
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			const auto facts     = shown();
			note("pending=" + std::to_string(implementation->PendingReviewMutation.has_value() ? 1 : 0) + " native=" + std::to_string(mutations.NativePosts) + " " + facts.text());
			if (implementation->PendingReviewMutation.has_value() || mutations.NativePosts == probe.NativeBefore) {
				return result::pending;
			}
			probe.Evidence.Written = mutations.NativePosts == probe.NativeBefore + 1U && mutations.LastPath == path
				&& probe.Anchor && mutations.LastLine == probe.Anchor->Line + 1U;
			if (!probe.Evidence.Written) {
				return fail("published-not-exact");
			}
			// The late course leaves as soon as Publish answers.
			if (aTiming == timing::settled) {
				if (!facts.all() || !ready()) {
					return result::pending;
				}
				probe.Evidence.ShownBefore = true;
			}
			probe.Phase = state_type::phase::awaiting_other_file;
			return result::pending;
		}
		case state_type::phase::awaiting_other_file: {
			const auto revealed = reveal_fixture_file(aApplication, other);
			note("reveal=" + std::to_string(static_cast<int>(revealed)) + " ready=" + std::to_string(ready() ? 1 : 0));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("other-file-not-revealed");
			}
			if (revealed != detail::fixture_file_reveal_result::shown) {
				return result::pending;
			}
			probe.Evidence.OtherFileShown = true;
			if (window->github_refresh_action()->signalsBlocked()) {
				window->github_refresh_action()->blockSignals(false);
				window->github_refresh_action()->trigger();
				return result::pending;
			}
			if (!ready()) {
				return result::pending;
			}
			probe.Phase = state_type::phase::awaiting_return;
			return result::pending;
		}
		case state_type::phase::awaiting_return: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed-again");
			}
			if (revealed == detail::fixture_file_reveal_result::shown) {
				probe.Waited = 0U;
				probe.Phase  = state_type::phase::awaiting_settled;
			}
			return result::pending;
		}
		case state_type::phase::awaiting_settled: {
			// Settled: nothing is being read, and the comment has had forty
			// ticks (two seconds) to show.
			const auto facts     = shown();
			const auto selection = window->comparison_surface()->selection();
			note("ready=" + std::to_string(ready() ? 1 : 0) + " waited=" + std::to_string(probe.Waited) + " " + facts.text()
				 + " selected=" + (selection ? std::to_string(selection->PreferredLine.Line) : std::string{ "-" }) + " notes=" + std::to_string(cards->cards().size()));
			if (!ready() || (!facts.all() && ++probe.Waited < 40U)) {
				return result::pending;
			}
			probe.Evidence.Kept             = facts.Kept;
			probe.Evidence.ShownAfterReturn = facts.all();
			if (probe.Evidence.ShownAfterReturn) {
				probe.Phase = state_type::phase::passed;
				return result::passed;
			}
			// Gone: say whether a later refresh brings it back, then fail.
			window->github_refresh_action()->trigger();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_refreshed;
			return result::pending;
		}
		case state_type::phase::awaiting_refreshed: {
			const auto facts = shown();
			note("ready=" + std::to_string(ready() ? 1 : 0) + " waited=" + std::to_string(probe.Waited) + " " + facts.text());
			if ((!ready() || !facts.all()) && ++probe.Waited < 80U) {
				return result::pending;
			}
			return fail(facts.all() ? "gone-after-return refresh-restored=1" : "gone-after-return refresh-restored=0");
		}
		case state_type::phase::passed:
			return result::passed;
		case state_type::phase::failed:
			break;
		}
		return result::failed;
	}

	auto detail::github_application_fixture_access::exercise_reply_draft(
		github_application& aApplication, const detail::reply_draft_course aCourse) -> detail::reply_draft_probe_result
	{
		using result     = detail::reply_draft_probe_result;
		using course     = detail::reply_draft_course;
		using state_type = reply_draft_probe_state;
		if (aCourse == course::outdated_publish || aCourse == course::outdated_write || aCourse == course::outdated_restart) {
			return exercise_outdated_reply(aApplication, aCourse);
		}
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->ReplyDraftProbe.has_value()) {
			implementation->ReplyDraftProbe.emplace();
		}
		auto& probe = *implementation->ReplyDraftProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr || implementation->WorkbenchHost == nullptr
			|| implementation->ReviewMutationHost == nullptr) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const cards  = window->conversation_panel() != nullptr ? window->conversation_panel()->cards() : nullptr;
		auto* const editor = window->review_editor();
		if (cards == nullptr || editor == nullptr) {
			return result::pending;
		}
		const auto fail = [&probe](const std::string_view aWhy) {
			probe.PrivateEffectsHold.reset();
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
		constexpr auto path     = std::string_view{ "src/engine/renderer.cpp" };
		constexpr auto threadId = std::string_view{ "PRRT_cc_1" };
		const auto     identity = "review-thread:" + std::string{ threadId };
		const auto     words    = QStringLiteral("question: Is this still needed once the clear runs first? (BPR-1374 unsent reply)");
		const auto     more     = words + QStringLiteral(" Typed after the Refresh.");
		const auto     edited   = std::string{ "issue (blocking): draw_scene() runs before the clear. (edited on GitHub)" };
		const auto     ready    = [&] {
			return window->github_refresh_action()->isEnabled() && window->github_status_text() == "Pull request ready."
				&& implementation->WorkbenchHost->outstanding_requests() == 0U && !implementation->PendingReviewMutation.has_value()
				&& !implementation->PrivateDraftCommandPending && !implementation->PrivateDraftSelectionPending;
		};
		const auto sent = [&] {
			const auto now = implementation->Fixture->review_mutation_evidence();
			return std::tie(now.Submissions, now.NativePosts, now.FilePosts, now.Replies, now.Resolutions)
				!= std::tie(probe.Before.Submissions, probe.Before.NativePosts, probe.Before.FilePosts, probe.Before.Replies, probe.Before.Resolutions);
		};
		const auto saved = [&]() -> std::optional<core::revisioned_review_draft> {
			if (!implementation->SharedDrafts || !probe.Draft) {
				return std::nullopt;
			}
			return implementation->SharedDrafts->read(implementation->DraftConsumer, *probe.Draft).Accepted;
		};
		const auto cardFor = [&](const std::string& aId) -> const qt::thread_card* {
			const auto found = std::ranges::find(cards->cards(), aId, &qt::thread_card::Id);
			return found == cards->cards().end() ? nullptr : &*found;
		};
		const auto keptId = [&] { return probe.Draft ? qt::conversation_cards_view::kept_reply_identity(*probe.Draft) : std::string{ }; };
		const auto button = [&](const char* aName) -> QAbstractButton* {
			auto* const open = cards->open_card();
			return open != nullptr ? open->findChild<QAbstractButton*>(QString::fromLatin1(aName)) : nullptr;
		};
		// [[BPR-1374]]. With BPR_REPLY_DRAFT_RENDER_DIRECTORY set, the window
		// as the owner will see it, in light and dark, for the look.
		const auto renders = qEnvironmentVariable("BPR_REPLY_DRAFT_RENDER_DIRECTORY");
		const auto render  = [&](const QString& aName) {
			if (renders.isEmpty() || window->appearance_settings() == nullptr) {
				return;
			}
			QDir{ }.mkpath(renders);
			for (const auto& [mode, name] : std::array{ std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
				window->appearance_settings()->choose_mode(mode);
				for (auto turn = 0; turn < 6; ++turn) {
					QCoreApplication::processEvents();
				}
				static_cast<void>(window->grab().save(renders + QLatin1Char{ '/' } + aName + QLatin1Char{ '-' } + name + QStringLiteral(".png"), "PNG"));
			}
			window->appearance_settings()->choose_mode(core::appearance_mode::light);
			for (auto turn = 0; turn < 6; ++turn) {
				QCoreApplication::processEvents();
			}
		};
		const auto openReply = [&]() -> bool {
			if (window->current_modality() != qt::review_modality::review) {
				window->choose_modality(qt::review_modality::review);
				QCoreApplication::processEvents();
				return false;
			}
			if (cards->open_thread_id() != identity) {
				cards->open_thread(identity);
				QCoreApplication::processEvents();
			}
			auto* const field = button("threadReplyField");
			if (field == nullptr || !field->isEnabled()) {
				return false;
			}
			field->click();
			QCoreApplication::processEvents();
			return true;
		};
		const auto held = [&](const QString& aWords) {
			const auto& draft = implementation->ReviewDraft;
			return draft && probe.Draft && draft->Id == *probe.Draft && draft->ReplyToThread == std::optional<std::string>{ std::string{ threadId } }
			&& draft->Body == aWords.toStdString() && draft->State == core::review_draft_state::drafting;
		};
		const auto finish = [&] {
			if (aCourse == course::deleted && !probe.HeldDiscard) {
				return fail("discard-not-held");
			}
			probe.Evidence.NothingSent = !sent();
			const auto& evidence       = probe.Evidence;
			if (!(evidence.Written && evidence.ServerShown && evidence.Kept && evidence.Presented && evidence.Flagged && evidence.Acted && evidence.NothingSent)) {
				return fail("evidence");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		};
		switch (probe.Phase) {
		case state_type::phase::awaiting_file: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed");
			}
			if (revealed == detail::fixture_file_reveal_result::shown) {
				probe.Before = implementation->Fixture->review_mutation_evidence();
				probe.Phase  = aCourse == course::restart ? state_type::phase::awaiting_restored : state_type::phase::awaiting_reply_open;
			}
			return result::pending;
		}
		case state_type::phase::awaiting_reply_open:
			note("ready=" + std::to_string(ready() ? 1 : 0) + " open=" + cards->open_thread_id());
			if (!ready() || !openReply()) {
				return result::pending;
			}
			probe.Phase = state_type::phase::awaiting_reply_draft;
			return result::pending;
		case state_type::phase::awaiting_reply_draft: {
			const auto& draft = implementation->ReviewDraft;
			note("reply=" + (draft && draft->ReplyToThread ? *draft->ReplyToThread : std::string{ "-" }) + " editor=" + std::to_string(cards->reply_editor() != nullptr ? 1 : 0));
			if (!draft || draft->ReplyToThread != std::optional<std::string>{ std::string{ threadId } } || cards->reply_editor() == nullptr) {
				return result::pending;
			}
			probe.Draft = draft->Id;
			cards->reply_editor()->setPlainText(words);
			probe.Phase = state_type::phase::awaiting_reply_saved;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_saved: {
			const auto stored = saved();
			note("held=" + std::to_string(held(words) ? 1 : 0) + " saved=" + std::to_string(stored && stored->Draft.Body == words.toStdString() ? 1 : 0) + " ready=" + std::to_string(ready() ? 1 : 0));
			if (!held(words) || !stored || stored->Draft.Body != words.toStdString() || !ready()) {
				return result::pending;
			}
			probe.Evidence.Written = !sent();
			render(QStringLiteral("written"));
			if (aCourse == course::write) {
				// Quit with the reply saved; `restart` reads it back.
				probe.Evidence.ServerShown = probe.Evidence.Kept = probe.Evidence.Presented = probe.Evidence.Flagged = probe.Evidence.Acted = true;
				return finish();
			}
			auto& fixture = *implementation->Fixture;
			if (aCourse == course::edited) {
				fixture.stage_server_root(std::string{ threadId }, edited, std::nullopt);
			} else if (aCourse == course::resolved) {
				fixture.stage_server_root(std::string{ threadId }, std::nullopt, true);
			} else if (aCourse == course::deleted || aCourse == course::kept) {
				fixture.stage_server_thread_deleted(std::string{ threadId });
			}
			if (aCourse == course::deleted) {
				// Hold only private storage, while the reread and visible note
				// complete normally. Confirm must retain this exact discard.
				probe.PrivateEffectsHold.emplace(hold_private_review_effects(aApplication));
			}
			window->github_refresh_action()->trigger();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_refreshed;
			return result::pending;
		}
		case state_type::phase::awaiting_refreshed: {
			const auto* const thread   = cardFor(identity);
			const auto* const kept     = cardFor(keptId());
			const auto&       snapshot = implementation->WindowRead->accepted_snapshot();
			const auto        onServer = snapshot && std::ranges::any_of(snapshot->ReviewThreads, [&](const application::github_review_thread_record& aThread) { return aThread.Thread.value() == threadId; });
			const auto        stored   = saved();
			const auto        gone     = aCourse == course::deleted || aCourse == course::kept;
			switch (aCourse) {
			case course::refresh:          probe.Evidence.ServerShown = onServer && thread != nullptr; break;
			case course::edited:           probe.Evidence.ServerShown = onServer && thread != nullptr && !thread->Posts.empty() && thread->Posts.front().Body.toStdString() == edited; break;
			case course::resolved:         probe.Evidence.ServerShown = onServer && thread != nullptr && thread->Resolved; break;
			case course::deleted:
			case course::kept:             probe.Evidence.ServerShown = snapshot && !onServer && thread == nullptr; break;
			case course::write:
			case course::restart:
			case course::outdated_publish:
			case course::outdated_write:
			case course::outdated_restart: break;
			}
			probe.Evidence.Kept = stored && stored->Draft.Body == words.toStdString() && stored->Draft.State == core::review_draft_state::drafting
				&& stored->Draft.ReplyToThread == std::optional<std::string>{ std::string{ threadId } };
			if (gone) {
				probe.Evidence.Presented = kept != nullptr && kept->Draft && kept->Placed && kept->Line == std::optional<std::uint32_t>{ 6U } && !kept->Posts.empty() && kept->Posts.front().Body == words;
				probe.Evidence.Flagged   = kept != nullptr && kept->UnsentReply == std::optional{ core::reply_draft_standing::thread_deleted }
					&& kept->Notice == QStringLiteral("The thread this replies to was deleted on GitHub.");
			} else {
				probe.Evidence.Presented = held(words) && thread != nullptr && thread->UnsentReply.has_value();
				probe.Evidence.Flagged   = thread != nullptr
					&& (aCourse == course::resolved ? thread->UnsentReply == std::optional{ core::reply_draft_standing::thread_resolved } && thread->Notice == QStringLiteral("The thread this replies to was resolved on GitHub.")
													: thread->UnsentReply == std::optional{ core::reply_draft_standing::answerable } && thread->Notice.isEmpty());
			}
			const auto& draft = implementation->ReviewDraft;
			note("ready=" + std::to_string(ready() ? 1 : 0) + " waited=" + std::to_string(probe.Waited) + " server=" + std::to_string(probe.Evidence.ServerShown ? 1 : 0)
				 + " kept=" + std::to_string(probe.Evidence.Kept ? 1 : 0) + " presented=" + std::to_string(probe.Evidence.Presented ? 1 : 0) + " flagged=" + std::to_string(probe.Evidence.Flagged ? 1 : 0)
				 + " editor=" + (draft ? std::to_string(draft->Id.value()) + ":" + (draft->ReplyToThread ? *draft->ReplyToThread : std::string{ "-" }) : std::string{ "none" })
				 + " thread=" + std::to_string(thread != nullptr ? 1 : 0) + " keptNote=" + std::to_string(kept != nullptr ? 1 : 0) + " saved=" + (stored ? std::string{ core::name(stored->Draft.State) } : std::string{ "none" }));
			if (!ready() || (!(probe.Evidence.ServerShown && probe.Evidence.Kept && probe.Evidence.Presented && probe.Evidence.Flagged) && ++probe.Waited < 80U)) {
				return result::pending;
			}
			if (!probe.Evidence.ServerShown || !probe.Evidence.Kept || !probe.Evidence.Presented || !probe.Evidence.Flagged) {
				return fail("after-refresh");
			}
			render(QStringLiteral("refreshed-") + (aCourse == course::refresh ? QStringLiteral("refresh") : aCourse == course::edited ? QStringLiteral("edited")
													   : aCourse == course::resolved                                                  ? QStringLiteral("resolved")
																																	  : QStringLiteral("deleted")));
			switch (aCourse) {
			case course::refresh:
				probe.Phase = state_type::phase::awaiting_typed;
				return result::pending;
			case course::edited:
				probe.Evidence.Acted = true;
				return finish();
			case course::resolved:
				probe.Evidence.Acted = editor->publish_action()->isEnabled();
				return finish();
			case course::deleted:
			case course::kept:
				cards->open_thread(keptId());
				QCoreApplication::processEvents();
				render(QStringLiteral("kept-open"));
				probe.Phase = aCourse == course::deleted ? state_type::phase::awaiting_copied : state_type::phase::awaiting_kept;
				return result::pending;
			case course::write:
			case course::restart:
			case course::outdated_publish:
			case course::outdated_write:
			case course::outdated_restart: break;
			}
			return fail("course");
		}
		case state_type::phase::awaiting_typed: {
			// The reply in place is still the same draft: what is typed after
			// the Refresh is saved into it.
			if (cards->reply_editor() == nullptr) {
				note("reopen");
				static_cast<void>(openReply());
				return result::pending;
			}
			if (cards->reply_editor()->toPlainText() != more) {
				cards->reply_editor()->setPlainText(more);
				return result::pending;
			}
			const auto stored = saved();
			note("held=" + std::to_string(held(more) ? 1 : 0) + " saved=" + std::to_string(stored && stored->Draft.Body == more.toStdString() ? 1 : 0));
			if ((!held(more) || !stored || stored->Draft.Body != more.toStdString() || !ready()) && ++probe.Waited < 160U) {
				return result::pending;
			}
			probe.Evidence.Acted = held(more) && stored && stored->Draft.Body == more.toStdString();
			return finish();
		}
		case state_type::phase::awaiting_copied: {
			auto* const copy = button("threadCopyReply");
			note("copy=" + std::to_string(copy != nullptr ? 1 : 0) + " open=" + cards->open_thread_id());
			if (copy == nullptr) {
				return ++probe.Waited < 200U ? result::pending : fail("no-copy");
			}
			QGuiApplication::clipboard()->clear();
			copy->click();
			QCoreApplication::processEvents();
			if (QGuiApplication::clipboard()->text() != words) {
				return fail("copy-text clipboard=" + QGuiApplication::clipboard()->text().toStdString());
			}
			auto* const discard = button("threadDiscardReply");
			if (discard == nullptr) {
				return fail("no-discard");
			}
			discard->click();
			QCoreApplication::processEvents();
			auto* const question = window->findChild<QDialog*>(QStringLiteral("discardReplyConfirmation"));
			auto* const yes      = question != nullptr ? question->findChild<QAbstractButton*>(QStringLiteral("discardReplyConfirmationConfirm")) : nullptr;
			if (yes == nullptr) {
				return fail("no-question");
			}
			// Asked first: nothing is discarded until the question is answered.
			if (const auto stored = saved(); !stored || stored->Draft.State != core::review_draft_state::drafting) {
				return fail("discarded-before-asked");
			}
			if (!probe.PrivateEffectsHold || implementation->PrivateDraftsReady || (implementation->PrivateReview && implementation->PrivateReview->binding())) {
				return fail("discard-store-not-held");
			}
			yes->click();
			QCoreApplication::processEvents();
			const auto retained = saved();
			probe.HeldDiscard   = retained && retained->Draft.State == core::review_draft_state::drafting;
			probe.PrivateEffectsHold.reset();
			if (!probe.HeldDiscard) {
				return fail("discard-before-store-resumes");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_discarded;
			return result::pending;
		}
		case state_type::phase::awaiting_discarded: {
			const auto stored = saved();
			const auto gone   = (!stored || stored->Draft.State == core::review_draft_state::discarded) && cardFor(keptId()) == nullptr;
			note("saved=" + (stored ? std::string{ core::name(stored->Draft.State) } : std::string{ "none" }) + " note=" + std::to_string(cardFor(keptId()) != nullptr ? 1 : 0));
			if (!gone && ++probe.Waited < 160U) {
				return result::pending;
			}
			probe.Evidence.Acted = gone;
			return finish();
		}
		case state_type::phase::awaiting_kept: {
			// Pressed once the window is settled, and again if a press made
			// while the drafts were still being restored changed nothing.
			if (ready() && implementation->PrivateDraftsReady && probe.Waited % 40U == 0U) {
				if (auto* const keep = button("threadKeepReplyAsComment"); keep != nullptr) {
					keep->click();
					QCoreApplication::processEvents();
				}
			}
			const auto stored = saved();
			const auto line   = std::ranges::find_if(cards->cards(), [&](const qt::thread_card& aCard) { return aCard.Draft && !aCard.UnsentReply.has_value() && aCard.Line == std::optional<std::uint32_t>{ 6U }; });
			const auto kept   = stored && !stored->Draft.ReplyToThread && stored->Draft.Body == words.toStdString() && stored->Draft.State == core::review_draft_state::drafting
				&& stored->Draft.Anchor.Line == 5U && line != cards->cards().end() && line->Placed && cardFor(keptId()) == nullptr;
			note("comment=" + std::to_string(stored && !stored->Draft.ReplyToThread ? 1 : 0) + " note=" + std::to_string(line != cards->cards().end() ? 1 : 0));
			if (!kept && ++probe.Waited < 160U) {
				return result::pending;
			}
			render(QStringLiteral("kept-as-comment"));
			probe.Evidence.Acted = kept;
			return finish();
		}
		case state_type::phase::awaiting_restored: {
			// After the restart: the saved reply, found among the restored
			// drafts, marked on its thread's note.
			if (!probe.Draft && implementation->SharedDrafts) {
				for (const auto& draft : implementation->SharedDrafts->recovery_snapshot()) {
					if (draft.Draft.ReplyToThread == std::optional<std::string>{ std::string{ threadId } } && draft.Draft.Body == words.toStdString()
						&& draft.Draft.State == core::review_draft_state::drafting) {
						probe.Draft = draft.Draft.Id;
					}
				}
			}
			const auto* const thread   = cardFor(identity);
			probe.Evidence.Written     = true;
			probe.Evidence.Kept        = probe.Draft.has_value();
			probe.Evidence.ServerShown = thread != nullptr;
			probe.Evidence.Flagged     = thread != nullptr && thread->UnsentReply == std::optional{ core::reply_draft_standing::answerable } && thread->Notice.isEmpty();
			note("ready=" + std::to_string(ready() ? 1 : 0) + " drafts=" + std::to_string(implementation->PrivateDraftsReady ? 1 : 0) + " saved=" + std::to_string(probe.Draft ? 1 : 0)
				 + " thread=" + std::to_string(thread != nullptr ? 1 : 0) + " marked=" + std::to_string(probe.Evidence.Flagged ? 1 : 0) + " waited=" + std::to_string(probe.Waited));
			if (!ready() || (!(probe.Evidence.Kept && probe.Evidence.Flagged) && ++probe.Waited < 120U)) {
				return result::pending;
			}
			if (!probe.Evidence.Kept || !probe.Evidence.Flagged) {
				return fail("not-restored");
			}
			render(QStringLiteral("restarted"));
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_reopened;
			return result::pending;
		}
		case state_type::phase::awaiting_reopened: {
			// Reply on the thread opens the saved reply, with its words.
			if (!held(words)) {
				note("opening editor=" + (implementation->ReviewDraft ? std::to_string(implementation->ReviewDraft->Id.value()) : std::string{ "none" }));
				if (ready() && probe.Waited++ % 10U == 0U) {
					static_cast<void>(openReply());
				}
				return probe.Waited < 200U ? result::pending : fail("reply-not-reopened");
			}
			if (cards->reply_editor() == nullptr) {
				static_cast<void>(openReply());
				return ++probe.Waited < 200U ? result::pending : fail("no-reply-editor");
			}
			probe.Evidence.Presented = true;
			probe.Evidence.Acted     = cards->reply_editor()->toPlainText() == words;
			render(QStringLiteral("restarted-reply"));
			return finish();
		}
		case state_type::phase::passed:
			return result::passed;
		case state_type::phase::awaiting_outdated:
		case state_type::phase::awaiting_published:
		case state_type::phase::failed:
			break;
		}
		return result::failed;
	}

	/// [[BPR-1745]]. The owner, 2026-10-22: "Allow replying to an
	/// outdated thread from the drawer, like GitHub does". Over
	/// conventional-comments-v1, `PRRT_cc_1` on src/engine/renderer.cpp is
	/// made outdated by a push (its line left the diff; every post was written
	/// on an earlier commit), the public Refresh reads it so, and its Reply in
	/// the Comments drawer opens a reply bound to the thread, with no refusal
	/// and one quiet line saying the thread's line has changed. Before the
	/// change the Reply was refused with "This thread's line isn't in the code
	/// shown, so it can't take a reply here." and no draft was made.
	auto detail::github_application_fixture_access::exercise_outdated_reply(
		github_application& aApplication, const detail::reply_draft_course aCourse) -> detail::reply_draft_probe_result
	{
		using result               = detail::reply_draft_probe_result;
		using course               = detail::reply_draft_course;
		using state_type           = reply_draft_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->ReplyDraftProbe.has_value()) {
			implementation->ReplyDraftProbe.emplace();
		}
		auto& probe = *implementation->ReplyDraftProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr || implementation->WorkbenchHost == nullptr
			|| implementation->ReviewMutationHost == nullptr) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const cards  = window->conversation_panel() != nullptr ? window->conversation_panel()->cards() : nullptr;
		if (cards == nullptr || window->review_editor() == nullptr) {
			return result::pending;
		}
		const auto fail = [&probe](const std::string_view aWhy) {
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
		constexpr auto path      = std::string_view{ "src/engine/renderer.cpp" };
		constexpr auto threadId  = std::string_view{ "PRRT_cc_1" };
		constexpr auto writtenOn = std::string_view{ "3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c" };
		const auto     identity  = "review-thread:" + std::string{ threadId };
		const auto     words     = QStringLiteral("question: Is this still true after the push? (outdated-thread reply)");
		const auto     ready     = [&] {
			return window->github_refresh_action()->isEnabled() && window->github_status_text() == "Pull request ready."
				&& implementation->WorkbenchHost->outstanding_requests() == 0U && !implementation->PendingReviewMutation.has_value()
				&& !implementation->PrivateDraftCommandPending && !implementation->PrivateDraftSelectionPending;
		};
		const auto now      = [&] { return implementation->Fixture->review_mutation_evidence(); };
		const auto sentNone = [&] {
			const auto after = now();
			return std::tie(after.Submissions, after.NativePosts, after.FilePosts, after.Replies, after.Resolutions)
				== std::tie(probe.Before.Submissions, probe.Before.NativePosts, probe.Before.FilePosts, probe.Before.Replies, probe.Before.Resolutions);
		};
		const auto saved = [&]() -> std::optional<core::revisioned_review_draft> {
			if (!implementation->SharedDrafts || !probe.Draft) {
				return std::nullopt;
			}
			return implementation->SharedDrafts->read(implementation->DraftConsumer, *probe.Draft).Accepted;
		};
		const auto cardFor = [&](const std::string& aId) -> const qt::thread_card* {
			const auto found = std::ranges::find(cards->cards(), aId, &qt::thread_card::Id);
			return found == cards->cards().end() ? nullptr : &*found;
		};
		const auto button = [&](const char* aName) -> QAbstractButton* {
			auto* const open = cards->open_card();
			return open != nullptr ? open->findChild<QAbstractButton*>(QString::fromLatin1(aName)) : nullptr;
		};
		const auto refused = [&] {
			auto* const notice = window->conversation_panel()->notice_label();
			return notice != nullptr && notice->isVisibleTo(window) && !notice->text().isEmpty();
		};
		const auto outdatedNote = [&] {
			auto* const open  = cards->open_card();
			auto* const label = open != nullptr ? open->findChild<QLabel*>(QStringLiteral("threadReplyOutdatedNote")) : nullptr;
			return label != nullptr && label->isVisibleTo(window) && label->text() == QStringLiteral("Outdated: this thread's line has changed since it was written");
		};
		const auto renders = qEnvironmentVariable("BPR_REPLY_DRAFT_RENDER_DIRECTORY");
		const auto render  = [&](const QString& aName) {
			if (renders.isEmpty() || window->appearance_settings() == nullptr) {
				return;
			}
			QDir{ }.mkpath(renders);
			for (const auto& [mode, name] : std::array{ std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
				window->appearance_settings()->choose_mode(mode);
				for (auto turn = 0; turn < 6; ++turn) {
					QCoreApplication::processEvents();
				}
				static_cast<void>(window->grab().save(renders + QStringLiteral("/outdated-") + aName + QLatin1Char{ '-' } + name + QStringLiteral(".png"), "PNG"));
			}
			window->appearance_settings()->choose_mode(core::appearance_mode::light);
			for (auto turn = 0; turn < 6; ++turn) {
				QCoreApplication::processEvents();
			}
		};
		const auto openReply = [&]() -> bool {
			if (window->current_modality() != qt::review_modality::review) {
				window->choose_modality(qt::review_modality::review);
				QCoreApplication::processEvents();
				return false;
			}
			if (cards->open_thread_id() != identity) {
				cards->open_thread(identity);
				QCoreApplication::processEvents();
			}
			auto* const field = button("threadReplyField");
			if (field == nullptr || !field->isEnabled() || !field->isVisibleTo(window)) {
				return false;
			}
			field->click();
			QCoreApplication::processEvents();
			return true;
		};
		const auto held = [&](const QString& aWords) {
			const auto& draft = implementation->ReviewDraft;
			return draft && probe.Draft && draft->Id == *probe.Draft && core::thread_bound_reply(*draft) && draft->ReplyToThread == std::optional<std::string>{ std::string{ threadId } }
			&& draft->Body == aWords.toStdString() && draft->State == core::review_draft_state::drafting;
		};
		const auto finish = [&] {
			const auto& evidence = probe.Evidence;
			if (!(evidence.Written && evidence.ServerShown && evidence.Kept && evidence.Presented && evidence.Flagged && evidence.Acted && evidence.NothingSent)) {
				return fail("evidence");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		};
		switch (probe.Phase) {
		case state_type::phase::awaiting_file: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed");
			}
			if (revealed != detail::fixture_file_reveal_result::shown || !ready()) {
				return result::pending;
			}
			// The push: the thread's line leaves the diff, and the public
			// Refresh reads it outdated.
			probe.Before = now();
			implementation->Fixture->stage_server_thread_outdated(std::string{ threadId }, std::string{ writtenOn });
			window->github_refresh_action()->trigger();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_outdated;
			return result::pending;
		}
		case state_type::phase::awaiting_outdated: {
			const auto* const thread   = implementation->accepted_thread(identity);
			const auto* const card     = cardFor(identity);
			probe.Evidence.ServerShown = thread != nullptr && thread->Outdated && !thread->Line && card != nullptr && card->ReplyOnThread;
			note("ready=" + std::to_string(ready() ? 1 : 0) + " outdated=" + std::to_string(thread != nullptr && thread->Outdated ? 1 : 0) + " card=" + std::to_string(card != nullptr ? 1 : 0)
				 + " onThread=" + std::to_string(card != nullptr && card->ReplyOnThread ? 1 : 0) + " waited=" + std::to_string(probe.Waited));
			if (!ready() || (!probe.Evidence.ServerShown && ++probe.Waited < 160U)) {
				return result::pending;
			}
			if (!probe.Evidence.ServerShown) {
				return fail("not-outdated");
			}
			probe.Waited = 0U;
			probe.Phase  = aCourse == course::outdated_restart ? state_type::phase::awaiting_restored : state_type::phase::awaiting_reply_open;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_open:
			note("ready=" + std::to_string(ready() ? 1 : 0) + " open=" + cards->open_thread_id());
			if (!ready() || !openReply()) {
				return ++probe.Waited < 400U ? result::pending : fail("no-reply-field");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_reply_draft;
			return result::pending;
		case state_type::phase::awaiting_reply_draft: {
			const auto& draft = implementation->ReviewDraft;
			note("reply=" + (draft && draft->ReplyToThread ? *draft->ReplyToThread : std::string{ "-" }) + " bound=" + std::to_string(draft && core::thread_bound_reply(*draft) ? 1 : 0)
				 + " draft=" + (draft ? std::to_string(draft->Id.value()) + ":" + std::to_string(draft->Anchor.Line) : std::string{ "none" })
				 + " private=" + std::to_string(implementation->PrivateReview ? 1 : 0) + std::to_string(implementation->PrivateDraftsReady ? 1 : 0)
				 + std::to_string(implementation->PrivateDraftSelectionPending ? 1 : 0) + std::to_string(implementation->PendingThreadReply ? 1 : 0)
				 + " selected=" + std::to_string(implementation->SelectedSemanticSelection ? 1 : 0) + " diff=" + std::to_string(implementation->SelectedDiffPublication ? 1 : 0)
				 + " editor=" + std::to_string(cards->reply_editor() != nullptr ? 1 : 0) + " refused=" + std::to_string(refused() ? 1 : 0)
				 + " notice=" + window->conversation_panel()->notice_label()->text().toStdString());
			if (refused()) {
				return fail("refused");
			}
			if (!draft || !core::thread_bound_reply(*draft) || draft->ReplyToThread != std::optional<std::string>{ std::string{ threadId } } || cards->reply_editor() == nullptr) {
				return ++probe.Waited < 200U ? result::pending : fail("no-thread-bound-reply");
			}
			probe.Evidence.Flagged = outdatedNote();
			if (!probe.Evidence.Flagged) {
				return fail("no-outdated-note");
			}
			probe.Draft = draft->Id;
			cards->reply_editor()->setPlainText(words);
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_reply_saved;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_saved: {
			const auto stored = saved();
			note("held=" + std::to_string(held(words) ? 1 : 0) + " saved=" + std::to_string(stored && stored->Draft.Body == words.toStdString() ? 1 : 0) + " ready=" + std::to_string(ready() ? 1 : 0));
			if (!held(words) || !stored || stored->Draft.Body != words.toStdString() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("not-saved");
			}
			probe.Evidence.Written   = sentNone();
			probe.Evidence.Kept      = core::thread_bound_reply(stored->Draft);
			probe.Evidence.Presented = cards->reply_editor() != nullptr && cards->reply_editor()->toPlainText() == words;
			render(QStringLiteral("written"));
			if (aCourse == course::outdated_write) {
				// Quit with the reply saved; `outdated-restart` reads it back.
				probe.Evidence.Acted       = true;
				probe.Evidence.NothingSent = sentNone();
				return finish();
			}
			auto* const publish = button("threadReplyPublish");
			if (publish == nullptr || !publish->isEnabled()) {
				return ++probe.Waited < 400U ? result::pending : fail("publish-not-offered");
			}
			publish->click();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_published;
			return result::pending;
		}
		case state_type::phase::awaiting_published: {
			const auto        after  = now();
			const auto        stored = saved();
			const auto* const card   = cardFor(identity);
			const auto        landed = card != nullptr && std::ranges::any_of(card->Posts, [&](const qt::card_post& aPost) { return aPost.Body == words; });
			const auto        bound  = after.Replies == probe.Before.Replies + 1U && after.LastReplyThread == threadId && after.LastReplyThreadBound && after.LastReplyCommit == writtenOn
				&& after.NativePosts == probe.Before.NativePosts && after.FilePosts == probe.Before.FilePosts && after.Resolutions == probe.Before.Resolutions;
			note("ready=" + std::to_string(ready() ? 1 : 0) + " replies=" + std::to_string(after.Replies - probe.Before.Replies) + " bound=" + std::to_string(after.LastReplyThreadBound ? 1 : 0)
				 + " commit=" + after.LastReplyCommit + " saved=" + (stored ? std::string{ core::name(stored->Draft.State) } : std::string{ "none" }) + " landed=" + std::to_string(landed ? 1 : 0)
				 + " waited=" + std::to_string(probe.Waited));
			if (!ready() || (!(bound && stored && stored->Draft.State == core::review_draft_state::published && landed) && ++probe.Waited < 400U)) {
				return result::pending;
			}
			// Exactly one write: the reply, bound to the thread and its commit.
			probe.Evidence.NothingSent = bound;
			probe.Evidence.Acted       = stored && stored->Draft.State == core::review_draft_state::published && landed;
			render(QStringLiteral("published"));
			return finish();
		}
		case state_type::phase::awaiting_restored: {
			// After the restart: the saved reply bound to the outdated thread,
			// found among the restored drafts, marked on its thread's note.
			if (!probe.Draft && implementation->SharedDrafts) {
				for (const auto& draft : implementation->SharedDrafts->recovery_snapshot()) {
					if (core::thread_bound_reply(draft.Draft) && draft.Draft.ReplyToThread == std::optional<std::string>{ std::string{ threadId } } && draft.Draft.Body == words.toStdString()
						&& draft.Draft.State == core::review_draft_state::drafting) {
						probe.Draft = draft.Draft.Id;
					}
				}
			}
			const auto* const thread = cardFor(identity);
			probe.Evidence.Written   = true;
			probe.Evidence.Kept      = probe.Draft.has_value();
			const auto marked        = thread != nullptr && thread->UnsentReply == std::optional{ core::reply_draft_standing::answerable } && thread->Notice.isEmpty();
			note("ready=" + std::to_string(ready() ? 1 : 0) + " drafts=" + std::to_string(implementation->PrivateDraftsReady ? 1 : 0) + " saved=" + std::to_string(probe.Draft ? 1 : 0)
				 + " marked=" + std::to_string(marked ? 1 : 0) + " waited=" + std::to_string(probe.Waited));
			if (!ready() || (!(probe.Evidence.Kept && marked) && ++probe.Waited < 160U)) {
				return result::pending;
			}
			if (!probe.Evidence.Kept || !marked) {
				return fail("not-restored");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_reopened;
			return result::pending;
		}
		case state_type::phase::awaiting_reopened: {
			if (refused()) {
				return fail("refused");
			}
			if (!held(words) || cards->reply_editor() == nullptr) {
				note("opening editor=" + (implementation->ReviewDraft ? std::to_string(implementation->ReviewDraft->Id.value()) : std::string{ "none" }));
				if (ready() && probe.Waited++ % 10U == 0U) {
					static_cast<void>(openReply());
				}
				return probe.Waited < 200U ? result::pending : fail("reply-not-reopened");
			}
			probe.Evidence.Flagged     = outdatedNote();
			probe.Evidence.Presented   = cards->reply_editor()->toPlainText() == words;
			probe.Evidence.Acted       = held(words);
			probe.Evidence.NothingSent = sentNone();
			render(QStringLiteral("restarted"));
			return finish();
		}
		case state_type::phase::passed:
			return result::passed;
		default:
			break;
		}
		return fail("phase");
	}

	auto detail::github_application_fixture_access::reply_draft_evidence(
		const github_application& aApplication) -> detail::reply_draft_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->ReplyDraftProbe.has_value() ? implementation->ReplyDraftProbe->Evidence : detail::reply_draft_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::reply_draft_diagnostic(
		const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->ReplyDraftProbe.has_value() ? implementation->ReplyDraftProbe->Diagnostic : std::string{ };
	}

	auto detail::github_application_fixture_access::exercise_refresh_pick(
		github_application& aApplication, const detail::refresh_pick_course aCourse) -> detail::refresh_pick_probe_result
	{
		using result               = detail::refresh_pick_probe_result;
		using state_type           = refresh_pick_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->RefreshPickProbe.has_value()) {
			implementation->RefreshPickProbe.emplace();
		}
		auto& probe = *implementation->RefreshPickProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr || implementation->WorkbenchHost == nullptr
			|| implementation->TabHost == nullptr || !implementation->WindowRead || !implementation->ReadDecision) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const view   = window->files_drawer() != nullptr ? window->files_drawer()->contents_view() : nullptr;
		const auto  fail   = [&probe](const std::string_view aWhy) {
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
		constexpr auto path   = std::string_view{ "src/engine/renderer.cpp" };
		constexpr auto other  = std::string_view{ "src/engine/renderer.h" };
		const auto     shown  = core::file_identity_of(path);
		const auto     picked = core::file_identity_of(other);
		const auto&    last   = implementation->LastSelectedDiff;
		const auto     ready  = [&] {
			return window->github_refresh_action()->isEnabled() && window->github_status_text() == "Pull request ready."
				&& implementation->WorkbenchHost->outstanding_requests() == 0U && !implementation->WindowRead->state().PendingPullRequest.has_value();
		};
		const auto active = [&]() -> const core::document_tab* {
			const auto& tabs = implementation->TabHost->state();
			return tabs.Active ? core::find_tab(tabs, *tabs.Active) : nullptr;
		};
		const auto activeFile = [&]() -> std::optional<core::file_identity> {
			const auto* const tab = active();
			return tab != nullptr ? tab->View.File : std::nullopt;
		};
		const auto name = [&](const std::optional<core::file_identity>& aFile) -> std::string {
			if (!aFile) {
				return "-";
			}
			if (*aFile == shown) {
				return "cpp";
			}
			return *aFile == picked ? "h" : "other";
		};
		const auto facts = [&] {
			return "selected=" + name(implementation->WorkbenchHost->selected()) + " tab=" + name(activeFile())
				+ " diff=" + (last ? std::string{ last->Request.File.PresentationPath } : std::string{ "-" })
				+ " review=" + std::to_string(window->current_modality() == qt::review_modality::review ? 1 : 0)
				+ " ready=" + std::to_string(ready() ? 1 : 0) + " load=" + std::to_string(implementation->TabLoad ? 1 : 0);
		};
		// Review shows the picked file: chosen, in the active tab, its diff on screen.
		const auto showsPick = [&] {
			const auto chosen = implementation->WorkbenchHost->selected() == std::optional{ picked } && activeFile() == std::optional{ picked };
			const auto drawn  = last && last->Request.File.PresentationPath == other && window->comparison_surface()->current_result() != nullptr;
			return window->current_modality() == qt::review_modality::review && chosen && drawn;
		};
		switch (probe.Phase) {
		case state_type::phase::awaiting_file: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed");
			}
			if (revealed == detail::fixture_file_reveal_result::shown) {
				probe.Phase = state_type::phase::awaiting_line;
			}
			return result::pending;
		}
		case state_type::phase::awaiting_line: {
			// A line the reviewer chooses in renderer.cpp, which its tab keeps:
			// the place the window holds before the read.
			auto* const surface = window->comparison_surface();
			if (!probe.Line) {
				const auto opened = surface->selection() ? std::optional{ surface->selection()->PreferredLine.Line } : std::nullopt;
				for (auto candidate = std::uint32_t{ 1U }; candidate < 32U && !probe.Line; ++candidate) {
					if (candidate != opened && surface->select_conversation_line(core::diff_side::right, candidate)) {
						probe.Line = candidate;
					}
				}
			}
			const auto* const tab  = active();
			const auto        kept = probe.Line && surface->selection() && surface->selection()->PreferredLine.Line == *probe.Line && tab != nullptr
				&& tab->View.File == std::optional{ shown } && tab->View.Selection == surface->selection();
			note(facts() + " line=" + (probe.Line ? std::to_string(*probe.Line) : std::string{ "-" }) + " kept=" + std::to_string(kept ? 1 : 0));
			if (!ready() || !kept || implementation->TabLoad || implementation->TabAdvanceQueued) {
				return result::pending;
			}
			const auto& accepted = implementation->WindowRead->accepted_snapshot();
			probe.ShownBefore    = accepted ? std::optional{ accepted->Snapshot } : std::nullopt;
			// The read's answer waits on the window's read lane until the pick.
			implementation->ReadDecision->hold(true);
			if (aCourse == detail::refresh_pick_course::refresh) {
				window->github_refresh_action()->trigger();
			} else {
				implementation->reread_after_publication();
			}
			probe.Phase = state_type::phase::awaiting_read;
			return result::pending;
		}
		case state_type::phase::awaiting_read: {
			const auto row = drawer_row(*window, other);
			note(facts() + " reading=" + std::to_string(implementation->WindowRead->state().PendingPullRequest ? 1 : 0) + " row=" + std::to_string(row.isValid() ? 1 : 0));
			if (!implementation->WindowRead->state().PendingPullRequest || view == nullptr || !row.isValid()
				|| implementation->WorkbenchHost->outstanding_requests() != 0U || implementation->TabLoad || implementation->TabAdvanceQueued) {
				return result::pending;
			}
			// The window takes a picked file into the active tab on a later
			// turn. Holding that turn until the read lands gives the order a
			// busy event loop can give: the pick, the read, then the tab.
			implementation->TabAdvanceQueued = true;
			view->setCurrentIndex(row);
			implementation->ReadDecision->hold(false);
			if (implementation->WorkbenchHost->selected() != std::optional{ picked }) {
				implementation->TabAdvanceQueued = false;
				return fail("pick-refused");
			}
			probe.Phase = state_type::phase::awaiting_landed;
			return result::pending;
		}
		case state_type::phase::awaiting_landed: {
			const auto& accepted = implementation->WindowRead->accepted_snapshot();
			const auto  landed   = accepted && std::optional{ accepted->Snapshot } != probe.ShownBefore && !implementation->WindowRead->state().PendingPullRequest;
			note(facts() + " landed=" + std::to_string(landed ? 1 : 0));
			if (!landed) {
				return result::pending;
			}
			// The read retired the workbench's selection while the active tab
			// still named renderer.cpp: the pick shows nowhere.
			probe.Evidence.Raced             = activeFile() == std::optional{ shown } && !implementation->WorkbenchHost->selected().has_value();
			implementation->TabAdvanceQueued = false;
			if (!probe.Evidence.Raced) {
				return fail("read-not-between-pick-and-tab");
			}
			implementation->schedule_tab_presentation();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_settled;
			return result::pending;
		}
		case state_type::phase::awaiting_settled: {
			note(facts() + " waited=" + std::to_string(probe.Waited));
			const auto settled = ready() && !implementation->TabLoad && implementation->WorkbenchHost->selected().has_value() && last
				&& window->comparison_surface()->current_result() != nullptr;
			if (!probe.Evidence.Kept) {
				if (!settled) {
					return ++probe.Waited < 400U ? result::pending : fail("not-settled");
				}
				if (!showsPick()) {
					return fail("pick-lost");
				}
				probe.Evidence.Kept = true;
				probe.Waited        = 0U;
				return result::pending;
			}
			if (!settled || !showsPick()) {
				return fail("pick-lost-later");
			}
			if (++probe.Waited < 20U) {
				return result::pending;
			}
			probe.Evidence.Stayed = true;
			probe.Phase           = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::passed: return result::passed;
		case state_type::phase::failed: return result::failed;
		}
		return result::failed;
	}

	auto detail::github_application_fixture_access::refresh_pick_evidence(const github_application& aApplication) -> detail::refresh_pick_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->RefreshPickProbe.has_value() ? implementation->RefreshPickProbe->Evidence : detail::refresh_pick_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::refresh_pick_diagnostic(const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->RefreshPickProbe.has_value() ? implementation->RefreshPickProbe->Diagnostic : std::string{ };
	}

	auto detail::github_application_fixture_access::exercise_draft_delete(
		github_application& aApplication, const detail::draft_delete_course aCourse) -> detail::draft_delete_probe_result
	{
		using result               = detail::draft_delete_probe_result;
		using course               = detail::draft_delete_course;
		using state_type           = draft_delete_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->DraftDeleteProbe.has_value()) {
			implementation->DraftDeleteProbe.emplace();
		}
		auto& probe = *implementation->DraftDeleteProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr || implementation->WorkbenchHost == nullptr || implementation->ReviewMutationHost == nullptr) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const cards  = window->conversation_panel() != nullptr ? window->conversation_panel()->cards() : nullptr;
		auto* const editor = window->review_editor();
		auto* const model  = window->conversation_model();
		if (cards == nullptr || editor == nullptr || model == nullptr) {
			return result::pending;
		}
		const auto fail = [&probe](const std::string_view aWhy) {
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
		constexpr auto path  = std::string_view{ "src/engine/renderer.cpp" };
		const auto     words = std::array{
			std::string{ "issue: this draft on a changed line is deleted. (BPR-1862 line)" },
			std::string{ "issue: this draft outside every hunk is deleted. (BPR-1862 off-hunk)" },
			std::string{ "The agent's answer, drafted outside every hunk, is deleted. (BPR-1862 agent)" },
			std::string{ "The agent's answer, drafted in a review, is deleted from Finish your review. (BPR-1862 agent review)" },
		};
		// Drafted again from the same turn, and kept: `reopened` finds it.
		const auto again = std::string{ "The agent's answer, drafted again outside every hunk, is kept. (BPR-1862 agent again)" };
		// The draft an agent turn holds, while it says Go to draft.
		const auto turnHolds = [&](const std::optional<std::string>& aKey) -> std::optional<core::review_draft_id> {
	#if defined(BPR_AGENT_INTEGRATIONS)
			if (aKey) {
				if (const auto found = implementation->AgentAnswerDrafts.find(*aKey); found != implementation->AgentAnswerDrafts.end()) {
					return found->second.Drafted;
				}
			}
	#else
			static_cast<void>(aKey);
	#endif
			return std::nullopt;
		};
		const auto  index = aCourse == course::line ? 0U : aCourse == course::off_hunk ? 1U
																					   : 2U;
		const auto& mine  = words[index];
		const auto  ready = [&] {
			return window->github_refresh_action()->isEnabled() && window->github_status_text() == "Pull request ready."
				&& implementation->WorkbenchHost->outstanding_requests() == 0U && !implementation->PendingReviewMutation.has_value()
				&& !implementation->PrivateDraftCommandPending && !implementation->PrivateDraftSelectionPending;
		};
		const auto saved = [&] {
			return implementation->SharedDrafts ? implementation->SharedDrafts->recovery_snapshot() : std::vector<core::revisioned_review_draft>{ };
		};
		// An active draft in the store with these words.
		const auto stored = [&](const std::string& aWords) {
			const auto all = saved();
			return std::ranges::any_of(all, [&aWords](const core::revisioned_review_draft& aDraft) {
				return aDraft.Draft.Body == aWords && (aDraft.Draft.State == core::review_draft_state::drafting || aDraft.Draft.State == core::review_draft_state::publishing);
			});
		};
		// The words anywhere the window shows a draft: the drawer's field, the
		// editor, or a note's post.
		const auto shown = [&](const std::string& aWords) {
			const auto text = QString::fromStdString(aWords);
			if (cards->draft_editor() != nullptr && cards->draft_editor()->toPlainText() == text) {
				return true;
			}
			if (editor->body_editor()->toPlainText() == text) {
				return true;
			}
			return std::ranges::any_of(cards->cards(), [&text](const qt::thread_card& aCard) {
				return aCard.Draft && std::ranges::any_of(aCard.Posts, [&text](const qt::card_post& aPost) { return aPost.Body == text; });
			});
		};
		const auto facts = [&] {
			const auto& draft = implementation->ReviewDraft;
			return "ready=" + std::to_string(ready() ? 1 : 0) + " draft=" + (draft ? std::to_string(draft->Id.value()) : std::string{ "-" })
				+ " state=" + (draft ? std::string{ core::name(draft->State) } : std::string{ "-" }) + " body=" + (draft ? draft->Body : std::string{ "-" })
				+ " stored=" + std::to_string(stored(mine) ? 1 : 0) + " shown=" + std::to_string(shown(mine) ? 1 : 0)
				+ " field=" + std::to_string(cards->draft_editor() != nullptr && cards->draft_editor()->toPlainText().toStdString() == mine ? 1 : 0)
				+ " editor=" + std::to_string(editor->body_editor()->toPlainText().toStdString() == mine ? 1 : 0) + " cards=" + [&] {
					  auto ids = std::string{ };
					  for (const auto& card : cards->cards()) {
						  if (card.Draft) {
							  ids += card.Id + "[" + (card.Posts.empty() ? std::string{ "-" } : card.Posts.front().Body.toStdString()) + "],";
						  }
					  }
					  return ids;
				  }()
				+ " writing=" + std::to_string(cards->writing_draft() ? 1 : 0) + " status=" + editor->mutation_status_label()->text().toStdString() + " bar=" + window->statusBar()->currentMessage().toStdString();
		};
		switch (probe.Phase) {
		case state_type::phase::awaiting_file: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed");
			}
			if (revealed == detail::fixture_file_reveal_result::shown) {
				if (window->current_modality() != qt::review_modality::review) {
					window->choose_modality(qt::review_modality::review);
					QCoreApplication::processEvents();
				}
				probe.Phase = state_type::phase::awaiting_lines;
			}
			return result::pending;
		}
		case state_type::phase::awaiting_lines: {
			note(facts() + " drafts=" + std::to_string(implementation->PrivateDraftsReady ? 1 : 0));
			if (!ready() || !implementation->PrivateDraftsReady || window->comparison_surface()->current_result() == nullptr) {
				return result::pending;
			}
			if (aCourse == course::reopened) {
				// After the restart: none of the deleted drafts is kept, and
				// none shows.
				for (const auto& written : words) {
					if (stored(written)) {
						return fail("reopened-store-kept:" + written);
					}
					if (shown(written)) {
						return fail("reopened-view-shows:" + written);
					}
				}
				// The agent turn's second draft, never deleted, is kept.
				if (!stored(again)) {
					return fail("reopened-kept-draft-lost");
				}
				probe.Evidence.Written = probe.Evidence.Asked = probe.Evidence.ViewGone = probe.Evidence.StoreGone = true;
				probe.Waited                                                                                       = 0U;
				probe.Phase                                                                                        = state_type::phase::awaiting_stayed;
				return result::pending;
			}
			// [[BPR-1735]]. GitHub's own diff of the file decides which lines
			// are inside a hunk.
			const auto lines = implementation->shown_github_diff_lines();
			if (!lines) {
				return result::pending;
			}
			if (!*lines) {
				return fail("github-diff-unknown");
			}
			const auto all = saved();
			for (auto candidate = std::uint32_t{ 0U }; candidate < 96U && !probe.Line; ++candidate) {
				const auto outside = core::place_on_github_diff(*lines, core::diff_side::right, candidate, std::nullopt) == core::github_diff_placement::outside_diff;
				if (outside != (aCourse != course::line)) {
					continue;
				}
				if (std::ranges::any_of(all, [candidate](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Anchor.Line == candidate && !aDraft.Draft.Body.empty(); })) {
					continue;
				}
				if (window->comparison_surface()->select_conversation_line(core::diff_side::right, candidate) && window->conversation_panel()->anchor_line().has_value()
					&& model->rowCount() == 0) {
					probe.Line = candidate;
				}
			}
			if (!probe.Line) {
				return fail("no-line");
			}
			probe.Phase = state_type::phase::awaiting_line;
			return result::pending;
		}
		case state_type::phase::awaiting_line: {
			const auto& draft = implementation->ReviewDraft;
			note(facts() + " line=" + std::to_string(*probe.Line));
			if (!ready() || !draft || draft->ReplyToThread || draft->Anchor.Line != *probe.Line || draft->Anchor.Side != core::diff_side::right
				|| draft->State != core::review_draft_state::drafting || !window->comment_action()->isEnabled()) {
				return result::pending;
			}
			if (aCourse == course::agent) {
	#if defined(BPR_AGENT_INTEGRATIONS)
				// [[BPR-1773]]. The Draft comment an agent's answer offers,
				// for a question asked with this line selected.
				const auto target = implementation->decide_answer_draft();
				const auto found  = implementation->AgentAnswerDrafts.find(target.Key);
				if (found == implementation->AgentAnswerDrafts.end() || found->second.Place != core::answer_draft_place::line_comment || found->second.First != *probe.Line) {
					return fail("agent-target-not-the-line");
				}
				probe.AgentKey = target.Key;
				implementation->draft_agent_answer(target.Key, QString::fromStdString(mine), [&probe](const bool aDrafted, const QString& aWhy) {
					probe.AgentAsked = true;
					if (!aDrafted) {
						probe.Diagnostic += " | agent-refused=" + aWhy.toStdString();
					}
				});
	#else
				return fail("agent-integrations-off");
	#endif
			} else {
				window->comment_action()->trigger();
				QCoreApplication::processEvents();
				if (cards->draft_editor() == nullptr || cards->draft_editor()->isReadOnly()) {
					return fail("drawer-draft-not-editable");
				}
				cards->draft_editor()->setPlainText(QString::fromStdString(mine));
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_saved;
			return result::pending;
		}
		case state_type::phase::awaiting_saved: {
			const auto& draft = implementation->ReviewDraft;
			note(facts());
			if (!ready() || !draft || draft->Body != mine || !stored(mine)) {
				return ++probe.Waited < 2000U ? result::pending : fail("not-saved");
			}
			if (aCourse == course::agent) {
				// The turn says Go to draft, on this draft.
				if (!probe.AgentAsked) {
					return ++probe.Waited < 2000U ? result::pending : fail("agent-not-answered");
				}
				if (turnHolds(probe.AgentKey) != std::optional{ draft->Id }) {
					return fail("agent-turn-not-on-its-draft");
				}
			}
			probe.Draft            = draft->Id;
			probe.Evidence.Written = true;
			// The draft opens in the drawer, where its Delete is.
			if (!cards->writing_draft()) {
				window->comment_action()->trigger();
				QCoreApplication::processEvents();
			}
			auto* const open   = cards->open_card();
			auto* const remove = open != nullptr ? open->findChild<QAbstractButton*>(QStringLiteral("threadDeleteDraft")) : nullptr;
			if (remove == nullptr || !remove->isEnabled() || !remove->isVisibleTo(window)) {
				return ++probe.Waited < 2000U ? result::pending : fail("no-delete");
			}
			remove->click();
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_question;
			return result::pending;
		}
		case state_type::phase::awaiting_question: {
			auto* const question = window->delete_draft_confirmation();
			auto* const yes      = question != nullptr ? question->findChild<QAbstractButton*>(QStringLiteral("deleteDraftConfirmationConfirm")) : nullptr;
			note(facts());
			if (yes == nullptr || !question->isVisible()) {
				return fail("delete-asked-nothing");
			}
			yes->click();
			QCoreApplication::processEvents();
			probe.Evidence.Asked = true;
			probe.Waited         = 0U;
			probe.Phase          = state_type::phase::awaiting_deleted;
			return result::pending;
		}
		case state_type::phase::awaiting_deleted: {
			note(facts() + " waited=" + std::to_string(probe.Waited));
			if (!ready() || stored(mine) || shown(mine)) {
				return ++probe.Waited < 400U ? result::pending : fail("not-deleted");
			}
			probe.Evidence.StoreGone = true;
			probe.Evidence.ViewGone  = true;
			// The line chosen again reopens nothing with these words.
			static_cast<void>(window->comparison_surface()->select_conversation_line(core::diff_side::right, *probe.Line == 0U ? 1U : *probe.Line - 1U));
			QCoreApplication::processEvents();
			static_cast<void>(window->comparison_surface()->select_conversation_line(core::diff_side::right, *probe.Line));
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_stayed;
			return result::pending;
		}
		case state_type::phase::awaiting_stayed: {
			note(facts() + " waited=" + std::to_string(probe.Waited));
			for (const auto& written : words) {
				if (stored(written) || shown(written)) {
					return fail("came-back:" + written);
				}
			}
			if (!ready() || ++probe.Waited < 20U) {
				return probe.Waited < 2000U ? result::pending : fail("not-settled");
			}
			probe.Evidence.Stayed = true;
			if (aCourse != course::agent) {
				probe.Phase = state_type::phase::passed;
				return result::passed;
			}
			// The turn let its deleted draft go, and Draft comment pressed
			// again makes a new one.
			probe.Evidence.Released = !turnHolds(probe.AgentKey).has_value();
			if (!probe.Evidence.Released) {
				return fail("agent-turn-still-holds-deleted-draft");
			}
			const auto& draft = implementation->ReviewDraft;
			if (!draft || draft->Anchor.Line != *probe.Line || draft->State != core::review_draft_state::drafting || !window->comment_action()->isEnabled()) {
				return probe.Waited < 2000U ? result::pending : fail("line-draft-not-back");
			}
	#if defined(BPR_AGENT_INTEGRATIONS)
			probe.AgentAsked = false;
			implementation->draft_agent_answer(*probe.AgentKey, QString::fromStdString(again), [&probe](const bool aDrafted, const QString& aWhy) {
				probe.AgentAsked = true;
				if (!aDrafted) {
					probe.Diagnostic += " | agent-again-refused=" + aWhy.toStdString();
				}
			});
	#endif
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_again_saved;
			return result::pending;
		}
		case state_type::phase::awaiting_again_saved: {
			const auto& draft = implementation->ReviewDraft;
			note(facts() + " again=" + std::to_string(stored(again) ? 1 : 0));
			if (!ready() || !probe.AgentAsked || !draft || draft->Body != again || !stored(again)) {
				return ++probe.Waited < 2000U ? result::pending : fail("again-not-saved");
			}
			if (draft->Id == probe.Draft || turnHolds(probe.AgentKey) != std::optional{ draft->Id }) {
				return fail("again-not-a-fresh-draft");
			}
			probe.Again = draft->Id;
			// Then a second turn's draft on a line GitHub's diff holds, which a
			// review can carry; it leaves the review from Finish your review.
			const auto lines = implementation->shown_github_diff_lines();
			if (!lines || !*lines) {
				return fail("github-diff-unknown");
			}
			if (cards->writing_draft()) {
				window->comment_action()->trigger();
				QCoreApplication::processEvents();
			}
			for (auto candidate = std::uint32_t{ 0U }; candidate < 96U && !probe.ReviewLine; ++candidate) {
				if (candidate != *probe.Line && core::place_on_github_diff(*lines, core::diff_side::right, candidate, std::nullopt) != core::github_diff_placement::outside_diff
					&& window->comparison_surface()->select_conversation_line(core::diff_side::right, candidate) && model->rowCount() == 0) {
					probe.ReviewLine = candidate;
				}
			}
			if (!probe.ReviewLine) {
				return fail("no-review-line");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_review_line;
			return result::pending;
		}
		case state_type::phase::awaiting_review_line: {
			const auto& draft = implementation->ReviewDraft;
			note(facts() + " reviewLine=" + std::to_string(*probe.ReviewLine));
			if (!ready() || !draft || draft->ReplyToThread || draft->Anchor.Line != *probe.ReviewLine || draft->State != core::review_draft_state::drafting
				|| !window->comment_action()->isEnabled()) {
				return ++probe.Waited < 2000U ? result::pending : fail("review-line-draft");
			}
	#if defined(BPR_AGENT_INTEGRATIONS)
			const auto target = implementation->decide_answer_draft();
			probe.ReviewKey   = target.Key;
			probe.AgentAsked  = false;
			implementation->draft_agent_answer(target.Key, QString::fromStdString(words[3]), [&probe](const bool aDrafted, const QString& aWhy) {
				probe.AgentAsked = true;
				if (!aDrafted) {
					probe.Diagnostic += " | agent-review-refused=" + aWhy.toStdString();
				}
			});
	#endif
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_review_saved;
			return result::pending;
		}
		case state_type::phase::awaiting_review_saved: {
			const auto& draft = implementation->ReviewDraft;
			note(facts() + " review=" + std::to_string(stored(words[3]) ? 1 : 0));
			if (!ready() || !probe.AgentAsked || !draft || draft->Body != words[3] || !stored(words[3])) {
				return ++probe.Waited < 2000U ? result::pending : fail("review-not-saved");
			}
			if (turnHolds(probe.ReviewKey) != std::optional{ draft->Id }) {
				return fail("review-turn-not-on-its-draft");
			}
			probe.Draft = draft->Id;
			// Start a review takes it into a review.
			if (!cards->writing_draft()) {
				window->comment_action()->trigger();
				QCoreApplication::processEvents();
			}
			auto* const open   = cards->open_card();
			auto* const review = open != nullptr ? open->findChild<QAbstractButton*>(QStringLiteral("threadReviewDraft")) : nullptr;
			if (review == nullptr || !review->isEnabled()) {
				return ++probe.Waited < 2000U ? result::pending : fail("no-start-a-review");
			}
			review->click();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_review_pending;
			return result::pending;
		}
		case state_type::phase::awaiting_review_pending: {
			const auto all     = saved();
			const auto pending = std::ranges::any_of(all, [&](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Id == probe.Draft && core::pending_in_review(aDraft.Draft); });
			note(facts() + " pending=" + std::to_string(pending ? 1 : 0));
			if (!ready() || !pending || cards->writing_draft()) {
				return ++probe.Waited < 2000U ? result::pending : fail("review-not-pending");
			}
			if (!window->review_sheet_open()) {
				window->finish_review_action()->trigger();
				QCoreApplication::processEvents();
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_review_finish;
			return result::pending;
		}
		case state_type::phase::awaiting_review_finish: {
			auto* const summary = window->review_submission_editor_view();
			auto        visible = std::vector<QToolButton*>{ };
			for (auto* const button : summary->pending_list()->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingDelete"))) {
				if (button->isVisibleTo(window)) {
					visible.push_back(button);
				}
			}
			note(facts() + " sheet=" + std::to_string(window->review_sheet_open() ? 1 : 0) + " rows=" + std::to_string(visible.size()));
			if (!window->review_sheet_open() || visible.size() != 1U) {
				return ++probe.Waited < 2000U ? result::pending : fail("finish-review-row");
			}
			visible.front()->click();
			QCoreApplication::processEvents();
			auto* const question = summary->delete_confirmation();
			auto* const yes      = question != nullptr ? question->findChild<QAbstractButton*>(QStringLiteral("deleteCommentConfirmationConfirm")) : nullptr;
			if (yes == nullptr) {
				return fail("row-delete-asked-nothing");
			}
			yes->click();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_review_deleted;
			return result::pending;
		}
		case state_type::phase::awaiting_review_deleted: {
			note(facts() + " review=" + std::to_string(stored(words[3]) ? 1 : 0) + " held=" + std::to_string(turnHolds(probe.ReviewKey) ? 1 : 0));
			if (!ready() || stored(words[3]) || shown(words[3]) || turnHolds(probe.ReviewKey).has_value()) {
				return ++probe.Waited < 400U ? result::pending : fail("review-comment-not-let-go");
			}
			// The first turn's second draft, never deleted, is kept.
			if (!stored(again) || turnHolds(probe.AgentKey) != std::optional{ probe.Again }) {
				return fail("kept-draft-lost");
			}
			probe.Evidence.Fresh = true;
			probe.Phase          = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::passed: return result::passed;
		case state_type::phase::failed: return result::failed;
		}
		return result::failed;
	}

	auto detail::github_application_fixture_access::draft_delete_evidence(const github_application& aApplication) -> detail::draft_delete_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->DraftDeleteProbe.has_value() ? implementation->DraftDeleteProbe->Evidence : detail::draft_delete_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::draft_delete_diagnostic(const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->DraftDeleteProbe.has_value() ? implementation->DraftDeleteProbe->Diagnostic : std::string{ };
	}
#endif
}
