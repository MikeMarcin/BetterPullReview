// Ordinary chosen-account CLI Publish in the drawer, including remembered
// read trust, strict-capability refusal and GET-only lost-result recovery.
// No live GitHub, gh process or credential. The designated fixture grants
// stay unavailable outside this fixture composition.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "drawer_single_comment.hpp"
#include "private_data_root.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
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
	using e2e::draft_publish;
	using e2e::send_single;
	using e2e::single_offered;

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

	void capture_settled(qt::workbench_window& aWindow, const QString& aSuffix)
	{
		const auto root = qEnvironmentVariable("BPR_QT_RENDER_PATH");
		if (root.isEmpty()) {
			return;
		}
		QElapsedTimer elapsed;
		elapsed.start();
		while (elapsed.elapsed() < 900) {
			QApplication::processEvents();
			QThread::msleep(2);
		}
		expect(aWindow.grab().save(root + aSuffix + ".png"), "settled full inline publication surface renders");
	}

	void refuse_recovery(app::github_application& aApplication, const app::detail::github_public_path_fixture_options& aOptions,
						 qt::conversation_cards_view& aCards, const core::review_comment_draft& aOriginal,
						 std::optional<application::review_mutation_failure> aFailure, const QString& aReason)
	{
		const auto before = github_application_fixture_access::review_mutation_evidence(aApplication);
		{
			const auto lock                                        = std::scoped_lock{ aOptions.ReviewSubmissionServer->Mutex };
			aOptions.ReviewSubmissionServer->CliAcquisitionFailure = aFailure;
		}
		aOptions.GithubCliStrictCapability->store(aFailure.has_value());
		send_single(aCards);
		auto& editor = *aApplication.window().review_editor();
		wait_for([&] { return draft_status(aCards).contains(aReason) && editor.publication_retry_offered(); },
				 "refused recovery keeps the same retry in the public drawer");
		const auto after = github_application_fixture_access::review_mutation_evidence(aApplication);
		expect(draft_status(aCards).contains(QStringLiteral("still unknown")) && !draft_status(aCards).contains(QStringLiteral("Not published")),
			   "a refused check cannot claim the original comment was not published");
		expect(editor.shown_draft() && *editor.shown_draft() == aOriginal && editor.body_editor()->isReadOnly(),
			   "body, anchor, publication identity and publishing lock are retained");
		expect(after.NativePosts == before.NativePosts && after.FilePosts == before.FilePosts && after.StoredComments == 1U,
			   "every refused recovery makes zero additional POSTs");
		expect(after.DurableJournalReads == before.DurableJournalReads,
			   "early capability or coordinator observation refusal is covered before session recovery loads");
		expect(after.Submissions == before.Submissions && after.TargetObservations == before.TargetObservations + (aFailure ? 1U : 0U),
			   "credential refusal reaches the coordinator observation; capability refusal stops before it");
		auto* const retry = draft_publish(aCards);
		expect(retry && retry->isEnabled() && retry->text() == QStringLiteral("Retry"), "the ordinary new-comment action never replaces the retry");
		std::cout << "M3_RECOVERY_REFUSAL reason=" << aReason.toStdString() << " posts=" << after.NativePosts << " stored=" << after.StoredComments << '\n';
	}

	enum class course : std::uint8_t {
		published,
		lost_then_retried,
		unsupported,
		credential_unavailable,
		tool_failed,
		cancelled,
	};

	void run(const course aCourse)
	{
		const auto name      = aCourse == course::published ? std::string_view{ "published" }
			: aCourse == course::unsupported                ? std::string_view{ "unsupported" }
															: std::string_view{ "lost-then-retried" };
		auto       temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "private temporary application root");
		auto options                          = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot           = e2e::private_data_root(temporary);
		options.ReviewSubmissionServer        = std::make_shared<app::detail::review_submission_fixture_state>();
		options.PublicPreviewEnabledCourse    = true;
		options.IndependentWindowReads        = true;
		options.LoseFirstReviewMutationResult = aCourse == course::lost_then_retried;
		options.GithubCliStrictCapability     = std::make_shared<std::atomic_bool>(true);
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
		auto& firstWindow = application->window();
		firstWindow.resize(1440, 900);
		firstWindow.show();
		QApplication::processEvents();

		// Every confirmation is answered Yes, as the owner did, except the
		// publication's in the unsupported course; each one asked is counted.
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
					prompt->button(aCourse == course::unsupported ? QMessageBox::Cancel : QMessageBox::Yes)->click();
					return;
				}
				++readOptIns;
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		consent.start(1);

		firstWindow.home_action()->trigger();
		QApplication::processEvents();
		firstWindow.home_landing()->secondary_button()->click();
		wait_for([&] { return firstWindow.session_panel()->status() == core::github_status::session_ready; }, "Use GitHub CLI connects the discovered account");
		// Publication after restored read trust still requires the ordinary
		// exact-operation gesture and its grant; trust itself issues no write.
		expect(readOptIns == 1U, "first explicit selection accepts read trust once");
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "first lifetime drains its remembered selection");
		application.reset();
		startup.reset();
		options.GithubCliDiscoveryAccounts->clear();
		terminal = false;
		startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup->is_quiescent(); }, "publication lifetime restarts");
		expect(application != nullptr, "publication lifetime starts");
		auto& window = application->window();
		window.resize(1440, 900);
		window.show();
		wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "publication lifetime restores exact saved read authority without inventory");
		expect(readOptIns == 1U, "restored read authority asks no second read confirmation");
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
		if (aCourse == course::unsupported) {
			options.GithubCliStrictCapability->store(false);
		}
		const auto text = QString::fromStdString(body);
		if (aCourse == course::credential_unavailable || aCourse == course::tool_failed || aCourse == course::cancelled) {
			options.ReviewSubmissionServer->CliAcquisitionFailure = aCourse == course::credential_unavailable ? application::review_mutation_failure::credential_unavailable
				: aCourse == course::tool_failed                                                              ? application::review_mutation_failure::credential_tool_failed
																											  : application::review_mutation_failure::cancelled;
		}
		if (aCourse == course::unsupported || options.ReviewSubmissionServer->CliAcquisitionFailure) {
			send_single(*cards);
			const auto said = wait_until([&] { return aCourse == course::unsupported ? draft_status(*cards).contains(QStringLiteral("does not support")) : draft_status(*cards).contains(QStringLiteral("draft is kept")); }, 5000);
			std::cout << "BPR_1329:" << name << ":status=\"" << draft_status(*cards).toStdString() << "\" publish_asks=" << publishAsks << '\n';
			expect(said, "the draft says publication is unavailable and keeps its text");
			capture_settled(window, "-inline-" + QString::number(static_cast<unsigned int>(aCourse)));
			if (aCourse == course::credential_unavailable || aCourse == course::tool_failed) {
				expect(draft_status(*cards).contains(QStringLiteral("octo-reviewer")) && draft_status(*cards).contains(QStringLiteral("github.com")), "inline failure uses its failed operation's chosen account and server");
			}

			const auto after = github_application_fixture_access::review_mutation_evidence(*application);
			expect(publishAsks == 0U && after.NativePosts == before.NativePosts && after.StoredComments == 0U, "unsupported strict capability sends nothing");
			publish = draft_publish(*cards);
			expect(draft_card(*cards) != nullptr && publish != nullptr && single_offered(*cards) && publish->isEnabled()
					   && (publish->text() == QStringLiteral("Add single comment") || (aCourse == course::cancelled && publish->text() == QStringLiteral("Retry"))),
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
				  << github_application_fixture_access::held_review_mutation_terminals(*application) << '\n';
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
					  << "\" editor_status=\"" << window.review_editor()->mutation_status_text().toStdString() << "\" publish_asks=" << publishAsks << '\n';
			expect(settled, "the draft says in words that the result is unavailable and retry is safe");
			expect(publishAsks == 0U, "ordinary publication opened no CLI modal");
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
					  << " editor_enabled=" << window.review_editor()->publish_action()->isEnabled() << '\n';
			expect(publish != nullptr && single_offered(*cards) && publish->isEnabled() && publish->text() == QStringLiteral("Retry"),
				   "the draft offers Retry for the same publication");
			const auto original = *window.review_editor()->shown_draft();
			for (unsigned repeated = 0; repeated < 2; ++repeated) {
				refuse_recovery(*application, options, *cards, original, application::review_mutation_failure::credential_unavailable, QStringLiteral("credential access"));
			}
			capture_settled(window, QStringLiteral("-unknown-credential"));
			refuse_recovery(*application, options, *cards, original, application::review_mutation_failure::credential_tool_failed, QStringLiteral("could not check"));
			refuse_recovery(*application, options, *cards, original, application::review_mutation_failure::cancelled, QStringLiteral("check was cancelled"));
			refuse_recovery(*application, options, *cards, original, std::nullopt, QStringLiteral("cannot check"));
			{
				const auto lock = std::scoped_lock{ options.ReviewSubmissionServer->Mutex };
				options.ReviewSubmissionServer->CliAcquisitionFailure.reset();
			}
			options.GithubCliStrictCapability->store(true);
			send_single(*cards);
		}

		const auto shown = wait_until([&] { return posted_card(*cards, text) != nullptr && draft_card(*cards) == nullptr; });
		const auto after = github_application_fixture_access::review_mutation_evidence(*application);
		std::cout << "BPR_1329:" << name << ":publish_asks=" << publishAsks << " read_opt_ins=" << readOptIns
				  << " native_posts=" << (after.NativePosts - before.NativePosts) << " stored=" << after.StoredComments
				  << " draft_card=" << (draft_card(*cards) != nullptr) << " posted_card=" << (posted_card(*cards, text) != nullptr)
				  << " status=\"" << window.status_text().toStdString() << "\" editor_status=\"" << window.review_editor()->mutation_status_text().toStdString() << "\"\n";
		expect(publishAsks == 0U, "ordinary Publish and retry open no CLI modal");
		expect(after.NativePosts == before.NativePosts + 1U && after.StoredComments == 1U && after.LastBody == body, "the fixture server holds exactly the one comment, with the draft's text");
		expect(shown, "the drawer shows the posted thread in place of the draft");
		expect(posted_count(*cards, text) == 1U, "one posted thread, not two");
		const auto* const posted = posted_card(*cards, text);
		expect(cards->open_card() != nullptr && cards->open_card()->isVisible() && cards->open_card()->property("threadKey").toString().startsWith(QString::fromStdString(posted->Id)),
			   "the posted thread is the one open in the drawer");
		capture_settled(window, QStringLiteral("-reconciled"));
		std::cout << "BPR_1329:" << name << ":pass\n";

		consent.stop();
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "all composed hosts quiesce");
	}
}

int main(int aArgc, char** aArgv)
try {
	QApplication qt{ aArgc, aArgv };
	// One course by name, or the complete bounded fixture course.
	const auto only = aArgc > 1 ? std::string_view{ aArgv[1] } : std::string_view{ };
	for (const auto& [course, name] : { std::pair{ course::published, std::string_view{ "published" } },
										std::pair{ course::lost_then_retried, std::string_view{ "lost-then-retried" } },
										std::pair{ course::unsupported, std::string_view{ "unsupported" } },
										std::pair{ course::credential_unavailable, std::string_view{ "credential-unavailable" } },
										std::pair{ course::tool_failed, std::string_view{ "tool-failed" } },
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
