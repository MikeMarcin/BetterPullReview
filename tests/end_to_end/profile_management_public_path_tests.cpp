#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/appearance_settings_store.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/settings_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>

namespace {
	using namespace bpr;
	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 15000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
	auto item(qt::profile_management_window& aManagement, const QString& aLabel) -> QListWidgetItem*
	{
		for (auto index = 0; index < aManagement.profiles()->count(); ++index) {
			auto* candidate = aManagement.profiles()->item(index);
			if (candidate->text() == aLabel) {
				return candidate;
			}
		}
		return nullptr;
	}
	auto create(qt::profile_management_window& aManagement, const QString& aLabel) -> core::bpr_profile_id
	{
		aManagement.name_editor()->setText(aLabel);
		expect(aManagement.create_button()->isEnabled(), "Create is reachable");
		aManagement.create_button()->click();
		wait_for([&] { return item(aManagement, aLabel) != nullptr; }, "created profile is durably projected");
		auto* created = item(aManagement, aLabel);
		aManagement.profiles()->setCurrentItem(created);
		return core::bpr_profile_id{ created->data(Qt::UserRole).toULongLong() };
	}
	auto open(app::profile_desktop& aDesktop, qt::profile_management_window& aManagement, const core::bpr_profile_id aProfile) -> app::github_application*
	{
		expect(aManagement.open_button()->isEnabled(), "Open Profile is reachable");
		aManagement.open_button()->click();
		wait_for([&] { return aDesktop.runtime(aProfile) != nullptr; }, "selected profile runtime opens");
		return aDesktop.runtime(aProfile);
	}
	void capture(qt::profile_management_window& aManagement)
	{
		const auto path = qEnvironmentVariable("BPR_PROFILE_MANAGEMENT_ARTIFACT_ROOT");
		if (path.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(path), "management artifact directory");
		QApplication::processEvents();
		expect(aManagement.grab().save(path + QStringLiteral("/profile-management.png")), "management raster");
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	QApplication qtApplication{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private desktop test root");
	const auto root = std::filesystem::path{ QFileInfo{ temporary.path() }.canonicalFilePath().toStdString() };
	{
		auto store = adapters::sqlite::make_appearance_settings_store({ .StorageRoot = root });
		expect(store.has_value(), "legacy appearance store");
		auto settings                        = application::appearance_settings{ };
		settings.Mode                        = core::appearance_mode::dark;
		settings.Typography.Ui.Font.Family   = "Sans Serif";
		settings.Typography.Code.Font.Family = "Monospace";
		expect((*store)->store(settings, { }).has_value(), "tuned Default fixture");
	}
	app::github_application* initial  = nullptr;
	auto                     terminal = false;
	auto                     options  = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot       = root;
	auto desktop                      = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				initial = *aResult;
			}
		});
	expect(!terminal, "desktop startup remains queued");
	wait_for([&] { return terminal; }, "desktop initial startup");
	expect(initial != nullptr, "initial runtime admitted");
	initial->show();
	wait_for([&] { return initial->status().ProfileMode == core::appearance_mode::dark; }, "Default preserves tuned preferences");
	auto* manage = initial->window().findChild<QAction*>(QStringLiteral("manageProfilesAction"));
	expect(manage != nullptr, "profile menu exposes management");
	wait_for([&] { return manage->isEnabled(); }, "profile menu becomes available");
	manage->trigger();
	wait_for([&] { return desktop->management_window() != nullptr && desktop->management_window()->isVisible(); }, "modeless management opens from real menu");
	auto& management = *desktop->management_window();
	wait_for([&] { return management.create_button()->isEnabled(); }, "registry is ready for management");
	const auto workId = create(management, QStringLiteral("Work"));
	expect(workId != core::default_bpr_profile_id, "new profile has distinct durable identity");
	auto* work = open(*desktop, management, workId);
	wait_for([&] { return work->status().AppearancePersistenceAvailable; }, "Work profile settings composed");
	expect(work->status().ProfileMode == core::appearance_mode::system, "new profile begins with standard preferences");
	expect(initial->status().ProfileMode == core::appearance_mode::dark, "new profile does not replace Default");
	const auto stableRow = QPersistentModelIndex{ management.profiles()->model()->index(management.profiles()->currentRow(), 0) };
	management.name_editor()->setText(QStringLiteral("Work renamed"));
	management.rename_button()->click();
	wait_for([&] { return work->window().windowTitle().contains(QStringLiteral("Work renamed")); }, "accepted rename reaches open chrome");
	expect(stableRow.isValid() && stableRow.data().toString() == QStringLiteral("Work renamed"), "rename preserves semantic model index");
	management.open_button()->click();
	QApplication::processEvents();
	expect(desktop->runtime(workId) == work, "reopening live profile activates its existing runtime");
	capture(management);

	const auto writes = work->status().AppearanceDurableWrites;
	work->window().open_settings(qt::settings_page::appearance);
	auto* mode = work->window().appearance_settings()->mode_selector();
	mode->setCurrentIndex(mode->findData(static_cast<int>(core::appearance_mode::high_contrast_dark)));
	wait_for([&] { return work->status().AppearanceDurableWrites > writes; }, "Work preference persists through real Settings");
	initial->window().close();
	wait_for([&] { return desktop->runtime(core::default_bpr_profile_id) == nullptr; }, "closed initial profile drains and retires");
	expect(!work->status().Closing, "closing initial profile leaves Work usable");
	management.present();
	const auto personalId = create(management, QStringLiteral("Personal"));
	auto*      personal   = open(*desktop, management, personalId);
	expect(personal != work, "desktop owner remains usable after initial profile closes");
	work->window().close();
	wait_for([&] { return desktop->runtime(workId) == nullptr; }, "Work closes before owner reuse");
	management.profiles()->setCurrentItem(item(management, QStringLiteral("Work renamed")));
	work = open(*desktop, management, workId);
	wait_for([&] { return work->status().ProfileMode == core::appearance_mode::high_contrast_dark; }, "renamed Work restores its private preferences");
	auto* quit = work->window().findChild<QAction*>(QStringLiteral("quitProfilesAction"));
	expect(quit != nullptr, "native Quit action is composed");
	quit->trigger();
	wait_for([&] { return desktop->is_quiescent(); }, "desktop Quit drains every profile and startup");
	desktop.reset();
	for (const auto& logRoot : { root, root / "profiles" / ("profile-" + std::to_string(workId.value())), root / "profiles" / ("profile-" + std::to_string(personalId.value())) }) {
		std::ifstream     stream{ logRoot / "logs/diagnostics.log" };
		const std::string text{ std::istreambuf_iterator<char>{ stream }, { } };
		expect(text.find("op=application outcome=started") != std::string::npos
				   && text.find("op=application outcome=shutdown") != std::string::npos,
			   "each concurrent profile retains its own startup and shutdown diagnostics");
	}

	// Closing the last visible management surface must drain the desktop too.
	initial  = nullptr;
	terminal = false;
	desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				initial = *aResult;
			}
		});
	wait_for([&] { return terminal; }, "second desktop startup");
	expect(initial != nullptr, "second desktop admits Default");
	initial->show();
	wait_for([&] { return desktop->management_window() != nullptr; }, "desktop management composed");
	initial->window().close();
	wait_for([&] { return desktop->runtime(core::default_bpr_profile_id) == nullptr && desktop->management_window()->isVisible(); }, "last profile retirement presents management");
	desktop->management_window()->close();
	wait_for([&] { return desktop->is_quiescent(); }, "last management close drains invisible desktop owner");
	desktop.reset();
	std::cout << "profile-management-public-path:pass\n";
	return EXIT_SUCCESS;
}
