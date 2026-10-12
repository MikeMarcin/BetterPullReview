// RFC0063: retained summary/Files behavior through the real composition and fake server.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAbstractScrollArea>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRect>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QSplitterHandle>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QThread>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidget>
#include <QWheelEvent>

#include <QMouseEvent>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 20000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
	void settle()
	{
		for (int turn = 0; turn < 12; ++turn) {
			QApplication::processEvents();
		}
	}
	auto description() -> std::string
	{
		auto body = std::string{ "# Preserve the reviewer's place\n\nA summary that explains the renderer changes while Files stays reachable.\n\n" };
		for (int section = 1; section <= 60; ++section) {
			body += "## Renderer case " + std::to_string(section) + "\n\nKeep the accepted document and exact file identity while resizing. 日本語 🦊\n\n- One retained body\n- Independent scrolling\n- Literal `<b>source</b>`\n\n";
		}
		return body + "END OF ADMITTED DESCRIPTION\n";
	}
	auto check_results() -> std::string
	{
		auto rows = QJsonArray{ };
		for (int index = 0; index < 24; ++index) {
			auto row = QJsonObject{ { "node_id", QStringLiteral("check_%1").arg(index) }, { "head_sha", "b20cb443428212543f5911e091a0ffeb407d23cf" }, { "name", index == 0 ? QStringLiteral("Branch Freshness") : index == 1 ? QStringLiteral("CI / Extended integration test with a long readable result name 日本語 🦊")
																																																							 : QStringLiteral("Check %1").arg(index) },
									{ "status", "completed" },
									{ "conclusion", index == 0 ? "failure" : index <= 16 ? "success"
																						 : "skipped" },
									{ "app", QJsonObject{ { "slug", "ci-fixture" } } } };
			if (index != 17) {
				row.insert("details_url", index == 18 ? "javascript:alert(1)" : "https://ci.example.invalid/details");
				row.insert("output", QJsonObject{ { "title", index == 0 ? "Branch is behind its base" : "Completed" }, { "summary", index == 0 ? "Update the branch before merging. The current head lacks the latest base changes." : "<img src=https://remote.invalid/image> is literal provider text." } });
			}
			rows.append(row);
		}
		return QJsonDocument{ QJsonObject{ { "total_count", 24 }, { "check_runs", rows } } }.toJson(QJsonDocument::Compact).toStdString();
	}
	void render(app::github_application& aApplication, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_OVERVIEW_RENDER_ROOT");
		if (!root.isEmpty()) {
			expect(QDir{ }.mkpath(root), "render directory created");
			const auto modes = std::array{ core::appearance_mode::system, core::appearance_mode::light, core::appearance_mode::dark,
										   core::appearance_mode::high_contrast_system, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark };
			for (const auto mode : modes) {
				expect(access::select_appearance_mode(aApplication, mode), "composed appearance is accepted before rendering");
				settle();
				auto& window = aApplication.window();
				expect(window.overview_page()->palette().color(QPalette::Window) == window.palette().color(QPalette::Window), "Overview inherits the accepted native palette");
				const auto path = root + QLatin1Char{ '/' } + aName + QLatin1Char{ '-' } + QString::fromUtf8(core::stable_name(mode)) + QStringLiteral(".png");
				expect(window.grab().save(path), "actual composed window render saved");
			}
			expect(access::select_appearance_mode(aApplication, core::appearance_mode::system), "render course restores System mode");
			settle();
		}
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication  qtApplication{ aArgc, aArgv };
	const auto    attended = aArgc > 1 && std::string_view{ aArgv[1] } == "--attended";
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-overview-summary-XXXXXX") };
	const auto    dataRoot = e2e::private_data_root(temporary);
	expect(dataRoot.has_value(), "private composed course root");
	auto       server         = std::make_shared<app::detail::review_submission_fixture_state>();
	const auto body           = description();
	server->StagedDescription = body;
	server->CheckRuns         = check_results();
	auto requests             = QJsonArray{ };
	for (int index = 0; index < 3; ++index) {
		requests.append(QJsonObject{ { "id", QStringLiteral("request-team-%1").arg(index) }, { "asCodeOwner", true }, { "requestedReviewer", QJsonObject{ { "__typename", "Team" }, { "id", QStringLiteral("team-%1").arg(index) }, { "slug", QStringLiteral("owners-%1").arg(index) }, { "name", index == 1 ? "International renderer and platform maintainers 日本語 🦊" : "Owner group" }, { "organization", QJsonObject{ { "login", "bpr-fixtures" } } } } } });
	}
	requests.append(QJsonObject{ { "id", "request-user" }, { "asCodeOwner", false }, { "requestedReviewer", QJsonObject{ { "__typename", "User" }, { "id", "user-new" }, { "login", "first-time-reviewer" } } } });
	requests.append(QJsonObject{ { "id", "request-again" }, { "asCodeOwner", false }, { "requestedReviewer", QJsonObject{ { "__typename", "User" }, { "id", "user-approved" }, { "login", "approved-and-requested-again" } } } });
	const auto mergeEnvelope = [](QJsonObject aFields) {
		aFields.insert("id", "PR_21");
		aFields.insert("headRefOid", "b20cb443428212543f5911e091a0ffeb407d23cf");
		aFields.insert("baseRefOid", "c58160c2ffe27d7cf5e6759d78b80f023547c789");
		aFields.insert("baseRefName", "main");
		return QJsonDocument{ QJsonObject{ { "data", QJsonObject{ { "repository", QJsonObject{ { "id", "R_nested-folders" }, { "pullRequest", aFields } } } } } } }.toJson(QJsonDocument::Compact).toStdString();
	};
	server->ReviewRequests   = mergeEnvelope(QJsonObject{ { "reviewRequests", QJsonObject{ { "totalCount", requests.size() }, { "nodes", requests }, { "pageInfo", QJsonObject{ { "hasNextPage", false }, { "endCursor", "END" } } } } } });
	server->MergeAssessment  = mergeEnvelope(QJsonObject{ { "mergeStateStatus", "BLOCKED" }, { "reviewDecision", "REVIEW_REQUIRED" }, { "autoMergeRequest", QJsonValue::Null } });
	server->BranchRules      = R"([{"type":"pull_request","ruleset_source_type":"Organization","ruleset_source":"bpr-fixtures","ruleset_id":42,"parameters":{"required_approving_review_count":2,"require_code_owner_review":true,"require_last_push_approval":true,"dismiss_stale_reviews_on_push":true,"required_review_thread_resolution":true}},{"type":"required_status_checks","ruleset_source_type":"Repository","ruleset_source":"bpr-fixtures/nested-folders","ruleset_id":43,"parameters":{"strict_required_status_checks_policy":true,"required_status_checks":[{"context":"Branch Freshness","integration_id":null},{"context":"Missing CI","integration_id":null}]}}])";
	server->BranchProtection = R"({"url":"https://api.github.com/repos/bpr-fixtures/nested-folders/branches/main/protection","required_pull_request_reviews":null,"required_status_checks":null})";
	server->ReviewHistory    = R"([{"id":4001,"node_id":"review-approved","state":"APPROVED","body":"Reviewed before re-request","commit_id":"b20cb443428212543f5911e091a0ffeb407d23cf","user":{"node_id":"user-approved","login":"approved-and-requested-again","type":"User"}}])";

	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = dataRoot;
	options.ReviewSubmissionServer = server;
	options.RepeatReadSessions     = true;
	std::unique_ptr<app::github_application> application;
	bool                                     terminal = false;
	auto                                     startup  = access::start(app::detail::github_public_path_fixture::nested_folders_v1, options, [&](auto aResult) {
		terminal = true;
		if (aResult) {
			application = std::move(*aResult);
		}
	});
	wait_for([&] { return terminal && startup->is_quiescent(); }, "composed startup terminal");
	expect(application != nullptr, "composed startup succeeds");
	application->show();
	auto& window = application->window();
	window.resize(1280, 900);
	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
	window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
	window.home_open_action()->trigger();
	auto* page = window.overview_page();
	wait_for([&] { return window.showing_overview() && page->projection() && page->body_view()->original_body() == body; }, "accepted long body appears on normal Overview");
	auto* split  = page->findChild<QSplitter*>(QStringLiteral("overviewSummarySplitter"));
	auto* grip   = split->handle(1);
	auto* reader = page->body_view();
	settle();
	expect(std::abs(split->sizes()[0] - split->sizes()[1]) <= 2 && page->files_view()->isVisible() && page->entry_button()->isVisible(), "long composed summary starts half-sized with Files and side entry");
	auto* checkDisclosure  = page->findChild<QToolButton*>(QStringLiteral("overviewChecksDisclosure"));
	auto* checkList        = page->findChild<QTreeWidget*>(QStringLiteral("overviewCheckList"));
	auto* checkExplanation = page->findChild<QPlainTextEdit*>(QStringLiteral("overviewCheckDescription"));
	auto* checkOpen        = page->findChild<QPushButton*>(QStringLiteral("overviewOpenCheck"));
	expect(page->projection()->Checks.Passed == 16 && page->projection()->Checks.Failed == 1 && page->projection()->Checks.Skipped == 7 && page->projection()->ChecksComplete && checkList->topLevelItemCount() == 25,
		   "24 current results and matching complete totals reach the ordinary composed Overview");
	expect(checkDisclosure->isChecked() && checkList->currentItem()->text(0).contains(QStringLiteral("Branch Freshness")) && checkExplanation->toPlainText().contains(QStringLiteral("Update the branch before merging")),
		   "failure automatically expands with its name, actual outcome and supplied explanation");
	auto* mergeText             = page->findChild<QLabel*>(QStringLiteral("overviewMergeSummary"));
	auto* rosterText            = page->findChild<QLabel*>(QStringLiteral("overviewPendingReviewers"));
	auto* requirementDisclosure = page->findChild<QToolButton*>(QStringLiteral("overviewRequirementDisclosure"));
	auto* requirementText       = page->findChild<QLabel*>(QStringLiteral("overviewRequirementDetails"));
	expect(mergeText->text().contains(QStringLiteral("merging is blocked")) && mergeText->text().contains(QStringLiteral("Review required")) && mergeText->text().contains(QStringLiteral("Branch Freshness")) && mergeText->text().contains(QStringLiteral("Waiting for check result: Missing CI")), "simultaneous review, known check failure and missing required result are visible beside summary");
	for (int index = 0; index < 3; ++index) {
		expect(rosterText->text().contains(QStringLiteral("@bpr-fixtures/owners-%1").arg(index)), "all three provider-marked code-owner teams are named directly");
	}
	expect(rosterText->text().contains(QStringLiteral("Waiting on code-owner review from")) && rosterText->text().contains(QStringLiteral("first-time-reviewer")) && rosterText->text().contains(QStringLiteral("approved-and-requested-again")), "new users and re-requested approver remain in distinct waiting roster");
	expect(std::ranges::any_of(page->projection()->Participants, [](const auto& aPerson) { return aPerson.Login == "approved-and-requested-again" && aPerson.Signoff == application::overview_signoff::approved; }), "prior approval remains history beside the current request");
	requirementDisclosure->setFocus();
	QKeyEvent showRequirements{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
	QApplication::sendEvent(requirementDisclosure, &showRequirements);
	QKeyEvent releaseRequirements{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier };
	QApplication::sendEvent(requirementDisclosure, &releaseRequirements);
	expect(requirementDisclosure->isChecked() && requirementText->isVisible() && requirementText->text().contains(QStringLiteral("Organization: bpr-fixtures (ruleset 42)")) && requirementText->text().contains(QStringLiteral("Auto-merge is not enabled")) && requirementText->text().contains(QStringLiteral("Remaining approvals are not established")) && rosterText->accessibleName() == rosterText->text(), "keyboard details expose exact origin and evidence limits, with matching accessible waiting names");
	render(*application, QStringLiteral("requirement-details"));
	requirementDisclosure->click();
	render(*application, QStringLiteral("review-blockers-initial"));
	const auto opens = access::browser_opens(*application);
	checkOpen->click();
	expect(access::browser_opens(*application) == opens + 1 && access::browser_last_address(*application) == "https://ci.example.invalid/details", "explicit check gesture reaches the admitted recording browser through the public host");
	const auto findCheck = [&](const QString& aIdentity) {
		for (int index = 0; index < checkList->topLevelItemCount(); ++index) {
			if (checkList->topLevelItem(index)->data(0, Qt::UserRole) == aIdentity) {
				return checkList->topLevelItem(index);
			}
		}
		return static_cast<QTreeWidgetItem*>(nullptr);
	};
	checkList->setCurrentItem(findCheck(QStringLiteral("check:check_17")));
	expect(!checkOpen->isEnabled() && checkExplanation->toPlainText().contains(QStringLiteral("No details were supplied")), "missing output and destination remain readable with unavailable action");
	checkList->setCurrentItem(findCheck(QStringLiteral("check:check_18")));
	checkOpen->click();
	expect(!checkOpen->isEnabled() && access::browser_opens(*application) == opens + 1 && checkExplanation->toPlainText().contains(QStringLiteral("<img src=https://remote.invalid/image>")), "unsafe links are refused and provider HTML remains inert selectable text");
	checkList->setCurrentItem(findCheck(QStringLiteral("check:check_0")));
	auto checkSelection = checkExplanation->textCursor();
	checkSelection.setPosition(8);
	checkSelection.setPosition(20, QTextCursor::KeepAnchor);
	checkExplanation->setTextCursor(checkSelection);
	auto* selectedCheck = checkList->currentItem();
	checkDisclosure->click();
	window.github_refresh_action()->trigger();
	wait_for([&] { return window.github_refresh_action()->isEnabled() && page->projection()->ChecksComplete; }, "check detail public refresh settles");
	expect(!checkDisclosure->isChecked() && checkList->currentItem() == selectedCheck && checkExplanation->textCursor().anchor() == 8 && checkExplanation->textCursor().position() == 20, "accepted refresh preserves collapse, exact selected check and reading selection");

	{
		const std::scoped_lock lock{ server->Mutex };
		requests.removeLast();
		requests.removeLast();
		requests.append(QJsonObject{ { "id", "replacement-request" }, { "asCodeOwner", false }, { "requestedReviewer", QJsonObject{ { "__typename", "User" }, { "id", "replacement-user" }, { "login", "replacement-reviewer" } } } });
		server->ReviewRequests = mergeEnvelope(QJsonObject{ { "reviewRequests", QJsonObject{ { "totalCount", requests.size() }, { "nodes", requests }, { "pageInfo", QJsonObject{ { "hasNextPage", false }, { "endCursor", "END" } } } } } });
		auto       history     = *server->ReviewHistory;
		const auto offset      = history.find("APPROVED");
		expect(offset != std::string::npos, "approval fixture can be dismissed");
		history.replace(offset, 8, "DISMISSED");
		server->ReviewHistory = history;
	}
	window.github_refresh_action()->trigger();
	wait_for([&] { return window.github_refresh_action()->isEnabled() && rosterText->text().contains(QStringLiteral("replacement-reviewer")); }, "accepted ordinary refresh replaces request identities");
	expect(!rosterText->text().contains(QStringLiteral("first-time-reviewer")) && !rosterText->text().contains(QStringLiteral("approved-and-requested-again")) && std::ranges::any_of(page->projection()->Participants, [](const auto& aPerson) { return aPerson.Login == "approved-and-requested-again" && aPerson.Signoff == application::overview_signoff::dismissed; }), "removed requests vanish and dismissed review history remains separate after composed refresh");
	checkDisclosure->setFocus();
	QKeyEvent showChecks{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
	QApplication::sendEvent(checkDisclosure, &showChecks);
	QKeyEvent releaseChecks{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier };
	QApplication::sendEvent(checkDisclosure, &releaseChecks);
	expect(checkDisclosure->isChecked() && checkDisclosure->accessibleName().contains(QStringLiteral("Check details")) && checkList->accessibleName() == QStringLiteral("Check results"), "native keyboard disclosure and result accessible names work");
	auto* side = page->findChild<QScrollArea*>(QStringLiteral("overviewSideScroll"));
	side->ensureWidgetVisible(checkExplanation);
	render(*application, QStringLiteral("check-details"));
	const auto originalFont = qtApplication.font();
	auto       enlarged     = originalFont;
	enlarged.setPointSizeF(originalFont.pointSizeF() * 1.35);
	qtApplication.setFont(enlarged);
	window.resize(900, 600);
	settle();
	side->ensureWidgetVisible(checkExplanation);
	expect(checkExplanation->viewport()->width() > 80 && checkOpen->minimumSizeHint().width() <= checkOpen->width() && page->entry_button()->isEnabled() && side->widget()->width() <= side->viewport()->width(), "narrow enlarged check explanation and action remain usable with review entry available");
	render(*application, QStringLiteral("check-details-enlarged"));
	side->ensureWidgetVisible(rosterText);
	settle();
	expect(rosterText->wordWrap() && rosterText->width() <= side->viewport()->width() && rosterText->height() >= rosterText->heightForWidth(rosterText->width()) && requirementDisclosure->minimumSizeHint().width() <= requirementDisclosure->width(), "narrow enlarged roster wraps long qualified team names and keeps native detail control usable");
	render(*application, QStringLiteral("review-blockers-enlarged"));
	qtApplication.setFont(originalFont);
	window.resize(1280, 900);
	settle();
	side->verticalScrollBar()->setValue(0);
	render(*application, QStringLiteral("review-blockers"));
	render(*application, QStringLiteral("long-default"));
	if (attended) {
		std::cout << "OVERVIEW_ATTENDED_READY source=synthetic window=Overview\n"
				  << std::flush;
		const auto result = QApplication::exec();
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "attended shutdown drains");
		application.reset();
		expect(e2e::remove_course_root(temporary), "attended private root cleaned");
		return result;
	}
	auto*      text     = reader->rendered_view();
	auto*      document = text->document();
	const auto revision = document->revision();
	auto*      fullView = page->findChild<QToolButton*>(QStringLiteral("overviewFullView"));
	const auto corner   = [&] {
		auto*      area     = reader->source_visible() ? static_cast<QAbstractScrollArea*>(reader->source_view()) : reader->rendered_view();
		const auto at       = area->viewport()->mapTo(page, QPoint{ area->viewport()->width(), 0 });
		const auto control  = QRect{ fullView->mapTo(page, QPoint{ }), fullView->size() };
		const auto readerAt = QRect{ reader->mapTo(page, QPoint{ }), reader->size() };
		return fullView->isVisible() && control.right() + 1 == at.x() && control.top() == at.y() && readerAt.contains(control);
	};
	expect(fullView && fullView->accessibleName() == QStringLiteral("Enter full view") && corner()
			   && reader->rendered_view()->height() == reader->height(),
		   "composed full-view control rides the reader's top-right corner and the reader reclaims action rows");
	expect(reader->mapTo(page, reader->rect().topRight()).x() == page->files_view()->mapTo(page, page->files_view()->rect().topRight()).x(),
		   "composed reader spans the full content width to the Files right edge, with no toggle column");
	text->selectAll();
	text->copy();
	expect(QApplication::clipboard()->mimeData()->data(QStringLiteral("text/plain")).toStdString() == body
			   && !QApplication::clipboard()->mimeData()->hasHtml(),
		   "composed rendered selection copies original Markdown with no HTML");
	text->setTextCursor(QTextCursor{ document });
	QWheelEvent wheel{ QPointF{ 10, 10 }, QPointF{ 10, 10 }, { }, QPoint{ 0, -120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
	QApplication::sendEvent(text->viewport(), &wheel);
	expect(text->verticalScrollBar()->value() > 0 && page->files_view()->verticalScrollBar()->value() == 0, "normal Markdown wheel scrolls independently of Files");
	text->verticalScrollBar()->setValue(text->verticalScrollBar()->maximum());
	QApplication::sendEvent(text->viewport(), &wheel);
	expect(page->files_view()->verticalScrollBar()->value() == 0, "summary boundary does not pass wheel scrolling into Files");
	QApplication::sendEvent(page->files_view()->viewport(), &wheel);
	expect(page->files_view()->verticalScrollBar()->value() > 0 && text->verticalScrollBar()->value() == text->verticalScrollBar()->maximum(), "Files scrolls without moving the summary");
	auto selection = text->textCursor();
	selection.setPosition(120);
	selection.setPosition(100, QTextCursor::KeepAnchor);
	text->setTextCursor(selection);
	grip->setFocus();
	QKeyEvent down{ QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier };
	QApplication::sendEvent(grip, &down);
	settle();
	const auto fraction = page->summary_fraction();
	expect(fraction && text->document() == document && text->textCursor().anchor() == selection.anchor() && text->textCursor().position() == selection.position(), "normal resize keeps the accepted document and directed selection");
	int resets = 0;
	QObject::connect(page->file_model(), &QAbstractItemModel::modelReset, page, [&] { ++resets; });
	window.github_refresh_action()->trigger();
	expect(!window.github_refresh_action()->isEnabled(), "Refresh admits a live read before checking completion");
	wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready && window.github_refresh_action()->isEnabled(); }, "ordinary public Refresh settles");
	settle();
	expect(page->summary_fraction() == fraction && text->document() == document && document->revision() == revision && resets == 0, "same-body public Refresh retains fraction, reader revision and Files model");
	text->customContextMenuRequested(QPoint{ 12, 12 });
	settle();
	auto* context = text->findChild<QMenu*>(QStringLiteral("markdownReaderContextMenu"));
	expect(context != nullptr, "composed reader opens its context menu");
	auto* sourceAction = context->findChild<QAction*>(QStringLiteral("markdownContextSourceToggle"));
	expect(sourceAction && sourceAction->text() == QStringLiteral("View source"), "composed source switching is in the reader context menu");
	sourceAction->trigger();
	context->close();
	settle();
	auto* source = reader->source_view();
	QApplication::sendEvent(source->viewport(), &wheel);
	expect(source->verticalScrollBar()->value() > 0, "normal source pane admits wheel navigation");
	QKeyEvent pageDown{ QEvent::KeyPress, Qt::Key_PageDown, Qt::NoModifier };
	QApplication::sendEvent(source, &pageDown);
	expect(source->verticalScrollBar()->value() > 0, "normal source pane admits page navigation");
	source->selectAll();
	source->copy();
	expect(QApplication::clipboard()->text().toStdString() == body, "complete admitted source remains selectable and copyable in normal pane");
	render(*application, QStringLiteral("source-resized"));
	expect(!QApplication::clipboard()->mimeData()->hasHtml(), "composed source copying supplies no HTML");
	auto*      full         = page->findChild<QToolButton*>(QStringLiteral("overviewFullView"));
	auto*      back         = page->findChild<QToolButton*>(QStringLiteral("overviewFullView"));
	const auto retainedFile = page->file_model()->index_for_path(QString::fromStdString(page->projection()->Files.back().Path));
	page->files_view()->setCurrentIndex(retainedFile);
	page->files_view()->scrollTo(retainedFile);
	const auto retainedFileScroll = page->files_view()->verticalScrollBar()->value();
	full->click();
	settle();
	render(*application, QStringLiteral("full-reader"));
	expect(back->isVisible() && !back->isChecked() && !back->isCheckable() && back->accessibleName() == QStringLiteral("Exit full view") && !page->files_view()->isVisible() && corner(), "composed full reader has Exit full view in the same reader corner");
	back->click();
	settle();
	expect(page->summary_fraction() == fraction && source->textCursor().hasSelection() && page->files_view()->currentIndex() == retainedFile && page->files_view()->verticalScrollBar()->value() == retainedFileScroll, "full-reader return keeps normal fraction, source selection and Files place");
	window.modality()->action(qt::review_modality::discuss)->trigger();
	window.modality()->action(qt::review_modality::overview)->trigger();
	settle();
	expect(page->summary_fraction() == fraction && reader->source_visible(), "Discuss roundtrip preserves split and reader mode");
	window.resize(900, 520);
	settle();
	expect(page->summary_fraction() == fraction && split->sizes()[0] > 0 && split->sizes()[1] > 0, "narrow composed window keeps both panes without overwriting preference");
	render(*application, QStringLiteral("narrow"));
	window.resize(1280, 900);
	reader->show_source(false);
	QMouseEvent reset{ QEvent::MouseButtonDblClick, QPointF{ grip->rect().center() }, QPointF{ grip->mapToGlobal(grip->rect().center()) }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
	QApplication::sendEvent(grip, &reset);
	settle();
	expect(!page->summary_fraction(), "native reset clears the composed preferred fraction");
	{
		const auto lock           = std::scoped_lock{ server->Mutex };
		server->StagedDescription = "A short summary with no empty reading well.";
	}
	window.github_refresh_action()->trigger();
	wait_for([&] { return reader->original_body() == "A short summary with no empty reading well."; }, "edited short body arrives through public Refresh");
	settle();
	expect(split->sizes()[0] < split->sizes()[1] && grip->isVisible() && text->textCursor().position() == 0, "short body fits naturally, keeps grip and clears old reading endpoints");
	render(*application, QStringLiteral("short"));
	{
		const auto lock           = std::scoped_lock{ server->Mutex };
		server->StagedDescription = std::string{ };
	}
	window.github_refresh_action()->trigger();
	wait_for([&] { return reader->original_body().empty(); }, "empty body arrives through public Refresh");
	settle();
	auto* empty = page->findChild<QLabel*>(QStringLiteral("overviewNoDescription"));
	expect(!grip->isVisible() && empty->isVisible() && empty->mapTo(page, empty->rect().bottomLeft()).y() < page->files_view()->mapTo(page, QPoint{ 0, 0 }).y(), "empty composed summary stays compact above Files, with no unused pane or grip");
	render(*application, QStringLiteral("empty"));
	{
		const auto lock           = std::scoped_lock{ server->Mutex };
		server->StagedDescription = body;
	}
	window.github_refresh_action()->trigger();
	wait_for([&] { return reader->original_body() == body; }, "long body reappears through accepted refresh");
	page->entry_button()->click();
	wait_for([&] { return window.current_modality() == qt::review_modality::review; }, "Start/Continue review follows composed entry semantics");
	window.modality()->action(qt::review_modality::overview)->trigger();
	settle();
	const auto actualFile = page->file_model()->index_for_path(QString::fromStdString(page->projection()->Files.front().Path));
	expect(actualFile.isValid(), "accepted file model exposes a file under its folder");
	page->files_view()->setCurrentIndex(actualFile);
	QKeyEvent enter{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
	QApplication::sendEvent(page->files_view(), &enter);
	wait_for([&] { return !window.showing_overview() && window.comparison_surface()->current_result() != nullptr; }, "file activation reaches a real composed source comparison");
	const auto mutation = access::review_mutation_evidence(*application);
	expect(mutation.Submissions == 0 && server->StagedWrites == 0 && server->Submissions == 0, "summary course performs no provider writes");
	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "composed course shuts down and drains");
	application.reset();
	expect(e2e::remove_course_root(temporary), "composed private course root cleaned");
	std::cout << "OVERVIEW_SUMMARY_PUBLIC_PATH_OK qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString() << " dpr=" << qtApplication.devicePixelRatio() << " bytes=" << body.size() << " resets=" << resets << " writes=0\n";
}
