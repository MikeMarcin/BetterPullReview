/// \file
/// \brief [[BPR-2282]]. A file dropped onto the shipped window opens the
/// import dialog, preloaded, through the real composition.
///
/// The owner, 2026-11-19: "Can we make it so that if you drag and drop a
/// setting profile export file onto the app window it brings up the import
/// dialog for it." Over returning-review-v1, drag events carrying a file are
/// sent to the window as Qt delivers them. A Profile Settings file with an
/// agent opens Import Profile Settings over the window with that agent; Cancel
/// imports nothing. A file holding a prompt pack opens it with its prompt
/// Added; a second drop while the dialog is open opens nothing; Cancel, and a
/// drop again still says Added (nothing was imported); Import, and a drop
/// again says Unchanged. A file that is not Profile Settings says why in the
/// status bar and opens nothing; two files are refused.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/core/agent_configuration.hpp"
#include "bpr/core/prompt_pack.hpp"
#include "bpr/qt/agent_configuration_dialogs.hpp"
#include "bpr/qt/window_file_drop.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QMimeData>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QUrl>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	template <typename Predicate>
	void wait(Predicate aPredicate, const std::string& aMessage)
	{
		if (!e2e::wait_until_plain(aPredicate, e2e::default_agent_budget)) {
			throw std::runtime_error{ aMessage };
		}
	}

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			throw std::runtime_error{ aMessage };
		}
	}

	void write(const QString& aPath, const std::string& aBytes)
	{
		auto file = QFile{ aPath };
		expect(file.open(QIODevice::WriteOnly) && file.write(aBytes.data(), static_cast<qint64>(aBytes.size())) == static_cast<qint64>(aBytes.size()), "a test file is written");
	}

	/// Enter, move and drop, as Qt delivers a drag over the window; whether
	/// the window took the drag.
	auto drag_and_drop(QWidget& aWindow, const QStringList& aPaths) -> bool
	{
		auto mime = QMimeData{ };
		auto urls = QList<QUrl>{ };
		for (const auto& path : aPaths) {
			urls.push_back(QUrl::fromLocalFile(path));
		}
		mime.setUrls(urls);
		const auto at      = aWindow.rect().center();
		const auto actions = Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
		auto       enter   = QDragEnterEvent{ QPointF{ at }, actions, &mime, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(&aWindow, &enter);
		if (!enter.isAccepted()) {
			return false;
		}
		auto move = QDragMoveEvent{ QPointF{ at }, actions, &mime, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(&aWindow, &move);
		auto drop = QDropEvent{ QPointF{ at }, actions, &mime, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(&aWindow, &drop);
		QApplication::processEvents();
		return drop.isAccepted();
	}

	[[nodiscard]] auto open_dialogs() -> QList<qt::agent_configuration_import_dialog*>
	{
		auto found = QList<qt::agent_configuration_import_dialog*>{ };
		for (auto* const widget : QApplication::topLevelWidgets()) {
			if (auto* const dialog = dynamic_cast<qt::agent_configuration_import_dialog*>(widget); dialog != nullptr && dialog->isVisible()) {
				found.push_back(dialog);
			}
		}
		return found;
	}

	[[nodiscard]] auto dialog_opens(const std::string& aMessage) -> qt::agent_configuration_import_dialog*
	{
		wait([] { return open_dialogs().size() == 1; }, aMessage);
		return open_dialogs().front();
	}

	void cancel(qt::agent_configuration_import_dialog& aDialog)
	{
		aDialog.reject();
		wait([] { return open_dialogs().isEmpty(); }, "Cancel closes the dialog");
	}

	constexpr auto pack_id   = std::string_view{ "0f8e4c2a-1b3d-4e5f-8a9b-0c1d2e3f4a5b" };
	constexpr auto prompt_id = std::string_view{ "11111111-1111-4111-8111-111111111111" };

	[[nodiscard]] auto prompt_pack_file() -> std::string
	{
		auto configuration       = core::agent_configuration{ };
		configuration.ExportedBy = "BetterPullReview";
		configuration.PromptPack = { .Id = std::string{ pack_id }, .Label = "Team prompts", .Version = 1U };
		auto body                = core::prompt_macro_body{ };
		body.Name                = "Look again";
		body.Template            = "Look at {pr.title} again.";
		configuration.PromptMacros.push_back(core::portable_prompt_macro{ .Id = std::string{ prompt_id }, .Place = core::prompt_place::agent_tab, .Body = body });
		auto bytes = core::encode_agent_configuration(configuration);
		expect(bytes.has_value(), "the prompt pack file encodes");
		return *bytes;
	}

	[[nodiscard]] auto pack_standing(const qt::agent_configuration_import_dialog& aDialog) -> core::prompt_pack_standing
	{
		expect(aDialog.preview().File.PromptPack.has_value() && aDialog.pack_plan().Items.size() == 1U, "the dialog holds the file's one-prompt pack");
		return aDialog.pack_plan().Items.front().Standing;
	}

	void course(app::github_application& aApplication, const QTemporaryDir& aFiles)
	{
		auto&       window = aApplication.window();
		auto* const drop   = window.findChild<QObject*>(QStringLiteral("windowFileDrop"), Qt::FindDirectChildrenOnly);
		expect(drop != nullptr && window.acceptDrops(), "the window takes file drops");

		// A Profile Settings export with an agent.
		const auto settings = aFiles.filePath(QStringLiteral("team.bprsettings"));
		auto       agent    = core::portable_agent{ };
		agent.PortableId    = "team-claude";
		agent.Name          = "Team Claude";
		agent.ProgramName   = "claude";
		auto shared         = core::agent_configuration{ };
		shared.ExportedBy   = "BetterPullReview";
		shared.Agents.push_back(agent);
		const auto sharedBytes = core::encode_agent_configuration(shared);
		expect(sharedBytes.has_value(), "the settings file encodes");
		write(settings, *sharedBytes);
		expect(drag_and_drop(window, { settings }), "the window takes one dropped file");
		auto* dialog = dialog_opens("dropping a Profile Settings file opens the import dialog");
		expect(dialog->parentWidget() == &window, "the dialog opens over the window the file was dropped on");
		expect(dialog->preview().File.Agents.size() == 1U && dialog->preview().File.Agents.front().Agent.Name == "Team Claude", "the dialog is preloaded with the dropped file");
		cancel(*dialog);

		// A prompt pack: Added; a drop while the dialog is open opens nothing.
		const auto pack = aFiles.filePath(QStringLiteral("prompts.json"));
		write(pack, prompt_pack_file());
		expect(drag_and_drop(window, { pack }), "the window takes the prompt pack");
		dialog = dialog_opens("dropping a prompt pack opens the import dialog");
		expect(pack_standing(*dialog) == core::prompt_pack_standing::added, "the pack's prompt is new here");
		expect(!drag_and_drop(window, { settings }), "a drop while the dialog is open is refused");
		for (auto turn = 0; turn < 50; ++turn) {
			QApplication::processEvents();
		}
		expect(open_dialogs().size() == 1, "a drop while the dialog is open opens no second dialog");
		cancel(*dialog);

		// Cancel imported nothing: the prompt is still new.
		expect(drag_and_drop(window, { pack }), "the pack is dropped again");
		dialog = dialog_opens("the pack's dialog opens again");
		expect(pack_standing(*dialog) == core::prompt_pack_standing::added, "Cancel imported nothing");
		// Import: now it is here.
		expect(dialog->apply_button() != nullptr && dialog->apply_button()->isEnabled(), "Import is offered");
		dialog->apply_button()->click();
		wait([&] { return window.statusBar()->currentMessage().startsWith(QStringLiteral("Imported into this profile")); }, "the window says the import landed");
		wait([] { return open_dialogs().isEmpty(); }, "the dialog closes on Import");
		expect(drag_and_drop(window, { pack }), "the pack is dropped once more");
		dialog = dialog_opens("the pack's dialog opens once more");
		expect(pack_standing(*dialog) == core::prompt_pack_standing::unchanged, "the confirmed import stored the prompt");
		cancel(*dialog);

		// A file that is not Profile Settings: why, in the status bar.
		const auto notes = aFiles.filePath(QStringLiteral("notes.bprsettings"));
		write(notes, R"({"name":"just some JSON"})");
		window.statusBar()->clearMessage();
		expect(drag_and_drop(window, { notes }), "an unknown file is taken: what it is, only its content says");
		wait([&] { return !window.statusBar()->currentMessage().isEmpty(); }, "the window says why the file was not opened");
		const auto said = window.statusBar()->currentMessage();
		std::cout << "WINDOW_FILE_DROP:unknown=" << said.toStdString() << '\n';
		expect(said.contains(QStringLiteral("not a BetterPullReview Profile Settings file")) && said.endsWith(QStringLiteral("Nothing was imported.")), "the word names why");
		expect(open_dialogs().isEmpty(), "an unknown file opens nothing");

		// [[BPR-2283]] (owner: "don't keep legacy support"). A .bprtheme theme
		// and a version 1 Profile Settings export are not Profile Settings.
		for (const auto& [name, bytes] : { std::pair{ QStringLiteral("old.bprtheme"), std::string{ "bpr-theme 2\nbase dark\ncode_background #000000ff\n" } },
										   std::pair{ QStringLiteral("old.bpr-config.json"), std::string{ R"({"format":"bpr.profile-settings","version":1,"exported_by":"x","agents":[],"prompts":[]})" } } }) {
			const auto legacy = aFiles.filePath(name);
			write(legacy, bytes);
			window.statusBar()->clearMessage();
			expect(drag_and_drop(window, { legacy }), "a legacy file is taken: only its content says what it is");
			wait([&] { return !window.statusBar()->currentMessage().isEmpty(); }, "the window says why the legacy file was not opened");
			expect(window.statusBar()->currentMessage() == QStringLiteral("The file is not a BetterPullReview Profile Settings file. Nothing was imported."), "a legacy file is not Profile Settings");
			expect(open_dialogs().isEmpty(), "a legacy file opens nothing");
		}

		// Two files: refused at the door.
		window.statusBar()->clearMessage();
		expect(!drag_and_drop(window, { settings, pack }), "two files are refused");
		expect(window.statusBar()->currentMessage() == qt::window_file_drop::several_files_text(), "the window says to drop one file at a time");
		for (auto turn = 0; turn < 50; ++turn) {
			QApplication::processEvents();
		}
		expect(open_dialogs().isEmpty(), "two files open nothing");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication  qtApplication{ argc, argv };
	QTemporaryDir data;
	QTemporaryDir files;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot || !files.isValid()) {
		std::cerr << "private data root refused\n";
		return EXIT_FAILURE;
	}
	std::unique_ptr<app::github_application> application;
	try {
		auto options       = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
		options.EpochPhase = app::detail::github_epoch_fixture_phase::reuse;
		auto terminal      = false;
		auto startup       = access::start(app::detail::github_public_path_fixture::returning_review_v1, std::move(options), [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
		expect(application != nullptr, "startup succeeds");
		application->show();
		application->window().resize(1280, 860);
		course(*application, files);
		application->begin_shutdown();
		wait([&] { return application->status().ShutdownComplete; }, "shutdown");
	} catch (const std::exception& aError) {
		std::cerr << "FAILED: " << aError.what() << '\n';
		if (application) {
			application->begin_shutdown();
			static_cast<void>(e2e::wait_until_plain([&] { return application->status().ShutdownComplete; }, e2e::default_agent_budget));
		}
		return EXIT_FAILURE;
	}
	std::cout << "window-file-drop-public-path:pass\n";
	return EXIT_SUCCESS;
}
