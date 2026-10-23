#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/diagnostic_settings_panel.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAccessible>
#include <QApplication>
#include <QButtonGroup>
#include <QDialog>
#include <QDir>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QStyle>
#include <QVBoxLayout>

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (auto round = 0; round < 4; ++round) {
			QApplication::processEvents();
		}
	}

	auto raster(QWidget& aWidget) -> QImage
	{
		auto result = QImage{ aWidget.size() * 2, QImage::Format_ARGB32_Premultiplied };
		result.setDevicePixelRatio(2.0);
		result.fill(aWidget.palette().color(QPalette::Window));
		aWidget.render(&result, QPoint{ }, QRegion{ }, QWidget::DrawChildren);
		return result;
	}

	void capture(QWidget& aWidget, const QString& aName)
	{
		const auto directory = qEnvironmentVariable("BPR_SHARED_BUTTON_RENDER_DIR");
		if (!directory.isEmpty()) {
			expect(QDir{ }.mkpath(directory), "render directory exists");
			expect(aWidget.grab().save(QDir{ directory }.filePath(aName + QStringLiteral(".png"))), "full surface render saved");
		}
	}

	void key(QWidget& aWidget, const int aKey)
	{
		auto press   = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
		auto release = QKeyEvent{ QEvent::KeyRelease, aKey, Qt::NoModifier };
		QApplication::sendEvent(&aWidget, &press);
		QApplication::sendEvent(&aWidget, &release);
		settle();
	}

	void test_push_contract()
	{
		QDialog dialog;
		auto*   column    = new QVBoxLayout{ &dialog };
		auto*   field     = new QLineEdit{ &dialog };
		auto*   secondary = new qt::quiet_push_button{ QStringLiteral("&Cancel"), &dialog };
		auto*   primary   = new QPushButton{ QStringLiteral("Save"), &dialog };
		secondary->setAutoDefault(false);
		secondary->setAccessibleName(QStringLiteral("Cancel the draft"));
		secondary->setAccessibleDescription(QStringLiteral("Keep the saved version"));
		primary->setDefault(true);
		column->addWidget(field);
		column->addWidget(secondary);
		column->addWidget(primary);
		int cancelled = 0;
		int saved     = 0;
		QObject::connect(secondary, &QPushButton::clicked, [&cancelled] { ++cancelled; });
		QObject::connect(primary, &QPushButton::clicked, [&saved] { ++saved; });
		dialog.show();
		dialog.activateWindow();
		secondary->setFocus(Qt::TabFocusReason);
		settle();
		key(*secondary, Qt::Key_Space);
		expect(cancelled == 1 && saved == 0, "Space activates a secondary push action once");
		field->setFocus();
		key(*field, Qt::Key_Return);
		expect(saved == 1 && cancelled == 1 && primary->isDefault(), "Return keeps the dialog's primary default");
		QPushButton mnemonicReference{ QStringLiteral("&Cancel") };
		expect(secondary->shortcut() == mnemonicReference.shortcut(), "mnemonics follow the platform native push behavior");
		auto* accessible = QAccessible::queryAccessibleInterface(secondary);
		expect(accessible != nullptr && accessible->role() == QAccessible::Button
				   && accessible->text(QAccessible::Name) == QStringLiteral("Cancel the draft")
				   && accessible->text(QAccessible::Description) == QStringLiteral("Keep the saved version"),
			   "native push accessibility survives");
		secondary->setEnabled(false);
		key(*secondary, Qt::Key_Space);
		secondary->click();
		expect(cancelled == 1, "disabled push actions refuse keyboard and direct clicks");
		secondary->setEnabled(true);
		const auto before = secondary->sizeHint();
		secondary->setText(QStringLiteral("Annuler les modifications de cette proposition de révision"));
		expect(secondary->sizeHint().width() > before.width(), "translated labels grow the measured width");
		auto larger = secondary->font();
		larger.setPointSizeF(larger.pointSizeF() * 1.5);
		secondary->setFont(larger);
		expect(secondary->sizeHint().height() > before.height(), "font changes refresh geometry");

		// Exact raster equality with the native class proves specialized fallbacks.
		qt::quiet_push_button quiet{ QStringLiteral("Action") };
		QPushButton           native{ QStringLiteral("Action") };
		quiet.setAutoDefault(false);
		native.setAutoDefault(false);
		quiet.resize(140, 32);
		native.resize(140, 32);
		quiet.setDefault(true);
		native.setDefault(true);
		expect(raster(quiet) == raster(native), "a default button retains native painting");
		// The update panel's lead command becomes the default and back; its
		// measured size follows, so the panel refreshes geometry on the change.
		expect(quiet.sizeHint() == native.sizeHint(), "a default button keeps the native size");
		quiet.setDefault(false);
		expect(quiet.sizeHint() != native.sizeHint(), "a secondary button measures the shared chip again");
		native.setDefault(false);
		QMenu quietMenu;
		QMenu nativeMenu;
		quiet.setMenu(&quietMenu);
		native.setMenu(&nativeMenu);
		expect(raster(quiet) == raster(native), "a push menu retains its native arrow and painting");
		quiet.setMenu(nullptr);
		native.setMenu(nullptr);
		quiet.setText({ });
		native.setText({ });
		const auto glyph = QApplication::style()->standardIcon(QStyle::SP_DialogOpenButton);
		quiet.setIcon(glyph);
		native.setIcon(glyph);
		expect(raster(quiet) == raster(native), "icon-only push tools retain native painting");
		quiet.setText(QStringLiteral("Open details"));
		quiet.setIconSize(QSize{ 20, 20 });
		const auto iconWidth = quiet.sizeHint().width();
		quiet.setIcon({ });
		expect(iconWidth >= quiet.sizeHint().width() + 24, "text and icon sizing reserves the icon and gap");
	}

	void test_production_surfaces()
	{
		qt::diagnostic_settings_panel                    diagnostics;
		std::vector<application::diagnostic_preferences> preferences;
		int                                              opened = 0;
		diagnostics.set_dispatch({ .Apply = [&preferences](const auto& aPreferences) { preferences.push_back(aPreferences); }, .Open = [&opened] { ++opened; } });
		auto projection     = application::diagnostic_settings_projection{ };
		projection.Writable = true;
		diagnostics.adopt(projection);
		auto* detail = diagnostics.findChild<QButtonGroup*>(QStringLiteral("diagnosticDetail"));
		expect(detail != nullptr && detail->buttons().size() == 2, "the real diagnostic factory supplies its choices");
		auto* selected = detail->button(0);
		auto* other    = detail->button(1);
		auto* open     = diagnostics.findChild<QPushButton*>(QStringLiteral("diagnosticOpen"));
		auto* clear    = diagnostics.findChild<QPushButton*>(QStringLiteral("diagnosticClear"));
		for (auto* control : { selected, other, static_cast<QAbstractButton*>(open) }) {
			expect(dynamic_cast<qt::quiet_push_button*>(control) != nullptr, "diagnostic choices and Open opt in");
		}
		expect(dynamic_cast<qt::quiet_push_button*>(clear) == nullptr, "destructive Clear keeps its separate presentation");
		qt::profile_management_window profiles;
		profiles.adopt({ .Profiles = { { core::default_bpr_profile_id, "Default", application::profile_management_runtime::closed } }, .Ready = true });
		for (auto* control : { profiles.create_button(), profiles.rename_button(), profiles.cancel_button(), profiles.try_again_button() }) {
			expect(dynamic_cast<qt::quiet_push_button*>(control) != nullptr, "profile secondaries opt in");
		}
		expect(dynamic_cast<qt::quiet_push_button*>(profiles.open_button()) == nullptr && dynamic_cast<qt::quiet_push_button*>(profiles.save_button()) == nullptr,
			   "profile Open and Save retain primary defaults");
		qt::workbench_window workbench;
		workbench.settings()->resize(1100, 900);
		const auto original = diagnostics.palette();
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			qt::appearance_profile profile;
			static_cast<void>(profile.select_mode(mode));
			const auto colours = profile.derive(original);
			const auto name    = QString::fromUtf8(core::stable_name(profile.variant()).data(), static_cast<qsizetype>(core::stable_name(profile.variant()).size()));
			diagnostics.setPalette(colours);
			diagnostics.resize(760, 550);
			diagnostics.show();
			projection.Capture.Preferences.Detail = application::diagnostic_detail::standard;
			diagnostics.adopt(projection);
			selected->clearFocus();
			other->clearFocus();
			other->setAttribute(Qt::WA_UnderMouse, false);
			settle();
			const auto ground = colours.color(QPalette::Window);
			const auto chosen = raster(*selected);
			const auto bare   = raster(*other);
			const auto edge   = chosen.pixelColor(0, selected->height());
			expect(qt::contrast_ratio(qt::to_appearance_color(edge), qt::to_appearance_color(ground)) >= 3.0, "push choices share the 3:1 border in every theme");
			expect(bare.pixelColor(0, other->height()) == ground, "unselected push choices stay bare");
			capture(diagnostics, name + QStringLiteral("-diagnostics"));
			other->setAttribute(Qt::WA_UnderMouse, true);
			const auto hover = raster(*other);
			other->setDown(true);
			expect(raster(*other) != hover, "push press feedback differs from hover");
			capture(diagnostics, name + QStringLiteral("-diagnostics-pressed"));
			other->setDown(false);
			other->setAttribute(Qt::WA_UnderMouse, false);
			diagnostics.activateWindow();
			other->setFocus(Qt::TabFocusReason);
			settle();
			expect(other->hasFocus() && raster(*other) != bare, "keyboard focus paints a visible push ring");
			capture(diagnostics, name + QStringLiteral("-diagnostics-focus"));
			const auto count = preferences.size();
			key(*other, Qt::Key_Space);
			expect(preferences.size() == count + 1U && preferences.back().Detail == application::diagnostic_detail::verbose
					   && selected->isChecked() && !other->isChecked(),
				   "Space dispatches once and keeps the accepted diagnostic choice until publication");
			projection.Capture.Preferences = preferences.back();
			diagnostics.adopt(projection);
			expect(other->isChecked() && !selected->isChecked(), "published diagnostic choices stay exclusive");
			open->click();
			expect(opened > 0, "the Open command still dispatches");
			const auto enabled = raster(*other);
			projection.Busy    = true;
			diagnostics.adopt(projection);
			other->click();
			expect(preferences.size() == count + 1U && raster(*other) != enabled, "busy production choices are disabled and refuse dispatch");
			capture(diagnostics, name + QStringLiteral("-diagnostics-disabled"));
			projection.Busy = false;
			diagnostics.adopt(projection);
			profiles.setPalette(colours);
			profiles.present();
			settle();
			capture(profiles, name + QStringLiteral("-profiles"));
			profiles.rename_button()->click();
			expect(profiles.editing(), "the migrated Rename still opens the editor");
			profiles.cancel_button()->click();
			expect(!profiles.editing(), "the migrated Cancel keeps native command behavior");
			workbench.setPalette(colours);
			for (const auto page : { qt::settings_page::home_and_queue, qt::settings_page::notifications, qt::settings_page::agents_and_context }) {
				workbench.settings()->present(page);
				settle();
				capture(*workbench.settings(), name + QStringLiteral("-settings-%1").arg(static_cast<int>(page)));
			}
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication app{ argc, argv };
	test_push_contract();
	test_production_surfaces();
	std::cout << "quiet-buttons-tests:pass qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString()
			  << " style=" << QApplication::style()->name().toStdString() << " dpr=" << app.devicePixelRatio() << '\n';
	return EXIT_SUCCESS;
}
