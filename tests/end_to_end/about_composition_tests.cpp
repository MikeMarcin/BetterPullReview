// [[BPR-PENDING-AA]]. Help › About as the composition builds it: the facts it
// reads for this build (version, configuration, the recorded commit, the
// running Qt), the installed legal folder and which of its documents count as
// installed (regular files only, never a link or a folder), and the menu item
// on a window's bar, whose links reach the platform launch through the core's
// reference pages and the system browser opener with exactly the admitted
// address. The launch is the tests-only substitute: no browser opens.
#include "../support/test_expectation.hpp"
#include "about_desktop.hpp"
#include "system_browser_opener.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QUrl>
#include <QtVersion>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

#if !defined(BPR_TEST_PROJECT_VERSION) || !defined(BPR_TEST_CONFIGURATION)
	#error "tests/end_to_end/CMakeLists.txt gives this file the project version and configuration"
#endif

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	std::vector<QString>* Launched = nullptr;

	auto launch(const QUrl& aAddress) -> bool
	{
		if (Launched != nullptr) {
			Launched->push_back(aAddress.toString());
		}
		return true;
	}

	void touch(const std::filesystem::path& aPath)
	{
		auto file = QFile{ QString::fromStdU16String(aPath.u16string()) };
		expect(file.open(QIODevice::WriteOnly) && file.write("notice\n") == 7, "a scratch document is written");
	}

	void test_facts()
	{
		const auto folder = app::installed_legal_folder();
#if defined(Q_OS_MACOS)
		expect(folder.filename() == "ThirdParty" && folder.parent_path().filename() == "Resources", "on macOS the documents are in the bundle's Resources/ThirdParty");
#else
		expect(folder.filename() == "ThirdParty", "elsewhere they are in ThirdParty");
#endif
		expect(folder.parent_path().parent_path() == std::filesystem::path{ QCoreApplication::applicationDirPath().toStdU16String() }.parent_path()
				   || folder.parent_path() == std::filesystem::path{ QCoreApplication::applicationDirPath().toStdU16String() },
			   "next to the running executable");

		auto scratch = QTemporaryDir{ };
		expect(scratch.isValid(), "a scratch folder is made");
		const auto root  = std::filesystem::path{ scratch.path().toStdU16String() };
		auto       facts = app::about_facts_of_this_build(root);
		expect(facts.Version == QStringLiteral(BPR_TEST_PROJECT_VERSION), "the version is the project's");
		expect(facts.Configuration == QStringLiteral(BPR_TEST_CONFIGURATION), "the configuration is the build's");
		expect(facts.QtVersion == QString::fromLatin1(qVersion()) && !facts.System.isEmpty(), "Qt is the running one and the system is named");
		expect(facts.Copyright == QStringLiteral("© 2026 BetterPullReview contributors."), "the copyright is the licence's");
		const auto commit = QRegularExpression{ QStringLiteral("^[0-9a-f]{40}$") };
		const auto date   = QRegularExpression{ QStringLiteral("^[0-9]{4}-[0-9]{2}-[0-9]{2}$") };
		expect((facts.Commit.isEmpty() && facts.CommitDate.isEmpty()) || (commit.match(facts.Commit).hasMatch() && date.match(facts.CommitDate).hasMatch()),
			   "the recorded commit is a whole SHA-1 with its date, or nothing");
		expect(!facts.NoticesInstalled && !facts.SourceOfferInstalled && facts.LegalFolder.isEmpty(), "an empty folder installs neither document");

		touch(root / "THIRD-PARTY-NOTICES.txt");
		touch(root / "SOURCE-OFFERS.txt");
		facts = app::about_facts_of_this_build(root);
		expect(facts.NoticesInstalled && facts.SourceOfferInstalled && facts.LegalFolder == QDir::toNativeSeparators(scratch.path()),
			   "both regular files count, and the folder is named as a reader types it");

		auto linked = QTemporaryDir{ };
		expect(linked.isValid(), "a second scratch folder is made");
		const auto other = std::filesystem::path{ linked.path().toStdU16String() };
		auto       error = std::error_code{ };
		std::filesystem::create_symlink(root / "THIRD-PARTY-NOTICES.txt", other / "THIRD-PARTY-NOTICES.txt", error);
		std::filesystem::create_directory(other / "SOURCE-OFFERS.txt", error);
		facts = app::about_facts_of_this_build(other);
		expect(!facts.NoticesInstalled && !facts.SourceOfferInstalled, "a link to the notices and a folder named like the offer count as neither");
	}

	void test_menu_reaches_the_browser()
	{
		auto launched        = std::vector<QString>{ };
		Launched             = &launched;
		const auto  previous = app::detail::substitute_browser_launch(&launch);
		auto        menus    = app::make_about_menu();
		auto        window   = QMainWindow{ };
		auto* const help     = window.menuBar()->addMenu(QStringLiteral("&Help"));
		help->setObjectName(QStringLiteral("releaseUpdateMenu"));
		menus->attach(*window.menuBar());
		window.show();
		auto* const about = help->actions().back();
		expect(about->objectName() == QStringLiteral("aboutAction") && about->menuRole() == QAction::AboutRole, "the composed menu adds About to Help");
		about->trigger();
		QApplication::processEvents();
		auto* const page = dynamic_cast<qt::about_window*>(window.findChild<QDialog*>(QStringLiteral("aboutWindow"), Qt::FindDirectChildrenOnly));
		expect(page != nullptr && page->isVisible(), "and the item shows the page");
		expect(page->facts().Version == QStringLiteral(BPR_TEST_PROJECT_VERSION), "with this build's facts");
		// A tests-enabled build tree installs no documents beside its executables.
		expect(page->documents_absent_label()->isVisible(), "and, built from source, no installed documents");
		page->page_button(core::reference_page::source_repository)->click();
		page->page_button(core::reference_page::game_core_tech)->click();
		static_cast<void>(app::detail::substitute_browser_launch(previous));
		Launched = nullptr;
		expect(launched == std::vector<QString>{ QStringLiteral("https://github.com/MikeMarcin/BetterPullReview"), QStringLiteral("https://gamecoretech.com/") },
			   "Source code and gamecoretech.com reach the platform launch with exactly their admitted addresses, once each");
		expect(page->status_label()->isHidden(), "and the page says nothing when each opened");
		page->hide();
	}
}

auto main(int argc, char** argv) -> int
try {
	auto application = QApplication{ argc, argv };
	test_facts();
	test_menu_reaches_the_browser();
	std::cout << "about-composition-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
