/// \file
/// \brief RFC-BPR-0127 ([[BPR-2160]]). One keep action on the agent surfaces,
/// and the two private-context opt-ins, through the shipped window on
/// nested-folders-v1 with the suggestions fixture agent as the only Codex.
///
/// - AC10, the Agent answer footer: an answer about the pull request (no
///   thread, no lines) offers Keep local alone, which keeps a Local comment
///   on the pull request and becomes Go to comment; an answer about selected
///   lines offers Draft comment alone, which drafts at those lines and
///   becomes Go to draft.
/// - AC10, finding cards and the finding queue: the finding whose line is in
///   its file offers Draft comment; the one past the file's end and the one
///   on a file the pull request does not change offer Keep local, which keeps
///   a Local comment on the file and on the pull request. The queue offers
///   the core's one.
/// - AC12: with a Local comment and a draft present, a turn request carries
///   Local comments only with the Local opt-in and the draft only with the
///   drafts opt-in, each independently (all four settings).
/// - Nothing reaches GitHub.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>
#include <QTemporaryDir>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	template <typename Predicate>
	void wait(Predicate aPredicate, const std::string& aMessage)
	{
		if (!e2e::wait_until_plain(aPredicate, e2e::extended_agent_budget)) {
			throw std::runtime_error{ aMessage };
		}
	}

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			throw std::runtime_error{ aMessage };
		}
	}

	void shutdown(app::github_application& aApplication)
	{
		aApplication.begin_shutdown();
		wait([&] { return aApplication.status().ShutdownComplete; }, "shutdown");
	}

	/// The Agent tab's setup finds the fixture `codex` and its Check passes,
	/// as a person's does; nothing is asked about the pull request.
	void agent_ready(qt::workbench_window& aWindow)
	{
		aWindow.open_agent_question();
		auto*      tab   = aWindow.agent_tab();
		const auto ready = [tab] { return tab->setup_readiness() == qt::agent_setup_readiness::ready; };
		wait([tab] { return tab->setup_readiness() != qt::agent_setup_readiness::checking; }, "the agent setup looks for agents");
		if (!ready()) {
			aWindow.open_agent_setup();
			const auto check = [&aWindow] {
				auto* const button = aWindow.agent_setup()->findChild<QPushButton*>(QStringLiteral("agentSetupCheck"));
				return button != nullptr && button->isVisible() && button->isEnabled() ? button : nullptr;
			};
			wait([&] { return check() != nullptr; }, "the setup offers Check it works on the fixture codex");
			check()->click();
			wait(ready, "Check it works passes on the fixture codex");
			aWindow.open_agent_question();
		}
		auto* const selector = aWindow.agent_panel()->profile_selector();
		wait([selector] { return selector->findData(QVariant::fromValue<qulonglong>(core::codex_jsonl_profile_id.value())) >= 0; }, "the fixture codex is offered");
		selector->setCurrentIndex(selector->findData(QVariant::fromValue<qulonglong>(core::codex_jsonl_profile_id.value())));
	}

	[[nodiscard]] auto shown(const QWidget* aWidget) -> bool { return aWidget != nullptr && !aWidget->isHidden(); }

	[[nodiscard]] auto card_with(const qt::agent_tab_view& aTab, const std::size_t aTurn, const QString& aClaim) -> QWidget*
	{
		for (auto* const card : aTab.turn_cards(aTurn)) {
			for (auto* const widget : card->findChildren<QWidget*>(QStringLiteral("agentFindingClaim"))) {
				const auto* const text = dynamic_cast<const qt::markdown_body_view*>(widget);
				if (text != nullptr && text->original_body().find(aClaim.toStdString()) != std::string::npos) {
					return card;
				}
			}
		}
		return nullptr;
	}
}

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication qtApplication{ argc, argv };
	if (argc < 3 || std::string_view{ argv[1] } != "keep") {
		std::cerr << "usage: agent_keep_action_public_path keep CODEX\n";
		return EXIT_FAILURE;
	}
	const auto    stub = std::filesystem::path{ argv[2] };
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "private data root refused\n";
		return EXIT_FAILURE;
	}
	std::unique_ptr<app::github_application> application;
	try {
		auto options                 = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = *dataRoot };
		options.CodexAgentExecutable = stub;
		options.CodexSearch          = app::detail::claude_search_environment{ .PathVariable = stub.parent_path().string() };
		auto terminal                = false;
		auto startup                 = access::start(app::detail::github_public_path_fixture::nested_folders_v1,
													 std::move(options), [&](auto aResult) { terminal = true; if (aResult) { application = std::move(*aResult); } });
		wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
		expect(application != nullptr, "startup succeeds");
		application->show();
		auto& window = application->window();
		window.resize(1440, 960);
		window.github_connect_action()->trigger();
		wait([&] { return window.github_open_action()->isEnabled(); }, "connected");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
		window.home_open_action()->trigger();
		const auto path = QStringLiteral("src/engine/renderer.cpp");
		wait([&] { return !window.showing_home() && window.showing_overview() && access::agent_invocation_evidence(*application).Quiescent; }, "the pull request opens on Overview");
		agent_ready(window);
		auto* const tab      = window.agent_tab();
		auto* const cards    = window.conversation_panel()->cards();
		const auto  mutation = access::review_mutation_evidence(*application);
		const auto  ask      = [&](const QString& aQuestion, const std::string& aStep) {
			const auto before = tab->turn_count();
			wait([&] {
				if (tab->turn_count() == before && tab->state() != qt::agent_tab_state::working) {
					window.side_chat()->composer()->setPlainText(aQuestion);
					tab->ask_button()->click();
				}
				return tab->turn_count() == before + 1U;
			},
				 aStep + ": sent");
			wait([&] { return tab->state() != qt::agent_tab_state::working && access::agent_invocation_evidence(*application).Quiescent && !tab->turn_answer(before).isEmpty(); }, aStep + ": answered");
			return before;
		};

		// AC10 with the owner's v006 amendment ("Don't have keep local there,
		// just draft is fine, they can move it to local after"): every agent
		// surface offers Draft alone; where nothing takes a draft it is
		// disabled with the pencil struck, and nothing offers Keep local.
		const auto prTurn = ask(QStringLiteral("What could this change break?"), "pull-request answer");
		auto*      keep   = tab->turn_note_button(prTurn);
		auto*      draft  = tab->turn_draft_button(prTurn);
		expect(!shown(keep) && shown(draft) && !draft->isEnabled() && draft->text() == QStringLiteral("Draft") && draft->property("draftRefused").toBool(),
			   "an answer no thread or line takes offers Draft, disabled and struck, and no Keep local: overview=" + std::to_string(window.showing_overview()) + " answer=" + tab->turn_answer(prTurn).left(80).toStdString());
		std::cout << "keep-action: footer-refused=pass\n";
		wait([&] { return window.open_file_in_review(path) && window.active_document_path() == path && window.comparison_surface() != nullptr && window.comparison_surface()->current_result() != nullptr; },
			 "the file opens in Review");
		auto* const surface = window.comparison_surface();

		// AC10, finding cards: the placed one drafts; the others offer Draft
		// disabled.
		auto* const placed    = card_with(*tab, prTurn, QStringLiteral("resets `frame_budget_` on every call"));
		auto* const gone      = card_with(*tab, prTurn, QStringLiteral("An unused member is left behind."));
		auto* const elsewhere = card_with(*tab, prTurn, QStringLiteral("The guide still names the renderer's old path."));
		std::cout << "keep-action: cards=" << tab->turn_cards(prTurn).size() << " placed=" << (placed != nullptr) << " gone=" << (gone != nullptr) << " elsewhere=" << (elsewhere != nullptr) << '\n';
		expect(placed != nullptr && gone != nullptr && elsewhere != nullptr, "the answer's findings are cards under it");
		const auto draftOf = [](QWidget* aCard) { return aCard->findChild<QAbstractButton*>(QStringLiteral("agentFindingDraftComment")); };
		const auto noLocal = [](QWidget* aCard) { return aCard->findChild<QAbstractButton*>(QStringLiteral("agentFindingKeepLocal")) == nullptr; };
		tab->refresh();
		expect(draftOf(placed) != nullptr && draftOf(placed)->isEnabled() && noLocal(placed), "a finding whose line is in its file offers Draft");
		// The gone one's line is known to be past the file's end once its
		// file is shown, which it is.
		expect(!draftOf(gone)->isEnabled() && draftOf(gone)->property("draftRefused").toBool() && noLocal(gone), "a finding past its file's end offers Draft disabled and struck");
		expect(!draftOf(elsewhere)->isEnabled() && draftOf(elsewhere)->property("draftRefused").toBool() && noLocal(elsewhere), "a finding on a file the pull request does not change offers Draft disabled and struck");
		draftOf(placed)->click();
		wait([&] { return placed->findChild<QLabel*>(QStringLiteral("agentFindingState"))->text().startsWith(QStringLiteral("Drafted")); },
			 "Draft drafts the placed finding");
		std::cout << "keep-action: cards=pass\n";
		// Kept as a draft, it can be moved to Local from its card (the state
		// control): the owner's "they can move it to local after".

		// AC10, the answer footer about selected lines: Draft drafts there.
		wait([&] { return window.open_file_in_review(path) && surface->current_result() != nullptr; }, "the file is shown again");
		expect(surface->select_conversation_line(core::diff_side::right, 4U), "a line is selected");
		const auto linesTurn = ask(QStringLiteral("What do these lines do?"), "lines answer");
		keep                 = tab->turn_note_button(linesTurn);
		draft                = tab->turn_draft_button(linesTurn);
		expect(shown(draft) && draft->isEnabled() && draft->text() == QStringLiteral("Draft") && !shown(keep), "an answer about selected lines offers Draft");
		draft->click();
		wait([&] { return draft->text() == QStringLiteral("Go to draft") || draft->text().startsWith(QStringLiteral("Not drafted")); }, "Draft drafts the answer");
		expect(draft->text() == QStringLiteral("Go to draft"), "Draft becomes Go to draft: " + draft->text().toStdString());
		wait([&] { const auto& shownDraft = window.review_editor()->shown_draft(); return shownDraft && !shownDraft->Body.empty() && shownDraft->Anchor.Line == 4U; }, "the draft stands at the selected line");
		std::cout << "keep-action: footer-draft=pass\n";
		// The owner, of a report's Go to draft: "How come when I clicked goto
		// draft it didnt switch to the comments tab? Then goto draft went to
		// the comment but didn't select it, it was still collapsed". An
		// answer's Go to draft goes the same way: with the right tray on the
		// Agent tab, the draft closed and another line selected, it turns the
		// tray to Comments and opens the draft in its composer. The drawer
		// and the tray are then left as they stood.
		{
			auto* const tray       = window.tray_tabs();
			const auto  trayShows  = [tray] { return tray->tabText(tray->currentIndex()); };
			const auto  wasWriting = cards->writing_draft();
			const auto  wasTab     = tray->currentIndex();
			const auto  words      = QString::fromStdString(window.review_editor()->shown_draft()->Body);
			const auto  closeDraft = [&] {
				auto* const close = cards->open_card() != nullptr ? cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadClose")) : nullptr;
				expect(close != nullptr && close->isEnabled(), "the open draft offers Close");
				close->click();
				wait([&] { return !cards->writing_draft(); }, "the draft is left a closed note");
			};
			if (wasWriting) {
				closeDraft();
			}
			expect(surface->select_conversation_line(core::diff_side::right, 1U), "another line is selected");
			wait([&] { const auto& shownDraft = window.review_editor()->shown_draft(); return !shownDraft || shownDraft->Anchor.Line != 4U; }, "the answer's draft is another line's saved draft");
			window.open_agent_question();
			wait([&] { return trayShows().startsWith(QStringLiteral("Agent")); }, "the right tray shows the Agent tab");
			expect(draft->text() == QStringLiteral("Go to draft") && draft->isEnabled() && !cards->writing_draft(), "the answer still offers Go to draft, its draft closed");
			draft->click();
			if (!e2e::wait_until_plain([&] { return trayShows().startsWith(QStringLiteral("Comments")) && cards->writing_draft() && cards->draft_editor() != nullptr; }, e2e::extended_agent_budget)) {
				throw std::runtime_error{ "an answer's Go to draft turns the right tray to Comments and opens the draft; the tray shows " + trayShows().toStdString() + (cards->writing_draft() ? ", the draft open" : ", the draft closed") };
			}
			expect(window.review_editor()->shown_draft() && window.review_editor()->shown_draft()->Anchor.Line == 4U && cards->draft_editor()->toPlainText() == words, "the open draft is the answer's, at its line, with its words");
			const auto* const opened = cards->open_card();
			expect(opened->geometry().top() >= 0 && opened->geometry().bottom() < cards->viewport()->height(), "the whole draft is in the drawer's view");
			const auto* const focus = QApplication::focusWidget();
			expect(focus != nullptr && (focus == opened || opened->isAncestorOf(focus)), "the draft holds the keyboard");
			if (!wasWriting) {
				closeDraft();
			}
			tray->setCurrentIndex(wasTab);
			QApplication::processEvents();
			std::cout << "keep-action: footer-go-to-draft=pass\n";
		}
		// A Local comment of mine for the opt-ins below (no agent surface
		// keeps one now).
		expect(access::seed_local_comment(*application, "LOCAL-ONLY: kept by hand.", false), "a Local comment is stored");
		// AC10, the finding queue offers the core's one keep action, for the
		// delivery of the turn asked with the file open, whose findings are
		// anchored against it where their lines hold.
		if (auto* const queue = window.finding_queue(); queue != nullptr && queue->finding_count() > 0U) {
			auto draftOffered = 0;
			auto keepOffered  = 0;
			for (std::size_t ordinal = 0U; ordinal < queue->finding_count(); ++ordinal) {
				queue->select(ordinal);
				const auto d = queue->action(qt::finding_queue_action::convert_to_draft_comment)->isVisible();
				const auto k = queue->action(qt::finding_queue_action::keep_local)->isVisible();
				expect(d && !k, "the queue offers Draft comment and never Keep local");
				draftOffered += d ? 1 : 0;
				keepOffered += k ? 1 : 0;
			}
			std::cout << "keep-action: queue draft=" << draftOffered << " keep=" << keepOffered << '\n';
			expect(keepOffered == 0, "no Keep local in the queue");
		}

		// RFC-BPR-0127, "One action where there were two": Ask agent on my
		// own unsent comment in either state, by one path. My draft's menu
		// attaches it as my Local comment's does; asked, it is recorded in
		// Private history and the agent gets that copy and its lines.
		{
			auto* const menu     = cards->thread_menu(qt::conversation_cards_view::draft_identity());
			auto*       askDraft = static_cast<QAction*>(nullptr);
			for (auto* const action : menu != nullptr ? menu->actions() : QList<QAction*>{ }) {
				if (action->objectName() == QStringLiteral("draftAskAgentAction")) {
					askDraft = action;
				}
			}
			expect(askDraft != nullptr && askDraft->isEnabled() && askDraft->text() == QStringLiteral("Ask agent about this draft"), "my draft's menu offers Ask agent about this draft");
			askDraft->trigger();
			delete menu;
			QApplication::processEvents();
			expect(tab->conversation_chip_text() == QStringLiteral("Your draft on src/engine/renderer.cpp line 5"), "the draft is the question's chip: " + tab->conversation_chip_text().toStdString());
			const auto asked = ask(QStringLiteral("Is this draft clear?"), "draft question");
			const auto chat  = access::agent_chat_evidence(*application);
			expect(chat.LastConversation && chat.LastConversation->Subject == core::conversation_subject::private_note && chat.LastConversation->Line == std::optional<std::uint32_t>{ 5U },
				   "the agent gets my draft as my own comment, with its line");
			expect(!tab->turn_answer(asked).isEmpty(), "and answers");
			tab->conversation_chip_close()->click();
			QApplication::processEvents();
			std::cout << "keep-action: ask-draft=pass\n";
		}

		// AC12. Each opt-in carries its own kind, and only its own.
		auto* const chat       = window.side_chat();
		auto* const localOptIn = chat->findChild<QCheckBox*>(QStringLiteral("sideChatNoteOptIn"));
		auto* const draftOptIn = chat->findChild<QCheckBox*>(QStringLiteral("sideChatDraftOptIn"));
		expect(localOptIn != nullptr && draftOptIn != nullptr && localOptIn->text() == QStringLiteral("Include my local comments") && draftOptIn->text() == QStringLiteral("Include my drafts"),
			   "the two opt-ins name local comments and drafts");
		for (const auto& [local, drafts] : { std::pair{ false, false }, std::pair{ true, false }, std::pair{ false, true }, std::pair{ true, true } }) {
			// Each toggle says both, for the chat's session now.
			localOptIn->setChecked(!local);
			localOptIn->setChecked(local);
			draftOptIn->setChecked(!drafts);
			draftOptIn->setChecked(drafts);
			QApplication::processEvents();
			const auto turns = access::agent_chat_evidence(*application).Turns + access::agent_chat_evidence(*application).RefusalsPresented;
			window.side_chat()->composer()->setPlainText(QStringLiteral("AC12 opt-in probe"));
			tab->ask_button()->click();
			wait([&] { QApplication::processEvents(); const auto evidence = access::agent_chat_evidence(*application); return evidence.LastTurn && evidence.Turns + evidence.RefusalsPresented > turns && access::agent_invocation_evidence(*application).Quiescent && tab->state() != qt::agent_tab_state::working; },
				 "the probe is asked");
			const auto evidence = access::agent_chat_evidence(*application);
			std::cout << "keep-action: opt-in local=" << local << " drafts=" << drafts << " carried-local=" << evidence.LastTurn->IncludeNotes << " carried-draft=" << evidence.LastTurn->IncludeDraft << '\n';
			expect(evidence.LastTurn->IncludeNotes == local && evidence.LastTurn->IncludeDraft == drafts, "a request carries Local comments only with their opt-in, and the draft only with its own");
		}
		localOptIn->setChecked(false);
		draftOptIn->setChecked(false);

		const auto after = access::review_mutation_evidence(*application);
		expect(after.Submissions == mutation.Submissions && after.StoredComments == mutation.StoredComments, "nothing reached GitHub");
		shutdown(*application);
		std::cout << "keep-action:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "keep-action: " << aError.what() << '\n';
		if (application) {
			auto& window = application->window();
			std::cerr << "agent-panel: " << window.agent_panel()->status_text().toStdString() << "\nrefusal: " << window.agent_tab()->send_refusal_text().toStdString()
					  << "\nturns: " << window.agent_tab()->turn_count() << " state: " << static_cast<int>(window.agent_tab()->state()) << '\n';
			shutdown(*application);
		}
		return EXIT_FAILURE;
	}
}
