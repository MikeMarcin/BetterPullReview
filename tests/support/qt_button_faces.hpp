#ifndef BPR_TESTS_SUPPORT_QT_BUTTON_FACES_HPP
#define BPR_TESTS_SUPPORT_QT_BUTTON_FACES_HPP

/// \file
/// \brief Measures quiet buttons against the face they are actually painted on.
///
/// \details The macOS style gives an auto-raise `QToolButton` or a flat
/// `QPushButton` outside a toolbar a native bezel in the *system* appearance's
/// grey, whatever the profile's palette. A Dark profile on a light system put
/// light marks on a pale face (about 1.1:1), and the offscreen renders in the
/// default style never showed it (BPR-2056, BPR-2060). These helpers find
/// every such button on a surface, or take the ones a test names, and measure
/// each one's ink against its painted face inside the surface as drawn, at
/// rest, hovered, pressed, with keyboard focus and, when it is checkable, both
/// checked and not. Words are held to the profile's text floor and a mark to
/// its indicator floor. A checkable button's two states must differ by at
/// least the indicator floor somewhere on its face, and accessibility must
/// report the state it shows.
///
/// Run under `QT_QPA_PLATFORM=cocoa`, the macOS style looks are painted as
/// the application paints them; every surface must carry
/// `Qt::WA_DontShowOnScreen` so nothing reaches the desktop.

#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"

#include <QAbstractButton>
#include <QAccessible>
#include <QApplication>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QPushButton>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::tests::button_faces {
	/// \brief A style, the system appearance it runs under (unset: whatever
	/// the platform says), and the name its results and pictures carry.
	struct look
	{
		QString         Style;
		Qt::ColorScheme Scheme = Qt::ColorScheme::Unknown;
		QString         Key;
	};

	/// \brief One built-in theme.
	struct theme
	{
		const char*           Name = "";
		core::appearance_mode Mode = core::appearance_mode::light;
	};

	/// \brief The four built-in themes.
	[[nodiscard]] inline auto themes() -> std::array<theme, 4>
	{
		return {
			theme{ "light", core::appearance_mode::light },
			theme{ "dark", core::appearance_mode::dark },
			theme{ "high-contrast-light", core::appearance_mode::high_contrast_light },
			theme{ "high-contrast-dark", core::appearance_mode::high_contrast_dark },
		};
	}

	/// \brief The default style, and the macOS style under a light and a dark
	/// system appearance wherever it exists. On macOS the default style is
	/// the macOS style, so it is run only as those two.
	[[nodiscard]] inline auto looks() -> std::vector<look>
	{
		static const auto original = QApplication::style()->name();
		const auto        macOS    = original.compare(QStringLiteral("macOS"), Qt::CaseInsensitive) == 0;
		auto              result   = macOS ? std::vector<look>{ } : std::vector<look>{ look{ .Style = original, .Key = original.toLower() } };
		if (macOS || QStyleFactory::keys().contains(QStringLiteral("macOS"), Qt::CaseInsensitive)) {
			result.push_back(look{ .Style = QStringLiteral("macOS"), .Scheme = Qt::ColorScheme::Light, .Key = QStringLiteral("macos-system-light") });
			result.push_back(look{ .Style = QStringLiteral("macOS"), .Scheme = Qt::ColorScheme::Dark, .Key = QStringLiteral("macos-system-dark") });
		}
		return result;
	}

	/// \brief Puts \p aLook in use. Returns whether the style took.
	inline auto use(const look& aLook) -> bool
	{
		QApplication::setStyle(QStyleFactory::create(aLook.Style));
		if (aLook.Scheme != Qt::ColorScheme::Unknown) {
			QGuiApplication::styleHints()->setColorScheme(aLook.Scheme);
		}
		return QApplication::style()->name().compare(aLook.Style, Qt::CaseInsensitive) == 0;
	}

	/// \brief A profile in \p aMode, as the window applies it.
	[[nodiscard]] inline auto profile_for(const core::appearance_mode aMode) -> qt::appearance_profile
	{
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(aMode));
		return profile;
	}

	/// \brief The floors of \p aProfile: words, and a mark or state.
	struct floors
	{
		double Text      = 4.5;
		double Indicator = 3.0;
	};

	[[nodiscard]] inline auto floors_of(const qt::appearance_profile& aProfile) -> floors
	{
		return {
			.Text      = static_cast<double>(core::minimum_contrast_hundredths(core::contrast_kind::text, aProfile.variant())) / 100.0,
			.Indicator = static_cast<double>(core::minimum_contrast_hundredths(core::contrast_kind::indicator, aProfile.variant())) / 100.0,
		};
	}

	[[nodiscard]] inline auto ratio(const QColor& aOne, const QColor& aOther) -> double
	{
		return static_cast<double>(core::contrast_ratio_hundredths(qt::to_appearance_color(aOne), qt::to_appearance_color(aOther))) / 100.0;
	}

	/// \brief The ground (the commonest colour) and the ink (the pixel that
	/// differs from it most) of \p aImage.
	[[nodiscard]] inline auto ground_and_ink(const QImage& aImage) -> std::pair<QColor, QColor>
	{
		auto counts = std::map<QRgb, int>{ };
		for (auto y = 0; y < aImage.height(); ++y) {
			for (auto x = 0; x < aImage.width(); ++x) {
				++counts[aImage.pixel(x, y) | 0xFF000000U];
			}
		}
		if (counts.empty()) {
			return { };
		}
		const auto ground = QColor{ std::ranges::max_element(counts, { }, &std::map<QRgb, int>::value_type::second)->first };
		auto       ink    = ground;
		auto       best   = 1.0;
		for (const auto& [pixel, count] : counts) {
			const auto candidate = QColor{ pixel };
			if (const auto measured = ratio(candidate, ground); measured > best) {
				best = measured;
				ink  = candidate;
			}
		}
		return { ground, ink };
	}

	/// \brief \p aButton as \p aRoot paints it, in device pixels.
	[[nodiscard]] inline auto painted(QWidget& aRoot, QAbstractButton& aButton, const int aInset) -> QImage
	{
		const auto place = QRect{ aButton.mapTo(&aRoot, QPoint{ 0, 0 }), aButton.size() }.adjusted(aInset, aInset, -aInset, -aInset);
		return aRoot.grab(place).toImage();
	}

	/// \brief Whether the button shows words (held to the text floor) rather
	/// than a mark alone (held to the indicator floor).
	[[nodiscard]] inline auto shows_words(const QAbstractButton& aButton) -> bool
	{
		if (aButton.text().isEmpty()) {
			return false;
		}
		if (const auto* const tool = qobject_cast<const QToolButton*>(&aButton); tool != nullptr) {
			return tool->icon().isNull() || tool->toolButtonStyle() != Qt::ToolButtonIconOnly;
		}
		return true;
	}

	/// \brief A quiet button: an auto-raise tool button or a flat push button.
	[[nodiscard]] inline auto quiet(const QAbstractButton& aButton) -> bool
	{
		if (const auto* const tool = qobject_cast<const QToolButton*>(&aButton); tool != nullptr) {
			return tool->autoRaise();
		}
		if (const auto* const push = qobject_cast<const QPushButton*>(&aButton); push != nullptr) {
			return push->isFlat();
		}
		return false;
	}

	[[nodiscard]] inline auto in_toolbar(const QWidget& aWidget) -> bool
	{
		for (const auto* parent = aWidget.parentWidget(); parent != nullptr; parent = parent->parentWidget()) {
			if (qobject_cast<const QToolBar*>(parent) != nullptr) {
				return true;
			}
		}
		return false;
	}

	/// \brief Every quiet button \p aRoot shows, outside a real toolbar.
	[[nodiscard]] inline auto quiet_buttons(QWidget& aRoot) -> std::vector<QAbstractButton*>
	{
		auto found = std::vector<QAbstractButton*>{ };
		for (auto* const button : aRoot.findChildren<QAbstractButton*>()) {
			if (quiet(*button) && button->isVisibleTo(&aRoot) && !in_toolbar(*button) && button->width() > 4 && button->height() > 4) {
				found.push_back(button);
			}
		}
		return found;
	}

	/// \brief The worst measurement of a sweep, and what failed.
	struct report
	{
		double                   Worst = 1000.0;
		std::string              WorstName;
		std::vector<std::string> Failures;
		std::vector<std::string> Known;
		std::vector<std::string> Swept;

		void merge(const report& aOther)
		{
			if (aOther.Worst < Worst) {
				Worst     = aOther.Worst;
				WorstName = aOther.WorstName;
			}
			Failures.insert(Failures.end(), aOther.Failures.begin(), aOther.Failures.end());
			Known.insert(Known.end(), aOther.Known.begin(), aOther.Known.end());
			Swept.insert(Swept.end(), aOther.Swept.begin(), aOther.Swept.end());
		}
	};

	/// \brief The parts, one after another.
	[[nodiscard]] inline auto joined(const std::initializer_list<std::string_view> aParts) -> std::string
	{
		auto result = std::string{ };
		for (const auto part : aParts) {
			result.append(part);
		}
		return result;
	}

	[[nodiscard]] inline auto name_of(const QAbstractButton& aButton) -> std::string
	{
		auto name = aButton.objectName();
		if (name.isEmpty()) {
			name = aButton.accessibleName().isEmpty() ? aButton.text() : aButton.accessibleName();
		}
		return name.toStdString();
	}

	/// \brief A known shortfall outside the face: the button's words are held
	/// to `Floor` instead of the theme's, for the reason given, and every
	/// measurement between the two is reported as known rather than passed.
	struct known_shortfall
	{
		double      Floor = 4.5;
		std::string Reason;
	};

	/// \brief Measures each of \p aButtons inside \p aRoot in every state a
	/// person sees it in; each ends as it began. Disabled buttons are named
	/// but not measured. Every measurement is printed. \p aKnown names, by
	/// object name, buttons with a known shortfall.
	[[nodiscard]] inline auto sweep(QWidget& aRoot, const std::vector<QAbstractButton*>& aButtons, const std::string& aName, const floors& aFloors,
									const std::map<std::string, known_shortfall>& aKnown = { }) -> report
	{
		auto       result = report{ };
		const auto inset  = static_cast<int>(std::ceil(qt::focus_ring_width)) + 1;
		for (auto* const button : aButtons) {
			const auto name  = aName + " " + name_of(*button);
			const auto words = shows_words(*button);
			const auto floor = words ? aFloors.Text : aFloors.Indicator;
			result.Swept.push_back(name_of(*button));
			if (!button->isEnabled()) {
				std::cout << "button-face " << name << ": disabled, not measured\n";
				continue;
			}
			const auto check = [&](const std::string& aState) {
				button->update();
				QApplication::processEvents();
				const auto [ground, ink] = ground_and_ink(painted(aRoot, *button, inset));
				const auto measured      = ratio(ink, ground);
				std::cout << "button-face " << name << ' ' << aState << " (" << (words ? "words" : "mark") << "): ground " << ground.name().toStdString() << " ink "
						  << ink.name().toStdString() << " ratio " << measured << '\n';
				if (measured < result.Worst) {
					result.Worst     = measured;
					result.WorstName = joined({ name, " ", aState });
				}
				if (measured + 0.005 < floor) {
					const auto known = aKnown.find(name_of(*button));
					if (known != aKnown.end() && measured + 0.005 >= std::min(floor, known->second.Floor)) {
						result.Known.push_back(joined({ name, " ", aState, ": ", std::to_string(measured), " under ", std::to_string(floor), ", known: ", known->second.Reason }));
					} else {
						result.Failures.push_back(joined({ name, " ", aState, ": ", std::to_string(measured), " under ", std::to_string(floor) }));
					}
				}
			};
			const auto states = [&](const std::string& aPrefix) {
				check(aPrefix + "at rest");
				button->setAttribute(Qt::WA_UnderMouse, true);
				check(aPrefix + "hovered");
				button->setDown(true);
				check(aPrefix + "pressed");
				button->setDown(false);
				button->setAttribute(Qt::WA_UnderMouse, false);
				button->update();
			};
			states("");
			if (button->isCheckable()) {
				const auto was     = button->isChecked();
				const auto blocked = button->blockSignals(true);
				const auto before  = painted(aRoot, *button, 0);
				button->setChecked(!was);
				QApplication::processEvents();
				const auto after = painted(aRoot, *button, 0);
				states(was ? "unchecked, " : "checked, ");
				auto apart = 1.0;
				if (before.size() == after.size()) {
					for (auto y = 0; y < before.height(); ++y) {
						for (auto x = 0; x < before.width(); ++x) {
							apart = std::max(apart, ratio(before.pixelColor(x, y), after.pixelColor(x, y)));
						}
					}
				}
				std::cout << "button-face " << name << " checked against unchecked: " << apart << '\n';
				if (apart + 0.005 < aFloors.Indicator) {
					result.Failures.push_back(name + ": its checked state differs by " + std::to_string(apart) + ", under " + std::to_string(aFloors.Indicator));
				}
				if (auto* const accessible = QAccessible::queryAccessibleInterface(button); accessible != nullptr) {
					const auto state = accessible->state();
					if (!state.checkable || static_cast<bool>(state.checked) != button->isChecked()) {
						result.Failures.push_back(name + ": accessibility does not report the checked state it shows");
					}
				}
				button->setChecked(was);
				button->blockSignals(blocked);
				QApplication::processEvents();
			}
			if ((button->focusPolicy() & Qt::TabFocus) != 0) {
				button->clearFocus();
				button->setFocus(Qt::TabFocusReason);
				check("with keyboard focus");
				button->clearFocus();
			}
		}
		return result;
	}

	/// \brief The directory renders go to, or empty: `BPR_BUTTON_FACE_RENDER_DIR`.
	[[nodiscard]] inline auto render_directory() -> QString
	{
		const auto folder = qEnvironmentVariable("BPR_BUTTON_FACE_RENDER_DIR");
		if (!folder.isEmpty()) {
			QDir{ }.mkpath(folder);
		}
		return folder;
	}

	/// \brief Saves \p aWidget as `<name>.png` in the render directory, if any.
	inline void render(QWidget& aWidget, const QString& aName)
	{
		if (const auto folder = render_directory(); !folder.isEmpty()) {
			static_cast<void>(aWidget.grab().save(folder + QLatin1Char{ '/' } + aName + QStringLiteral(".png")));
		}
	}
} // namespace bpr::tests::button_faces

#endif
