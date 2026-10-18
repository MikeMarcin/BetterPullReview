/// \file
/// \brief [[BPR-1602]]. Settings › Storage & privacy › Troubleshooting as the
/// owner chose it (design A, with shorter rows): four rows, each asking its
/// confirmation; the confirmation's Removes and Keeps lists; the
/// acknowledgement that gates a destructive reset; Cancel as the default; the
/// request that reaches the dispatch; Settings search; the recovery dialog's
/// three answers; and the status notice. Saves Light and Dark renders when
/// `BPR_PROFILE_RESET_RENDER_DIR` names a folder.
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/profile_data_reset_views.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QStatusBar>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using reset = core::profile_data_reset;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (auto round = 0; round < 6; ++round) {
			QApplication::processEvents();
		}
	}

	void save_render(QWidget& aWidget, const QString& aName)
	{
		const auto folder = qEnvironmentVariable("BPR_PROFILE_RESET_RENDER_DIR");
		if (!folder.isEmpty()) {
			static_cast<void>(aWidget.grab().save(folder + QLatin1Char{ '/' } + aName + QStringLiteral(".png")));
		}
	}

	/// The confirmations open under `aOwner`; the class has no Q_OBJECT, so by
	/// object name.
	auto confirmations(QWidget& aOwner) -> std::vector<qt::profile_reset_confirmation*>
	{
		auto found = std::vector<qt::profile_reset_confirmation*>{ };
		for (auto* dialog : aOwner.findChildren<QDialog*>(QStringLiteral("profileResetConfirmation"))) {
			if (auto* question = dynamic_cast<qt::profile_reset_confirmation*>(dialog)) {
				found.push_back(question);
			}
		}
		return found;
	}

	auto open_question(qt::profile_reset_panel& aPanel, const reset aReset) -> qt::profile_reset_confirmation*
	{
		aPanel.button(aReset)->click();
		settle();
		qt::profile_reset_confirmation* question = nullptr;
		for (auto* candidate : confirmations(*aPanel.window())) {
			if (candidate->isVisible() && candidate->reset() == aReset) {
				question = candidate;
			}
		}
		expect(question != nullptr && question->isVisible() && question->reset() == aReset, "the row's button asks its confirmation");
		return question;
	}

	void test_the_group_on_storage_and_privacy()
	{
		qt::workbench_window window;
		window.set_profile_label(QStringLiteral("Default"));
		auto* const panel = window.profile_reset_panel_widget();
		expect(panel != nullptr && panel->title() == QStringLiteral("Troubleshooting"), "Storage & privacy has a Troubleshooting group");
		auto* const page = window.settings()->page_widget(qt::settings_page::storage_and_privacy);
		expect(page != nullptr && page->isAncestorOf(panel), "the group is on the Storage & privacy page");
		for (const auto& [each, text] : { std::pair{ reset::caches, "Clear caches…" }, std::pair{ reset::settings, "Reset settings…" },
										  std::pair{ reset::review_data, "Reset review data…" }, std::pair{ reset::everything, "Reset everything…" } }) {
			auto* const button = panel->button(each);
			expect(button != nullptr && button->text() == QString::fromUtf8(text), "each row's button names its reset");
			expect(!button->accessibleDescription().isEmpty() && button->accessibleDescription().size() < 60, "each row says one short line");
		}
		// Settings search finds each reset by its name on this page.
		window.settings()->present(qt::settings_page::storage_and_privacy);
		settle();
		window.settings()->search_field()->setText(QStringLiteral("Reset review"));
		settle();
		auto* const results = window.settings()->search_results();
		expect(results->count() > 0 && results->item(0)->text().contains(QStringLiteral("Reset review data")), "search finds Reset review data");
		window.settings()->search_field()->clear();
		settle();
	}

	void test_confirmations()
	{
		qt::workbench_window window;
		window.set_profile_label(QStringLiteral("Default"));
		window.show();
		auto* const panel    = window.profile_reset_panel_widget();
		auto        requests = std::vector<core::profile_data_reset_request>{ };
		window.set_profile_data_reset_dispatch([&requests](const core::profile_data_reset_request& aRequest) { requests.push_back(aRequest); });

		// Clear caches: one confirmation, no acknowledgement and no copy.
		auto* caches = open_question(*panel, reset::caches);
		expect(caches->wording().Title == QStringLiteral("Clear caches for “Default”?"), "the title names the profile");
		expect(caches->copy_box() == nullptr && caches->acknowledge_box() == nullptr, "Clear caches asks no acknowledgement and offers no copy");
		expect(caches->confirm_button()->isEnabled(), "Clear caches can be confirmed at once");
		expect(caches->cancel_button()->isDefault() && QApplication::focusWidget() == caches->cancel_button(), "Cancel is the default and has focus");
		QKeyEvent escape{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(caches, &escape);
		settle();
		expect(requests.empty(), "Escape cancels without a request");

		caches = open_question(*panel, reset::caches);
		caches->confirm_button()->click();
		settle();
		expect(requests.size() == 1U && requests.back() == core::profile_data_reset_request{ reset::caches }, "confirming Clear caches dispatches it");

		// Reset settings keeps saved repositories and observer rules.
		auto* settings = open_question(*panel, reset::settings);
		expect(settings->wording().Keeps.contains(QStringLiteral("Saved repositories and servers")) && settings->wording().Keeps.contains(QStringLiteral("Observer rules")),
			   "Reset settings says it keeps saved repositories and observer rules");
		expect(settings->acknowledge_box() == nullptr, "Reset settings needs no acknowledgement");
		settings->cancel_button()->click();
		settle();

		// Reset review data waits for the acknowledgement; the copy is on.
		auto* review = open_question(*panel, reset::review_data);
		expect(review->copy_box() != nullptr && review->copy_box()->isChecked(), "a copy is offered and on by default");
		expect(review->acknowledge_box() != nullptr && !review->acknowledge_box()->isChecked(), "the acknowledgement starts unticked");
		expect(!review->confirm_button()->isEnabled(), "Reset review data waits for the acknowledgement");
		review->confirm_button()->click();
		settle();
		expect(requests.size() == 1U, "a disabled reset dispatches nothing");
		review->acknowledge_box()->setChecked(true);
		expect(review->confirm_button()->isEnabled(), "the acknowledgement enables the reset");
		review->copy_box()->setChecked(false);
		review->confirm_button()->click();
		settle();
		expect(requests.size() == 2U && requests.back() == core::profile_data_reset_request{ reset::review_data, true, false }, "the request carries the acknowledgement and the copy choice");

		// Reset everything names the saved repositories and rules it clears.
		auto* everything = open_question(*panel, reset::everything);
		expect(everything->wording().Removes.join(QLatin1Char{ ' ' }).contains(QStringLiteral("saved repositories, observer rules")), "Reset everything says it clears saved repositories and observer rules");
		expect(everything->wording().Keeps.contains(QStringLiteral("Sign-in to your accounts and hosts")), "Reset everything keeps sign-in");
		everything->acknowledge_box()->setChecked(true);
		everything->confirm_button()->click();
		settle();
		expect(requests.size() == 3U && requests.back() == core::profile_data_reset_request{ reset::everything, true, true }, "Reset everything dispatches with its copy");

		// A refusal is said on the group; a completed reset on the status bar.
		window.refuse_profile_data_reset(QStringLiteral("BetterPullReview is busy with this profile."));
		expect(panel->status_label()->isVisibleTo(panel) && panel->status_label()->text().contains(QStringLiteral("busy")), "a refusal is said on the group");
		window.present_profile_data_reset({ reset::caches, core::release_maintenance_reason::none, 12U }, { });
		expect(window.statusBar()->currentMessage().startsWith(QStringLiteral("Caches cleared.")), "the reopened window says what was reset");
		expect(!panel->status_label()->isVisibleTo(panel), "and clears the group's refusal");
	}

	void test_notices()
	{
		using reason = core::release_maintenance_reason;
		expect(qt::profile_data_reset_notice({ reset::everything, reason::none, 40U, true }, QStringLiteral("BetterPullReview reset 2026-10-18 14.02.33")).contains(QStringLiteral("BetterPullReview reset 2026-10-18 14.02.33")),
			   "the notice names the copy folder");
		expect(qt::profile_data_reset_notice({ reset::settings, reason::none, 4U, false, false }, { }).contains(QStringLiteral("Reset everything clears them")), "a queue display that could not be restored is said");
		expect(qt::profile_data_reset_notice({ reset::review_data, reason::incomplete_inventory }, { }).contains(QStringLiteral("Nothing was removed")), "a refusal says nothing was removed");
	}

	void test_recovery_dialog()
	{
		{
			qt::profile_recovery_dialog dialog{ QStringLiteral("Default"), QStringLiteral("A local database in this profile could not be opened.") };
			expect(dialog.windowTitle() == QStringLiteral("Profile “Default” couldn’t open"), "the dialog names the profile that could not open");
			expect(dialog.clear_button()->isDefault() && dialog.clear_button()->text() == QStringLiteral("Clear caches and try again"), "clearing caches is the default");
			expect(dialog.details_label()->text().contains(QStringLiteral("--reset-profile-data")), "Details name the start-up flag");
			dialog.clear_button()->click();
			expect(dialog.result() == QDialog::Accepted && dialog.request() == core::profile_data_reset_request{ reset::caches }, "Clear caches and try again asks for the caches");
		}
		{
			qt::profile_recovery_dialog dialog{ QStringLiteral("Default"), QStringLiteral("cause") };
			dialog.quit_button()->click();
			expect(dialog.result() == QDialog::Rejected && !dialog.request(), "Quit asks for nothing");
		}
		{
			qt::profile_recovery_dialog dialog{ QStringLiteral("Default"), QStringLiteral("cause") };
			dialog.show();
			dialog.everything_button()->click();
			settle();
			const auto  open     = confirmations(dialog);
			auto* const question = open.empty() ? nullptr : open.back();
			expect(question != nullptr && question->isVisible() && question->reset() == reset::everything, "Reset everything… asks its confirmation first");
			expect(!dialog.request(), "nothing is chosen before it is confirmed");
			question->acknowledge_box()->setChecked(true);
			question->confirm_button()->click();
			settle();
			expect(dialog.request() == core::profile_data_reset_request{ reset::everything, true, true }, "the confirmed reset is the dialog's answer");
		}
		{
			qt::profile_recovery_dialog dialog{ QStringLiteral("Default"), { } };
			expect(dialog.windowTitle() == QStringLiteral("Reset local data for “Default”?"), "asked for with the flag, the dialog says so");
			dialog.choose(qt::profile_recovery_choice::reset_everything, core::profile_data_reset_request{ reset::everything, false, false });
			expect(!dialog.request(), "an unacknowledged answer is refused");
		}
	}

	void test_renders()
	{
		if (qEnvironmentVariable("BPR_PROFILE_RESET_RENDER_DIR").isEmpty()) {
			return;
		}
		qt::workbench_window window;
		window.set_profile_label(QStringLiteral("Default"));
		window.set_storage_location(QStringLiteral("~/Library/Application Support/BetterPullReview/BetterPullReview"));
		const auto original = window.palette();
		for (const auto& [mode, name] : { std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			const auto palette = profile.derive(original);
			window.setPalette(palette);
			window.settings()->setPalette(palette);
			window.settings()->resize(1100, 820);
			window.settings()->present(qt::settings_page::storage_and_privacy);
			settle();
			if (auto* const scroll = window.settings()->findChild<QScrollArea*>(QStringLiteral("settingsPageScroll-%1").arg(static_cast<int>(qt::settings_page::storage_and_privacy)))) {
				scroll->ensureWidgetVisible(window.profile_reset_panel_widget(), 0, 40);
			}
			settle();
			save_render(*window.settings(), QStringLiteral("page-storage-") + name);
			for (const auto& [each, slug] : { std::pair{ reset::caches, "caches" }, std::pair{ reset::settings, "settings" }, std::pair{ reset::review_data, "review" }, std::pair{ reset::everything, "everything" } }) {
				auto* const question = new qt::profile_reset_confirmation{ nullptr, each, QStringLiteral("Default") };
				question->setPalette(palette);
				question->show();
				settle();
				save_render(*question, QStringLiteral("confirm-%1-%2").arg(QString::fromLatin1(slug), name));
				question->close();
				settle();
			}
			qt::profile_recovery_dialog dialog{ QStringLiteral("Default"), QStringLiteral("A local database in this profile could not be opened.") };
			dialog.setPalette(palette);
			dialog.show();
			settle();
			save_render(dialog, QStringLiteral("recovery-") + name);
		}
		window.setPalette(original);
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_the_group_on_storage_and_privacy();
	test_confirmations();
	test_notices();
	test_recovery_dialog();
	test_renders();
	std::cout << "profile-data-reset-views-tests:pass\n";
	return EXIT_SUCCESS;
}
