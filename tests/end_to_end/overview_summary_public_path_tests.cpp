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

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QSplitterHandle>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QThread>
#include <QTreeView>
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
	auto       server              = std::make_shared<app::detail::review_submission_fixture_state>();
	const auto body                = description();
	server->StagedDescription      = body;
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
	auto*       text     = reader->rendered_view();
	auto*       document = text->document();
	const auto  revision = document->revision();
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
	reader->show_source(true);
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
	auto*      full         = page->findChild<QPushButton*>(QStringLiteral("overviewReadDescription"));
	auto*      back         = page->findChild<QPushButton*>(QStringLiteral("overviewReturnToFiles"));
	const auto retainedFile = page->file_model()->index_for_path(QString::fromStdString(page->projection()->Files.back().Path));
	page->files_view()->setCurrentIndex(retainedFile);
	page->files_view()->scrollTo(retainedFile);
	const auto retainedFileScroll = page->files_view()->verticalScrollBar()->value();
	full->click();
	settle();
	expect(back->isVisible() && !page->files_view()->isVisible(), "composed full reader has Return to Files");
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
