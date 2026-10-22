// [[BPR-1720]]. Help › About BetterPullReview: the Game Core Tech logo
// drawn at the device's own pixel size, the name, version and build line,
// Copy version info and what it copies, the maker and source links and the
// installed legal documents through the host's actions (and what the page
// says when one does not open), the absent-documents sentence of a build made
// from source, the menu item each bar gains, keyboard traversal and
// activation, and contrast in the four built-in themes. With
// --render-directory it saves the page in Light and Dark at the run's scale.
#include "../support/test_expectation.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/about_window.hpp"
#include "bpr/qt/appearance_profile.hpp"

#include <QAbstractButton>
#include <QAccessible>
#include <QAccessibleEvent>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	void settle(const int aMilliseconds = 40)
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		do {
			QApplication::processEvents(QEventLoop::AllEvents, 10);
			QThread::msleep(2);
		} while (timer.elapsed() < aMilliseconds);
	}

	std::vector<QString>* Announcements = nullptr;

	void capture(QAccessibleEvent* const aEvent)
	{
		if (Announcements != nullptr && aEvent != nullptr && aEvent->type() == QAccessible::Announcement) {
			Announcements->push_back(static_cast<const QAccessibleAnnouncementEvent*>(aEvent)->message());
		}
	}

	[[nodiscard]] auto released_facts() -> qt::about_facts
	{
		auto facts                 = qt::about_facts{ };
		facts.Version              = QStringLiteral("0.14.0");
		facts.Commit               = QStringLiteral("0123456789abcdef0123456789abcdef01234567");
		facts.CommitDate           = QStringLiteral("2026-10-21");
		facts.Configuration        = QStringLiteral("Release");
		facts.QtVersion            = QStringLiteral("6.12.0");
		facts.System               = QStringLiteral("macOS 26.0 (arm64)");
		facts.Copyright            = QStringLiteral("© 2026 BetterPullReview contributors.");
		facts.LegalFolder          = QStringLiteral("/Applications/BetterPullReview.app/Contents/Resources/ThirdParty");
		facts.NoticesInstalled     = true;
		facts.SourceOfferInstalled = true;
		return facts;
	}

	[[nodiscard]] auto source_build_facts() -> qt::about_facts
	{
		auto facts                 = released_facts();
		facts.Commit               = QString{ };
		facts.CommitDate           = QString{ };
		facts.Configuration        = QStringLiteral("Debug");
		facts.LegalFolder          = QString{ };
		facts.NoticesInstalled     = false;
		facts.SourceOfferInstalled = false;
		return facts;
	}

	/// What the page asked its host, and what the host answers.
	struct host
	{
		std::vector<core::reference_page> Pages;
		std::vector<qt::about_document>   Documents;
		core::markdown_link_result        PageResult     = core::markdown_link_result::opened;
		bool                              DocumentResult = true;

		[[nodiscard]] auto actions() -> qt::about_actions
		{
			auto actions     = qt::about_actions{ };
			actions.OpenPage = [this](const core::reference_page aPage) {
				Pages.push_back(aPage);
				return PageResult;
			};
			actions.OpenDocument = [this](const qt::about_document aDocument) {
				Documents.push_back(aDocument);
				return DocumentResult;
			};
			return actions;
		}
	};

	void press(QWidget* const aTarget, const int aKey)
	{
		auto down = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier, aKey == Qt::Key_Space ? QStringLiteral(" ") : QString{ } };
		QApplication::sendEvent(aTarget, &down);
		auto up = QKeyEvent{ QEvent::KeyRelease, aKey, Qt::NoModifier, aKey == Qt::Key_Space ? QStringLiteral(" ") : QString{ } };
		QApplication::sendEvent(aTarget, &up);
		settle();
	}

	void test_content_and_copy()
	{
		auto       backend = host{ };
		auto       page    = qt::about_window{ released_facts(), backend.actions() };
		const auto facts   = released_facts();
		expect(page.windowTitle() == QStringLiteral("About BetterPullReview") && page.accessibleName() == page.windowTitle(), "the window is titled and named About BetterPullReview");
		expect(page.version_label()->text() == QStringLiteral("Version 0.14.0"), "the version reads Version 0.14.0");
		expect(page.build_label()->text() == QStringLiteral("Commit 0123456789 from 2026-10-21"), "the build line names the short commit and its date");
		expect(page.build_label()->toolTip() == facts.Commit, "and its tooltip is the whole commit");
		auto changed         = facts;
		changed.LocalChanges = true;
		expect(qt::about_build_line(changed) == QStringLiteral("Commit 0123456789 from 2026-10-21, with local changes"), "a build with local changes says so");
		expect(page.logo()->accessibleName() == QStringLiteral("Game Core Tech logo"), "the logo has an accessible name");

		const auto report = qt::about_version_report(facts);
		expect(report
				   == QStringLiteral("BetterPullReview 0.14.0\nCommit: 0123456789abcdef0123456789abcdef01234567 (2026-10-21)\nConfiguration: Release\nQt: 6.12.0\nSystem: macOS 26.0 (arm64)\n"),
			   "the version report is one fact a line: version, whole commit and date, configuration, Qt and system");
		expect(qt::about_version_report(source_build_facts()).contains(QStringLiteral("Commit: unavailable\n")), "a build with no commit reports it unavailable");

		auto said           = std::vector<QString>{ };
		Announcements       = &said;
		const auto previous = QAccessible::installUpdateHandler(capture);
		QApplication::clipboard()->setText(QStringLiteral("before"));
		page.show();
		settle();
		page.copy_button()->click();
		settle();
		static_cast<void>(QAccessible::installUpdateHandler(previous));
		Announcements = nullptr;
		expect(QApplication::clipboard()->text() == report, "Copy version info puts exactly the report on the clipboard");
		expect(page.copy_button()->text() == QStringLiteral("Copied"), "and the button says Copied");
		expect(said == std::vector<QString>{ QStringLiteral("Version info copied") }, "and the copy is announced once");
		settle(2200);
		expect(page.copy_button()->text() == QStringLiteral("Copy version info"), "and returns to Copy version info");
		expect(backend.Pages.empty() && backend.Documents.empty(), "copying opens nothing");
		page.hide();
	}

	void test_links_and_documents()
	{
		auto backend = host{ };
		auto page    = qt::about_window{ released_facts(), backend.actions() };
		page.show();
		settle();
		auto* const product = page.page_button(core::reference_page::product_site);
		auto* const maker   = page.page_button(core::reference_page::game_core_tech);
		auto* const source  = page.page_button(core::reference_page::source_repository);
		auto* const notices = page.document_button(qt::about_document::third_party_notices);
		auto* const offer   = page.document_button(qt::about_document::source_offer);
		expect(product != nullptr && product->text() == QStringLiteral("betterpullreview.com") && product->isVisible(), "the product's site is shown");
		expect(maker != nullptr && maker->text() == QStringLiteral("gamecoretech.com") && maker->isVisible(), "the maker's link is shown");
		expect(source != nullptr && source->text() == QStringLiteral("Source code") && source->isVisible(), "the source link is shown");
		{
			// Owner amendment, 2026-10-21: betterpullreview.com first, Source
			// code beside it, then "Made by Game Core Tech" with its site.
			const auto  at       = [&page](const QWidget* const aWidget) { return aWidget->mapTo(&page, QPoint{ 0, 0 }); };
			auto* const credit   = page.findChild<QLabel*>(QStringLiteral("aboutMaker"));
			const auto  sameLine = at(product).y() == at(source).y();
			expect(sameLine && at(product).x() + product->width() <= at(source).x(), "betterpullreview.com comes first, with Source code beside it");
			expect(credit != nullptr && credit->text() == QStringLiteral("Made by Game Core Tech") && at(credit).y() >= at(source).y() + source->height(),
				   "Made by Game Core Tech comes under them");
			expect(credit != nullptr && at(maker).y() >= at(credit).y() + credit->height(), "with gamecoretech.com under the credit");
		}
		expect(page.page_button(core::reference_page::conventional_comments) == nullptr, "and no other page has a button");
		expect(notices != nullptr && notices->text() == QStringLiteral("Third-party notices") && notices->isVisible(), "a release shows its third-party notices");
		expect(offer != nullptr && offer->text() == QStringLiteral("Corresponding source") && offer->isVisible(), "and its corresponding-source offer");
		expect(page.documents_absent_label()->isHidden(), "and not the sentence for a build without them");
		const auto lgpl = page.findChild<QLabel*>(QStringLiteral("aboutQtLicence"));
		expect(lgpl != nullptr && lgpl->text().contains(QStringLiteral("Qt 6.12.0 is used under the GNU LGPL version\u00a03; you may replace it.")), "the page states Qt's version and licence");
		const auto copyright = page.findChild<QLabel*>(QStringLiteral("aboutCopyright"));
		expect(copyright != nullptr && copyright->text() == QStringLiteral("© 2026 BetterPullReview contributors. Released under the MIT License."), "and the copyright and the project's licence");

		product->click();
		source->click();
		maker->click();
		notices->click();
		offer->click();
		settle();
		expect(backend.Pages == std::vector<core::reference_page>{ core::reference_page::product_site, core::reference_page::source_repository, core::reference_page::game_core_tech },
			   "each link names its page to the host, once, in order");
		expect(backend.Documents == std::vector<qt::about_document>{ qt::about_document::third_party_notices, qt::about_document::source_offer },
			   "and each document button names its document");
		expect(page.status_label()->isHidden(), "nothing is said while each opened");

		backend.PageResult = core::markdown_link_result::failed;
		source->click();
		settle();
		expect(page.status_label()->isVisible()
				   && page.status_label()->text() == QStringLiteral("The browser could not open github.com/MikeMarcin/BetterPullReview. Open that address yourself."),
			   "a browser that fails is named with the address to open");
		backend.PageResult = core::markdown_link_result::unavailable;
		maker->click();
		settle();
		expect(page.status_label()->text() == QStringLiteral("There is no browser to open gamecoretech.com with."), "no browser is said as such");
		product->click();
		settle();
		expect(page.status_label()->text() == QStringLiteral("There is no browser to open betterpullreview.com with."), "and names the product's site for its link");
		backend.DocumentResult = false;
		notices->click();
		settle();
		expect(page.status_label()->text() == QStringLiteral("The third-party notices did not open. They are in /Applications/BetterPullReview.app/Contents/Resources/ThirdParty."),
			   "a document that does not open names the folder it is in");
		backend.PageResult = core::markdown_link_result::opened;
		maker->click();
		settle();
		expect(page.status_label()->isHidden(), "and the line goes once a link opens");

		auto quiet = qt::about_window{ released_facts(), qt::about_actions{ } };
		quiet.page_button(core::reference_page::source_repository)->click();
		expect(quiet.status_label()->text() == QStringLiteral("There is no browser to open github.com/MikeMarcin/BetterPullReview with."), "a page with no host action opens nothing and says so");
		page.hide();
	}

	void test_build_from_source()
	{
		auto backend = host{ };
		auto page    = qt::about_window{ source_build_facts(), backend.actions() };
		page.show();
		settle();
		expect(page.build_label()->text() == QStringLiteral("No commit is recorded for this build") && page.build_label()->toolTip().isEmpty(),
			   "a build with no commit says why");
		expect(page.document_button(qt::about_document::third_party_notices)->isHidden() && page.document_button(qt::about_document::source_offer)->isHidden(),
			   "a build without installed documents offers no button for them");
		expect(page.documents_absent_label()->isVisible() && page.documents_absent_label()->text() == QStringLiteral("Built from source, so no notices or source offer are installed."),
			   "and says where they come from instead");
		page.hide();
	}

	void test_keyboard()
	{
		auto backend = host{ };
		auto owner   = QWidget{ };
		owner.resize(200, 100);
		owner.show();
		auto* const page = new qt::about_window{ released_facts(), backend.actions(), &owner };
		page->show();
		page->activateWindow();
		settle(80);
		expect(QApplication::focusWidget() == page->close_button(), "Close has the keyboard when the page opens");
		expect(page->close_button()->isDefault(), "and Return closes");
		const auto order   = std::vector<QWidget*>{ page->page_button(core::reference_page::product_site), page->page_button(core::reference_page::source_repository), page->page_button(core::reference_page::game_core_tech), page->copy_button(),
													page->document_button(qt::about_document::third_party_notices), page->document_button(qt::about_document::source_offer), page->close_button() };
		auto       reached = std::vector<QWidget*>{ };
		for (std::size_t step = 0; step < order.size(); ++step) {
			press(QApplication::focusWidget() != nullptr ? QApplication::focusWidget() : page, Qt::Key_Tab);
			reached.push_back(QApplication::focusWidget());
		}
		expect(reached == order, "Tab goes from Close to betterpullreview.com, the source link, the maker's link, Copy version info, the notices, the corresponding source and back to Close");
		for (auto* const control : order) {
			expect((control->focusPolicy() & Qt::TabFocus) != 0, "every control takes Tab");
			expect(!control->accessibleName().isEmpty() || !static_cast<QAbstractButton*>(control)->text().isEmpty(), "and has a name");
		}
		page->page_button(core::reference_page::source_repository)->setFocus(Qt::TabFocusReason);
		press(page->page_button(core::reference_page::source_repository), Qt::Key_Space);
		expect(backend.Pages == std::vector<core::reference_page>{ core::reference_page::source_repository }, "Space on the source link opens it");
		page->document_button(qt::about_document::source_offer)->setFocus(Qt::TabFocusReason);
		press(page->document_button(qt::about_document::source_offer), Qt::Key_Space);
		expect(backend.Documents == std::vector<qt::about_document>{ qt::about_document::source_offer }, "Space on Corresponding source opens the offer");
		page->copy_button()->setFocus(Qt::TabFocusReason);
		QApplication::clipboard()->setText(QStringLiteral("before"));
		press(page->copy_button(), Qt::Key_Space);
		expect(QApplication::clipboard()->text() == qt::about_version_report(released_facts()), "Space on Copy version info copies the report");
		press(page->copy_button(), Qt::Key_Escape);
		expect(page->isHidden(), "Esc hides the page");
		owner.hide();
	}

	void test_menu()
	{
		auto        backend = host{ };
		auto        menus   = qt::about_menu{ released_facts(), backend.actions() };
		auto        window  = QMainWindow{ };
		auto* const help    = window.menuBar()->addMenu(QStringLiteral("&Help"));
		help->setObjectName(QStringLiteral("releaseUpdateMenu"));
		help->addAction(QStringLiteral("&Keyboard Shortcuts"));
		menus.attach(*window.menuBar());
		menus.attach(*window.menuBar());
		const auto items = help->actions();
		const auto about = std::ranges::count_if(items, [](const QAction* const aAction) { return aAction->objectName() == QStringLiteral("aboutAction"); });
		expect(about == 1 && items.back()->objectName() == QStringLiteral("aboutAction") && items.back()->text() == QStringLiteral("About BetterPullReview"),
			   "a bar gains one About BetterPullReview, last in Help, however often it is attached");
		expect(items.back()->menuRole() == QAction::AboutRole, "with the About role, which macOS shows first in the application menu");
		expect(items.size() == 3 && items[1]->isSeparator(), "under a rule off macOS");
		window.show();
		items.back()->trigger();
		settle();
		auto* const page = dynamic_cast<qt::about_window*>(window.findChild<QDialog*>(QStringLiteral("aboutWindow"), Qt::FindDirectChildrenOnly));
		expect(page != nullptr && page->isVisible() && page->parentWidget() == &window, "the item shows the page as the window's own child, which takes its theme");
		page->hide();
		expect(menus.present(*window.menuBar()) == page && window.findChildren<QDialog*>(QStringLiteral("aboutWindow"), Qt::FindDirectChildrenOnly).size() == 1, "and again raises the same page");

		auto resident = QMenuBar{ };
		menus.attach(resident);
		auto* const residentHelp = resident.findChild<QMenu*>(QStringLiteral("releaseUpdateMenu"), Qt::FindDirectChildrenOnly);
		expect(residentHelp != nullptr && residentHelp->actions().size() == 1 && residentHelp->actions().front()->objectName() == QStringLiteral("aboutAction"),
			   "a bar with no Help gains one holding About");
		auto* const unowned = menus.present(resident);
		expect(unowned != nullptr && unowned->parentWidget() == nullptr && unowned->isVisible() && menus.present(resident) == unowned,
			   "a bar no window owns shows one page of its own");
		unowned->hide();
		window.hide();
		expect(backend.Pages.empty() && backend.Documents.empty(), "showing the page opens nothing");
	}

	[[nodiscard]] auto ratio(const QColor& aOne, const QColor& aOther) -> double
	{
		return qt::contrast_ratio(qt::to_appearance_color(aOne), qt::to_appearance_color(aOther));
	}

	/// The page in one built-in theme, as a window's child takes it.
	struct themed
	{
		QWidget                           Owner;
		std::unique_ptr<qt::about_window> Page;
		std::string                       Name;

		themed(const core::appearance_mode aMode, const qt::about_facts& aFacts, qt::about_actions aActions)
		{
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(aMode));
			profile.apply_to(Owner);
			Name = std::string{ core::stable_name(profile.variant()) };
			Page = std::make_unique<qt::about_window>(aFacts, std::move(aActions), &Owner);
			Page->setAttribute(Qt::WA_DeleteOnClose, false);
		}
	};

	void test_theme_change()
	{
		auto subject = themed{ core::appearance_mode::light, released_facts(), qt::about_actions{ } };
		subject.Page->show();
		settle(40);
		const auto before  = subject.Page->palette().color(QPalette::Active, QPalette::Window);
		const auto edgeWas = subject.Page->logo()->pixmap().toImage().pixelColor(subject.Page->logo()->pixmap().width() / 2, 0);
		auto       profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::dark));
		profile.apply_to(subject.Owner);
		settle(40);
		const auto after   = subject.Page->palette().color(QPalette::Active, QPalette::Window);
		const auto edgeNow = subject.Page->logo()->pixmap().toImage().pixelColor(subject.Page->logo()->pixmap().width() / 2, 0);
		expect(after == subject.Owner.palette().color(QPalette::Active, QPalette::Window) && after != before, "an open page follows its window from Light to Dark");
		expect(edgeNow != edgeWas, "and draws the logo's edge again in the new text colour");

		auto larger = subject.Owner.font();
		larger.setPointSizeF(larger.pointSizeF() * 1.5);
		const auto narrowWas = subject.Page->build_label()->width();
		subject.Owner.setFont(larger);
		settle(40);
		auto* const name = subject.Page->findChild<QLabel*>(QStringLiteral("aboutName"));
		expect(name != nullptr && std::abs(name->font().pointSizeF() - larger.pointSizeF() * 1.75) < 0.01, "a larger window font sets the name at 1.75 times it");
		expect(subject.Page->build_label()->width() > narrowWas, "and widens the column with it");
		subject.Page->hide();
	}

	void test_themes_and_logo(const QString& aRenders)
	{
		const auto scale = qApp->devicePixelRatio();
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto  backend = host{ };
			auto  subject = themed{ mode, released_facts(), backend.actions() };
			auto& page    = *subject.Page;
			page.show();
			settle(60);
			const auto& colours = page.palette();
			const auto  ground  = colours.color(QPalette::Active, QPalette::Window);
			expect(ground == subject.Owner.palette().color(QPalette::Active, QPalette::Window)
					   && colours.color(QPalette::Active, QPalette::WindowText) == subject.Owner.palette().color(QPalette::Active, QPalette::WindowText),
				   subject.Name + ": the page takes its window's theme");
			expect(ratio(page.version_label()->palette().color(QPalette::Active, page.version_label()->foregroundRole()), ground) >= 4.5, subject.Name + ": the version reads at 4.5:1");
			expect(ratio(page.build_label()->palette().color(QPalette::Active, page.build_label()->foregroundRole()), ground) >= 3.0, subject.Name + ": the build line reads at 3:1 or more");
			for (auto* const link : { page.page_button(core::reference_page::product_site), page.page_button(core::reference_page::source_repository), page.page_button(core::reference_page::game_core_tech),
									  page.document_button(qt::about_document::third_party_notices) }) {
				expect(ratio(link->palette().color(QPalette::Active, QPalette::ButtonText), ground) >= 4.5, subject.Name + ": a link reads at 4.5:1");
			}
			const auto logo = page.logo()->pixmap();
			expect(!logo.isNull() && std::abs(logo.devicePixelRatio() - page.devicePixelRatioF()) < 0.01, subject.Name + ": the logo is drawn for this screen's pixel ratio");
			expect(logo.width() == static_cast<int>(std::lround(176 * page.devicePixelRatioF())) && page.logo()->width() == 176,
				   subject.Name + ": at 176 logical pixels, one image pixel to each device pixel");
			const auto image = logo.toImage();
			expect(image.pixelColor(0, 0).alpha() == 0, subject.Name + ": the plate's corner is rounded off");
			const auto middle = image.pixelColor(image.width() / 2, image.height() / 20);
			expect(middle.alpha() == 255 && middle.lightness() < 40, subject.Name + ": and the plate is the mark's own dark ground");
			if (!aRenders.isEmpty() && (mode == core::appearance_mode::light || mode == core::appearance_mode::dark)) {
				const auto file = QStringLiteral("%1/about-%2@%3x.png").arg(aRenders, QString::fromStdString(subject.Name)).arg(scale);
				expect(page.grab().save(file), "the render is saved: " + file.toStdString());
				auto bare = qt::about_window{ source_build_facts(), qt::about_actions{ }, &subject.Owner };
				bare.show();
				settle(40);
				expect(bare.grab().save(QStringLiteral("%1/about-%2-built-from-source@%3x.png").arg(aRenders, QString::fromStdString(subject.Name)).arg(scale)), "the built-from-source render is saved");
				bare.hide();
			}
			page.hide();
		}
	}
}

auto main(int argc, char** argv) -> int
try {
	auto application = QApplication{ argc, argv };
	auto renders     = QString{ };
	if (const auto arguments = QApplication::arguments(); arguments.indexOf(QStringLiteral("--render-directory")) >= 0) {
		const auto at = arguments.indexOf(QStringLiteral("--render-directory"));
		if (at + 1 < arguments.size()) {
			renders = arguments.at(at + 1);
			static_cast<void>(QDir{ }.mkpath(renders));
		}
	}
	test_content_and_copy();
	test_links_and_documents();
	test_build_from_source();
	test_keyboard();
	test_menu();
	test_theme_change();
	test_themes_and_logo(renders);
	std::cout << "about-window-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
