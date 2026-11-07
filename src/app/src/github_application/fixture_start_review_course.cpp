#include "review_submission_workflow.hpp"

#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/application/github_epoch_handoff.hpp"
	#include "bpr/application/github_read_model.hpp"
	#include "bpr/application/github_window_read_host.hpp"
	#include "bpr/application/review_ledger_application_host.hpp"
	#include "bpr/application/shared_draft_application_host.hpp"
	#include "bpr/application/workbench_application_host.hpp"
	#include "bpr/application/workbench_runtime_ports.hpp"
	#include "bpr/core/appearance_model.hpp"
	#include "bpr/core/diff_model.hpp"
	#include "bpr/core/diff_semantics.hpp"
	#include "bpr/core/diff_transition.hpp"
	#include "bpr/core/epoch_model.hpp"
	#include "bpr/core/github_model.hpp"
	#include "bpr/core/model.hpp"
	#include "bpr/core/review_ledger.hpp"
	#include "bpr/core/review_round_trip.hpp"
	#include "bpr/core/review_submission.hpp"
	#include "bpr/core/review_submission_unknown.hpp"
	#include "bpr/core/revisioned_review_draft.hpp"
	#include "bpr/qt/appearance_settings_panel.hpp"
	#include "bpr/qt/compose_review_sheet.hpp"
	#include "bpr/qt/conversation_cards_view.hpp"
	#include "bpr/qt/destructive_question.hpp"
	#include "bpr/qt/diff_workbench.hpp"
	#include "bpr/qt/github_conversation_model.hpp"
	#include "bpr/qt/github_conversation_panel.hpp"
	#include "bpr/qt/review_draft_editor.hpp"
	#include "bpr/qt/review_submission_editor.hpp"
	#include "bpr/qt/workbench_strip.hpp"
	#include "bpr/qt/workbench_window.hpp"
	#include "github_application/window_composition.hpp"
	#include "github_application/window_probe_states.hpp"
	#include "github_application_fixture_access.hpp"
	#include "github_public_path_fixture.hpp"
	#include "review_submission_fixture.hpp"
	#include <QAbstractButton>
	#include <QAction>
	#include <QAnyStringView>
	#include <QByteArray>
	#include <QByteArrayView>
	#include <QChar>
	#include <QCheckBox>
	#include <QColor>
	#include <QCoreApplication>
	#include <QDesktopServices>
	#include <QDir>
	#include <QEvent>
	#include <QFlags>
	#include <QJsonArray>
	#include <QJsonDocument>
	#include <QJsonObject>
	#include <QJsonValue>
	#include <QKeyEvent>
	#include <QLabel>
	#include <QLatin1Char>
	#include <QList>
	#include <QListWidget>
	#include <QListWidgetItem>
	#include <QMainWindow>
	#include <QObject>
	#include <QPainter>
	#include <QPixmap>
	#include <QPlainTextEdit>
	#include <QPointF>
	#include <QRect>
	#include <QScrollArea>
	#include <QSizeF>
	#include <QStatusBar>
	#include <QString>
	#include <QTextEdit>
	#include <QTextStream>
	#include <QToolButton>
	#include <QWidget>
	#include <algorithm>
	#include <array>
	#include <condition_variable>
	#include <cstddef>
	#include <mutex>
	#include <string_view>
	#include <tuple>
	#include <utility>
	#include <vector>
#endif

namespace bpr::app {
#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::exercise_start_review(
		github_application& aApplication, const detail::start_review_course aCourse) -> detail::start_review_probe_result
	{
		using result               = detail::start_review_probe_result;
		using course               = detail::start_review_course;
		using state_type           = start_review_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->StartReviewProbe.has_value()) {
			implementation->StartReviewProbe.emplace();
			// [[BPR-1735]]. This course's lines lie outside GitHub's diff of its
			// fixture; its subject is the review, so it places as a build before
			// the hunk rule did. The BPR-1735 courses cover placement.
			implementation->WholeDiffPlacementDisabled = true;
		}
		auto& probe = *implementation->StartReviewProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr || implementation->WorkbenchHost == nullptr
			|| implementation->ReviewMutationHost == nullptr || !implementation->ReviewSubmissionServer) {
			return result::pending;
		}
		auto* const window    = &*implementation->Window;
		auto* const cards     = window->conversation_panel() != nullptr ? window->conversation_panel()->cards() : nullptr;
		auto* const editor    = window->review_editor();
		auto* const model     = window->conversation_model();
		auto* const summary   = window->review_submission_editor_view();
		auto* const chip      = static_cast<qt::review_progress_chip*>(window->review_chip());
		auto* const chipShown = window->review_chip_action();
		if (cards == nullptr || editor == nullptr || model == nullptr || summary == nullptr || chip == nullptr || chipShown == nullptr) {
			return result::pending;
		}
		auto&      server = *implementation->ReviewSubmissionServer;
		const auto fail   = [&probe](const std::string_view aWhy) {
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
		constexpr auto other = std::string_view{ "src/engine/renderer.h" };
		// [[BPR-1541]]. The lines the page shows for an unreachable list and
		// for a server error.
		const auto unknownLine        = [](const core::review_submission_unknown_reason aReason) { return qt::review_submission_unknown_text(core::review_submission_unknown_notice{ .Reason = aReason }); };
		const auto unknownServerError = qt::review_submission_unknown_text(core::review_submission_unknown_notice{ .Reason = core::review_submission_unknown_reason::http_status, .StatusClass = 5U });
		const auto words              = std::array{
			QStringLiteral("question: Where does this value come from? (BPR-1345 first)"),
			QStringLiteral("nit: these two lines could be one. (BPR-1345 range)"),
			QStringLiteral("BPR-1345: this one goes on its own."),
		};
		// [[BPR-1345]] v002, `again`: the older draft and a summary that
		// is discarded.
		const auto older      = QStringLiteral("suggestion: an older draft, kept out of the review. (BPR-1345 again)");
		const auto summarised = QStringLiteral("A summary to discard. (BPR-1345 again)");
		const auto ready      = [&] {
			return window->github_refresh_action()->isEnabled() && window->github_status_text() == "Pull request ready."
				&& implementation->WorkbenchHost->outstanding_requests() == 0U && !implementation->PendingReviewMutation.has_value()
				&& !implementation->PrivateDraftCommandPending && !implementation->PrivateDraftSelectionPending;
		};
		const auto saved = [&] {
			return implementation->SharedDrafts ? implementation->SharedDrafts->recovery_snapshot() : std::vector<core::revisioned_review_draft>{ };
		};
		const auto pending = [&] {
			auto drafts = std::vector<core::revisioned_review_draft>{ };
			for (auto& draft : saved()) {
				if (core::pending_in_review(draft.Draft)) {
					drafts.push_back(std::move(draft));
				}
			}
			std::ranges::sort(drafts, { }, [](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Anchor.Line; });
			return drafts;
		};
		// Published drafts leave the saved list; each carried one is read by
		// its identity.
		const auto countPublished = [&] {
			return static_cast<std::size_t>(std::ranges::count_if(probe.Drafts, [&](const core::review_draft_id aDraft) {
				if (!implementation->SharedDrafts) {
					return false;
				}
				const auto read = implementation->SharedDrafts->read(implementation->DraftConsumer, aDraft);
				return read.Accepted && read.Accepted->Draft.State == core::review_draft_state::published && read.Accepted->Draft.InReview;
			}));
		};
		const auto countHeld = [&] {
			const auto all = saved();
			return static_cast<std::size_t>(std::ranges::count_if(all, [](const core::revisioned_review_draft& aDraft) { return core::held_by_review(aDraft.Draft); }));
		};
		const auto posts = [&server] {
			const auto lock = std::scoped_lock{ server.Mutex };
			return server.ReviewPosts;
		};
		const auto chipSays = [&](const std::size_t aCount) {
			return chipShown->isVisible() && chip->pending() == aCount && !chip->submitting()
				&& chip->summary() == QStringLiteral("Review · %1 pending").arg(aCount);
		};
		const auto composerButton = [&](const char* aName) -> QAbstractButton* {
			auto* const open = cards->open_card();
			return open != nullptr ? open->findChild<QAbstractButton*>(QString::fromLatin1(aName)) : nullptr;
		};
		// The first current-head line no thread or draft is on, chosen in Review.
		const auto pick = [&]() -> std::optional<std::uint32_t> {
			for (auto candidate = std::uint32_t{ 0U }; candidate < 64U; ++candidate) {
				if (std::ranges::find(probe.Lines, candidate) != probe.Lines.end() || probe.Third == std::optional{ candidate }
					|| (!probe.Lines.empty() && candidate == probe.Lines.front() + 1U)) {
					continue;
				}
				// A saved draft's line is passed over before it is selected, so
				// the selection does not move every tick (v002, `again`).
				if (std::ranges::none_of(saved(), [candidate](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Anchor.Line == candidate && !aDraft.Draft.Body.empty(); })
					&& window->comparison_surface()->select_conversation_line(core::diff_side::right, candidate)
					&& window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0) {
					return candidate;
				}
			}
			return std::nullopt;
		};
		// Opens the selected line's draft in the drawer and writes its words.
		const auto write = [&](const QString& aWords) -> bool {
			const auto& draft = implementation->ReviewDraft;
			if (!draft || draft->ReplyToThread || !window->comment_action()->isEnabled()) {
				return false;
			}
			if (!cards->writing_draft()) {
				window->comment_action()->trigger();
				QCoreApplication::processEvents();
			}
			auto* const field = cards->draft_editor();
			if (field == nullptr) {
				return false;
			}
			if (field->toPlainText() != aWords) {
				field->setPlainText(aWords);
				QCoreApplication::processEvents();
			}
			return true;
		};
		const auto written = [&](const QString& aWords) {
			const auto& draft = implementation->ReviewDraft;
			return draft && draft->Body == aWords.toStdString() && draft->State == core::review_draft_state::drafting && ready();
		};
		// Each pending draft is a note at its line, marked pending, with its
		// gutter marker.
		const auto drawn = [&](const std::vector<core::revisioned_review_draft>& aPending) {
			return !aPending.empty() && std::ranges::all_of(aPending, [&](const core::revisioned_review_draft& aDraft) {
				const auto line = aDraft.Draft.Anchor.Line;
				const auto card = std::ranges::find_if(cards->cards(), [line](const qt::thread_card& aCard) { return aCard.Draft && aCard.Line == std::optional{ line + 1U }; });
				return card != cards->cards().end() && card->Pending && card->Placed
					&& std::ranges::any_of(window->comparison_surface()->line_marks(core::diff_side::right), [line](const qt::diff_line_mark& aMark) { return aMark.Line == line && aMark.Draft; });
			});
		};
		const auto finishPage = [&]() -> QWidget* {
			// [[BPR-1590]]. Finish your review is a sheet, or its expanded page.
			if (window->review_sheet_open()) {
				return window->review_sheet();
			}
			auto* const page = window->findChild<QScrollArea*>(QStringLiteral("composeReviewPage"));
			return page != nullptr && page->isVisibleTo(window) ? page : nullptr;
		};
		const auto openFinish = [&]() -> bool {
			if (finishPage() == nullptr) {
				chip->click();
				QCoreApplication::processEvents();
			}
			return finishPage() != nullptr;
		};
		const auto shown = [&](const std::size_t aCount) {
			return summary->title_label()->text() == QStringLiteral("Finish your review") && summary->pending_rows().size() == static_cast<qsizetype>(aCount)
				&& summary->carries_label()->isVisibleTo(window)
				&& summary->carries_label()->text() == (aCount == 1U ? QStringLiteral("1 pending comment") : QStringLiteral("%1 pending comments").arg(aCount));
		};
		// [[BPR-1345]] v002. A key in the drawer draft's field, as typed.
		const auto press = [&](const Qt::Key aKey, const Qt::KeyboardModifiers aModifiers) -> bool {
			auto* const field = cards->draft_editor();
			if (field == nullptr) {
				return false;
			}
			auto down = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
			auto up   = QKeyEvent{ QEvent::KeyRelease, aKey, aModifiers };
			QCoreApplication::sendEvent(field, &down);
			QCoreApplication::sendEvent(field, &up);
			QCoreApplication::processEvents();
			return true;
		};
		// [[BPR-1345]] v002. With BPR_START_REVIEW_RENDER_DIRECTORY set, the
		// window as the owner will see it, in each theme, for the look.
		const auto renders = qEnvironmentVariable("BPR_START_REVIEW_RENDER_DIRECTORY");
		const auto render  = [&](const QString& aName, QWidget* aWidget = nullptr) {
			if (renders.isEmpty()) {
				return;
			}
			QDir{ }.mkpath(renders);
			static_cast<void>((aWidget != nullptr ? aWidget : static_cast<QWidget*>(window))->grab().save(renders + QLatin1Char{ '/' } + aName + QStringLiteral(".png"), "PNG"));
		};
		const auto renderThemes = [&](const QString& aName) {
			if (renders.isEmpty() || window->appearance_settings() == nullptr) {
				return;
			}
			const auto modes = std::array{ std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") },
										   std::pair{ core::appearance_mode::high_contrast_light, QStringLiteral("high-contrast-light") },
										   std::pair{ core::appearance_mode::high_contrast_dark, QStringLiteral("high-contrast-dark") } };
			for (const auto& [mode, name] : modes) {
				window->appearance_settings()->choose_mode(mode);
				for (auto turn = 0; turn < 6; ++turn) {
					QCoreApplication::processEvents();
				}
				render(aName + QLatin1Char{ '-' } + name);
			}
			window->appearance_settings()->choose_mode(core::appearance_mode::light);
			for (auto turn = 0; turn < 6; ++turn) {
				QCoreApplication::processEvents();
			}
		};
		// [[BPR-1345]] v003. A question in the window's every theme, drawn
		// over the window it asks in, as the owner sees it.
		const auto renderQuestion = [&](const QString& aName, QWidget* const aQuestion) {
			if (renders.isEmpty() || aQuestion == nullptr || window->appearance_settings() == nullptr) {
				return;
			}
			QDir{ }.mkpath(renders);
			const auto modes = std::array{ std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") },
										   std::pair{ core::appearance_mode::high_contrast_light, QStringLiteral("high-contrast-light") },
										   std::pair{ core::appearance_mode::high_contrast_dark, QStringLiteral("high-contrast-dark") } };
			for (const auto& [mode, name] : modes) {
				window->appearance_settings()->choose_mode(mode);
				for (auto turn = 0; turn < 6; ++turn) {
					QCoreApplication::processEvents();
				}
				auto       scene    = window->grab();
				const auto question = aQuestion->grab();
				auto       painter  = QPainter{ &scene };
				painter.fillRect(scene.rect(), QColor{ 0, 0, 0, 90 });
				const auto ratio = scene.devicePixelRatio();
				const auto size  = question.deviceIndependentSize();
				painter.drawPixmap(QPointF{ (scene.width() / ratio - size.width()) / 2.0, (scene.height() / ratio - size.height()) / 2.0 }, question);
				painter.end();
				static_cast<void>(scene.save(renders + QLatin1Char{ '/' } + aName + QLatin1Char{ '-' } + name + QStringLiteral(".png"), "PNG"));
				static_cast<void>(question.save(renders + QLatin1Char{ '/' } + aName + QLatin1Char{ '-' } + name + QStringLiteral("-alone.png"), "PNG"));
			}
			window->appearance_settings()->choose_mode(core::appearance_mode::light);
			for (auto turn = 0; turn < 6; ++turn) {
				QCoreApplication::processEvents();
			}
		};
		// [[BPR-1345]] v002. Presses a question's deleting button.
		const auto confirm = [](QWidget* const aBox, const char* aButton) -> bool {
			auto* const yes = aBox != nullptr ? aBox->findChild<QAbstractButton*>(QString::fromLatin1(aButton)) : nullptr;
			if (yes == nullptr) {
				return false;
			}
			yes->click();
			QCoreApplication::processEvents();
			return true;
		};
		// [[BPR-1345]] v002. The page as the owner asked for it: Comment
		// offered, the details closed, and what is published said once.
		// [[BPR-1853]]. The fixture's viewer, fixture-author, opened this pull
		// request, and GitHub takes only a comment from its author: Approve
		// and Request changes are off and each says why.
		const auto calm = [&] {
			auto* const comment = summary->disposition_button(core::review_disposition::comment);
			auto* const changes = summary->disposition_button(core::review_disposition::request_changes);
			auto* const approve = summary->disposition_button(core::review_disposition::approve);
			return comment->isEnabled() && !approve->isEnabled() && approve->toolTip().startsWith(QStringLiteral("You can't approve your own pull request")) && !changes->isEnabled()
				&& changes->toolTip().startsWith(QStringLiteral("You can't request changes on your own pull request"))
				&& !summary->details_toggle()->isChecked() && !summary->target_editor()->isVisibleTo(window)
				&& summary->note_label()->isVisibleTo(window) && summary->note_label()->text() == QStringLiteral("Nothing is published until you submit.");
		};
		const auto startOne = [&](const state_type::phase aAfter) -> result {
			switch (probe.Phase) {
			case state_type::phase::awaiting_first_line: {
				const auto line = pick();
				note("line=" + std::to_string(line.value_or(0U)) + " ready=" + std::to_string(ready() ? 1 : 0));
				if (!line || !ready() || !write(words[0])) {
					return result::pending;
				}
				probe.Lines        = { *line };
				probe.NativeBefore = implementation->Fixture->review_mutation_evidence().NativePosts;
				probe.Phase        = state_type::phase::awaiting_first_text;
				return result::pending;
			}
			case state_type::phase::awaiting_first_text:
				note("written=" + std::to_string(written(words[0]) ? 1 : 0));
				if (!written(words[0])) {
					return result::pending;
				}
				probe.Phase = state_type::phase::awaiting_first_offer;
				return result::pending;
			case state_type::phase::awaiting_first_offer: {
				auto* const single = composerButton("threadPublishDraft");
				auto* const review = composerButton("threadReviewDraft");
				note("single=" + (single ? single->text().toStdString() : std::string{ "-" }) + " review=" + (review ? review->text().toStdString() : std::string{ "-" }));
				if (single == nullptr || review == nullptr || !review->isEnabled()) {
					return result::pending;
				}
				if (single->text() != QStringLiteral("Add single comment") || review->text() != QStringLiteral("Start a review") || chipShown->isVisible()) {
					return fail("first-offer-not-start-a-review");
				}
				review->click();
				QCoreApplication::processEvents();
				probe.Phase = state_type::phase::awaiting_first_pending;
				return result::pending;
			}
			case state_type::phase::awaiting_first_pending: {
				const auto now = pending();
				note("pending=" + std::to_string(now.size()) + " chip=" + chip->summary().toStdString() + " writing=" + std::to_string(cards->writing_draft() ? 1 : 0));
				if (now.size() != 1U || !chipSays(1U) || cards->writing_draft() || !ready() || !drawn(now)) {
					return result::pending;
				}
				const auto& mark       = now.front().Draft.InReview;
				probe.Evidence.Started = now.front().Draft.Anchor.Line == probe.Lines.front() && now.front().Draft.Body == words[0].toStdString()
					&& mark->Path == path && mark->Submission.empty() && implementation->Fixture->review_mutation_evidence().NativePosts == probe.NativeBefore
					&& posts().empty();
				if (!probe.Evidence.Started) {
					return fail("first-not-pending-exactly");
				}
				probe.Drafts = { now.front().Draft.Id };
				probe.Phase  = aAfter;
				return result::pending;
			}
			default:
				return result::pending;
			}
		};
		// [[BPR-1590]] v002. The owner's look: real-looking pending comments,
		// each written on a free line through the drawer's own Start a review
		// and Add review comment, then a summary in Finish review.
		if (aCourse == course::showcase) {
			const auto showcase = std::array{
				QStringLiteral("issue (blocking): The retry budget resets every frame, so a view that keeps failing never stops retrying. Move the counter onto the view."),
				QStringLiteral("suggestion: Compute the scale once per frame and pass it in.\n```suggestion\n    const auto scale = device_scale(view);\n```"),
				QStringLiteral("question: Does the second view share the depth buffer with the first, or does each view own one?"),
				// [[BPR-1590]] v003. A very long comment, whose row stays one line.
				QStringLiteral("thought: The clear and the draw race on the second view.\n\nA fence between them would make the order explicit, and it costs one wait per frame on the views that need it. "
							   "The first view never sees the race because its clear is recorded on the same queue as its draw, but the second view's clear goes through the transfer queue when "
							   "the depth buffer is shared, and nothing orders that queue against the graphics queue.\n\nI tried three orders locally. Recording the clear on the graphics queue "
							   "fixes it but costs the overlap we wanted from the transfer queue in the first place; a timeline semaphore keeps the overlap and fixes it, at one wait per frame; "
							   "a pipeline barrier alone does not help, because the barrier only orders work within one queue.\n\nSo my vote is the timeline semaphore, with a comment at the "
							   "submit saying why the second view waits, since the next person to touch this will otherwise remove the wait as redundant. Happy to pair on it if that helps."),
				QStringLiteral("nitpick (non-blocking): `std::max` reads better than the ternary here."),
			};
			const auto index = probe.Lines.size();
			switch (probe.Phase) {
			case state_type::phase::awaiting_file: {
				const auto revealed = reveal_fixture_file(aApplication, path);
				if (revealed == detail::fixture_file_reveal_result::failed) {
					return fail("file-not-revealed");
				}
				if (revealed == detail::fixture_file_reveal_result::shown) {
					probe.Phase = state_type::phase::awaiting_first_line;
				}
				return result::pending;
			}
			case state_type::phase::awaiting_first_line: {
				if (index == showcase.size()) {
					probe.Phase = state_type::phase::awaiting_finish_page;
					return result::pending;
				}
				// The line is chosen once; selecting it again every tick would
				// keep its draft's selection pending.
				if (!probe.Third) {
					probe.Third = pick();
					return probe.Third || ++probe.Waited < 600U ? result::pending : fail("showcase-no-line");
				}
				note("line=" + std::to_string(*probe.Third) + " ready=" + std::to_string(ready() ? 1 : 0) + " comment=" + std::to_string(window->comment_action()->isEnabled() ? 1 : 0));
				if (!ready() || !write(showcase[index])) {
					return ++probe.Waited < 600U ? result::pending : fail("showcase-line");
				}
				const auto line = probe.Third;
				probe.Third.reset();
				probe.Lines.push_back(*line);
				probe.Phase = state_type::phase::awaiting_first_text;
				return result::pending;
			}
			case state_type::phase::awaiting_first_text:
				if (!written(showcase[index - 1U])) {
					return ++probe.Waited < 2'000U ? result::pending : fail("showcase-text");
				}
				probe.Phase = state_type::phase::awaiting_first_offer;
				return result::pending;
			case state_type::phase::awaiting_first_offer: {
				auto* const review = composerButton("threadReviewDraft");
				if (review == nullptr || !review->isEnabled()) {
					return ++probe.Waited < 2'000U ? result::pending : fail("showcase-offer");
				}
				review->click();
				QCoreApplication::processEvents();
				probe.Phase = state_type::phase::awaiting_first_pending;
				return result::pending;
			}
			case state_type::phase::awaiting_first_pending:
				if (pending().size() != index || cards->writing_draft() || !ready()) {
					return ++probe.Waited < 2'000U ? result::pending : fail("showcase-pending");
				}
				probe.Waited = 0U;
				probe.Phase  = state_type::phase::awaiting_first_line;
				return result::pending;
			case state_type::phase::awaiting_finish_page:
				if (!window->review_sheet_open()) {
					window->finish_review_action()->trigger();
					QCoreApplication::processEvents();
					return result::pending;
				}
				if (summary->pending_rows().size() != static_cast<qsizetype>(showcase.size()) || summary->body_editor()->isReadOnly()) {
					return ++probe.Waited < 2'000U ? result::pending : fail("showcase-finish");
				}
				summary->body_editor()->setPlainText(QStringLiteral("Nice cleanup overall. The retry budget on the second view needs fixing before this lands; everything else is small and can follow."));
				QCoreApplication::processEvents();
				probe.Phase = state_type::phase::passed;
				return result::passed;
			default:
				return fail("showcase-phase");
			}
		}
		switch (probe.Phase) {
		case state_type::phase::awaiting_file: {
			// [[BPR-1833]]. These courses keep working on the same pull request
			// after Comment. BPR-1816 defaults a confirmed Submit to Home, so
			// choose the existing stay option through Settings and wait for the
			// accepted projection. The other verdicts retain their defaults.
			if (aCourse == course::finish || aCourse == course::again) {
				auto* const comment = window->finish_review_switch(core::review_disposition::comment);
				if (comment == nullptr) {
					return fail("finish-comment-setting-missing");
				}
				const auto& choices = window->behaviour().FinishReview;
				if (choices.Approve != core::review_finish_return::home || choices.RequestChanges != core::review_finish_return::home) {
					return fail("other-finish-defaults-changed");
				}
				if (!probe.FinishReturnChosen) {
					if (comment->isChecked()) {
						comment->click();
					}
					probe.FinishReturnChosen = true;
					return result::pending;
				}
				if (choices.Comment != core::review_finish_return::pull_request || comment->isChecked()) {
					return result::pending;
				}
			}
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed");
			}
			if (revealed == detail::fixture_file_reveal_result::shown) {
				probe.Phase = aCourse == course::finish ? state_type::phase::awaiting_restored
					: aCourse == course::restored       ? state_type::phase::awaiting_restored_unknown
					: aCourse == course::again          ? state_type::phase::awaiting_old_line
														: state_type::phase::awaiting_first_line;
			}
			return result::pending;
		}
		case state_type::phase::awaiting_first_line:
		case state_type::phase::awaiting_first_text:
		case state_type::phase::awaiting_first_offer:
		case state_type::phase::awaiting_first_pending:
			return startOne(aCourse == course::pending || aCourse == course::again ? state_type::phase::awaiting_second_line : state_type::phase::awaiting_finish_page);
		case state_type::phase::awaiting_second_line: {
			// The range ends on the line below its first, which is free.
			auto line = pick();
			if (line && *line == 0U) {
				line = std::nullopt;
			}
			note("line=" + std::to_string(line.value_or(0U)));
			if (!line || !ready() || !write(words[1])) {
				return result::pending;
			}
			probe.Lines.push_back(*line);
			probe.Phase = state_type::phase::awaiting_second_text;
			return result::pending;
		}
		case state_type::phase::awaiting_second_text: {
			const auto& draft = implementation->ReviewDraft;
			note("written=" + std::to_string(written(words[1]) ? 1 : 0) + " start=" + (draft && draft->StartLine ? std::to_string(*draft->StartLine) : std::string{ "-" }));
			if (!written(words[1])) {
				return result::pending;
			}
			if (!draft->StartLine) {
				static_cast<void>(editor->change_range(probe.Lines.back() - 1U));
				return result::pending;
			}
			probe.Phase = state_type::phase::awaiting_second_offer;
			return result::pending;
		}
		case state_type::phase::awaiting_second_offer: {
			auto* const single = composerButton("threadPublishDraft");
			auto* const review = composerButton("threadReviewDraft");
			auto* const field  = cards->draft_editor();
			note("single=" + (single ? single->text().toStdString() : std::string{ "-" }) + " review=" + (review ? review->text().toStdString() : std::string{ "-" }));
			if (single == nullptr || review == nullptr || field == nullptr || !review->isEnabled()) {
				return result::pending;
			}
			if (single->text() != QStringLiteral("Add single comment") || review->text() != QStringLiteral("Add review comment") || !chipSays(1U)) {
				return fail("second-offer-not-add-review-comment");
			}
			if (aCourse == course::again) {
				renderThemes(QStringLiteral("drawer-add-review-comment"));
			}
			// ⌘↩ is the main action while a review is in progress.
			static_cast<void>(press(Qt::Key_Return, Qt::ControlModifier));
			probe.Phase = state_type::phase::awaiting_second_pending;
			return result::pending;
		}
		case state_type::phase::awaiting_second_pending: {
			const auto now = pending();
			note("pending=" + std::to_string(now.size()) + " chip=" + chip->summary().toStdString());
			if (now.size() != 2U || !chipSays(2U) || cards->writing_draft() || !ready()) {
				return result::pending;
			}
			const auto& range    = now.back().Draft;
			probe.Evidence.Added = range.Anchor.Line == probe.Lines.back() && range.StartLine == std::optional{ probe.Lines.back() - 1U }
				&& range.Body == words[1].toStdString() && implementation->Fixture->review_mutation_evidence().NativePosts == probe.NativeBefore && posts().empty();
			if (!probe.Evidence.Added) {
				return fail("second-not-pending-exactly");
			}
			if (!drawn(now)) {
				return result::pending;
			}
			probe.Evidence.Drawn = true;
			probe.Phase          = aCourse == course::again ? state_type::phase::awaiting_finish_again : state_type::phase::awaiting_third_line;
			return result::pending;
		}
		case state_type::phase::awaiting_third_line: {
			const auto line = pick();
			note("line=" + std::to_string(line.value_or(0U)));
			if (!line || !ready() || !write(words[2])) {
				return result::pending;
			}
			probe.Third = line;
			probe.Phase = state_type::phase::awaiting_third_text;
			return result::pending;
		}
		case state_type::phase::awaiting_third_text: {
			auto* const single = composerButton("threadPublishDraft");
			note("written=" + std::to_string(written(words[2]) ? 1 : 0));
			if (!written(words[2]) || single == nullptr || !single->isEnabled()) {
				return result::pending;
			}
			// v002: ⌘⇧↩ is Add single comment.
			static_cast<void>(press(Qt::Key_Return, Qt::ControlModifier | Qt::ShiftModifier));
			probe.Phase = state_type::phase::awaiting_third_published;
			return result::pending;
		}
		case state_type::phase::awaiting_third_published: {
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			note("native=" + std::to_string(mutations.NativePosts) + " pending=" + std::to_string(pending().size()));
			if (implementation->PendingReviewMutation.has_value() || mutations.NativePosts == probe.NativeBefore || !ready()) {
				return result::pending;
			}
			probe.Evidence.SingleAlone = mutations.NativePosts == probe.NativeBefore + 1U && mutations.LastPath == path && probe.Third
				&& mutations.LastLine == *probe.Third + 1U && mutations.LastBody == words[2].toStdString() && pending().size() == 2U && chipSays(2U) && posts().empty();
			if (!probe.Evidence.SingleAlone) {
				return fail("single-comment-not-alone");
			}
			probe.Phase = state_type::phase::awaiting_other_file;
			return result::pending;
		}
		case state_type::phase::awaiting_other_file: {
			const auto revealed = reveal_fixture_file(aApplication, other);
			note("reveal=" + std::to_string(static_cast<int>(revealed)) + " chip=" + chip->summary().toStdString());
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("other-file-not-revealed");
			}
			if (revealed != detail::fixture_file_reveal_result::shown || !ready()) {
				return result::pending;
			}
			if (!chipSays(2U)) {
				return fail("other-file-lost-the-review");
			}
			probe.Phase = state_type::phase::awaiting_return;
			return result::pending;
		}
		case state_type::phase::awaiting_return: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			const auto now      = pending();
			note("reveal=" + std::to_string(static_cast<int>(revealed)) + " pending=" + std::to_string(now.size()));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed-again");
			}
			if (revealed != detail::fixture_file_reveal_result::shown || !ready() || !drawn(now)) {
				return result::pending;
			}
			probe.Evidence.NavigationKept = now.size() == 2U && chipSays(2U) && posts().empty()
				&& implementation->Fixture->review_mutation_evidence().NativePosts == probe.NativeBefore + 1U;
			if (!probe.Evidence.NavigationKept) {
				return fail("navigation-changed-the-review");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::awaiting_restored: {
			// After the restart: the two the pending course left.
			const auto now = pending();
			note("pending=" + std::to_string(now.size()) + " chip=" + chip->summary().toStdString() + " ready=" + std::to_string(ready() ? 1 : 0));
			if (now.size() != 2U || !chipSays(2U) || !ready() || !drawn(now)) {
				return ++probe.Waited < 400U ? result::pending : fail("not-restored");
			}
			probe.Evidence.Restored = now.front().Draft.Body == words[0].toStdString() && now.back().Draft.Body == words[1].toStdString()
				&& now.back().Draft.StartLine == std::optional{ now.back().Draft.Anchor.Line - 1U } && posts().empty();
			if (!probe.Evidence.Restored) {
				return fail("restored-not-exact");
			}
			for (const auto& draft : now) {
				probe.Lines.push_back(draft.Draft.Anchor.Line);
				probe.Drafts.push_back(draft.Draft.Id);
			}
			probe.Phase = state_type::phase::awaiting_finish_page;
			return result::pending;
		}
		case state_type::phase::awaiting_finish_page: {
			const auto expected = aCourse == course::finish ? 2U : 1U;
			const auto opened   = openFinish();
			note("opened=" + std::to_string(opened ? 1 : 0) + " title=" + summary->title_label()->text().toStdString() + " rows=" + std::to_string(summary->pending_rows().size())
				 + " comment=" + std::to_string(summary->disposition_button(core::review_disposition::comment)->isEnabled() ? 1 : 0) + " status=" + summary->status_label()->text().toStdString());
			if (!opened || !shown(expected) || !summary->disposition_button(core::review_disposition::comment)->isEnabled()) {
				return result::pending;
			}
			// [[BPR-1590]]. A sheet over the surface it was opened from.
			probe.Evidence.FinishOpened = window->review_sheet_open();
			// Comment, with no summary, is one of the three buttons that
			// submit (v002).
			probe.Evidence.CommentWithoutSummary = summary->shown_draft().has_value() && summary->body_editor()->toPlainText().isEmpty()
				&& summary->body_editor()->placeholderText() == QStringLiteral("Leave a summary (optional)") && calm();
			if (!probe.Evidence.FinishOpened || !probe.Evidence.CommentWithoutSummary) {
				return fail("finish-page-not-as-drawn");
			}
			{
				const auto lock         = std::scoped_lock{ server.Mutex };
				server.RefuseNextReview = aCourse == course::refused;
				server.LoseNextReview   = aCourse == course::uncertain || aCourse == course::kept;
			}
			// [[BPR-1475]]. What the submission's own reread is measured from.
			probe.ReadsBefore     = implementation->Fixture->evidence().AccountReadCount;
			probe.ShownBefore     = implementation->WindowRead->accepted_snapshot() ? std::optional{ implementation->WindowRead->accepted_snapshot()->Snapshot } : std::nullopt;
			probe.SelectionBefore = window->comparison_surface()->selection();
			summary->disposition_button(core::review_disposition::comment)->click();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = aCourse == course::refused ? state_type::phase::awaiting_refused : aCourse == course::uncertain || aCourse == course::kept ? state_type::phase::awaiting_held
																																					  : state_type::phase::awaiting_submitted;
			return result::pending;
		}
		case state_type::phase::awaiting_submitted: {
			const auto sent      = posts();
			const auto remaining = pending();
			const auto published = countPublished();
			note("posts=" + std::to_string(sent.size()) + " published=" + std::to_string(published) + " pending=" + std::to_string(remaining.size()) + " held=" + std::to_string(countHeld()) + " status=" + summary->status_label()->text().toStdString());
			if (sent.size() != 1U || published != 2U || chipShown->isVisible() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("not-submitted");
			}
			// The one request as the server received it.
			const auto object             = QJsonDocument::fromJson(QByteArray::fromStdString(sent.front())).object();
			const auto comments           = object.value(QStringLiteral("comments")).toArray();
			const auto line               = [](const std::uint32_t aLine) { return static_cast<int>(aLine) + 1; };
			const auto first              = comments.size() == 2 ? comments.at(0).toObject() : QJsonObject{ };
			const auto second             = comments.size() == 2 ? comments.at(1).toObject() : QJsonObject{ };
			probe.Evidence.SubmittedExact = object.value(QStringLiteral("event")).toString() == QStringLiteral("COMMENT") && object.value(QStringLiteral("body")).toString().isEmpty()
				&& comments.size() == 2 && probe.Lines.size() == 2U
				&& first.value(QStringLiteral("path")).toString() == QString::fromLatin1(path.data(), static_cast<qsizetype>(path.size()))
				&& first.value(QStringLiteral("line")).toInt() == line(probe.Lines[0]) && first.value(QStringLiteral("side")).toString() == QStringLiteral("RIGHT")
				&& !first.contains(QStringLiteral("start_line")) && first.value(QStringLiteral("body")).toString() == words[0]
				&& second.value(QStringLiteral("path")).toString() == QString::fromLatin1(path.data(), static_cast<qsizetype>(path.size()))
				&& second.value(QStringLiteral("line")).toInt() == line(probe.Lines[1]) && second.value(QStringLiteral("side")).toString() == QStringLiteral("RIGHT")
				&& second.value(QStringLiteral("start_line")).toInt() == line(probe.Lines[1] - 1U) && second.value(QStringLiteral("start_side")).toString() == QStringLiteral("RIGHT")
				&& second.value(QStringLiteral("body")).toString() == words[1];
			// [[BPR-1345]] v003. Confirmed, the page closes and the window
			// returns to Review, where Finish review was pressed, and says so.
			probe.Evidence.MarkedPublished = remaining.empty() && finishPage() == nullptr && window->current_modality() == qt::review_modality::review
				&& window->statusBar()->currentMessage() == QStringLiteral("Review submitted with 2 comments") && summary->confirmed_submissions() == 1U
				&& summary->shown_draft().has_value() && summary->shown_draft()->State == core::review_submission_state::drafting && summary->shown_draft()->Comments.empty();
			if (!probe.Evidence.SubmittedExact || !probe.Evidence.MarkedPublished) {
				note("modality=" + std::to_string(static_cast<int>(window->current_modality())) + " message=" + window->statusBar()->currentMessage().toStdString() + " confirmed=" + std::to_string(summary->confirmed_submissions()) + " finish=" + std::to_string(finishPage() != nullptr ? 1 : 0));
				return fail("submitted-not-exact");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_publication_reread;
			return result::pending;
		}
		case state_type::phase::awaiting_publication_reread: {
			// [[BPR-1475]]. Nothing here presses Refresh: the submission's own
			// reread brings a new snapshot, and the reviewer's place stays.
			const auto& snapshot  = implementation->WindowRead->accepted_snapshot();
			const auto  reads     = implementation->Fixture->evidence().AccountReadCount;
			const auto& context   = window->comparison_surface()->current_context();
			const auto  shownFile = context.has_value() && context->Key.File == core::file_identity_of(path);
			note("reads=" + std::to_string(reads) + " before=" + std::to_string(probe.ReadsBefore) + " ready=" + std::to_string(ready() ? 1 : 0) + " file=" + std::to_string(shownFile ? 1 : 0)
				 + " selection=" + std::to_string(window->comparison_surface()->selection() == probe.SelectionBefore ? 1 : 0));
			if (reads == probe.ReadsBefore || !snapshot || std::optional{ snapshot->Snapshot } == probe.ShownBefore || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("no-reread-after-publish");
			}
			probe.Evidence.RereadAfterPublish = !window->showing_home() && window->current_modality() == qt::review_modality::review && shownFile
				&& probe.SelectionBefore.has_value() && window->comparison_surface()->selection() == probe.SelectionBefore && finishPage() == nullptr;
			if (!probe.Evidence.RereadAfterPublish) {
				return fail("reread-moved-the-reviewer");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_note_line;
			return result::pending;
		}
		case state_type::phase::awaiting_note_line: {
			// A new note, written and left open in the drawer.
			const auto line = pick();
			note("line=" + std::to_string(line.value_or(0U)));
			if (!line || !ready() || !write(words[2])) {
				return ++probe.Waited < 400U ? result::pending : fail("no-note-to-hold-open");
			}
			probe.Third  = line;
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_note_text;
			return result::pending;
		}
		case state_type::phase::awaiting_note_text: {
			note("written=" + std::to_string(written(words[2]) ? 1 : 0) + " writing=" + std::to_string(cards->writing_draft() ? 1 : 0));
			if (!written(words[2]) || !cards->writing_draft()) {
				return ++probe.Waited < 400U ? result::pending : fail("note-not-written");
			}
			// The reread a publication owes, by the same path, with the note
			// open: this window has nothing else to publish here.
			probe.ReadsBefore     = implementation->Fixture->evidence().AccountReadCount;
			probe.ShownBefore     = implementation->WindowRead->accepted_snapshot()->Snapshot;
			probe.SelectionBefore = window->comparison_surface()->selection();
			implementation->reread_after_publication();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_note_reread;
			return result::pending;
		}
		case state_type::phase::awaiting_note_reread: {
			const auto& snapshot = implementation->WindowRead->accepted_snapshot();
			const auto  reads    = implementation->Fixture->evidence().AccountReadCount;
			auto* const field    = cards->draft_editor();
			note("reads=" + std::to_string(reads) + " before=" + std::to_string(probe.ReadsBefore) + " ready=" + std::to_string(ready() ? 1 : 0)
				 + " writing=" + std::to_string(cards->writing_draft() ? 1 : 0) + " field=" + std::to_string(field != nullptr ? 1 : 0) + " written=" + std::to_string(written(words[2]) ? 1 : 0)
				 + " open=" + std::to_string(cards->open_card() != nullptr ? 1 : 0) + " selection=" + std::to_string(window->comparison_surface()->selection() == probe.SelectionBefore ? 1 : 0)
				 + " modality=" + std::to_string(static_cast<int>(window->current_modality())) + " draft=" + (implementation->ReviewDraft ? std::to_string(implementation->ReviewDraft->Id.value()) + ":" + implementation->ReviewDraft->Body.substr(0, 8) : std::string{ "none" })
				 + " resume=" + std::to_string(implementation->ResumeComment ? 1 : 0) + " waited=" + std::to_string(probe.Waited));
			if (reads == probe.ReadsBefore || !snapshot || std::optional{ snapshot->Snapshot } == probe.ShownBefore || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("no-reread-with-the-note-open");
			}
			probe.Evidence.NoteKept = cards->writing_draft() && field != nullptr && field->toPlainText() == words[2] && written(words[2])
				&& window->current_modality() == qt::review_modality::review && window->comparison_surface()->selection() == probe.SelectionBefore;
			if (!probe.Evidence.NoteKept) {
				return ++probe.Waited < 400U ? result::pending : fail("reread-closed-the-note");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_recent_open;
			return result::pending;
		}
		case state_type::phase::awaiting_recent_open: {
			// [[BPR-1472]]. The owner's course: Home, then this pull request's
			// Recently viewed row. The shown read predates the review just
			// submitted, so the row reads the pull request again.
			const auto& snapshot = implementation->WindowRead->accepted_snapshot();
			auto* const recent   = window->home_recent_list();
			if (!snapshot || recent == nullptr) {
				return fail("nothing-shown-to-reopen");
			}
			const auto name = QStringLiteral("%1/%2#%3").arg(QString::fromStdString(snapshot->Repository.Owner), QString::fromStdString(snapshot->Repository.Name)).arg(snapshot->PullRequest.Number.value());
			window->choose_home(true);
			QCoreApplication::processEvents();
			auto row = -1;
			for (auto index = 0; index < recent->count(); ++index) {
				if (recent->item(index)->text().startsWith(name)) {
					row = index;
				}
			}
			note("recent=" + std::to_string(recent->count()) + " row=" + std::to_string(row) + " visible=" + std::to_string(recent->isVisible() ? 1 : 0));
			if (row < 0 || !recent->isVisible()) {
				return ++probe.Waited < 400U ? result::pending : fail("no-recent-row");
			}
			probe.ReadsBefore = implementation->Fixture->evidence().AccountReadCount;
			probe.ShownBefore = snapshot->Snapshot;
			recent->setCurrentRow(row);
			recent->setFocus(Qt::OtherFocusReason);
			auto open = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
			QCoreApplication::sendEvent(recent, &open);
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_recent_read;
			return result::pending;
		}
		case state_type::phase::awaiting_recent_read: {
			const auto& snapshot = implementation->WindowRead->accepted_snapshot();
			const auto  reads    = implementation->Fixture->evidence().AccountReadCount;
			note("reads=" + std::to_string(reads) + " before=" + std::to_string(probe.ReadsBefore) + " ready=" + std::to_string(ready() ? 1 : 0)
				 + " home=" + std::to_string(window->showing_home() ? 1 : 0));
			if (reads == probe.ReadsBefore || !snapshot || snapshot->Snapshot == probe.ShownBefore || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("reopen-reused-the-stale-read");
			}
			probe.Evidence.ReopenedFresh = !window->showing_home() && window->current_modality() == qt::review_modality::overview;
			if (!probe.Evidence.ReopenedFresh) {
				return fail("reopen-not-on-overview");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::awaiting_refused: {
			const auto sent = posts();
			const auto now  = pending();
			note("posts=" + std::to_string(sent.size()) + " pending=" + std::to_string(now.size()) + " held=" + std::to_string(countHeld()) + " status=" + summary->status_label()->text().toStdString());
			if (sent.size() != 1U || now.size() != 1U || countHeld() != 0U || !chipSays(1U) || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("refusal-not-settled");
			}
			// [[BPR-1345]] v003. A refusal keeps the page open, with its words.
			// [[BPR-1715]]. GitHub's refusal of the review is said as one.
			probe.Evidence.RefusedKept = now.front().Draft.Body == words[0].toStdString() && now.front().Draft.Anchor.Line == probe.Lines.front()
				&& now.front().Draft.InReview->Submission.empty() && finishPage() != nullptr && summary->confirmed_submissions() == 0U
				&& summary->status_label()->text().startsWith(QStringLiteral("Not published: GitHub refused this review."));
			if (!probe.Evidence.RefusedKept) {
				return fail("refusal-lost-the-draft");
			}
			if (!summary->discard_action()->isEnabled() || summary->discard_action()->text() != QStringLiteral("Discard review")) {
				return result::pending;
			}
			// v002: Discard review asks first, naming what it deletes.
			summary->discard_action()->trigger();
			QCoreApplication::processEvents();
			if (summary->discard_confirmation() == nullptr || !summary->discard_confirmation()->question().contains(QStringLiteral("1 pending comment"))) {
				return fail("discard-asked-nothing");
			}
			if (!confirm(summary->discard_confirmation(), "discardReviewConfirmationConfirm")) {
				return fail("discard-not-confirmed");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_discarded;
			return result::pending;
		}
		case state_type::phase::awaiting_discarded: {
			const auto all  = saved();
			const auto kept = std::ranges::find_if(all, [&](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Anchor.Line == probe.Lines.front(); });
			note("pending=" + std::to_string(pending().size()) + " chip=" + std::to_string(chipShown->isVisible() ? 1 : 0) + " page=" + std::to_string(finishPage() != nullptr ? 1 : 0));
			if (!pending().empty() || chipShown->isVisible() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("discard-not-settled");
			}
			// v002: deleted, not kept, and the page closed.
			probe.Evidence.DiscardDeleted = kept == all.end() && finishPage() == nullptr && posts().size() == 1U;
			if (!probe.Evidence.DiscardDeleted) {
				return fail("discard-kept-the-draft");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::awaiting_held: {
			const auto all  = saved();
			const auto held = std::ranges::find_if(all, [](const core::revisioned_review_draft& aDraft) { return core::held_by_review(aDraft.Draft); });
			note("posts=" + std::to_string(posts().size()) + " held=" + std::to_string(countHeld()) + " status=" + summary->status_label()->text().toStdString()
				 + " reconcile=" + std::to_string(summary->reconcile_action()->isEnabled() ? 1 : 0));
			if (held == all.end() || !summary->reconcile_action()->isEnabled() || !chipShown->isVisible() || !chip->submitting()) {
				return ++probe.Waited < 400U ? result::pending : fail("not-held");
			}
			probe.Evidence.UncertainHeld = posts().size() == 1U && held->Draft.Body == words[0].toStdString() && pending().empty()
				&& !editor->publication_retry_offered() && finishPage() != nullptr && summary->confirmed_submissions() == 0U
				&& summary->status_label()->text().startsWith(QStringLiteral("Review status is unknown."));
			if (!probe.Evidence.UncertainHeld) {
				return fail("uncertain-not-held-exactly");
			}
			// [[BPR-1541]]. The list could not be reached: the page says so
			// under the status, as part of its description, naming nothing.
			{
				const auto line            = summary->unknown_reason_label()->text();
				probe.Evidence.ReasonShown = !summary->unknown_reason_label()->isHidden() && line == unknownLine(core::review_submission_unknown_reason::network)
					&& summary->status_label()->accessibleDescription() == line && !line.contains(QStringLiteral("conventional")) && !line.contains(QStringLiteral("renderer"));
				render(QStringLiteral("unknown-reason"), summary);
				// [[BPR-1541]] v002. The held review in the window, in each theme.
				renderThemes(QStringLiteral("unknown-status"));
				if (!probe.Evidence.ReasonShown) {
					return fail("unknown-reason-not-shown");
				}
			}
			if (aCourse == course::kept) {
				probe.Phase = state_type::phase::passed;
				return result::passed;
			}
			{
				// Check status again: the list answers with a server error.
				const auto lock         = std::scoped_lock{ server.Mutex };
				server.ReviewReadStatus = 503;
			}
			summary->reconcile_action()->trigger();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_replaced;
			return result::pending;
		}
		case state_type::phase::awaiting_replaced: {
			const auto line = summary->unknown_reason_label()->text();
			note("reason=" + line.toStdString() + " reconcile=" + std::to_string(summary->reconcile_action()->isEnabled() ? 1 : 0));
			if (line != unknownServerError || !summary->reconcile_action()->isEnabled()) {
				return ++probe.Waited < 400U ? result::pending : fail("unknown-reason-not-replaced");
			}
			probe.Evidence.ReasonReplaced = posts().size() == 1U && summary->status_label()->accessibleDescription() == line;
			if (!probe.Evidence.ReasonReplaced) {
				return fail("unknown-reason-replaced-wrongly");
			}
			{
				const auto lock         = std::scoped_lock{ server.Mutex };
				server.HideReviewReads  = false;
				server.ReviewReadStatus = 0;
			}
			summary->reconcile_action()->trigger();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_reconciled;
			return result::pending;
		}
		case state_type::phase::awaiting_restored_unknown: {
			// [[BPR-1541]]. Restarted over the kept root, with the review
			// list's reads held: before Check status answers, the restored
			// review already says what its record kept.
			const auto line = summary->unknown_reason_label()->text();
			note("reason=" + line.toStdString() + " status=" + summary->status_label()->text().toStdString());
			if (line.isEmpty() || summary->unknown_reason_label()->isHidden()) {
				return ++probe.Waited < 400U ? result::pending : fail("restored-reason-not-shown");
			}
			probe.Evidence.ReasonRestored = line == unknownLine(core::review_submission_unknown_reason::network) && summary->shown_draft()
				&& summary->shown_draft()->State == core::review_submission_state::submitting && summary->status_label()->text().startsWith(QStringLiteral("Checking the original review"));
			if (!probe.Evidence.ReasonRestored) {
				return fail("restored-reason-not-the-kept-one");
			}
			{
				const auto lock         = std::scoped_lock{ server.Mutex };
				server.HideReviewReads  = true;
				server.ReviewReadStatus = 503;
				server.StallReviewReads = false;
			}
			server.ReadGate.notify_all();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_restored_replaced;
			return result::pending;
		}
		case state_type::phase::awaiting_restored_replaced: {
			const auto line = summary->unknown_reason_label()->text();
			note("reason=" + line.toStdString() + " status=" + summary->status_label()->text().toStdString());
			if (line != unknownServerError) {
				return ++probe.Waited < 400U ? result::pending : fail("restored-reason-not-replaced");
			}
			probe.Evidence.ReasonReplaced = summary->status_label()->text().startsWith(QStringLiteral("Review status is unknown."));
			if (!probe.Evidence.ReasonReplaced) {
				return fail("restored-reason-replaced-wrongly");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::awaiting_reconciled: {
			const auto published = countPublished();
			note("posts=" + std::to_string(posts().size()) + " published=" + std::to_string(published) + " held=" + std::to_string(countHeld()) + " status=" + summary->status_label()->text().toStdString());
			if (published != 1U || countHeld() != 0U || chipShown->isVisible() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("not-reconciled");
			}
			// Check status found it published: the page closes as on any
			// confirmation.
			probe.Evidence.Reconciled = posts().size() == 1U && finishPage() == nullptr && summary->confirmed_submissions() == 1U
				&& window->statusBar()->currentMessage() == QStringLiteral("Review submitted with 1 comment");
			if (!probe.Evidence.Reconciled) {
				return fail("reconciled-after-a-second-post");
			}
			// [[BPR-1541]]. Confirmed: no reason is shown.
			probe.Evidence.ReasonCleared = summary->unknown_reason_label()->isHidden() && summary->unknown_reason_label()->text().isEmpty();
			if (!probe.Evidence.ReasonCleared) {
				return fail("reason-kept-after-confirmation");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::awaiting_old_line: {
			// The older draft, written first and kept out of the review.
			const auto line = pick();
			note("line=" + std::to_string(line.value_or(0U)));
			if (!line || !ready() || !write(older)) {
				return result::pending;
			}
			probe.Old   = line;
			probe.Phase = state_type::phase::awaiting_old_text;
			return result::pending;
		}
		case state_type::phase::awaiting_old_text:
			note("written=" + std::to_string(written(older) ? 1 : 0));
			if (!written(older)) {
				return result::pending;
			}
			// Esc keeps a written draft at its line.
			static_cast<void>(press(Qt::Key_Escape, Qt::NoModifier));
			probe.Phase = state_type::phase::awaiting_old_kept;
			return result::pending;
		case state_type::phase::awaiting_old_kept: {
			const auto all  = saved();
			const auto kept = std::ranges::find_if(all, [&](const core::revisioned_review_draft& aDraft) { return probe.Old && aDraft.Draft.Anchor.Line == *probe.Old; });
			note("kept=" + std::to_string(kept != all.end() ? 1 : 0) + " writing=" + std::to_string(cards->writing_draft() ? 1 : 0));
			if (kept == all.end() || cards->writing_draft() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("older-draft-not-kept");
			}
			if (kept->Draft.Body != older.toStdString() || core::pending_in_review(kept->Draft)) {
				return fail("older-draft-not-as-written");
			}
			probe.OldDraft = kept->Draft.Id;
			probe.Waited   = 0U;
			probe.Phase    = state_type::phase::awaiting_first_line;
			return result::pending;
		}
		case state_type::phase::awaiting_finish_again: {
			// Finish review by its window action, ⌘⌥↩'s.
			auto* const finish = window->finish_review_action();
			note("finish=" + std::to_string(finish != nullptr && finish->isEnabled() ? 1 : 0) + " rows=" + std::to_string(summary->pending_rows().size()));
			if (finish == nullptr || !finish->isEnabled()) {
				return result::pending;
			}
			if (finishPage() == nullptr) {
				finish->trigger();
				QCoreApplication::processEvents();
			}
			if (finishPage() == nullptr || !shown(2U)) {
				return result::pending;
			}
			probe.Evidence.FinishOpened = window->review_sheet_open() && calm();
			if (!probe.Evidence.FinishOpened) {
				return fail("finish-again-not-as-drawn");
			}
			renderThemes(QStringLiteral("finish-your-review"));
			if (!probe.RowRevealed) {
				// A row goes to its comment: the second, the range.
				auto rows = std::vector<QAbstractButton*>{ };
				for (auto* const row : summary->pending_list()->findChildren<QAbstractButton*>(QStringLiteral("wholeReviewPendingOpen"))) {
					if (row->isVisibleTo(window)) {
						rows.push_back(row);
					}
				}
				if (rows.size() != 2U) {
					return fail("rows-not-two");
				}
				// [[BPR-1590]] v003. Threads' gesture: Return opens it in Review.
				rows.back()->setFocus(Qt::OtherFocusReason);
				auto open = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
				QCoreApplication::sendEvent(rows.back(), &open);
				QCoreApplication::processEvents();
				probe.Waited = 0U;
				probe.Phase  = state_type::phase::awaiting_row_revealed;
				return result::pending;
			}
			// Delete the second row, the range.
			auto visible = std::vector<QToolButton*>{ };
			for (auto* const button : summary->pending_list()->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingDelete"))) {
				if (button->isVisibleTo(window)) {
					visible.push_back(button);
				}
			}
			if (visible.size() != 2U) {
				return fail("row-deletes-not-two");
			}
			visible.back()->click();
			QCoreApplication::processEvents();
			renderQuestion(QStringLiteral("delete-comment-question"), summary->delete_confirmation());
			if (summary->delete_confirmation() == nullptr || !confirm(summary->delete_confirmation(), "deleteCommentConfirmationConfirm")) {
				return fail("row-delete-asked-nothing");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_row_deleted;
			return result::pending;
		}
		case state_type::phase::awaiting_row_revealed: {
			const auto selected = window->comparison_surface()->selection();
			note("page=" + std::to_string(finishPage() != nullptr ? 1 : 0) + " modality=" + std::to_string(static_cast<int>(window->current_modality()))
				 + " line=" + (selected ? std::to_string(selected->PreferredLine.Line) : std::string{ "-" }) + " open=" + cards->open_thread_id()
				 + " writing=" + std::to_string(cards->writing_draft() ? 1 : 0));
			// [[BPR-1590]] v003. Finish your review closes, the row's line
			// shows, and its draft opens in the drawer.
			if (window->review_sheet_open() || window->current_modality() != qt::review_modality::review || !selected
				|| selected->PreferredLine.Side != core::diff_side::right || probe.Lines.size() != 2U || selected->PreferredLine.Line != probe.Lines.back() || !ready()
				|| !editor->shown_draft().has_value() || editor->shown_draft()->Anchor.Line != probe.Lines.back() || cards->open_thread_id() != "draft:unpublished"
				|| !cards->writing_draft()) {
				return ++probe.Waited < 400U ? result::pending : fail("row-not-revealed");
			}
			probe.Evidence.RowRevealed = posts().empty() && pending().size() == 2U;
			if (!probe.Evidence.RowRevealed) {
				return fail("row-reveal-changed-the-review");
			}
			probe.RowRevealed = true;
			window->finish_review_action()->trigger();
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_finish_again;
			return result::pending;
		}
		case state_type::phase::awaiting_row_deleted: {
			const auto now = pending();
			note("pending=" + std::to_string(now.size()) + " rows=" + std::to_string(summary->pending_rows().size()));
			if (now.size() != 1U || !chipSays(1U) || summary->pending_rows().size() != 1 || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("row-not-deleted");
			}
			const auto all            = saved();
			probe.Evidence.RowDeleted = probe.Lines.size() == 2U && now.front().Draft.Anchor.Line == probe.Lines.front()
				&& std::ranges::none_of(all, [&](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Anchor.Line == probe.Lines.back(); })
				&& finishPage() != nullptr && posts().empty();
			if (!probe.Evidence.RowDeleted) {
				return fail("row-delete-not-exact");
			}
			summary->discard_action()->trigger();
			QCoreApplication::processEvents();
			renderQuestion(QStringLiteral("discard-review-question"), summary->discard_confirmation());
			if (summary->discard_confirmation() == nullptr || !summary->discard_confirmation()->question().contains(QStringLiteral("1 pending comment"))
				|| !confirm(summary->discard_confirmation(), "discardReviewConfirmationConfirm")) {
				return fail("discard-review-asked-nothing");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_review_discarded;
			return result::pending;
		}
		case state_type::phase::awaiting_review_discarded: {
			const auto all = saved();
			note("pending=" + std::to_string(pending().size()) + " page=" + std::to_string(finishPage() != nullptr ? 1 : 0) + " chip=" + std::to_string(chipShown->isVisible() ? 1 : 0));
			if (!pending().empty() || chipShown->isVisible() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("review-not-discarded");
			}
			probe.Evidence.DiscardDeleted = finishPage() == nullptr && posts().empty()
				&& std::ranges::none_of(all, [&](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Anchor.Line == probe.Lines.front(); })
				&& std::ranges::any_of(all, [&](const core::revisioned_review_draft& aDraft) { return std::optional{ aDraft.Draft.Id } == probe.OldDraft && aDraft.Draft.Body == older.toStdString(); });
			if (!probe.Evidence.DiscardDeleted) {
				return fail("discard-review-not-exact");
			}
			window->compose_review_action()->trigger();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_compose_page;
			return result::pending;
		}
		case state_type::phase::awaiting_compose_page: {
			const auto& draft = summary->shown_draft();
			note("page=" + std::to_string(finishPage() != nullptr ? 1 : 0) + " title=" + summary->title_label()->text().toStdString());
			if (finishPage() == nullptr || !draft || draft->State != core::review_submission_state::drafting || !summary->body_editor()->isVisibleTo(window)) {
				return ++probe.Waited < 400U ? result::pending : fail("compose-not-opened");
			}
			if (summary->title_label()->text() != QStringLiteral("Compose review") || !draft->Comments.empty() || !draft->Body.empty()) {
				return fail("compose-not-a-new-review");
			}
			summary->body_editor()->setPlainText(summarised);
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_compose_written;
			return result::pending;
		}
		case state_type::phase::awaiting_compose_written: {
			const auto& draft = summary->shown_draft();
			if (!draft || draft->Body != summarised.toStdString() || !summary->discard_action()->isEnabled()) {
				return ++probe.Waited < 400U ? result::pending : fail("compose-not-written");
			}
			summary->discard_action()->trigger();
			QCoreApplication::processEvents();
			if (summary->discard_confirmation() == nullptr || !confirm(summary->discard_confirmation(), "discardReviewConfirmationConfirm")) {
				return fail("compose-discard-asked-nothing");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_compose_discarded;
			return result::pending;
		}
		case state_type::phase::awaiting_compose_discarded: {
			note("page=" + std::to_string(finishPage() != nullptr ? 1 : 0));
			if (finishPage() != nullptr || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("compose-discard-not-closed");
			}
			probe.Evidence.SecondDiscard = posts().empty() && pending().empty();
			if (!probe.Evidence.SecondDiscard) {
				return fail("compose-discard-not-exact");
			}
			// Back to the older draft, at its line.
			if (!probe.Old || !window->comparison_surface()->select_conversation_line(core::diff_side::right, *probe.Old)) {
				return fail("older-line-not-selected");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_old_again;
			return result::pending;
		}
		case state_type::phase::awaiting_old_again: {
			const auto& draft = implementation->ReviewDraft;
			note("draft=" + std::to_string(draft && std::optional{ draft->Id } == probe.OldDraft ? 1 : 0) + " writing=" + std::to_string(cards->writing_draft() ? 1 : 0));
			if (!draft || std::optional{ draft->Id } != probe.OldDraft || !ready() || !write(older) || !written(older)) {
				return ++probe.Waited < 400U ? result::pending : fail("older-draft-not-opened");
			}
			// ⌘↩: Start a review.
			static_cast<void>(press(Qt::Key_Return, Qt::ControlModifier));
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_again_pending;
			return result::pending;
		}
		case state_type::phase::awaiting_again_pending: {
			const auto now = pending();
			note("pending=" + std::to_string(now.size()) + " chip=" + chip->summary().toStdString());
			if (now.size() != 1U || !chipSays(1U) || cards->writing_draft() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("older-draft-not-pending");
			}
			probe.Evidence.AgainStarted = std::optional{ now.front().Draft.Id } == probe.OldDraft && now.front().Draft.Body == older.toStdString() && posts().empty();
			if (!probe.Evidence.AgainStarted) {
				return fail("older-draft-not-pending-exactly");
			}
			probe.Drafts = { now.front().Draft.Id };
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_again_page;
			return result::pending;
		}
		case state_type::phase::awaiting_again_page: {
			auto* const state  = summary->findChild<QLabel*>(QStringLiteral("wholeReviewDraftState"));
			const auto  opened = openFinish();
			note("opened=" + std::to_string(opened ? 1 : 0) + " title=" + summary->title_label()->text().toStdString() + " rows=" + std::to_string(summary->pending_rows().size())
				 + " state=" + (state != nullptr ? state->text().toStdString() : std::string{ }) + " status=" + summary->status_label()->text().toStdString());
			if (!opened || !shown(1U)) {
				return ++probe.Waited < 400U ? result::pending : fail("again-page-not-shown");
			}
			// The owner saw "Review draft discarded." here.
			const auto said             = summary->status_label()->text() + (state != nullptr ? state->text() : QString{ });
			probe.Evidence.AgainOffered = !said.contains(QStringLiteral("discarded"), Qt::CaseInsensitive) && calm()
				&& summary->pending_rows().front().endsWith(older);
			if (!probe.Evidence.AgainOffered) {
				return fail("again-page-stale");
			}
			summary->disposition_button(core::review_disposition::comment)->click();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_again_submitted;
			return result::pending;
		}
		case state_type::phase::awaiting_again_submitted: {
			const auto sent      = posts();
			const auto published = countPublished();
			note("posts=" + std::to_string(sent.size()) + " published=" + std::to_string(published) + " status=" + summary->status_label()->text().toStdString());
			if (sent.size() != 1U || published != 1U || chipShown->isVisible() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("again-not-submitted");
			}
			const auto object             = QJsonDocument::fromJson(QByteArray::fromStdString(sent.front())).object();
			const auto comments           = object.value(QStringLiteral("comments")).toArray();
			probe.Evidence.AgainSubmitted = object.value(QStringLiteral("event")).toString() == QStringLiteral("COMMENT") && comments.size() == 1
				&& comments.at(0).toObject().value(QStringLiteral("body")).toString() == older && probe.Old
				&& comments.at(0).toObject().value(QStringLiteral("line")).toInt() == static_cast<int>(*probe.Old) + 1 && pending().empty()
				&& finishPage() == nullptr && window->statusBar()->currentMessage() == QStringLiteral("Review submitted with 1 comment");
			if (probe.Evidence.AgainSubmitted) {
				renderThemes(QStringLiteral("submitted-returned"));
			}
			if (!probe.Evidence.AgainSubmitted) {
				return fail("again-submitted-not-exact");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::passed:
			return result::passed;
		case state_type::phase::failed:
		default:
			break;
		}
		return result::failed;
	}

	auto detail::github_application_fixture_access::start_review_evidence(const github_application& aApplication) -> detail::start_review_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->StartReviewProbe.has_value()) {
			return { };
		}
		return implementation->StartReviewProbe->Evidence;
	}

	auto detail::github_application_fixture_access::start_review_diagnostic(const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->StartReviewProbe.has_value()) {
			return { };
		}
		return implementation->StartReviewProbe->Diagnostic;
	}

	auto detail::github_application_fixture_access::exercise_review_progress_push(
		github_application& aApplication, const detail::review_progress_push_course aCourse) -> detail::review_progress_push_result
	{
		using result               = detail::review_progress_push_result;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->ReviewProgressPushProbe.has_value()) {
			implementation->ReviewProgressPushProbe.emplace();
		}
		auto& probe = *implementation->ReviewProgressPushProbe;
		if (probe.Failed) {
			return result::failed;
		}
		if (probe.Passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr || implementation->WorkbenchHost == nullptr
			|| implementation->ReviewLedgerHost == nullptr || !implementation->LastAcceptedHandoff.has_value()) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const ledger = implementation->ReviewLedgerHost.get();
		const auto  fail   = [&probe](const std::string_view aWhy) {
			probe.Diagnostic += " failed=" + std::string{ aWhy };
			probe.Failed = true;
			return result::failed;
		};
		enum class gesture : std::uint8_t {
			accept,
			exclude,
		};
		struct course_file
		{
			std::string_view Path;
			gesture          Gesture;
		};
		// renderer.cpp is the one file the push changes.
		constexpr auto files = std::array{
			course_file{ .Path = "src/engine/renderer.cpp", .Gesture = gesture::accept },
			course_file{ .Path = "src/main.cpp", .Gesture = gesture::accept },
			course_file{ .Path = "src/ui/widgets/button.cpp", .Gesture = gesture::exclude },
		};
		if (probe.File == 0U && !probe.Revealed && probe.Marks.empty()) {
			const auto& evidence = implementation->Fixture->evidence();
			const auto  expected = core::parse_git_commit_oid(aCourse == detail::review_progress_push_course::after ? evidence.HeadTwoOid : evidence.HeadOneOid);
			if (!expected.has_value() || implementation->LastAcceptedHandoff->Head != *expected) {
				return ++probe.Waited < 400U ? result::pending : fail("head");
			}
			probe.Marks  = "head=" + std::string{ core::canonical_hex(implementation->LastAcceptedHandoff->Head).view() };
			probe.Waited = 0U;
		}
		if (probe.File == files.size()) {
			probe.Passed = true;
			return result::passed;
		}
		const auto& file = files[probe.File];
		if (!probe.Revealed) {
			const auto revealed = reveal_fixture_file(aApplication, file.Path);
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("reveal " + std::string{ file.Path });
			}
			probe.Revealed = revealed == detail::fixture_file_reveal_result::shown;
			return result::pending;
		}
		// The file's receipt is shown once the ledger has answered the
		// observation of exactly this file's material.
		const auto& selected = implementation->LastSelectedDiff;
		const auto  retained = ledger->retained();
		const auto  shown    = ledger->is_quiescent() && selected.has_value() && selected->Request.File.PresentationPath == file.Path
			&& retained.File == selected->Request.File.File && retained.Material == core::file_material(selected->Key);
		if (!shown) {
			return ++probe.Waited < 400U ? result::pending : fail("receipt " + std::string{ file.Path });
		}
		if (aCourse == detail::review_progress_push_course::before && !probe.Acted) {
			auto* const action = file.Gesture == gesture::accept ? window->accept_action() : window->exclude_action();
			if (action == nullptr || !action->isEnabled()) {
				return fail("gesture " + std::string{ file.Path });
			}
			probe.Acted = true;
			action->trigger();
			return result::pending;
		}
		if (aCourse == detail::review_progress_push_course::before && !(file.Gesture == gesture::accept ? retained.Accepted : retained.Excluded)) {
			return ++probe.Waited < 400U ? result::pending : fail("durable " + std::string{ file.Path });
		}
		const auto* const button = window->findChild<QWidget*>(QStringLiteral("workbenchReviewControlsButton"));
		probe.Marks += ";" + std::string{ file.Path } + "=" + (button != nullptr ? button->accessibleName().toStdString() : std::string{ "absent" });
		++probe.File;
		probe.Revealed = false;
		probe.Acted    = false;
		probe.Waited   = 0U;
		return result::pending;
	}

	auto detail::github_application_fixture_access::review_progress_push_marks(const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->ReviewProgressPushProbe.has_value() ? implementation->ReviewProgressPushProbe->Marks : std::string{ };
	}

	auto detail::github_application_fixture_access::review_progress_push_diagnostic(const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->ReviewProgressPushProbe.has_value() ? implementation->ReviewProgressPushProbe->Diagnostic : std::string{ };
	}

	auto detail::github_application_fixture_access::published_file_switch_evidence(
		const github_application& aApplication) -> detail::published_file_switch_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->PublishedFileSwitchProbe.has_value()) {
			return { };
		}
		return implementation->PublishedFileSwitchProbe->Evidence;
	}

	auto detail::github_application_fixture_access::published_file_switch_diagnostic(
		const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->PublishedFileSwitchProbe.has_value()) {
			return { };
		}
		return implementation->PublishedFileSwitchProbe->Diagnostic;
	}

	auto detail::github_application_fixture_access::threaded_reply_diagnostic(
		const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->ThreadedReplyProbe.has_value()) {
			return { };
		}
		return implementation->ThreadedReplyProbe->Diagnostic;
	}

	auto detail::github_application_fixture_access::threaded_reply_evidence(
		const github_application& aApplication) -> detail::threaded_reply_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->ThreadedReplyProbe.has_value()) {
			return { };
		}
		return implementation->ThreadedReplyProbe->Evidence;
	}

	auto detail::github_application_fixture_access::review_round_trip_composition_evidence(
		const github_application& aApplication) noexcept
		-> detail::review_round_trip_composition_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->ReviewRoundTripCompositionProbe.has_value()
			? implementation->ReviewRoundTripCompositionProbe->Evidence
			: detail::review_round_trip_composition_probe_evidence{ };
	}
#endif
}
