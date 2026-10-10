// [[BPR-1329]]. An inline draft published from its card in the Comments
// drawer, over a GitHub CLI session, through the per-publication GitHub CLI
// confirmation answered Yes. The owner's 0.1.5 report: "It popped up a
// warning about publishing with my gh account and I allowed it but nothing
// seemed to happen".
//
// The drawer's Publish runs the draft editor's own Publish, and every result
// of that publication used to be written only into the draft editor's status
// line, which sits on the inline composition page the drawer does not show.
// So from the drawer a publication was invisible whatever became of it. Since
// [[BPR-1212]] a new comment is written in the drawer, and opening its note
// writes it there again. Since [[BPR-1345]] that Publish is Add single
// comment; in the drawer's default width it is the split arrow's menu item
// beside Start a review, and the courses send it from there.
//
// Two compositions over the recorded GitHub.com fixture and its synthetic
// `gh auth status` account, each signed in through Use GitHub CLI:
//
// - success: the draft says the comment is publishing, then the posted
//   thread replaces it, open;
// - a lost result (the fixture stores the comment and loses its answer): the
//   draft stays, says why in words and offers Retry, and says so again when
//   it is closed and opened; Retry asks the confirmation again and the
//   drawer then shows the one posted thread;
// - the confirmation answered Cancel: nothing is sent, and the draft says so
//   and still offers Add single comment.
//
// No live GitHub, gh process or credential.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;
	using bpr::tests::expect;

	auto wait_until(const std::function<bool()>& aCondition, const qint64 aTimeoutMs = 20000) -> bool
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		return aCondition();
	}
	void wait_for(const std::function<bool()>& aCondition, const std::string_view aMessage, const qint64 aTimeoutMs = 20000)
	{
		expect(wait_until(aCondition, aTimeoutMs), aMessage);
	}

	auto draft_card(const qt::conversation_cards_view& aCards) -> const qt::thread_card*
	{
		const auto& cards = aCards.cards();
		const auto  found = std::ranges::find_if(cards, [](const qt::thread_card& aCard) { return aCard.Draft; });
		return found == cards.end() ? nullptr : &*found;
	}
	auto posted_card(const qt::conversation_cards_view& aCards, const QString& aBody) -> const qt::thread_card*
	{
		const auto& cards = aCards.cards();
		const auto  found = std::ranges::find_if(cards, [&aBody](const qt::thread_card& aCard) {
			return !aCard.Draft && !aCard.Posts.empty() && aCard.Posts.front().Body == aBody;
		});
		return found == cards.end() ? nullptr : &*found;
	}
	auto posted_count(const qt::conversation_cards_view& aCards, const QString& aBody) -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(aCards.cards(), [&aBody](const qt::thread_card& aCard) {
			return !aCard.Draft && !aCard.Posts.empty() && aCard.Posts.front().Body == aBody;
		}));
	}
	/// What the draft written in the drawer ([[BPR-1212]]) says about its
	/// publication, as a person sees it, under its Publish.
	auto draft_status(const qt::conversation_cards_view& aCards) -> QString
	{
		auto* const open = aCards.open_card();
		if (open == nullptr || !open->isVisible()) {
			return { };
		}
		auto* const label = open->findChild<QLabel*>(QStringLiteral("threadDraftStatus"));
		return label != nullptr && label->isVisibleTo(open) ? label->text() : QString{ };
	}
	auto draft_publish(const qt::conversation_cards_view& aCards) -> QAbstractButton*
	{
		auto* const open = aCards.open_card();
		return open == nullptr ? nullptr : open->findChild<QAbstractButton*>(QStringLiteral("threadPublishDraft"));
	}
	auto draft_more(const qt::conversation_cards_view& aCards) -> QAbstractButton*
	{
		auto* const open = aCards.open_card();
		return open == nullptr ? nullptr : open->findChild<QAbstractButton*>(QStringLiteral("threadReviewDraftMore"));
	}
	/// [[BPR-1345]] v002. Add single comment is offered as its own pill where
	/// the drawer has room for it beside the main action, and otherwise as the
	/// split arrow's menu item.
	auto single_offered(const qt::conversation_cards_view& aCards) -> bool
	{
		auto* const publish = draft_publish(aCards);
		auto* const more    = draft_more(aCards);
		return publish != nullptr && (publish->isVisible() || (more != nullptr && more->isVisible()));
	}
	/// Sends Add single comment as a person does: its pill, or the split
	/// arrow's menu item, which closes its menu before it acts.
	void send_single(const qt::conversation_cards_view& aCards)
	{
		auto* const publish = draft_publish(aCards);
		expect(publish != nullptr, "the open draft has Add single comment");
		if (publish->isVisible()) {
			publish->click();
			return;
		}
		auto* const more = draft_more(aCards);
		expect(more != nullptr && more->isVisible(), "the split arrow carries Add single comment");
		more->click();
		QApplication::processEvents();
		const QPointer menu = aCards.findChild<QMenu*>(QStringLiteral("threadReviewDraftMenu"));
		auto* const    item = menu != nullptr ? menu->findChild<QAction*>(QStringLiteral("threadReviewDraftSingle")) : nullptr;
		expect(item != nullptr && item->isEnabled() && item->text() == publish->text(), "the split arrow's menu offers the draft's single comment");
		menu->hide();
		item->trigger();
		if (menu != nullptr) {
			menu->close();
		}
	}

	enum class course : std::uint8_t {
		published,
		lost_then_retried,
		cancelled,
	};

	void run(const course aCourse)
	{
		const auto name      = aCourse == course::published ? std::string_view{ "published" }
			: aCourse == course::cancelled                  ? std::string_view{ "cancelled" }
															: std::string_view{ "lost-then-retried" };
		auto       temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "private temporary application root");
		auto options                          = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot           = e2e::private_data_root(temporary);
		options.PublicPreviewEnabledCourse    = true;
		options.IndependentWindowReads        = true;
		options.LoseFirstReviewMutationResult = aCourse == course::lost_then_retried;
		options.GithubCliDiscoveryAccounts    = std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{
			{ .Hostname = "github.com", .Login = "octo-reviewer" }
		};
		expect(options.ApplicationDataRoot.has_value(), "private desktop test root under the canonical storage policy");

		std::unique_ptr<app::github_application> application;
		auto                                     terminal = false;
		auto                                     startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "application startup reaches a terminal");
		expect(application != nullptr, "application startup succeeds");
		auto& window = application->window();
		window.resize(1440, 900);
		window.show();
		QApplication::processEvents();

		// Every confirmation is answered Yes, as the owner did, except the
		// publication's in the cancelled course; each one asked is counted.
		auto   readOptIns  = 0U;
		auto   publishAsks = 0U;
		QTimer consent;
		QObject::connect(&consent, &QTimer::timeout, [&] {
			const bpr::tests::exit_on_failure inTheEventLoop;
			if (auto* const prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
				if (prompt->text().startsWith(QStringLiteral("Publish inline comment"))) {
					expect(prompt->text().startsWith(QStringLiteral("Publish inline comment on github.com for octo-org/review-repository#17 as octo-reviewer?")),
						   "the publication confirmation names the operation, target and account");
					++publishAsks;
					prompt->button(aCourse == course::cancelled ? QMessageBox::Cancel : QMessageBox::Yes)->click();
					return;
				}
				++readOptIns;
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		consent.start(1);

		window.home_action()->trigger();
		QApplication::processEvents();
		window.home_landing()->secondary_button()->click();
		wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "Use GitHub CLI connects the discovered account");
		window.home_open_bar()->setText(QStringLiteral("https://github.com/octo-org/review-repository/pull/17"));
		window.home_open_action()->trigger();
		wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "the pull request opens through the GitHub CLI session");

		wait_for([&] { return github_application_fixture_access::reveal_fixture_file(*application, "review.cpp") == app::detail::fixture_file_reveal_result::shown; },
				 "review.cpp is shown in Review");
		const auto body = std::string{ "BPR-1329: published from the drawer after the GitHub CLI confirmation." };
		wait_for([&] {
			const auto seeded = github_application_fixture_access::seed_attended_comment_draft(*application, core::diff_side::right, body);
			expect(seeded != app::detail::fixture_file_reveal_result::failed, "a new comment can be written on a head line");
			return seeded == app::detail::fixture_file_reveal_result::shown;
		},
				 "a new inline draft holds its text");
		auto* const cards = window.conversation_panel()->cards();
		wait_for([&] { return cards->writing_draft() && single_offered(*cards); }, "the new comment is written in the drawer");
		auto* publish = draft_publish(*cards);
		expect(publish != nullptr && single_offered(*cards) && publish->isEnabled(), "the open draft offers Add single comment");
		const auto before = github_application_fixture_access::review_mutation_evidence(*application);
		const auto text   = QString::fromStdString(body);
		if (aCourse == course::cancelled) {
			send_single(*cards);
			const auto said = wait_until([&] { return draft_status(*cards).contains(QStringLiteral("cancelled")); }, 5000);
			std::cout << "BPR_1329:" << name << ":status=\"" << draft_status(*cards).toStdString() << "\" publish_asks=" << publishAsks << std::endl;
			expect(said, "the draft says the publication was not sent because the confirmation was cancelled");
			const auto after = github_application_fixture_access::review_mutation_evidence(*application);
			expect(publishAsks == 1U && after.Submissions == before.Submissions && after.StoredComments == 0U, "a cancelled confirmation sends nothing");
			publish = draft_publish(*cards);
			expect(draft_card(*cards) != nullptr && publish != nullptr && single_offered(*cards) && publish->isEnabled()
					   && publish->text() == QStringLiteral("Add single comment"),
				   "the draft stays, and Add single comment is still offered");
			std::cout << "BPR_1329:" << name << ":pass\n";
			consent.stop();
			application->begin_shutdown();
			wait_for([&] { return application->status().ShutdownComplete; }, "all composed hosts quiesce");
			return;
		}
		// The publication's answer is held on its way to the window, so what
		// the card says while it is in flight can be seen.
		github_application_fixture_access::hold_review_mutation_terminals(*application, true);
		send_single(*cards);
		const auto publishing   = wait_until([&] {
			return github_application_fixture_access::held_review_mutation_terminals(*application) != 0U
				&& draft_status(*cards).contains(QStringLiteral("Publishing"));
		});
		publish                 = draft_publish(*cards);
		const auto offeredTwice = publish == nullptr || publish->isEnabled();
		std::cout << "BPR_1329:" << name << ":in-flight status=\"" << draft_status(*cards).toStdString() << "\" held="
				  << github_application_fixture_access::held_review_mutation_terminals(*application) << std::endl;
		// Released before anything is judged, so a failure does not leave the
		// publication held through shutdown.
		github_application_fixture_access::hold_review_mutation_terminals(*application, false);
		expect(publishing, "while it is in flight, the draft says the comment is publishing");
		expect(!offeredTwice, "Publish is not offered twice while one is in flight");
		if (aCourse == course::lost_then_retried) {
			// The fixture stores the comment and loses its answer. The draft
			// card stays, says why, and offers Retry.
			wait_for([&] { return github_application_fixture_access::review_mutation_evidence(*application).LostResult; }, "the fixture loses the first publication's answer");
			const auto settled = wait_until([&] { return draft_card(*cards) != nullptr && draft_status(*cards).contains(QStringLiteral("retry is safe")); });
			std::cout << "BPR_1329:" << name << ":after-loss draft_card=" << (draft_card(*cards) != nullptr) << " status=\"" << draft_status(*cards).toStdString()
					  << "\" editor_status=\"" << window.review_editor()->mutation_status_text().toStdString() << "\" publish_asks=" << publishAsks << std::endl;
			expect(settled, "the draft says in words that the result is unavailable and retry is safe");
			expect(publishAsks == 1U, "the publication was confirmed once");
			// Closed, the draft is kept as its note; opened, it says so again.
			auto* const close = cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadClose"));
			expect(close != nullptr, "the draft in the drawer can be closed");
			close->click();
			wait_for([&] { return !cards->writing_draft() && draft_card(*cards) != nullptr; }, "the draft is kept as its note");
			cards->open_thread(draft_card(*cards)->Id);
			wait_for([&] { return cards->writing_draft() && draft_status(*cards).contains(QStringLiteral("retry is safe")); },
					 "opened again, the draft still says why it was not confirmed published");
			publish = draft_publish(*cards);
			std::cout << "BPR_1329:" << name << ":reopened publish=" << (publish != nullptr ? publish->text().toStdString() : std::string{ "none" })
					  << " enabled=" << (publish != nullptr && publish->isEnabled()) << " editor_publish=" << window.review_editor()->publish_action()->text().toStdString()
					  << " editor_enabled=" << window.review_editor()->publish_action()->isEnabled() << std::endl;
			expect(publish != nullptr && single_offered(*cards) && publish->isEnabled() && publish->text() == QStringLiteral("Retry"),
				   "the draft offers Retry for the same publication");
			send_single(*cards);
		}

		const auto shown = wait_until([&] { return posted_card(*cards, text) != nullptr && draft_card(*cards) == nullptr; });
		const auto after = github_application_fixture_access::review_mutation_evidence(*application);
		std::cout << "BPR_1329:" << name << ":publish_asks=" << publishAsks << " read_opt_ins=" << readOptIns
				  << " native_posts=" << (after.NativePosts - before.NativePosts) << " stored=" << after.StoredComments
				  << " draft_card=" << (draft_card(*cards) != nullptr) << " posted_card=" << (posted_card(*cards, text) != nullptr)
				  << " status=\"" << window.status_text().toStdString() << "\" editor_status=\"" << window.review_editor()->mutation_status_text().toStdString() << "\"\n";
		expect(publishAsks == (aCourse == course::published ? 1U : 2U), "each publication asks the GitHub CLI confirmation once");
		expect(after.StoredComments == 1U && after.LastBody == body, "the fixture server holds exactly the one comment, with the draft's text");
		expect(shown, "the drawer shows the posted thread in place of the draft");
		expect(posted_count(*cards, text) == 1U, "one posted thread, not two");
		const auto* const posted = posted_card(*cards, text);
		expect(cards->open_card() != nullptr && cards->open_card()->isVisible() && cards->open_card()->property("threadKey").toString().startsWith(QString::fromStdString(posted->Id)),
			   "the posted thread is the one open in the drawer");
		std::cout << "BPR_1329:" << name << ":pass\n";

		consent.stop();
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "all composed hosts quiesce");
	}
}

int main(int aArgc, char** aArgv)
try {
	QApplication qt{ aArgc, aArgv };
	// One course by name, or all three.
	const auto only = aArgc > 1 ? std::string_view{ aArgv[1] } : std::string_view{ };
	for (const auto& [course, name] : { std::pair{ course::published, std::string_view{ "published" } },
										std::pair{ course::lost_then_retried, std::string_view{ "lost-then-retried" } },
										std::pair{ course::cancelled, std::string_view{ "cancelled" } } }) {
		if (only.empty() || only == name) {
			run(course);
		}
	}
	std::cout << "inline-publish-cli-consent-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
